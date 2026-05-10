/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#include <limits>
#include <cassert>
#include <netinet/ip.h>

#include "link_queue.hh"
#include "timestamp.hh"
#include "util.hh"
#include "ezio.hh"
#include "abstract_packet_queue.hh"
#include "dualq_coupled_aqm.hh"

using namespace std;

LinkQueue::LinkQueue( const string & link_name, const string & filename, const string & logfile,
                      const bool repeat, const bool graph_throughput, const bool graph_delay,
                      unique_ptr<AbstractPacketQueue> && packet_queue,
                      const string & command_line )
    : next_delivery_( 0 ),
      schedule_(),
      base_timestamp_( timestamp() ),
      subtick_offsets_us_(),
      subtick_opportunities_(),
      cycle_duration_us_( 0 ),
      base_timestamp_us_( timestamp_us() ),
      packet_queue_( move( packet_queue ) ),
      packet_in_transit_( "", 0 ),
      packet_in_transit_bytes_left_( 0 ),
      output_queue_(),
      log_(),
      throughput_graph_( nullptr ),
      delay_graph_( nullptr ),
      repeat_( repeat ),
      finished_( false )
{
    assert_not_root();

    /* open filename and load schedule */
    ifstream trace_file( filename );

    if ( not trace_file.good() ) {
        throw runtime_error( filename + ": error opening for reading" );
    }

    string line;

    while ( trace_file.good() and getline( trace_file, line ) ) {
        if ( line.empty() ) {
            throw runtime_error( filename + ": invalid empty line" );
        }

        const uint64_t ms = myatoi( line );

        if ( not schedule_.empty() ) {
            if ( ms < schedule_.back() ) {
                throw runtime_error( filename + ": timestamps must be monotonically nondecreasing" );
            }
        }

        schedule_.emplace_back( ms );
    }

    if ( schedule_.empty() ) {
        throw runtime_error( filename + ": no valid timestamps found" );
    }

    if ( schedule_.back() == 0 ) {
        throw runtime_error( filename + ": trace must last for a nonzero amount of time" );
    }

    cycle_duration_us_ = schedule_.back() * 1000;

    /* build 250 us sub-tick opportunities for each trace millisecond */
    for ( size_t i = 0; i < schedule_.size(); ) {
        const uint64_t ms = schedule_.at( i );
        size_t j = i;
        while ( j < schedule_.size() and schedule_.at( j ) == ms ) {
            j++;
        }

        const unsigned int opportunities = j - i;
        const unsigned int base = opportunities / SUBTICKS_PER_MS;
        const unsigned int remainder = opportunities % SUBTICKS_PER_MS;

        for ( unsigned int subtick = 0; subtick < SUBTICKS_PER_MS; subtick++ ) {
            const unsigned int subtick_opportunities = base + ( subtick < remainder ? 1 : 0 );
            if ( subtick_opportunities == 0 ) {
                continue;
            }

            uint64_t subtick_offset_us = 0;
            if ( ms == 0 ) {
                subtick_offset_us = subtick * SUBTICK_US;
            } else {
                subtick_offset_us = ( ms - 1 ) * 1000 + ( subtick + 1 ) * SUBTICK_US;
            }

            subtick_offsets_us_.emplace_back( subtick_offset_us );
            subtick_opportunities_.emplace_back( subtick_opportunities );
        }

        i = j;
    }

    if ( subtick_offsets_us_.empty() ) {
        throw runtime_error( filename + ": no valid departure opportunities found" );
    }

    /* open logfile if called for */
    if ( not logfile.empty() ) {
        log_.reset( new ofstream( logfile ) );
        if ( not log_->good() ) {
            throw runtime_error( logfile + ": error opening for writing" );
        }

        *log_ << "# mahimahi mm-link (" << link_name << ") [" << filename << "] > " << logfile << endl;
        *log_ << "# command line: " << command_line << endl;
        *log_ << "# queue: " << packet_queue_->to_string() << endl;
        *log_ << "# init timestamp: " << initial_timestamp() << endl;
        *log_ << "# base timestamp: " << base_timestamp_ << endl;
        const char * prefix = getenv( "MAHIMAHI_SHELL_PREFIX" );
        if ( prefix ) {
            *log_ << "# mahimahi config: " << prefix << endl;
        }
    }

    /* create graphs if called for */
    if ( graph_throughput ) {
        // Check if this is a DualQCoupledAQM using dynamic_cast
        DualQCoupledAQM* dualpi2_queue = dynamic_cast<DualQCoupledAQM*>(packet_queue_.get());

        if ( dualpi2_queue ) {
            // For dualpi2, add two throughput lines:
            // index 3: Classic queue delay (orange)
            // index 4: L4S queue delay (light blue)
            throughput_graph_.reset( new BinnedLiveGraph( link_name + " [" + filename + "]",
                                                      { make_tuple( 1.0, 0.0, 0.0, 0.25, true ),
                                                        make_tuple( 0.0, 0.0, 0.4, 1.0, false ),
                                                        make_tuple( 1.0, 0.0, 0.0, 0.5, false ), 
                                                        make_tuple( 1.0, 0.5, 0.0, 1.0, false ),   // Classic - orange 
                                                        make_tuple( 0.4, 0.7, 1.0, 1.0, false )},  // L4S - light blue
                                                      "throughput (Mbps)",
                                                      8.0 / 1000000.0,
                                                      true,
                                                      500,
                                                      [] ( int, int & x ) { x = 0; } ) );

        } else {
            throughput_graph_.reset( new BinnedLiveGraph( link_name + " [" + filename + "]",
                                                      { make_tuple( 1.0, 0.0, 0.0, 0.25, true ),
                                                        make_tuple( 0.0, 0.0, 0.4, 1.0, false ),
                                                        make_tuple( 1.0, 0.0, 0.0, 0.5, false ) },
                                                      "throughput (Mbps)",
                                                      8.0 / 1000000.0,
                                                      true,
                                                      500,
                                                      [] ( int, int & x ) { x = 0; } ) );
        }

    }

    if ( graph_delay ) {
        // Check if this is a DualQCoupledAQM using dynamic_cast
        DualQCoupledAQM* dualpi2_queue = dynamic_cast<DualQCoupledAQM*>(packet_queue_.get());
        
        if ( dualpi2_queue ) {
            // For dualpi2, create a graph with two lines:
            // index 0: Classic queue delay (orange)
            // index 1: L4S queue delay (light blue)
            delay_graph_.reset( new BinnedLiveGraph( link_name + " delay [" + filename + "]",
                                                     { make_tuple( 1.0, 0.5, 0.0, 1.0, false ),   // Classic - orange 
                                                       make_tuple( 0.4, 0.7, 1.0, 1.0, false ) }, // L4S - light blue
                                                     "queueing delay (ms)",
                                                     1, false, 250,
                                                     [] ( int, int & x ) { x = -1; } ) );
        } else {
            // For non-dualpi2 queues, use single delay line (green)
            delay_graph_.reset( new BinnedLiveGraph( link_name + " delay [" + filename + "]",
                                                     { make_tuple( 0.0, 0.25, 0.0, 1.0, false ) },
                                                     "queueing delay (ms)",
                                                     1, false, 250,
                                                     [] ( int, int & x ) { x = -1; } ) );
        }
    }
}

