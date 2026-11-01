/* Adapted from project Ringmaster - timerfd.cc 
   Original source: https://github.com/microsoft/ringmaster/blob/main/src/util/timerfd.cc */

#include <unistd.h>
#include <cerrno>

#include "timerfd.hh"
#include "timestamp.hh"
#include "exception.hh"

using namespace std;

Timerfd::Timerfd( int clockid, int flags )
  : FileDescriptor( SystemCall( "timerfd_create", timerfd_create(clockid, flags) ) ),
    clockid_( clockid ),
    flags_( flags )
{}

void Timerfd::set_time( const timespec & initial_expiration,
                       const timespec & interval )
{
    itimerspec its;
    its.it_value = initial_expiration;
    its.it_interval = interval;

    SystemCall( "timerfd set_time", timerfd_settime(fd_num(), 0, &its, nullptr) );
}

void Timerfd::set_deadline_ns( const uint64_t deadline_ns )
{
    itimerspec its {};
    its.it_value.tv_sec = deadline_ns / NS_PER_SECOND;
    its.it_value.tv_nsec = deadline_ns % NS_PER_SECOND;

    /* an all-zero it_value would disarm the timer instead */
    if ( its.it_value.tv_sec == 0 and its.it_value.tv_nsec == 0 ) {
        its.it_value.tv_nsec = 1;
    }

    SystemCall( "timerfd set_deadline", timerfd_settime(fd_num(), TFD_TIMER_ABSTIME, &its, nullptr) );
}

void Timerfd::disarm( void )
{
    const itimerspec its {};
    SystemCall( "timerfd disarm", timerfd_settime(fd_num(), 0, &its, nullptr) );
}

uint64_t Timerfd::read_expirations( void )
{
    uint64_t expirations = 0;
    const ssize_t bytes_read = ::read( fd_num(), &expirations, sizeof( expirations ) );

    if ( bytes_read < 0 ) {
        if ( errno != EAGAIN ) {
            throw unix_error( "timerfd read" );
        }
        expirations = 0;
    } else if ( bytes_read != sizeof( expirations ) ) {
        throw runtime_error( "timerfd read: short read" );
    }

    register_read();

    return expirations;
}
