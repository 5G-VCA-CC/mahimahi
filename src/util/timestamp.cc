/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#include <ctime>

#include "timestamp.hh"
#include "exception.hh"

namespace {
    const uint64_t NS_PER_US = 1000;
    const uint64_t NS_PER_MS = 1000000;
}

uint64_t initial_timestamp( void )
{
    return initial_timestamp_ns() / NS_PER_MS;
}

uint64_t timestamp( void )
{
    return timestamp_ns() / NS_PER_MS;
}

uint64_t initial_timestamp_us( void )
{
    return initial_timestamp_ns() / NS_PER_US;
}

uint64_t timestamp_us( void )
{
    return timestamp_ns() / NS_PER_US;
}

uint64_t raw_timestamp_ns( void )
{
    timespec ts;
    SystemCall( "clock_gettime", clock_gettime( CLOCK_REALTIME, &ts ) );

    return ts.tv_nsec + uint64_t( ts.tv_sec ) * 1000000000;
}

uint64_t initial_timestamp_ns( void )
{
    static uint64_t initial_value_ns = raw_timestamp_ns();
    return initial_value_ns;
}

uint64_t timestamp_ns( void )
{
    return raw_timestamp_ns() - initial_timestamp_ns();
}