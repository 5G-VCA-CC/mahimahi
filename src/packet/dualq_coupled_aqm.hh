/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#ifndef DUALQ_COUPLED_AQM_HH
#define DUALQ_COUPLED_AQM_HH

#include <random>
//#include <thread>
#include <chrono>
#include <atomic>
#include <netinet/ip.h>

#include "poller.hh"
#include "timerfd.hh"

#include "abstract_packet_queue.hh"
#include "l4s_packet_queue.hh"
#include "classic_packet_queue.hh"

#include "abstract_l4s_scheduler.hh"
#include "weighted_round_robin_scheduler.hh"

/* Used to scale the delay diff */
#define ALPHA_BETA_GRANULARITY 6

#define NS_PER_MS 1000000

/*
   DualQ Coupled AQM, Implemented as DualQ PI2 based on RFC 9332.
*/

class DualQCoupledAQM : public AbstractPacketQueue
{
private:
    //This constant is copied from link_queue.hh.
    //It maybe better to get this in a more reliable way in the future.
    const static unsigned int PACKET_SIZE = 1504; /* default max TUN payload size */

    // default max TUN payload size from link_queue.hh.
    const static unsigned int MTU = 1504; /*  */

    const unsigned int byte_limit_;

    // Proportional Integral (PI) controller parameters

    // Trigger update every...    
    uint16_t t_update_ms_;

    Poller poller_ ;
    Timerfd timer_ ;

    /* From RFC 9332:
        13:   alpha = 0.1 * Tupdate / RTT_max^2      % PI integral gain in Hz
        14:   beta = 0.3 / RTT_max                   % PI proportional gain in Hz */

    uint32_t alpha_;
    uint32_t beta_;

    // Coupling factor
    uint32_t k_;

    // Target queue delay
    uint64_t target_ns_;

    uint64_t l4s_qdelay_ns_;
    uint64_t classic_qdelay_ns_;

    uint32_t max_rtt_ms_;

    // Dual queues
    L4SPacketQueue l4s_queue_;
    CLASSICPacketQueue classic_queue_;

    const SchedulerType scheduler_type_;
    std::unique_ptr<AbstractL4SScheduler> scheduler_;

    uint32_t satur_drop_pkts_;

    uint32_t pp_l_; 
    uint32_t pp_;
    uint32_t p_l_;
    uint32_t p_c_;
    uint32_t p_cl_;
    uint32_t p_Cmax_;
    uint32_t p_Lmax_;

    bool l4s_drop_on_overload_;

    std::atomic<bool> update_running_ {true};


    /*
    virtual const std::string & type( void ) const override
    {
        static const std::string type_ { "dualPI2" };
        return type_;
    }
    */

    void drop ( std::string reason );

    unsigned char get_ecn_bits ( QueuedPacket && p );

    void mark ( QueuedPacket & p );

    bool l4s_is_overloaded ( void ) { return p_cl_ >= p_Lmax_; }
    bool classic_is_overloaded ( void ) { return p_c_ >= p_Cmax_; }

    int64_t scale_delta ( uint64_t val );
    uint32_t scale_proba ( double prob );

    void scheduler_update ( void );

public:
    DualQCoupledAQM( const std::string & args );

    void enqueue( QueuedPacket && p ) override;
    QueuedPacket dequeue( void ) override;

    bool empty( void ) const override;

    std::string to_string( void ) const override;

    static unsigned int get_arg( const std::string & args, const std::string & name );

    unsigned int size_bytes( void ) const override;
    unsigned int size_packets( void ) const override;

    bool recur( AbstractDualPI2PacketQueue & queue, uint32_t likelihood );

    void set_periodic_update ( void );
    uint32_t calculate_base_aqm_prob ( uint64_t ref );

    ~DualQCoupledAQM ( void );
};

#endif /* DUALQ_COUPLED_AQM_HH */


