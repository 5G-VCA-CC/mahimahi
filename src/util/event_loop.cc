/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#include <algorithm>
#include <cstdlib>
#include <iostream>

#include "event_loop.hh"
#include "exception.hh"
#include "timestamp.hh"

using namespace std;
using namespace PollerShortNames;

namespace {
bool mmdbg_timing_enabled( void )
{
    static const bool enabled = getenv( "MAHIMAHI_MMDBG_TIMING" ) != nullptr;
    return enabled;
}

void mmdbg_log( const string & line )
{
    if ( mmdbg_timing_enabled() ) {
        cerr << line << endl;
    }
}
}

EventLoop::EventLoop()
    : signals_( { SIGCHLD, SIGCONT, SIGHUP, SIGTERM, SIGQUIT, SIGINT, SIGUSR1 } ),
      poller_(),
      child_processes_()
{
    signals_.set_as_mask(); /* block signals so we can later use signalfd to read them */
}

void EventLoop::add_simple_input_handler( FileDescriptor & fd,
                                          const Poller::Action::CallbackType & callback )
{
    poller_.add_action( Poller::Action( fd, Direction::In, callback ) );
}

Result EventLoop::handle_signal( const signalfd_siginfo & sig )
{
    switch ( sig.ssi_signo ) {
    case SIGCONT:
        /* resume child processes too */
        for ( auto & x : child_processes_ ) {
            x.second.resume();
        }
        break;

    case SIGCHLD:
        if ( child_processes_.empty() ) {
            throw runtime_error( "received SIGCHLD without any managed children" );
        }

        /* find which children are waitable */
        /* we can't count on getting exactly one SIGCHLD per waitable event, so search */
        for ( auto & procpair : child_processes_ ) {
            ChildProcess & proc = procpair.second;
            if ( proc.terminated() or ( !proc.waitable() ) ) {
                continue; /* not the process we're looking for */
            }

            proc.wait( true ); /* get process's change of state.
                               true => throws exception if no change available */

            if ( proc.terminated() ) {
                if ( proc.exit_status() != 0 and proc.exit_status() != procpair.first ) {
                    proc.throw_exception();
                }

                /* quit if all children have quit */
                if ( all_of( child_processes_.begin(), child_processes_.end(),
                             [] ( pair<int, ChildProcess> & x ) { return x.second.terminated(); } ) ) {
                    return ResultType::Exit;
                }

                return proc.exit_status() == procpair.first ? ResultType::Continue : ResultType::Exit;
            } else if ( !proc.running() ) {
                /* suspend parent too */
                SystemCall( "raise", raise( SIGSTOP ) );
            }
        }

        break;

    case SIGUSR1:
        handle_sigusr1();
        break;

    case SIGHUP:
    case SIGTERM:
    case SIGQUIT:
    case SIGINT:
        return ResultType::Exit;
    default:
        throw runtime_error( "EventLoop: unknown signal" );
    }

    return ResultType::Continue;
}

int EventLoop::internal_loop( const std::function<int(void)> & wait_time_us )
{
    TemporarilyUnprivileged tu;

    /* verify that signal mask is intact */
    SignalMask current_mask = SignalMask::current_mask();

    if ( !( signals_ == current_mask ) ) {
        throw runtime_error( "EventLoop: signal mask has been altered" );
    }

    SignalFD signal_fd( signals_ );

    /* we get signal -> main screen turn on */
    add_simple_input_handler( signal_fd.fd(),
                              [&] () { return handle_signal( signal_fd.read_signal() ); } );

    while ( true ) {
        const int requested_timeout_us = wait_time_us();
        const uint64_t poll_start_us = timestamp_us();
        const auto poll_result = poller_.poll_us( requested_timeout_us );
        const uint64_t poll_end_us = timestamp_us();

        if ( mmdbg_timing_enabled() ) {
            static uint64_t loop_idx = 0;
            loop_idx++;
            if ( requested_timeout_us == 0
                 or poll_result.result == Poller::Result::Type::Timeout
                 or ( loop_idx % 1024 == 0 ) ) {
                string result_name = "Success";
                if ( poll_result.result == Poller::Result::Type::Timeout ) {
                    result_name = "Timeout";
                } else if ( poll_result.result == Poller::Result::Type::Exit ) {
                    result_name = "Exit";
                }

                mmdbg_log( "MMDBG component=EventLoop fn=internal_loop_poll"
                           " loop_idx=" + to_string( loop_idx ) +
                           " requested_timeout_us=" + to_string( requested_timeout_us ) +
                           " poll_start_us=" + to_string( poll_start_us ) +
                           " poll_end_us=" + to_string( poll_end_us ) +
                           " actual_sleep_us=" + to_string( poll_end_us - poll_start_us ) +
                           " result=" + result_name );
            }
        }

        if ( poll_result.result == Poller::Result::Type::Exit ) {
            return poll_result.exit_status;
        }
    }
}
