#ifndef WEIGHTED_ROUND_ROBIN_SCHED_HH
#define WEIGHTED_ROUND_ROBIN_SCHED_HH

#include <queue>
#include <cassert>

#include "abstract_dualq_scheduler.hh"

#define MAX_WEIGHT 100

#define KERNEL_HLEN_DIFF 10

class WRRScheduler : public AbstractDualQScheduler
{
private: 
    // From the c_protection struct of the dualpi2 linux code.
    int32_t credit_;
    int32_t credit_init_;
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