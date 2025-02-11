/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#ifndef QUEUED_PACKET_HH
#define QUEUED_PACKET_HH

#include <string>
#include <cstdint>

struct QueuedPacket
{
    uint64_t arrival_time;
    uint64_t enqueue_time;
    std::string contents;

    QueuedPacket( const std::string & s_contents, uint64_t s_arrival_time, uint64_t aqm_enqueue_time = 0 )
        : arrival_time( s_arrival_time ), enqueue_time( aqm_enqueue_time ), contents( s_contents )
    {}

    uint64_t sojourn_time_in_ns ( uint64_t ref )
    { 
        /* enqueue_time needs to be initialized */
        assert (! enqueue_time );
        return ref - enqueue_time ;
    }
};

#endif /* QUEUED_PACKET_HH */
