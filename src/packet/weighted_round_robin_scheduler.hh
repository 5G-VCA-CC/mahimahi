#ifndef WEIGHTED_ROUND_ROBIN_SCHED_HH
#define WEIGHTED_ROUND_ROBIN_SCHED_HH

#include <queue>
#include <cassert>

#include "abstract_l4s_scheduler.hh"

#define MAX_WEIGHT 100

class WRRScheduler : public AbstractL4SScheduler
{
private: 
    // From the c_protection struct of the dualpi2 linux code.
    uint32_t credit_;
    uint32_t credit_init_;
    unsigned char classic_weight_;
    unsigned char l4s_weight_;

    int32_t credit_change_;

    void reset_credit () {
        credit_ = credit_init_;
    };

public:
    WRRScheduler (L4SPacketQueue & l4s_q, CLASSICPacketQueue & classic_q);

    QueueType select_queue( void ) override;

    void apply_credit_change ( void );
};

#endif /* WEIGHTED_ROUND_ROBIN_SCHED_HH */ 