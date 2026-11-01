/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#ifndef LOSS_QUEUE_HH
#define LOSS_QUEUE_HH

#include <queue>
#include <cstdint>
#include <string>
#include <random>

#include "file_descriptor.hh"

class LossQueue
{
private:
    std::queue<std::string> packet_queue_ {};

    virtual bool drop_packet( const std::string & packet ) = 0;

protected:
    std::default_random_engine prng_;

public:
    LossQueue();
    virtual ~LossQueue() {}

    void read_packet( const std::string & contents );

    void write_packets( FileDescriptor & fd );

    uint64_t next_event_time_ns( void );

    bool pending_output( void ) const { return not packet_queue_.empty(); }

    static bool finished( void ) { return false; }
};

class IIDLoss : public LossQueue
{
private:
    std::bernoulli_distribution drop_dist_;

    bool drop_packet( const std::string & packet ) override;

public:
    IIDLoss( const double loss_rate ) : drop_dist_( loss_rate ) {}
};

class StochasticSwitchingLink : public LossQueue
{
private:
    bool link_is_on_;
    std::exponential_distribution<> on_process_;
    std::exponential_distribution<> off_process_;

    uint64_t next_switch_time_;

    /* advance the on/off process up to the given time */
    void update_link_state( const uint64_t now );

    bool drop_packet( const std::string & packet ) override;

public:
    StochasticSwitchingLink( const double mean_on_time_, const double mean_off_time );
};

class PeriodicSwitchingLink : public LossQueue
{
private:
    bool link_is_on_;
    uint64_t on_time_, off_time_, next_switch_time_;

    /* advance the on/off schedule up to the given time */
    void update_link_state( const uint64_t now );

    bool drop_packet( const std::string & packet ) override;

public:
    PeriodicSwitchingLink( const double on_time, const double off_time );
};

#endif /* LOSS_QUEUE_HH */
