#include <algorithm>
//#include <iostream>

#include "dualq_coupled_aqm.hh"
#include "timestamp.hh"
#include "exception.hh"
#include "ezio.hh"

#include "abstract_packet_queue.hh"


using namespace std;

DualQCoupledAQM::DualQCoupledAQM( const string & args )
  : byte_limit_( get_arg( args, "bytes" ) ),
    k_ ( get_arg( args, "k" ) ),
    l4s_queue_ ( L4SPacketQueue ( args ) ),
    classic_queue_ ( CLASSICPacketQueue ( args ) ),
    scheduler_type_ ( static_cast<SchedulerType> (get_arg( args, "sched" ))),
    target_ns_ ( get_arg( args, "target" ) * 1000000 ),
    max_rtt_ms_ ( get_arg( args, "max_rtt" ) ),
    alpha_ ( get_arg( args, "alpha" ) ),
    beta_ ( get_arg( args, "beta" ) ),
    t_update_ms_ ( get_arg( args, "tupdate" )  ),
    satur_drop_pkts_ ( 0 ),
    pp_ ( 0 ),
    pp_l_ ( 0 ),
    p_l_ ( 0 ),
    p_cl_ ( 0 ),
    p_c_ ( 0 ),
    p_Cmax_ ( 0 ),
    l4s_drop_on_overload_ ( true )
{
    if ( byte_limit_ == 0 ) {
        throw runtime_error( "DualPI2 must have a byte limit." );
    }

    if ( k_ == 0 ) k_ = 2;
    p_Cmax_ = min(scale_proba( 1/ pow( k_, 2 ) ), MAX_PROB);
    p_Lmax_ = MAX_PROB;

    // TODO: adjust the following values!!

    if ( target_ns_ == 0 ) target_ns_ = 15000000; 
    if ( max_rtt_ms_ == 0 ) max_rtt_ms_ = 100;
    if ( alpha_ == 0 ) alpha_ = 100;
    if ( beta_ == 0 ) beta_ = 200;
    if ( t_update_ms_ == 0 ) t_update_ms_ = 16;

    if (scheduler_type_ == SchedulerType::NONE || scheduler_type_ == SchedulerType::WRR) {
        scheduler_ = std::unique_ptr<WRRScheduler>( new WRRScheduler(l4s_queue_, classic_queue_) );
    }

    l4s_qdelay_ns_ = 0;
    classic_qdelay_ns_ = 0;

    /* initialize base timestamp value */
    initial_timestamp_ns();

    using clock = chrono::steady_clock;
    uint64_t now = timestamp_ns();

    
    /* Start the periodic process that updates probs*/
    //periodic_worker_ = std::thread ( &DualQCoupledAQM::periodic_update, this );
}

void DualQCoupledAQM::enqueue( QueuedPacket && p )
{
    // 1 MTU of space is always allowed (assumed size of the arriving packet) 
    // to avoid bias against larger packets. Might end up causing 
    // underutilization of buffer space...
    // Use p.contents.size() instead of MTU to be more precise

    if ( size_bytes() + MTU > byte_limit_) {
        drop ("saturation");
        return;
    }

    // Record the packet's timestamp to calculate the sojourn time. 
    // Here, I am using the existing p.arrival_time, similar to CoDel.

    // Packet classifier
    unsigned char ecn_bits = get_ecn_bits ( std::move( p ) );

    if (( ecn_bits == IPTOS_ECN_ECT1 ) ||
        ( ecn_bits == IPTOS_ECN_CE )) {

        l4s_queue_.enqueue ( std::move( p ) );

    } else {
        classic_queue_.enqueue ( std::move( p ) );
    }
    
}

QueuedPacket DualQCoupledAQM::dequeue( void )
{
    QueuedPacket pkt ("empty", 0);

    // TODO: if both queues are empty, call dualpi2_reset_c_protection

    while ( size_bytes() > 0 ) {
        QueueType dequeue_from = scheduler_->select_queue();
        if ( dequeue_from == QueueType::L4S ) {
            pkt = l4s_queue_.dequeue ();
            
            if ( not l4s_is_overloaded() ) {
                // pp_, pp_l, p_c_ and p_cl calculated in the periodic update function
                // TODO: check periodic_update call!

                p_l_ = max (pp_l_, p_cl_);

                if ( recur(l4s_queue_, p_l_) ) {
                    mark(pkt);
                 }                      
            } else {
                if ( recur(l4s_queue_, p_c_) ) {
                    drop("saturation");
                    continue;
                 } 
                
                if ( recur(l4s_queue_, p_cl_) ) {
                    mark(pkt);
                 } 

            }
        } 
        else if ( dequeue_from == QueueType::Classic) { 
            pkt = classic_queue_.dequeue ();       
            
            if ( recur(classic_queue_, p_c_) ) {
                if (get_ecn_bits ( std::move( pkt ) ) == IPTOS_ECN_NOT_ECT ||
                    classic_is_overloaded() ) {
                        drop ("");
                        continue;
                }
                mark( pkt );
            }
        }

        // Apply the WRR credit change 
        if (scheduler_type_ == SchedulerType::WRR)
            dynamic_cast<WRRScheduler*>(scheduler_.get())->apply_credit_change();

        return pkt;
    }
}

