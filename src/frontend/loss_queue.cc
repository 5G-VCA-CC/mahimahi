/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#include <limits>
#include <stdexcept>

#include "loss_queue.hh"
#include "timestamp.hh"

using namespace std;

LossQueue::LossQueue()
    : prng_( random_device()() )
{}

void LossQueue::read_packet( const string & contents )
{
    if ( not drop_packet( contents ) ) {
        packet_queue_.emplace( contents );
    }
}

void LossQueue::write_packets( FileDescriptor & fd )
{
    while ( not packet_queue_.empty() ) {
        fd.write( packet_queue_.front() );
        packet_queue_.pop();
    }
}

uint64_t LossQueue::next_event_time_ns( void )
{
    /* packets leave as soon as the sibling is writable; nothing is timed */
    return numeric_limits<uint64_t>::max();
}

bool IIDLoss::drop_packet( const string & packet __attribute((unused)) )
{
    return drop_dist_( prng_ );
}

static const double NS_PER_SECOND_DOUBLE = NS_PER_SECOND;

StochasticSwitchingLink::StochasticSwitchingLink( const double mean_on_time, const double mean_off_time )
    : link_is_on_( false ),
      on_process_( 1.0 / (NS_PER_SECOND_DOUBLE * mean_off_time) ),
      off_process_( 1.0 / (NS_PER_SECOND_DOUBLE * mean_on_time) ),
      next_switch_time_( timestamp_ns() )
{}

uint64_t bound( const double x )
{
    const uint64_t MAX_INTERVAL_NS = uint64_t( 1 << 30 ) * NS_PER_MS;

    if ( x > MAX_INTERVAL_NS ) {
        return MAX_INTERVAL_NS;
    }

    return x;
}

void StochasticSwitchingLink::update_link_state( const uint64_t now )
{
    while ( next_switch_time_ <= now ) {
        /* switch */
        link_is_on_ = !link_is_on_;
        /* worried about integer overflow when mean time = 0 */
        next_switch_time_ += bound( (link_is_on_ ? off_process_ : on_process_)( prng_ ) );
    }
}

bool StochasticSwitchingLink::drop_packet( const string & packet __attribute((unused)) )
{
    update_link_state( timestamp_ns() );

    return !link_is_on_;
}

PeriodicSwitchingLink::PeriodicSwitchingLink( const double on_time, const double off_time )
    : link_is_on_( false ),
      on_time_( bound( NS_PER_SECOND_DOUBLE * on_time ) ),
      off_time_( bound( NS_PER_SECOND_DOUBLE * off_time ) ),
      next_switch_time_( timestamp_ns() )
{
  if ( on_time_ == 0 and off_time_ == 0 ) {
      throw runtime_error( "on_time and off_time cannot both be zero" );
  }
}

void PeriodicSwitchingLink::update_link_state( const uint64_t now )
{
    while ( next_switch_time_ <= now ) {
        /* switch */
        link_is_on_ = !link_is_on_;
        next_switch_time_ += link_is_on_ ? on_time_ : off_time_;
    }
}

bool PeriodicSwitchingLink::drop_packet( const string & packet __attribute((unused)) )
{
    update_link_state( timestamp_ns() );

    return !link_is_on_;
}
