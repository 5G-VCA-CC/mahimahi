#include "weighted_round_robin_scheduler.hh"

WRRScheduler::WRRScheduler (L4SPacketQueue & l4s_q, CLASSICPacketQueue & classic_q)
: AbstractL4SScheduler (l4s_q, classic_q),
    credit_ ( 0 ),
    credit_init_ ( 0 ),
    credit_change_ ( 0 ),
    classic_weight_ ( 10 ),
    l4s_weight_ ( MAX_WEIGHT - classic_weight_ )
{
    //TODO: initialize credit properly!

}

QueueType WRRScheduler::select_queue ( ) 
{
    if ( l4s_queue_.empty() && classic_queue_.empty() ) {
        reset_credit();
        return QueueType::NONE;
    }

    credit_change_ = 0;

    if ( not l4s_queue_.empty() && (classic_queue_.empty() || credit_ <= 0) ) {
        // The L4S queue can be dequeued.
        // Increase the credit using classic_weight_ if classic queue non-empty
        QueuedPacket* next_pkt = l4s_queue_.peek(); 
        credit_change_ = not classic_queue_.empty() ? 
            classic_weight_ * next_pkt->contents.size() : 0 ;
        
        return QueueType::L4S;
    }
    else if (not classic_queue_.empty()) {
        /* The classic queue can be dequeued.
           Decrease the credit using l4s_weight_ if L4S queue non-empty */ 
        QueuedPacket* next_pkt = classic_queue_.peek(); 
        credit_change_ = not l4s_queue_.empty() ? 
            (-1) * l4s_weight_ * next_pkt->contents.size() : 0 ;
        
        return QueueType::Classic;
    }
    // else both queues are empty, handled at the beginning.
}

void WRRScheduler::apply_credit_change ( )
{
    credit_ += credit_change_ ;
    credit_change_ = 0;
}