void LinkQueue::record_arrival( const uint64_t arrival_time, const size_t pkt_size )
{
    /* log it */
    if ( log_ ) {
        *log_ << arrival_time << " + " << pkt_size << endl;
    }

    /* meter it */
    if ( throughput_graph_ ) {
        throughput_graph_->add_value_now( 1, pkt_size );
    }
}

void LinkQueue::record_drop( const uint64_t time, const size_t pkts_dropped, const size_t bytes_dropped)
{
    /* log it */
    if ( log_ ) {
        *log_ << time << " d " << pkts_dropped << " " << bytes_dropped << endl;
    }
}

void LinkQueue::record_departure_opportunity( void )
{
    /* log the delivery opportunity */
    if ( log_ ) {
        *log_ << next_delivery_time() << " # " << PACKET_SIZE << endl;
    }

    /* meter the delivery opportunity */
    if ( throughput_graph_ ) {
        throughput_graph_->add_value_now( 0, PACKET_SIZE );
    }    
}

void LinkQueue::record_departure( const uint64_t departure_time, const QueuedPacket & packet )
{
    /* log the delivery */
    if ( log_ ) {
        *log_ << departure_time << " - " << packet.contents.size()
              << " " << departure_time - packet.arrival_time << endl;
    }

    /* meter the delivery */
    if ( throughput_graph_ ) {
        // Check if this is a DualQCoupledAQM using dynamic_cast
        DualQCoupledAQM* dualpi2_queue = dynamic_cast<DualQCoupledAQM*>(packet_queue_.get());
        
        // First, record the overall throughput
        throughput_graph_->add_value_now( 2, packet.contents.size() );
        
        if ( dualpi2_queue ) {
            // Then, record the individual queue throughputs
            if ( is_l4s_packet( packet ) ) {
                throughput_graph_->add_value_now( 4, packet.contents.size() );  // L4S delay - index 1 (light blue)
            } else {
                throughput_graph_->add_value_now( 3, packet.contents.size() );  // Classic delay - index 0 (orange)
            }
        }
    }

    if ( delay_graph_ ) {
        uint64_t packet_qdelay = departure_time - packet.arrival_time;
        
        // Check if this is a DualQCoupledAQM using dynamic_cast
        DualQCoupledAQM* dualpi2_queue = dynamic_cast<DualQCoupledAQM*>(packet_queue_.get());
        
        if ( dualpi2_queue ) {            
            // Record queue delay to the appropriate individual queue
            // TODO: move the is_l4s_packet check to the QueuedPAcket class to be used in enqueue as well

            if ( is_l4s_packet( packet ) ) {
                delay_graph_->set_max_value_now( 1, packet_qdelay );  // L4S delay - index 1 (light blue)
            } else {
                delay_graph_->set_max_value_now( 0, packet_qdelay );  // Classic delay - index 0 (orange)
            }
        } else {
            // For non-dualpi2 queues, record only overall delay (green, same color as dualpi2 overall)
            delay_graph_->set_max_value_now( 0, packet_qdelay );
        }
    }    
}

