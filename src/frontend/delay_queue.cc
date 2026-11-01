/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#include <limits>

#include "delay_queue.hh"
#include "timestamp.hh"

using namespace std;

void DelayQueue::read_packet( const string & contents )
{
    packet_queue_.emplace( timestamp_ns() + delay_ns_, contents );
}

void DelayQueue::write_packets( FileDescriptor & fd )
{
    while ( (!packet_queue_.empty())
            && (packet_queue_.front().first <= timestamp_ns()) ) {
        fd.write( packet_queue_.front().second );
        packet_queue_.pop();
    }
}

uint64_t DelayQueue::next_event_time_ns( void ) const
{
    if ( packet_queue_.empty() ) {
        return numeric_limits<uint64_t>::max();
    }

    return packet_queue_.front().first;
}

bool DelayQueue::pending_output( void ) const
{
    return (!packet_queue_.empty())
        && (packet_queue_.front().first <= timestamp_ns());
}
