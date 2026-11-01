/* Adapted from project Ringmaster - timerfd.hh 
   Original source: https://github.com/microsoft/ringmaster/blob/main/src/util/timerfd.hh */

#ifndef TIMERFD_HH
#define TIMERFD_HH

#include <cstdint>
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

    /* fire once at an absolute time (in ns) on this timer's clock */
    void set_deadline_ns( const uint64_t deadline_ns );

    void disarm( void );

    /* returns the number of expirations since the last read (0 if none) */
    uint64_t read_expirations( void );
};

#endif /* TIMERFD_HH */