bool DualQCoupledAQM::empty( void ) const
{
    return true;
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

void DualQCoupledAQM::drop ( std::string reason )
{
    if ( reason == "saturation") {
        satur_drop_pkts_++;
    } else if ( reason == "") {

    }
         
}

unsigned char DualQCoupledAQM::get_ecn_bits ( QueuedPacket && p )
{
    struct iphdr *ip_header = (struct iphdr *) p.contents[4];
    return ( ip_header->tos & IPTOS_ECN_MASK ) ; 
}

void DualQCoupledAQM::mark ( QueuedPacket & p )
{
    struct iphdr *ip_header = (struct iphdr *) p.contents[4];
    ip_header->tos = ( ip_header->tos & ~IPTOS_ECN_MASK ) |
        ( IPTOS_ECN_CE & IPTOS_ECN_MASK );

    // TODO: Recalculate the checksum!! 
    // See IP_ECN_set_ce in sch_dualpi2_upstream/include/net/inet_ecn.h

}

 /* Returns TRUE with a certain likelihood modeling a recurring (and deterministic) 
    pattern of marks/drops */ 
bool DualQCoupledAQM::recur( AbstractDualPI2PacketQueue & queue, uint32_t likelihood )
{
    uint32_t count = queue.get_recur_count() + likelihood;
    if ( count > 1) {
        queue.set_recur_count ( count - 1 );
        return true;
    }
    queue.set_recur_count ( count );
    return false;
}

int64_t DualQCoupledAQM::scale_delta ( uint64_t val )
{
    return val / ((1 << ( ALPHA_BETA_GRANULARITY + 1 )) -1) ;
}

uint32_t DualQCoupledAQM::scale_proba ( double prob )
{
    if ( prob < 0.0 || prob > 1.0 )
        throw runtime_error ("Probability out of range! Provided value: " + std::to_string(prob));

    return static_cast<uint32_t> (prob * MAX_PROB) ;
}

void DualQCoupledAQM::periodic_update ( void ) 
{
    using clock = chrono::steady_clock;
    uint64_t now = timestamp_ns();

    uint64_t l4s_qdelay_ns;

    while (update_running_) {
        pp_ = calculate_base_aqm_prob ( now );

        l4s_qdelay_ns = l4s_queue_.qdelay_in_ns ( now );
        pp_l_ = l4s_queue_.calculate_l4s_native_prob ( l4s_qdelay_ns ); 

        p_c_ = pow( pp_, 2 );
        p_cl_ = pp_ * k_ ;

        this_thread::sleep_until ( clock::now () + chrono::milliseconds(t_update_ms_) );
    }
    
}

uint32_t DualQCoupledAQM::calculate_base_aqm_prob ( uint64_t ref ) 
{
    /* From  RFC 9332   : dualpi2_update function
             Linux code : calculate_probability function  */

    uint64_t qdelay_old = max ( l4s_qdelay_ns_, classic_qdelay_ns_ ) ;

    uint32_t new_prob;

    // Update the qdelays
    l4s_qdelay_ns_ = l4s_queue_.qdelay_in_ns ( ref );
    classic_qdelay_ns_ = classic_queue_.qdelay_in_ns ( ref );

    uint64_t qdelay = max ( l4s_qdelay_ns_, classic_qdelay_ns_ ) ;

    int64_t delta = ( (int64_t)qdelay - target_ns_ ) * alpha_;
    delta += ( (int64_t)qdelay - qdelay_old ) * beta_;

    if ( delta > 0 ) {
        // prevent overflow
        new_prob = scale_delta( delta ) + pp_ ;
        if ( new_prob < pp_ )
            new_prob = MAX_PROB;
    }
    else {
        // prevent underflow
        new_prob = pp_ - scale_delta( delta * -1 );
        if ( new_prob > pp_ )
            new_prob = 0;
    }

    // TODO: check the capping of p' if no drop on overload

    return new_prob;
}

unsigned int DualQCoupledAQM::get_arg( const string & args, const string & name )
{
    auto offset = args.find( name );
    if ( offset == string::npos ) {
        return 0; /* default value */
    } else {
        /* extract the value */

        /* advance by length of name */
        offset += name.size();

        /* make sure next char is "=" */
        if ( args.substr( offset, 1 ) != "=" ) {
            throw runtime_error( "could not parse queue arguments: " + args );
        }

        /* advance by length of "=" */
        offset++;

        /* find the first non-digit character */
        auto offset2 = args.substr( offset ).find_first_not_of( "0123456789" );

        auto digit_string = args.substr( offset ).substr( 0, offset2 );

        if ( digit_string.empty() ) {
            throw runtime_error( "could not parse queue arguments: " + args );
        }
      
        return myatoi( digit_string );
    }
}

DualQCoupledAQM::~DualQCoupledAQM ( void )
{
    update_running_ = false;

    // if (periodic_worker_.joinable()) {
    //     periodic_worker_.join();
    // }
    

    // TODO: make sure there's resource freeing in case of interrupt
}
