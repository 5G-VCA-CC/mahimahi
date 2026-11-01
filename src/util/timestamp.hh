/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#ifndef TIMESTAMP_HH
#define TIMESTAMP_HH

#include <cstdint>
#include <string>

constexpr uint64_t NS_PER_MS = 1000000;
constexpr uint64_t NS_PER_SECOND = 1000000000;

/* nanoseconds elapsed (on CLOCK_MONOTONIC) since initial_timestamp_ns() */
uint64_t timestamp_ns( void );

/* CLOCK_MONOTONIC value, in nanoseconds, when mahimahi started */
uint64_t initial_timestamp_ns( void );

/* CLOCK_REALTIME value, in nanoseconds, read at the same moment as initial_timestamp_ns() */
uint64_t initial_realtime_ns( void );

/* parse a non-negative decimal number of milliseconds (e.g. "12" or "12.000250")
   into nanoseconds, exactly */
uint64_t ms_string_to_ns( const std::string & str );

/* format nanoseconds as milliseconds with nanosecond resolution (e.g. "12.000250") */
std::string ns_to_ms_string( const uint64_t ns );

uint64_t timestamp( void );
uint64_t initial_timestamp( void );

#endif /* TIMESTAMP_HH */
