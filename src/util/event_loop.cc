/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#include <algorithm>

#include "event_loop.hh"
#include "timerfd.hh"
#include "timestamp.hh"
#include "exception.hh"

using namespace std;
using namespace PollerShortNames;

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

int EventLoop::internal_loop( const std::function<uint64_t(void)> & next_deadline_ns )
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

    /* deadline reached -> wake up (an absolute timerfd deadline is not subject to timer slack) */
    Timerfd deadline_timer;
    uint64_t armed_deadline_ns = NO_DEADLINE;
    add_simple_input_handler( deadline_timer,
                              [&] () {
                                  deadline_timer.read_expirations();
                                  armed_deadline_ns = NO_DEADLINE;
                                  return ResultType::Continue;
                              } );

    while ( true ) {
        const uint64_t deadline_ns = next_deadline_ns();

        if ( deadline_ns != armed_deadline_ns ) {
            if ( deadline_ns == NO_DEADLINE ) {
                deadline_timer.disarm();
            } else {
                deadline_timer.set_deadline_ns( initial_timestamp_ns() + deadline_ns );
            }
            armed_deadline_ns = deadline_ns;
        }

        const auto poll_result = poller_.poll( -1 );
        if ( poll_result.result == Poller::Result::Type::Exit ) {
            return poll_result.exit_status;
        }
    }
}
