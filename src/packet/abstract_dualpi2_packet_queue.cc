#include <chrono>

#include "abstract_dualpi2_packet_queue.hh"
#include "timestamp.hh"

using namespace std;

#define DQ_COUNT_INVALID   (uint32_t)-1

AbstractDualPI2PacketQueue::AbstractDualPI2PacketQueue( const string & args )
  : DroppingPacketQueue(args)
{
  
}

