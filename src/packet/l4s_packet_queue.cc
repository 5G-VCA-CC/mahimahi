#include <chrono>

#include "l4s_packet_queue.hh"
#include "timestamp.hh"

using namespace std;

L4SPacketQueue::L4SPacketQueue( const string & args )
  : max_delay_thresh_ms_( get_arg( args, "l4s_max_threshold" ) ),
    min_delay_thresh_ms_ ( get_arg( args, "l4s_min_threshold" ) ),
    min_qlen_pkt_ ( get_arg( args, "l4s_min_len" ) )
{   
    if ( min_delay_thresh_ms_ == 0 ) {
        // Use the step function (as opposed to ramp)
        step_ = true;
    }
    else step_ = false;

    if ( max_delay_thresh_ms_ == 0 )
        max_delay_thresh_ms_ = 1; // ms

    if ( min_qlen_pkt_ == 0 )
        min_qlen_pkt_ = 1;
}

double L4SPacketQueue::calculate_l4s_native_prob ( uint64_t qdelay_ns )
{
    if ( size_packets() <= min_qlen_pkt_ )
        // Do not mark packets if under min_qlen_pkt_ (default is 1)
        return 0.0;

    // In both the step and the ramp methods:
    if ( qdelay_ns >= max_delay_thresh_ms_ * NS_PER_MS ) {
            return 1.0;
        }

    // Here, qdelay < max_delay_thresh_ms_

    if ( step_ ) {
        return 0.0;
    }
    else {
        // Use a ramp function: 'laqm (qdelay)' of RFC 9332

        if ( qdelay_ns > min_delay_thresh_ms_ * NS_PER_MS ) {
            return ( qdelay_ns - min_delay_thresh_ms_ * NS_PER_MS )/
                ( max_delay_thresh_ms_ * NS_PER_MS - min_delay_thresh_ms_ * NS_PER_MS );
        }
        
        return 0;
    }
}