void LinkQueue::read_packet( const string & contents )
{
    const uint64_t now_ns = timestamp_ns();
    const uint64_t now_us = now_ns / 1000;
    const uint64_t now = now_ns / 1000000;

    if ( contents.size() > PACKET_SIZE ) {
        throw runtime_error( "packet size is greater than maximum" );
    }

    rationalize( now_us );

    record_arrival( now, contents.size() );

    unsigned int bytes_before = packet_queue_->size_bytes();
    unsigned int packets_before = packet_queue_->size_packets();

    packet_queue_->enqueue( QueuedPacket( contents, now, now_ns ) );

    assert( packet_queue_->size_packets() <= packets_before + 1 );
    assert( packet_queue_->size_bytes() <= bytes_before + contents.size() );
    
    unsigned int missing_packets = packets_before + 1 - packet_queue_->size_packets();
    unsigned int missing_bytes = bytes_before + contents.size() - packet_queue_->size_bytes();
    if ( missing_packets > 0 || missing_bytes > 0 ) {
        record_drop( now, missing_packets, missing_bytes );
    }
}

uint64_t LinkQueue::next_delivery_time( void ) const
{
    if ( finished_ ) {
        return -1;
    } else {
        return next_subtick_time_us() / 1000;
    }
}

uint64_t LinkQueue::next_subtick_time_us( void ) const
{
    if ( finished_ ) {
        return -1;
    } else {
        return base_timestamp_us_ + subtick_offsets_us_.at( next_delivery_ );
    }
}

