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

int LossQueue::wait_time( void )
{
    return packet_queue_.empty() ? numeric_limits<int>::max() : 0;
}

bool IIDLoss::drop_packet( const string & packet __attribute((unused)) )
{
    return drop_dist_( prng_ );
}

static const double US_PER_SECOND = 1000000.0;

StochasticSwitchingLink::StochasticSwitchingLink( const double mean_on_time, const double mean_off_time )
    : link_is_on_( false ),
      on_process_( 1.0 / (US_PER_SECOND * mean_off_time) ),
      off_process_( 1.0 / (US_PER_SECOND * mean_on_time) ),
      next_switch_time_( timestamp_us() )
{}

uint64_t bound( const double x )
{
    constexpr uint64_t MAX_BOUNDED_INTERVAL_US = ( static_cast<uint64_t>( 1 ) << 30 ) * 1000;

    if ( x > MAX_BOUNDED_INTERVAL_US ) {
        return MAX_BOUNDED_INTERVAL_US;
    }

    return x;
}

int StochasticSwitchingLink::wait_time( void )
{
    const uint64_t now = timestamp_us();

    while ( next_switch_time_ <= now ) {
        /* switch */
        link_is_on_ = !link_is_on_;
        /* worried about integer overflow when mean time = 0 */
        next_switch_time_ += bound( (link_is_on_ ? off_process_ : on_process_)( prng_ ) );
    }

    if ( LossQueue::wait_time() == 0 ) {
        return 0;
    }

    if ( next_switch_time_ - now > static_cast<uint64_t>( numeric_limits<int>::max() ) ) {
        return numeric_limits<int>::max();
    }

    return next_switch_time_ - now;
}

bool StochasticSwitchingLink::drop_packet( const string & packet __attribute((unused)) )
{
    return !link_is_on_;
}

PeriodicSwitchingLink::PeriodicSwitchingLink( const double on_time, const double off_time )
    : link_is_on_( false ),
      on_time_( bound( US_PER_SECOND * on_time ) ),
      off_time_( bound( US_PER_SECOND * off_time ) ),
      next_switch_time_( timestamp_us() )
{
  if ( on_time_ == 0 and off_time_ == 0 ) {
      throw runtime_error( "on_time and off_time cannot both be zero" );
  }
}

int PeriodicSwitchingLink::wait_time( void )
{
    const uint64_t now = timestamp_us();

    while ( next_switch_time_ <= now ) {
        /* switch */
        link_is_on_ = !link_is_on_;
        next_switch_time_ += link_is_on_ ? on_time_ : off_time_;
    }

    if ( LossQueue::wait_time() == 0 ) {
        return 0;
    }

    if ( next_switch_time_ - now > static_cast<uint64_t>( numeric_limits<int>::max() ) ) {
        return numeric_limits<int>::max();
    }

    return next_switch_time_ - now;
}

bool PeriodicSwitchingLink::drop_packet( const string & packet __attribute((unused)) )
{
    return !link_is_on_;
}
