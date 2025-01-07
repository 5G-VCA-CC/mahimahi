/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#ifndef ABSTRACT_L4S_SCHEDULER
#define ABSTRACT_L4S_SCHEDULER

#include <string>

#include "l4s_packet_queue.hh"
#include "classic_packet_queue.hh"

enum class QueueType {
    L4S,
    Classic
};

enum class SchedulerType {
    NONE,
    WRR,
    TS_FIFO
};

class AbstractL4SScheduler
{
protected:
    // Dual queues
    L4SPacketQueue & l4s_queue_;
    CLASSICPacketQueue & classic_queue_;

public:

    AbstractL4SScheduler (L4SPacketQueue & l4s_q, CLASSICPacketQueue & classic_q) : 
        l4s_queue_ ( l4s_q ),
        classic_queue_( classic_q )
        {};

    virtual QueueType select_queue( void ) = 0;

    virtual ~AbstractL4SScheduler() = default;

};

#endif /* ABSTRACT_L4S_SCHEDULER */ 
