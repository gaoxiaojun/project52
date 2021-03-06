#ifndef _P52_IO_CORO_H_
#define _P52_IO_CORO_H_

#include "asio.h"
#include <memory>

#include <boost/optional.hpp>

#include <yamail/utility/capture.h>

class coro_strategy: public std::enable_shared_from_this<coro_strategy>
{
  boost::optional<asio::yield_context> yield_;

public:
  coro_strategy () {}

  coro_strategy (asio::yield_context yield)
    : yield_ (yield)
  {}
  
  template <class Client, class... Args>
  static void create (asio::io_service& io, Args&& ...args)
  {
    asio::spawn (io,
        [&] (boost::asio::yield_context yield)
        {
          auto&& cs = std::make_shared<coro_strategy> (yield);

          std::make_shared<Client> (cs, io, std::forward<Args> (args)...)->start ();
        }
    );
  }

  template <class Socket>
  static boost::system::error_code 
  close (Socket& socket, boost::system::error_code& ec)
  {
  	return socket.close (ec);
  }

  template <class Resolver, class Query, class Handler>
  void resolve (Resolver& resolver, Query&& query, Handler&& handler)
  {
    boost::system::error_code ec;
    auto iterator = resolver.async_resolve (
        std::forward<Query> (query), (*yield_) [ec]);

    resolver.get_io_service ().post (
      y::utility::capture (
        [ec, iterator] (Handler& handler) { handler (ec, iterator); },
        std::forward<Handler> (handler)
      )
    );
  }

  template <class Socket, class EndpointIterator, class Handler>
  void connect (Socket&& socket, EndpointIterator&& iter, Handler&& handler)
  {
    boost::system::error_code ec;
    auto iter2 = asio::async_connect (std::forward<Socket> (socket), 
        std::forward<EndpointIterator> (iter), (*yield_) [ec]);

    socket.get_io_service ().post (
      y::utility::capture (
        [ec, iter2] (Handler& handler) { handler (ec, iter2); },
        std::forward<Handler> (handler)
      )
    );
  }

  template <class Socket, class Buffer, class Handler>
  void write (Socket&& socket, Buffer&& buffer, Handler&& handler)
  {
    boost::system::error_code ec;
    auto bytes = asio::async_write (std::forward<Socket> (socket),
        std::forward<Buffer> (buffer), (*yield_) [ec]);

    socket.get_io_service ().post (
      y::utility::capture (
        [ec, bytes] (Handler& handler) { handler (ec, bytes); },
        std::forward<Handler> (handler)
      )
    );
  }

  template <class Socket, class Buffer, class Delim, class Handler>
  void read_until (Socket&& socket, Buffer&& buffer, 
      Delim&& delim, Handler&& handler)
  {
    boost::system::error_code ec;
    auto bytes = asio::async_read_until(std::forward<Socket> (socket),
        std::forward<Buffer> (buffer), std::forward<Delim> (delim),
        (*yield_) [ec]);

    socket.get_io_service ().post (
      y::utility::capture (
        [ec, bytes] (Handler& handler) { handler (ec, bytes); },
        std::forward<Handler> (handler)
      )
    );
  }

  template <class Socket, class Buffers, class Handler>
  void read_some (Socket& socket, Buffers const& buffers, Handler&& handler)
  {

    boost::system::error_code ec;
    auto bytes = socket.async_read_some (buffers, (*yield_) [ec]);

    socket.get_io_service ().post (
      y::utility::capture (
        [ec, bytes] (Handler& handler) { handler (ec, bytes); },
        std::forward<Handler> (handler)
      )
    );
  }
};

#endif // _P52_IO_CORO_H_
