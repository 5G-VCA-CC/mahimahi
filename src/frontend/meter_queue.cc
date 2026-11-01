/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#include <limits>

#include "meter_queue.hh"
#include "util.hh"
#include "timestamp.hh"

using namespace std;

MeterQueue::MeterQueue( const string & name, const bool graph )
    : packet_queue_(),
      graph_( nullptr )
{
    assert_not_root();

    if ( graph ) {
        graph_.reset( new BinnedLiveGraph( name, { make_tuple( 0.0, 0.0, 0.4, 1.0, false ) }, "throughput (Mbps)", 8.0 / 1000000.0, true, 500 * NS_PER_MS, [] ( int & x ) { x = 0; } ) );
    }
}

void MeterQueue::read_packet( const string & contents )
{
    packet_queue_.emplace( contents );

    /* meter it */
    if ( graph_ ) {
        graph_->add_value_now( 0, contents.size() );
    }
}

void MeterQueue::write_packets( FileDescriptor & fd )
{
    while ( not packet_queue_.empty() ) {
        fd.write( packet_queue_.front() );
        packet_queue_.pop();
    }
}

uint64_t MeterQueue::next_event_time_ns( void ) const
{
    /* packets leave as soon as the sibling is writable; nothing is timed */
    return numeric_limits<uint64_t>::max();
}
