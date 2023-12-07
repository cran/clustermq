/* SPDX-License-Identifier: MPL-2.0 */

#include "precompiled.hpp"
#include "stream_connecter_base.hpp"
#include "session_base.hpp"
#include "address.hpp"
#include "random.hpp"
#include "zmtp_engine.hpp"
#include "raw_engine.hpp"

#ifndef ZMQ_HAVE_WINDOWS
#include <unistd.h>
#else
#include <winsock2.h>
#endif

#include <limits>

zmq::stream_connecter_base_t::stream_connecter_base_t (
  zmq::io_thread_t *io_thread_,
  zmq::session_base_t *session_,
  const zmq::options_t &options_,
  zmq::address_t *addr_,
  bool delayed_start_) :
    own_t (io_thread_, options_),
    io_object_t (io_thread_),
    _addr (addr_),
    _s (retired_fd),
    _handle (static_cast<handle_t> (NULL)),
    _socket (session_->get_socket ()),
    _delayed_start (delayed_start_),
    _reconnect_timer_started (false),
    _current_reconnect_ivl (-1),
    _session (session_)
{
    zmq_assert (_addr);
    _addr->to_string (_endpoint);
    // TODO the return value is unused! what if it fails? if this is impossible
    // or does not matter, change such that endpoint in initialized using an
    // initializer, and make endpoint const
}

zmq::stream_connecter_base_t::~stream_connecter_base_t ()
{
    zmq_assert (!_reconnect_timer_started);
    zmq_assert (!_handle);
    zmq_assert (_s == retired_fd);
}

void zmq::stream_connecter_base_t::process_plug ()
{
    if (_delayed_start)
        add_reconnect_timer ();
    else
        start_connecting ();
}

void zmq::stream_connecter_base_t::process_term (int linger_)
{
    if (_reconnect_timer_started) {
        cancel_timer (reconnect_timer_id);
        _reconnect_timer_started = false;
    }

    if (_handle) {
        rm_handle ();
    }

    if (_s != retired_fd)
        close ();

    own_t::process_term (linger_);
}

void zmq::stream_connecter_base_t::add_reconnect_timer ()
{
    if (options.reconnect_ivl > 0) {
        const int interval = get_new_reconnect_ivl ();
        add_timer (interval, reconnect_timer_id);
        _socket->event_connect_retried (
          make_unconnected_connect_endpoint_pair (_endpoint), interval);
        _reconnect_timer_started = true;
    }
}

int zmq::stream_connecter_base_t::get_new_reconnect_ivl ()
{
    if (options.reconnect_ivl_max > 0) {
        int candidate_interval = 0;
        if (_current_reconnect_ivl == -1)
            candidate_interval = options.reconnect_ivl;
        else if (_current_reconnect_ivl > std::numeric_limits<int>::max () / 2)
            candidate_interval = std::numeric_limits<int>::max ();
        else
            candidate_interval = _current_reconnect_ivl * 2;

        if (candidate_interval > options.reconnect_ivl_max)
            _current_reconnect_ivl = options.reconnect_ivl_max;
        else
            _current_reconnect_ivl = candidate_interval;
        return _current_reconnect_ivl;
    } else {
        if (_current_reconnect_ivl == -1)
            _current_reconnect_ivl = options.reconnect_ivl;
        //  The new interval is the base interval + random value.
        const int random_jitter = generate_random () % options.reconnect_ivl;
        const int interval =
          _current_reconnect_ivl
              < std::numeric_limits<int>::max () - random_jitter
            ? _current_reconnect_ivl + random_jitter
            : std::numeric_limits<int>::max ();
        return interval;
    }
}

void zmq::stream_connecter_base_t::rm_handle ()
{
    rm_fd (_handle);
    _handle = static_cast<handle_t> (NULL);
}

void zmq::stream_connecter_base_t::close ()
{
    // TODO before, this was an assertion for _s != retired_fd, but this does not match usage of close
    if (_s != retired_fd) {
#ifdef ZMQ_HAVE_WINDOWS
        const int rc = closesocket (_s);
        wsa_assert (rc != SOCKET_ERROR);
#else
        const int rc = ::close (_s);
        errno_assert (rc == 0);
#endif
        _socket->event_closed (
          make_unconnected_connect_endpoint_pair (_endpoint), _s);
        _s = retired_fd;
    }
}

void zmq::stream_connecter_base_t::in_event ()
{
    //  We are not polling for incoming data, so we are actually called
    //  because of error here. However, we can get error on out event as well
    //  on some platforms, so we'll simply handle both events in the same way.
    out_event ();
}

void zmq::stream_connecter_base_t::create_engine (
  fd_t fd_, const std::string &local_address_)
{
    const endpoint_uri_pair_t endpoint_pair (local_address_, _endpoint,
                                             endpoint_type_connect);

    //  Create the engine object for this connection.
    i_engine *engine;
    if (options.raw_socket)
        engine = new (std::nothrow) raw_engine_t (fd_, options, endpoint_pair);
    else
        engine = new (std::nothrow) zmtp_engine_t (fd_, options, endpoint_pair);
    alloc_assert (engine);

    //  Attach the engine to the corresponding session object.
    send_attach (_session, engine);

    //  Shut the connecter down.
    terminate ();

    _socket->event_connected (endpoint_pair, fd_);
}

void zmq::stream_connecter_base_t::timer_event (int id_)
{
    zmq_assert (id_ == reconnect_timer_id);
    _reconnect_timer_started = false;
    start_connecting ();
}
