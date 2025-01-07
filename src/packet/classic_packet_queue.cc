#include <chrono>

#include "classic_packet_queue.hh"
#include "timestamp.hh"

using namespace std;

CLASSICPacketQueue::CLASSICPacketQueue( const string & args )
  : AbstractDualPI2PacketQueue(args)
{
  
}

void CLASSICPacketQueue::enqueue( QueuedPacket && p )
{
    calculate_drop_prob();



    assert( good() );
}

//returns true if packet should be dropped.
bool CLASSICPacketQueue::drop_early ()
{

    return false;
}

QueuedPacket CLASSICPacketQueue::dequeue( void )
{
    // TODO: Check if keep the DroppingPacketqueue link below
    QueuedPacket ret = std::move( DroppingPacketQueue::dequeue () );
    uint32_t now = timestamp();



    return ret;
}

void CLASSICPacketQueue::calculate_drop_prob( void )
{
    uint64_t now = timestamp();




}
