#include <algorithm>

#include "dualq_coupled_aqm.hh"
#include "timestamp.hh"
#include "exception.hh"
#include "ezio.hh"

#include "abstract_packet_queue.hh"


using namespace std;
using namespace PollerShortNames;

DualQCoupledAQM::DualQCoupledAQM( const string & args )
  : byte_limit_( get_arg( args, "bytes" ) ),
    packet_limit_( get_arg( args, "packets" ) ),
    //k_ ( get_arg( args, "k" ) ),
    l4s_queue_ ( L4SPacketQueue ( args ) ),
    classic_queue_ ( CLASSICPacketQueue ( args ) ),
    scheduler_type_ ( static_cast<SchedulerType> (get_arg( args, "sched" ))),
    target_ms_ ( get_arg( args, "target" ) ),
    max_rtt_ms_ ( get_arg( args, "max_rtt" ) ),
    alpha_ ( get_arg( args, "alpha" ) ),
    beta_ ( get_arg( args, "beta" ) ),
    t_update_ms_ ( get_arg( args, "tupdate" ) ),
    overload_drop_pkts_ ( 0 ),
    overflow_drop_pkts_ ( 0 ),
    not_ect_drop_pkts_ ( 0 ),
    pp_ ( 0 ),
    pp_l_ ( 0 ),
    p_l_ ( 0 ),
    p_cl_ ( 0 ),
    p_c_ ( 0 ),
    k_ ( 2 ),
    l4s_drop_on_overload_ ( true )
{
    if ( packet_limit_ == 0 and byte_limit_ == 0 ) {
        packet_limit_ = 10000; /* default value from Linux code. Represents 125 ms at 1 Gbps */
        byte_limit_ = packet_limit_ * MTU;
    }
    else if (packet_limit_ != 0) {
        // Prioritize packet_limit_ over byte_limit_
        byte_limit_ = packet_limit_ * MTU;

    }
    else if (byte_limit_ != 0) {
        packet_limit_ = byte_limit_ / MTU;
    }

    max_prob = 1.0;

    if ( target_ms_ == 0 ) target_ms_ = 15; 
    if ( max_rtt_ms_ == 0 ) max_rtt_ms_ = 100;
    if ( t_update_ms_ == 0 ) t_update_ms_ = 16; // RFC 9332: Tupdate = min(target, RTT_max/3)

    /* From RFC 9332:
        13:   alpha = 0.1 * Tupdate / RTT_max^2      % PI integral gain in Hz
        14:   beta = 0.3 / RTT_max                   % PI proportional gain in Hz */

    if ( alpha_ == 0 ) alpha_ = 0.16;
    if ( beta_ == 0 ) beta_ = 3.2;
    

    if (scheduler_type_ == SchedulerType::WRR) {
        scheduler_ = std::unique_ptr<WRRScheduler>( new WRRScheduler(l4s_queue_, classic_queue_) );
    }

    l4s_qdelay_ns_ = 0;
    classic_qdelay_ns_ = 0;

    /* Start the periodic process that updates probs*/
    set_periodic_update ();
}

void DualQCoupledAQM::enqueue( QueuedPacket && p )
{
    // 1 MTU of space is always allowed (assumed size of the arriving packet) 
    // to avoid bias against larger packets. Might end up causing 
    // underutilization of buffer space...
    // Use p.contents.size() instead of MTU to be more precise

    // check if the periodic update function is due, return immediately if not.
    poller_.poll( 0 );


    std::cout << "> Packet size (enqueue): " << std::to_string(p.contents.size()) << std::endl;
    if ( size_bytes() + MTU > byte_limit_) {
        drop (DropReason::Overflow);
        return;
    }

    // Packet classifier
    unsigned char ecn_bits = get_ecn_bits( p );

    if (( ecn_bits == IPTOS_ECN_ECT1 ) ||
        ( ecn_bits == IPTOS_ECN_CE )) {
            //std::cout << "> Calling L4S enqueue... " << std::endl;
            p.enqueue_time_ns = timestamp_ns();
            l4s_queue_.enqueue( std::move( p ) );

    } else {
        classic_queue_.enqueue( std::move( p ) );
    }
    
    poller_.poll( 0 );
}

QueuedPacket DualQCoupledAQM::dequeue( void )
{
    QueueType dequeue_from;
    uint64_t l4s_qdelay_ns;
    uint64_t now;

    do {
        poller_.poll( 0 );

        QueuedPacket pkt("", 0);
        dequeue_from = scheduler_->select_queue();

        if ( dequeue_from == QueueType::L4S ) {
            pkt = l4s_queue_.dequeue();

            std::cout << "Packet size bytes (dequeue): " << std::to_string(pkt.contents.size()) << std::endl;
            std::cout << "Packet arrival time ns: " << std::to_string(pkt.arrival_time_ns) << std::endl;
            std::cout << "Packet enqueue time ns: " << std::to_string(pkt.enqueue_time_ns) << std::endl;
            std::cout << "Diff: " << std::to_string(pkt.enqueue_time_ns - pkt.arrival_time_ns) << std::endl << std::endl;
            
            if ( not is_overloaded() ) {
                now = timestamp_ns();

                //l4s_qdelay_ns = l4s_queue_.qdelay_in_ns( now );
                l4s_qdelay_ns = now - pkt.enqueue_time_ns;
                pp_l_ = l4s_queue_.calculate_l4s_native_prob( l4s_qdelay_ns );

                p_l_ = max(pp_l_, p_cl_);
                

                if ( roll( p_l_) ) {
                    mark( pkt );
                }                      
            } else {
                if ( roll( p_c_) ) {
                    if ( can_mark_or_drop() ) 
                    {
                        drop(DropReason::Overload);
                        continue;
                    }
                } 

                if ( can_mark_or_drop() )
                {
                    mark( pkt );
                }
            }
            scheduler_update();
        } 
        else if ( dequeue_from == QueueType::Classic ) { 
            pkt = classic_queue_.dequeue();       
            
            if ( roll( p_c_) ) {
                if ( get_ecn_bits( pkt ) == IPTOS_ECN_NOT_ECT ||
                    is_overloaded() ) {
                        if ( can_mark_or_drop() )
                        {
                            if (is_overloaded()) drop(DropReason::Overload);
                            else drop(DropReason::NotECT);
                            continue;
                        }
                }
                if ( can_mark_or_drop() )
                {
                    mark( pkt );
                }
            }
            scheduler_update();
        }

        poller_.poll( 0 );
        return pkt;

    } while ( dequeue_from != QueueType::NONE );
}

