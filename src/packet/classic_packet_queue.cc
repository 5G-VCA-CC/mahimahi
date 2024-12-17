#include <chrono>

#include "classic_packet_queue.hh"
#include "timestamp.hh"

using namespace std;

#define DQ_COUNT_INVALID   (uint32_t)-1

CLASSICPacketQueue::CLASSICPacketQueue( const string & args )
  : DroppingPacketQueue(args)
{
    /*
    if ( qdelay_ref_ == 0 || max_burst_ == 0 ) {
      throw runtime_error( "PIE AQM queue must have qdelay_ref and max_burst parameters" );
    }
     */
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
    QueuedPacket ret = std::move( DroppingPacketQueue::dequeue () );
    uint32_t now = timestamp();



    return ret;
}

void CLASSICPacketQueue::calculate_drop_prob( void )
{
    uint64_t now = timestamp();




}
