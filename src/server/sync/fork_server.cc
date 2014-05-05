#include <cstdlib>
#include <iostream>
#include <thread>
#include <utility>
#include <boost/asio.hpp>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <common/args/server_simple.h>

#include "session.h"
#include "zc_session.h"

namespace asio = boost::asio;
using asio::ip::tcp;

void start_signal (asio::signal_set& sig, tcp::acceptor& acceptor)
{
  sig.async_wait (
    [&] (boost::system::error_code const& ec, int /*signal_number*/)
    {
    	if (! ec)
      {
        if (acceptor.is_open ())
        {
          int status = 0;
          while (waitpid(-1, &status, WNOHANG) > 0) 
          {
          }

          start_signal (sig, acceptor);
        }
      }
      else
      {
        std::cerr << "Signal error: " << ec.message() << std::endl;
        if (acceptor.is_open ())
          start_signal (sig, acceptor);
      }
    }
  );
}

template <typename Session>
void start_accept (tcp::acceptor& acceptor, asio::signal_set& sig, 
    tcp::socket& socket, Session session)
{
	asio::io_service& io_service (socket.get_io_service ());
  acceptor.async_accept (socket, 
    [&] (boost::system::error_code const& ec)
    {
      if (! ec)
      {
        // std::cout << "start_accept: forking...\n";
        io_service.notify_fork (asio::io_service::fork_prepare);
        switch (fork ())
        {
          case 0: 
          {
            io_service.notify_fork (asio::io_service::fork_child);
            acceptor.close ();
            sig.cancel ();
            int ret = (*session) (std::move (socket));
            exit (ret);
          }

          default:
            io_service.notify_fork (asio::io_service::fork_parent);
            socket.close ();
            start_accept (acceptor, sig, socket, session);
            break;

          case -1:
            std::cerr << "fork error: " << errno << "\n";
            abort ();
        }
      }
      else
      {
        std::cerr << "Accept error: " << ec.message() << std::endl;
        start_accept (acceptor, sig, socket, session);
      }
    }
  );
}

template <typename Session>
void server (asio::io_service& io_service, unsigned short port, Session session)
{
  tcp::acceptor a (io_service, tcp::endpoint(tcp::v4(), port));
	asio::signal_set sig (io_service, SIGCHLD);
  tcp::socket socket (io_service);

  start_signal (sig, a);
  start_accept (a, sig, socket, session);

  io_service.run ();
}

int main(int argc, char* argv[])
{
  try
  {
    p52::args::server_simple_args args;
    if (! p52::args::parse_args (argc, argv, args))
      return -1;

    asio::io_service io_service;

    if (args.zero_copy)
      server(io_service, args.port, &session);
    else
      server(io_service, args.port, &zc_session);
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}