void LinkQueue::use_a_delivery_opportunity( void )
{
    record_departure_opportunity();
}

void LinkQueue::advance_subtick( void )
{
    next_delivery_ = ( next_delivery_ + 1 ) % subtick_offsets_us_.size();

    /* wraparound */
    if ( next_delivery_ == 0 ) {
        if ( repeat_ ) {
            base_timestamp_us_ += cycle_duration_us_;
        } else {
            finished_ = true;
        }
    }
}

/* emulate the link up to the given timestamp */
/* this function should be called before enqueueing any packets and before
   calculating the wait_time until the next event */
void LinkQueue::rationalize( const uint64_t now )
{
    while ( next_subtick_time_us() <= now ) {
        const uint64_t this_delivery_time = next_delivery_time();
        const unsigned int opportunities_this_subtick = subtick_opportunities_.at( next_delivery_ );

        for ( unsigned int i = 0; i < opportunities_this_subtick; i++ ) {
            /* burn a delivery opportunity */
            unsigned int bytes_left_in_this_delivery = PACKET_SIZE;
            use_a_delivery_opportunity();

            while ( bytes_left_in_this_delivery > 0 ) {
                if ( not packet_in_transit_bytes_left_ ) {
                    if ( packet_queue_->empty() ) {
                        break;
                    }
                    packet_in_transit_ = packet_queue_->dequeue();
                    packet_in_transit_bytes_left_ = packet_in_transit_.contents.size();
                }

                assert( packet_in_transit_.arrival_time <= this_delivery_time );
                assert( packet_in_transit_bytes_left_ <= PACKET_SIZE );
                assert( packet_in_transit_bytes_left_ > 0 );
                assert( packet_in_transit_bytes_left_ <= packet_in_transit_.contents.size() );

                /* how many bytes of the delivery opportunity can we use? */
                const unsigned int amount_to_send = min( bytes_left_in_this_delivery,
                                                         packet_in_transit_bytes_left_ );

                /* send that many bytes */
                packet_in_transit_bytes_left_ -= amount_to_send;
                bytes_left_in_this_delivery -= amount_to_send;

                /* has the packet been fully sent? */
                if ( packet_in_transit_bytes_left_ == 0 ) {
                    record_departure( this_delivery_time, packet_in_transit_ );

                    /* this packet is ready to go */
                    output_queue_.push( move( packet_in_transit_.contents ) );
                }
            }
        }

        advance_subtick();
    }
}

void LinkQueue::write_packets( FileDescriptor & fd )
{
    while ( not output_queue_.empty() ) {
        fd.write( output_queue_.front() );
        output_queue_.pop();
    }
}

int LinkQueue::wait_time( void )
{
    const auto now = timestamp_us();

    rationalize( now );

    if ( next_subtick_time_us() <= now ) {
        return 0;
    } else {
        const uint64_t wait_us = next_subtick_time_us() - now;
        if ( wait_us > static_cast<uint64_t>( numeric_limits<int>::max() ) ) {
            return numeric_limits<int>::max();
        }
        return wait_us;
    }
}

bool LinkQueue::pending_output( void ) const
{
    return not output_queue_.empty();
}

bool LinkQueue::is_l4s_packet( const QueuedPacket & packet ) const 
{
    // Check if packet is large enough to contain IP header (4 byte offset + 20 byte IP header)
    if ( packet.contents.size() < 24 ) {
        return false;
    }
    
    // Get IP header (offset 4, same as in dualpi2 code)
    struct iphdr *ip_header = (struct iphdr *) &packet.contents[4];
    
    // Extract ECN bits from TOS field
    unsigned char ecn_bits = ip_header->tos & IPTOS_ECN_MASK;
    
    // L4S packets have ECT(1) or CE markings
    return (ecn_bits == IPTOS_ECN_ECT1) || (ecn_bits == IPTOS_ECN_CE);
}
