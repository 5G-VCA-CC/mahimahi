/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#ifndef DUALQ_COUPLED_AQM_HH
#define DUALQ_COUPLED_AQM_HH

#include <random>
#include <thread>
#include "abstract_packet_queue.hh"
#include "l4s_packet_queue.hh"
#include "classic_packet_queue.hh"

/*
   DualQ Coupled AQM, Implemented as DualQ PI2 based on RFC 9332.
*/

class DualQCoupledAQM : public AbstractPacketQueue
{
private:
    //This constant is copied from link_queue.hh.
    //It maybe better to get this in a more reliable way in the future.
    const static unsigned int PACKET_SIZE = 1504; /* default max TUN payload size */

    uint32_t k_;
    L4SPacketQueue lq_;
    CLASSICPacketQueue cq_;


    // TODO: Add components of the DualQ AQM
        // scheduler object


    /*
    virtual const std::string & type( void ) const override
    {
        static const std::string type_ { "dualPI2" };
        return type_;
    }
    */

    bool drop_early ( void );

    void calculate_drop_prob ( void );

public:
    DualQCoupledAQM( const std::string & args );

    void enqueue( QueuedPacket && p ) override;

    QueuedPacket dequeue( void ) override;

    bool empty( void ) const override;

    std::string to_string( void ) const override;

    unsigned int size_bytes( void ) const override;

    unsigned int size_packets( void ) const override;
};

#endif /* DUALQ_COUPLED_AQM_HH */


