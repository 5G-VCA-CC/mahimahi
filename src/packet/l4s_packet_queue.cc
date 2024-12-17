#include <chrono>

#include "l4s_packet_queue.hh"
#include "timestamp.hh"

using namespace std;

#define DQ_COUNT_INVALID   (uint32_t)-1

L4SPacketQueue::L4SPacketQueue( const string & args )
  : DroppingPacketQueue(args)
{
  /*
  if ( qdelay_ref_ == 0 || max_burst_ == 0 ) {
    throw runtime_error( "PIE AQM queue must have qdelay_ref and max_burst parameters" );
  }
   */
}

void L4SPacketQueue::enqueue( QueuedPacket && p )
{
  calculate_drop_prob();



  assert( good() );
}

//returns true if packet should be dropped.
bool L4SPacketQueue::drop_early ()
{

    return false;
}

QueuedPacket L4SPacketQueue::dequeue( void )
{
  QueuedPacket ret = std::move( DroppingPacketQueue::dequeue () );
  uint32_t now = timestamp();



  return ret;
}

void L4SPacketQueue::calculate_drop_prob( void )
{
  uint64_t now = timestamp();




}
