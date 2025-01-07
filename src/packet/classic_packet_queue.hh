/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#ifndef CLASSIC_PACKET_QUEUE_HH
#define CLASSIC_PACKET_QUEUE_HH

#include <random>
#include <thread>
#include "abstract_dualpi2_packet_queue.hh"

/*
   Classic queue implementation as part of the DualPI2 implentation described in RFC 9332.
*/

class CLASSICPacketQueue : public AbstractDualPI2PacketQueue
{
private:

    virtual const std::string & type( void ) const override
    {
        static const std::string type_ { "classic" };
        return type_;
    }

    bool drop_early ( void );

    void calculate_drop_prob ( void );

public:
    CLASSICPacketQueue( const std::string & args );

    void enqueue( QueuedPacket && p ) override;

    QueuedPacket dequeue( void ) override;
   
};

#endif /* CLASSIC_PACKET_QUEUE_HH */
