#define BOOST_SPIRIT_THREADSAFE
#define PHOENIX_THREADSAFE
#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include <boost/algorithm/string/predicate.hpp>

#include "../common/outbuf.h"
#include "../common/rfc822.h"
#include "../common/reply.h"

#include <zerocopy/streambuf.h>
#include <zerocopy/exp_iterator.h>

#include <boost/spirit/include/support_istream_iterator.hpp>

template <typename F>
std::unique_ptr<
zerocopy::basic_streambuf<char, std::char_traits<char>,
  F, std::allocator<char>, std::allocator<void>
>>
make_zc_streambuf (F&& func)
{
  return std::unique_ptr<
    zerocopy::basic_streambuf<char, std::char_traits<char>, F,
                    std::allocator<char>, std::allocator<void>>
  > (new zerocopy::basic_streambuf<char, std::char_traits<char>, F,
                      std::allocator<char>, std::allocator<void>> (
        0, 0, 0, 0, 0, std::forward<F> (func))
  );
}


namespace asio = boost::asio;
using asio::ip::tcp;

template <typename Socket>
class read_handler
{
  Socket& sock_;
  int secs_;
public:
  read_handler (Socket& sock) 
    : sock_ (sock)
    {}

  template <typename Streambuf>
  bool operator() (Streambuf* sb) const
  {
    std::size_t n = sock_.read_some (sb->prepare (1024));
    sb->commit (n);
    return true;
  };
};


class zc_session_no_timer 
    : public std::enable_shared_from_this<zc_session_no_timer>
{
public:
  explicit zc_session_no_timer (tcp::socket socket)
    : socket_(std::move(socket))
    , io_service_ (socket_.get_io_service ())
  {
  }

  void go()
  {
    auto self (shared_from_this());
    asio::spawn (io_service_,
        [this, self](asio::yield_context yield)
        {
          try
          {
            auto ibuf = make_zc_streambuf (
                read_handler<tcp::socket> (socket_)
            );

            auto obuf = make_outbuf_ptr (
              [this, &yield] (boost::system::error_code& ec, 
                asio::const_buffers_1 const& buf) -> std::size_t
              {
                return asio::async_write (socket_, buf, yield [ec]);
              }
            );

            std::istream is (&*ibuf);
            std::ostream os (&*obuf);

            // disable skipping of whitespaces
            is.unsetf (std::ios::skipws);

            os << "220 localhost STMP\r\n";

            for (;;)
            {
              std::string line;
              std::getline (is, line);

              if (! is.good () || ! os.good ())
              {
                std::cerr << "client hangup\n";
                break;
              }

              // std::cout << "got line = " << line << "\n";

              if( boost::algorithm::iequals (line, "DATA\r") ) {
                command_reply (os, 
                    "354 Enter mail, end with \".\" on a line by itself");

                auto first = ibuf->exp_begin ();
                auto last = ibuf->exp_end ();

                try {
                  if (rfc822::parse (first, last))
                  {
                    // std::cout << "parse ok\n";
                    ibuf->detach (last.get ());
                    command_reply (os);
                  }
                  else
                  {
                    std::cerr << "cannot parse message\n";
                    command_reply (os, "451 rfc2822 violation\r\n");
                  }
                } 
                catch (std::exception const& e)
                {
                  std::cerr << "cannot parse message: " << e.what () << "\n";
                  command_reply (os, std::string("451 ") + e.what() + "\r\n");
                }
              } else if( boost::algorithm::iequals (line, "QUIT\r") ) {
                std::cerr << "got QUIT, closing connection\n";
                command_reply (os, "221 Goodbye");
                break;
              } else if( boost::algorithm::istarts_with (line, "HELO") ) {
                command_reply (os);
              } else if( boost::algorithm::istarts_with (line, "MAIL ") ) {
                command_reply (os);
              } else if( boost::algorithm::istarts_with (line, "RCPT ") ) {
                command_reply (os);
              } else {
                std::cerr << "bad command: " << line << ", closing connection\n";
                command_reply (os, "400 Bad Command, Goodbye");
                break;
              }
            }
          }

          catch (std::exception& e)
          {
          	std::cerr << "exception: " << e.what () << "\n";
            socket_.close();
            abort ();
          }

          std::cout << "connection closed\n";

          socket_.close();
        });
  }

private:
  tcp::socket socket_;
  asio::io_service& io_service_;
};