/* This function applies any update to scheduler state needed before dequeue */
void DualQCoupledAQM::scheduler_update( void ) 
{
    // Apply the WRR credit change 
    if (scheduler_type_ == SchedulerType::WRR)
        dynamic_cast<WRRScheduler*>(scheduler_.get())->apply_credit_change();
}

bool DualQCoupledAQM::empty( void ) const
{
    return l4s_queue_.empty() && classic_queue_.empty();
}

std::string DualQCoupledAQM::to_string( void ) const
{
    return "dualPI2";
}

unsigned int DualQCoupledAQM::size_bytes( void ) const
{
    return l4s_queue_.size_bytes() + classic_queue_.size_bytes();
}

unsigned int DualQCoupledAQM::size_packets( void ) const
{
    return l4s_queue_.size_packets() + classic_queue_.size_packets();;
}

bool DualQCoupledAQM::can_mark_or_drop( void )
{
    if ( size_bytes() < 2 * MTU )
        return false;
    
    return true;
}

void DualQCoupledAQM::drop( DropReason reason )
{
    switch ( reason ) {
        case DropReason::Overflow:
            overflow_drop_pkts_++;
            break;
        case DropReason::Overload:
            overload_drop_pkts_++;
            break;
        case DropReason::NotECT:
            not_ect_drop_pkts_++;
            break;
    }
}

unsigned char DualQCoupledAQM::get_ecn_bits( QueuedPacket & p )
{
    struct iphdr *ip_header = (struct iphdr *) &p.contents[4];
    return ( ip_header->tos & IPTOS_ECN_MASK ) ; 
}

void DualQCoupledAQM::mark( QueuedPacket & p )
{
    struct iphdr *ip_header = (struct iphdr *) &p.contents[4];

    ip_header->tos = ( ip_header->tos & ~IPTOS_ECN_MASK ) | ( IPTOS_ECN_CE & IPTOS_ECN_MASK );

    // Zero out the checksum field before recalculating
    ip_header->check = 0;
    ip_header->check = calculate_ip_checksum ((unsigned short*) ip_header, ip_header->ihl << 2);

    struct iphdr *ip_header2 = (struct iphdr *) &p.contents[4];
}

bool DualQCoupledAQM::roll( double prob ) { 
    
    if (prob == 0.0) return false; 
    if (prob == 1.0) return true;
    
    // Thread-local engine to mimic per-CPU PRNG state in the Linux kernel
    // Only runs once per thread
    static thread_local std::mt19937 engine(std::random_device{}());

    uint32_t rand_int = engine(); // uniformly distributed in [0, 2^32 - 1]
    double rand_double = static_cast<double>(rand_int) / static_cast<double>(UINT32_MAX);
    
    return ( rand_double <= prob );
 }

void DualQCoupledAQM::set_periodic_update( void ) 
{
    const timespec interval { 0, t_update_ms_ * NS_PER_MS };
    timer_.set_time( interval, interval );

    
   
    poller_.add_action( Poller::Action( timer_, Direction::In, 
                                        [&] () {                                         
                                            string str = timer_.read();

                                            uint64_t now = timestamp_ns();
                                            pp_ = calculate_base_aqm_prob ( now );
                                            p_c_ = pow( pp_, 2 );
                                            p_cl_ = pp_ * k_ ;

                                            return ResultType::Continue;
                                        } ) ); 
}

double scale_to_prob ( double val ) 
{
    double scaled_val = val/64.0; 
    return scaled_val / static_cast<double>(std::numeric_limits<uint32_t>::max());
}

double DualQCoupledAQM::calculate_base_aqm_prob( uint64_t ref ) 
{
    /* From  RFC 9332   : dualpi2_update function
             Linux code : calculate_probability function  */

    uint64_t qdelay_old = max( l4s_qdelay_ns_, classic_qdelay_ns_ ) ;

    // Update the qdelays
    l4s_qdelay_ns_ = l4s_queue_.qdelay_in_ns( ref );
    classic_qdelay_ns_ = classic_queue_.qdelay_in_ns( ref );

    uint64_t qdelay = max( l4s_qdelay_ns_, classic_qdelay_ns_ ) ;

    double delta = ((int64_t)qdelay - (int64_t)target_ms_ * NS_PER_MS) * alpha_ +
                    ((int64_t)qdelay - (int64_t)qdelay_old) * beta_;

    double new_pp;
    if (delta > 0) {
		new_pp = scale_to_prob(delta) + pp_;
		
	} else {
		new_pp = pp_ - scale_to_prob(delta * -1);	
	}

    if ( new_pp > 1.0 ) {
        // prevent overflow
        new_pp = 1.0;
    }
    else if ( new_pp < 0.0) {
        // prevent underflow
        new_pp = 0.0;
    }

    return new_pp;
}

DualQCoupledAQM::~DualQCoupledAQM ( void )
{
    update_running_ = false;
}
