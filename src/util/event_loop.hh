/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#ifndef EVENT_LOOP_HH
#define EVENT_LOOP_HH

#include <vector>
#include <functional>
#include <cstdint>
#include <limits>

#include "poller.hh"
#include "file_descriptor.hh"
#include "signalfd.hh"
#include "child_process.hh"
#include "util.hh"

class EventLoop
{
private:
    SignalMask signals_;
    Poller poller_;
    std::vector<std::pair<int, ChildProcess>> child_processes_;
    PollerShortNames::Result handle_signal( const signalfd_siginfo & sig );

protected:
    void add_action( Poller::Action action ) { poller_.add_action( action ); }

    /* next_deadline_ns returns when the loop should next wake up, in timestamp_ns() time,
       or NO_DEADLINE to wait only for file descriptor events */
    int internal_loop( const std::function<uint64_t(void)> & next_deadline_ns );

    virtual void handle_sigusr1() {}

public:
    static constexpr uint64_t NO_DEADLINE = std::numeric_limits<uint64_t>::max();

    EventLoop();

    void add_simple_input_handler( FileDescriptor & fd, const Poller::Action::CallbackType & callback );

    template <typename... Targs>
    void add_child_process( Targs&&... Fargs )
    {
        child_processes_.emplace_back( -1,
                                       ChildProcess( std::forward<Targs>( Fargs )... ) );
    }

    template <typename... Targs>
    void add_special_child_process( const int continue_status,
                                    Targs&&... Fargs )
    {
        /* parent won't quit when this process quits */
        child_processes_.emplace_back( continue_status, ChildProcess( std::forward<Targs>( Fargs )... ) );
    }

    int loop( void ) { return internal_loop( [] () { return NO_DEADLINE; } ); } /* no timeout */

    virtual ~EventLoop() {}
};

#endif /* EVENT_LOOP_HH */
