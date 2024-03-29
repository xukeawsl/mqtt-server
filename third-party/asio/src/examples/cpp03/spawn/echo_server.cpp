//
// echo_server.cpp
// ~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2023 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <asio/detached.hpp>
#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/spawn.hpp>
#include <asio/steady_timer.hpp>
#include <asio/write.hpp>
#include <boost/bind/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <iostream>

using asio::ip::tcp;

class session : public boost::enable_shared_from_this<session>
{
public:
  explicit session(asio::io_context& io_context)
    : strand_(asio::make_strand(io_context)),
      socket_(io_context),
      timer_(io_context)
  {
  }

  tcp::socket& socket()
  {
    return socket_;
  }

  void go()
  {
    asio::spawn(strand_,
        boost::bind(&session::echo,
          shared_from_this(), boost::placeholders::_1),
        asio::detached_t());
    asio::spawn(strand_,
        boost::bind(&session::timeout,
          shared_from_this(), boost::placeholders::_1),
        asio::detached_t());
  }

private:
  void echo(asio::yield_context yield)
  {
    try
    {
      char data[128];
      for (;;)
      {
        timer_.expires_after(asio::chrono::seconds(10));
        std::size_t n = socket_.async_read_some(asio::buffer(data), yield);
        asio::async_write(socket_, asio::buffer(data, n), yield);
      }
    }
    catch (std::exception& e)
    {
      socket_.close();
      timer_.cancel();
    }
  }

  void timeout(asio::yield_context yield)
  {
    while (socket_.is_open())
    {
      asio::error_code ignored_ec;
      timer_.async_wait(yield[ignored_ec]);
      if (timer_.expiry() <= asio::steady_timer::clock_type::now())
        socket_.close();
    }
  }

  asio::strand<asio::io_context::executor_type> strand_;
  tcp::socket socket_;
  asio::steady_timer timer_;
};

void do_accept(asio::io_context& io_context,
    unsigned short port, asio::yield_context yield)
{
  tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), port));

  for (;;)
  {
    asio::error_code ec;
    boost::shared_ptr<session> new_session(new session(io_context));
    acceptor.async_accept(new_session->socket(), yield[ec]);
    if (!ec) new_session->go();
  }
}

int main(int argc, char* argv[])
{
  try
  {
    if (argc != 2)
    {
      std::cerr << "Usage: echo_server <port>\n";
      return 1;
    }

    asio::io_context io_context;

    asio::spawn(io_context,
        boost::bind(do_accept, boost::ref(io_context),
          atoi(argv[1]), boost::placeholders::_1),
        asio::detached_t());

    io_context.run();
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}
