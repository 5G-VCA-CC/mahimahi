/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#ifndef L4S_PACKET_QUEUE_HH
#define L4S_PACKET_QUEUE_HH

#include <random>
#include <thread>
#include "dropping_packet_queue.hh"

/*
   L4S queue implementation as part of the DualPI2 implentation described in RFC 9332.
*/

class L4SPacketQueue : public DroppingPacketQueue
{
private:
    //This constant is copied from link_queue.hh.
    //It maybe better to get this in a more reliable way in the future.
    const static unsigned int PACKET_SIZE = 1504; /* default max TUN payload size */




    virtual const std::string & type( void ) const override
    {
        static const std::string type_ { "l4s" };
        return type_;
    }

    bool drop_early ( void );

    void calculate_drop_prob ( void );

public:
    L4SPacketQueue( const std::string & args );

    void enqueue( QueuedPacket && p ) override;

    QueuedPacket dequeue( void ) override;
};

#endif /* L4S_PACKET_QUEUE_HH */
