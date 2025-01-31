/* Adapted from project Ringmaster - timerfd.cc 
   Original source: https://github.com/microsoft/ringmaster/blob/main/src/util/timerfd.cc */

#include <unistd.h>

#include "timerfd.hh"
#include "exception.hh"

using namespace std;

Timerfd::Timerfd( int clockid, int flags )
  : FileDescriptor( SystemCall( "timerfd_create", timerfd_create(clockid, flags) ) )
{
    clockid_ = clockid;
    flags_ = flags;
}

void Timerfd::set_time( const timespec & initial_expiration,
                       const timespec & interval )
{
    itimerspec its;
    its.it_value = initial_expiration;
    its.it_interval = interval;

    SystemCall( "timerfd set_time", timerfd_settime(fd_num(), 0, &its, nullptr) );
}