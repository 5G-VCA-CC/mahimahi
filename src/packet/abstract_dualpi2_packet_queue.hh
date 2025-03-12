/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#ifndef ABSTRACT_DUALPI2_PACKET_QUEUE_HH
#define ABSTRACT_DUALPI2_PACKET_QUEUE_HH

#include <queue>
// #include <cassert>

// #include <random>
// #include <thread>

#include <random>
#include <cstdint>

#include "abstract_packet_queue.hh"

/* Max value of an 32-bit integer */
#define MAX_PROB ((uint32_t)(~((uint32_t)0)))

class AbstractDualPI2PacketQueue : public AbstractPacketQueue
{
private:
    int queue_size_in_bytes_ = 0, queue_size_in_packets_ = 0;

    std::queue<QueuedPacket> internal_queue_ {};

    virtual const std::string & type( void ) const = 0;

protected:
    

public:
    void enqueue( QueuedPacket && p );

    QueuedPacket dequeue( void );

    bool empty( void ) const override;

    std::string to_string( void ) const override;

    unsigned int size_bytes( void ) const override;
    unsigned int size_packets( void ) const override;

    QueuedPacket& peek ( void );
    uint64_t qdelay_in_ms ( uint64_t ref );       
};


// Utilities
uint32_t scale_prob( double prob );
unsigned int get_arg( const std::string & args, const std::string & name );
void print_ipv4_header( QueuedPacket & p ); 

inline uint32_t rand32() 
{
    static std::mt19937 rng{std::random_device{}()}; // 32-bit Mersenne Twister
    return rng(); // Produces a 32-bit random value in [0, 2^32-1]
}

inline bool random_roll(uint32_t prob)
{
    return rand32() <= prob;
}

inline bool random_squared_roll(uint32_t prob)
{
    return random_roll(prob) && random_roll(prob);
}

#endif /* ABSTRACT_DUALPI2_PACKET_QUEUE_HH */
