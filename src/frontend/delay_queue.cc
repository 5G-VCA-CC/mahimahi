/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#include <limits>

#include "delay_queue.hh"
#include "timestamp.hh"

using namespace std;

void DelayQueue::read_packet( const string & contents )
{
    packet_queue_.emplace( timestamp_us() + delay_us_, contents );
}

void DelayQueue::write_packets( FileDescriptor & fd )
{
    while ( (!packet_queue_.empty())
            && (packet_queue_.front().first <= timestamp_us()) ) {
        fd.write( packet_queue_.front().second );
        packet_queue_.pop();
    }
}

int DelayQueue::wait_time( void ) const
{
    if ( packet_queue_.empty() ) {
        return numeric_limits<int>::max();
    }

    const auto now = timestamp_us();

    if ( packet_queue_.front().first <= now ) {
        return 0;
    } else {
        if ( packet_queue_.front().first - now > static_cast<uint64_t>( numeric_limits<int>::max() ) ) {
            return numeric_limits<int>::max();
        }
        return packet_queue_.front().first - now;
    }
}
