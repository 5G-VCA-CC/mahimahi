/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#include <ctime>
#include <limits>
#include <algorithm>
#include <stdexcept>

#include "timestamp.hh"
#include "exception.hh"

using namespace std;

static uint64_t raw_timestamp_ns( const clockid_t clock )
{
    timespec ts;
    SystemCall( "clock_gettime", clock_gettime( clock, &ts ) );

    return ts.tv_nsec + uint64_t( ts.tv_sec ) * NS_PER_SECOND;
}

struct InitialTimestamps
{
    uint64_t monotonic_ns;
    uint64_t realtime_ns;
};

static const InitialTimestamps & initial_timestamps( void )
{
    static const InitialTimestamps initial_values { raw_timestamp_ns( CLOCK_MONOTONIC ),
                                                    raw_timestamp_ns( CLOCK_REALTIME ) };
    return initial_values;
}

uint64_t initial_timestamp_ns( void )
{
    return initial_timestamps().monotonic_ns;
}

uint64_t initial_realtime_ns( void )
{
    return initial_timestamps().realtime_ns;
}

uint64_t timestamp_ns( void )
{
    return raw_timestamp_ns( CLOCK_MONOTONIC ) - initial_timestamp_ns();
}

uint64_t ms_string_to_ns( const string & str )
{
    const size_t FRACTION_DIGITS = 6; /* nanosecond resolution */

    const size_t point = str.find( '.' );
    const string integer_part = str.substr( 0, point );
    const string fraction_part = ( point == string::npos ) ? "" : str.substr( point + 1 );

    if ( integer_part.empty() and fraction_part.empty() ) {
        throw runtime_error( "Invalid millisecond value: \"" + str + "\"" );
    }

    if ( integer_part.find_first_not_of( "0123456789" ) != string::npos
         or fraction_part.find_first_not_of( "0123456789" ) != string::npos ) {
        throw runtime_error( "Invalid millisecond value: \"" + str + "\"" );
    }

    if ( fraction_part.size() > FRACTION_DIGITS
         and fraction_part.find_first_not_of( '0', FRACTION_DIGITS ) != string::npos ) {
        throw runtime_error( "Millisecond value has sub-nanosecond precision: \"" + str + "\"" );
    }

    const uint64_t MAX_MS = numeric_limits<uint64_t>::max() / NS_PER_MS - 1;

    uint64_t ms = 0;
    for ( const char digit : integer_part ) {
        if ( ms > MAX_MS / 10 ) {
            throw runtime_error( "Millisecond value too large: \"" + str + "\"" );
        }
        ms = ms * 10 + ( digit - '0' );
    }

    if ( ms > MAX_MS ) {
        throw runtime_error( "Millisecond value too large: \"" + str + "\"" );
    }

    uint64_t ns = 0;
    uint64_t place = NS_PER_MS / 10;
    for ( size_t i = 0; i < min( fraction_part.size(), FRACTION_DIGITS ); i++ ) {
        ns += ( fraction_part[ i ] - '0' ) * place;
        place /= 10;
    }

    return ms * NS_PER_MS + ns;
}

string ns_to_ms_string( const uint64_t ns )
{
    const string fraction = to_string( ns % NS_PER_MS );

    return to_string( ns / NS_PER_MS ) + "." + string( 6 - fraction.size(), '0' ) + fraction;
}
