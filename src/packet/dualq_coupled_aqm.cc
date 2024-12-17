#include <chrono>

#include "dualq_coupled_aqm.hh"
#include "timestamp.hh"

using namespace std;

DualQCoupledAQM::DualQCoupledAQM( const string & args )
  : k_ ( 2 ),
    lq_ ( 0 ),
    cq_ ( 0 )

{

  /*
  if ( qdelay_ref_ == 0 || max_burst_ == 0 ) {
    throw runtime_error( "Missing params:..." );
  }
   */


}

void DualQCoupledAQM::enqueue( QueuedPacket && p )
{
  calculate_drop_prob();


  //assert( good() );
}

QueuedPacket DualQCoupledAQM::dequeue( void )
{
    //QueuedPacket ret = std::move( AbstractPacketQueue::dequeue () );
    QueuedPacket ret (0, 0);
    //uint32_t now = timestamp();

    // ...

    return ret;
}

bool DualQCoupledAQM::empty( void ) const
{
    return false;
}

std::string DualQCoupledAQM::to_string( void ) const
{
    return "";
}

unsigned int DualQCoupledAQM::size_bytes( void ) const
{
    return 0;
}

unsigned int DualQCoupledAQM::size_packets( void ) const
{
    return 0;
}

//returns true if packet should be dropped.
bool DualQCoupledAQM::drop_early ()
{
    return false;
}

void DualQCoupledAQM::calculate_drop_prob( void )
{
  // uint64_t now = timestamp();

}









