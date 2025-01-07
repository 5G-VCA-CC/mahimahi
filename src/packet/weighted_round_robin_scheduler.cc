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
    credit_change_ = 0;
    unsigned int l4s_len_pkt = l4s_queue_.size_packets();
    unsigned int classic_len_pkt = classic_queue_.size_packets();

    assert ( l4s_len_pkt != 0 || classic_len_pkt != 0 );

    if ( l4s_len_pkt > 0 && (classic_len_pkt == 0 || credit_ <= 0) ) {
        // The L4S queue can be dequeued.
        // Increase the credit using classic_weight_ if classic queue non-empty
        QueuedPacket* next_pkt = l4s_queue_.peek(); 
        credit_change_ = classic_len_pkt ? 
            classic_weight_ * next_pkt->contents.size() : 0 ;
        
        return QueueType::L4S;
    }
    else if (classic_len_pkt > 0) {
        /* The classic queue can be dequeued.
           Decrease the credit using l4s_weight_ if L4S queue non-empty */ 
        QueuedPacket* next_pkt = classic_queue_.peek(); 
        credit_change_ = l4s_len_pkt ? 
            (-1) * l4s_weight_ * next_pkt->contents.size() : 0 ;
        
        return QueueType::Classic;
    }
    else {
        // If both queues are empty.
        // Never happens
    }




}

void WRRScheduler::apply_credit_change ( )
{
    credit_ += credit_change_ ;
}

