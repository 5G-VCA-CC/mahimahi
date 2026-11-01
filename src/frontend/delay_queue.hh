/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#ifndef DELAY_QUEUE_HH
#define DELAY_QUEUE_HH

#include <queue>
#include <cstdint>
#include <string>

#include "file_descriptor.hh"

class DelayQueue
{
private:
    uint64_t delay_ns_;
    std::queue< std::pair<uint64_t, std::string> > packet_queue_;
    /* release timestamp, contents */

public:
    DelayQueue( const uint64_t & s_delay_ns ) : delay_ns_( s_delay_ns ), packet_queue_() {}

    void read_packet( const std::string & contents );

    void write_packets( FileDescriptor & fd );

    uint64_t next_event_time_ns( void ) const;

    bool pending_output( void ) const;

    static bool finished( void ) { return false; }
};

#endif /* DELAY_QUEUE_HH */
