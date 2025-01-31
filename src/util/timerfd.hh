/* Adapted from project Ringmaster - timerfd.hh 
   Original source: https://github.com/microsoft/ringmaster/blob/main/src/util/timerfd.hh */

#ifndef TIMERFD_HH
#define TIMERFD_HH

#include <time.h>
#include <sys/timerfd.h>

#include "file_descriptor.hh"

class Timerfd : public FileDescriptor
{
private:
    int clockid_;
    int flags_;

public:
    Timerfd(int clockid = CLOCK_MONOTONIC, int flags = TFD_NONBLOCK);

    void set_time( const timespec & initial_expiration,
                    const timespec & interval );
};

#endif /* TIMERFD_HH */