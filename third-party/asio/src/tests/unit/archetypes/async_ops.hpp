//
// archetypes/async_ops.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2023 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef ARCHETYPES_ASYNC_OPS_HPP
#define ARCHETYPES_ASYNC_OPS_HPP

#include <functional>
#include "asio/associated_allocator.hpp"
#include "asio/associated_executor.hpp"
#include "asio/async_result.hpp"
#include "asio/error.hpp"

namespace archetypes {

namespace bindns = std;

template <typename CompletionToken>
typename asio::async_result<asio::decay_t<CompletionToken>,
  void()>::return_type
async_op_0(CompletionToken&& token)
{
  typedef typename asio::async_result<asio::decay_t<CompletionToken>,
    void()>::completion_handler_type handler_type;

  asio::async_completion<CompletionToken,
    void()> completion(token);

  typename asio::associated_allocator<handler_type>::type a
    = asio::get_associated_allocator(completion.completion_handler);

  typename asio::associated_executor<handler_type>::type ex
    = asio::get_associated_executor(completion.completion_handler);

  ex.post(static_cast<handler_type&&>(completion.completion_handler), a);

  return completion.result.get();
}

template <typename CompletionToken>
typename asio::async_result<asio::decay_t<CompletionToken>,
  void(asio::error_code)>::return_type
async_op_ec_0(bool ok, ASIO_MOVE_ARG(CompletionToken) token)
{
  typedef typename asio::async_result<asio::decay_t<CompletionToken>,
    void(asio::error_code)>::completion_handler_type handler_type;

  asio::async_completion<CompletionToken,
    void(asio::error_code)> completion(token);

  typename asio::associated_allocator<handler_type>::type a
    = asio::get_associated_allocator(completion.completion_handler);

  typename asio::associated_executor<handler_type>::type ex
    = asio::get_associated_executor(completion.completion_handler);

  if (ok)
  {
    ex.post(
        bindns::bind(
          static_cast<handler_type&&>(completion.completion_handler),
          asio::error_code()), a);
  }
  else
  {
    ex.post(
        bindns::bind(
          static_cast<handler_type&&>(completion.completion_handler),
          asio::error_code(asio::error::operation_aborted)), a);
  }

  return completion.result.get();
}

template <typename CompletionToken>
typename asio::async_result<asio::decay_t<CompletionToken>,
  void(std::exception_ptr)>::return_type
async_op_ex_0(bool ok, ASIO_MOVE_ARG(CompletionToken) token)
{
  typedef typename asio::async_result<asio::decay_t<CompletionToken>,
    void(std::exception_ptr)>::completion_handler_type handler_type;

  asio::async_completion<CompletionToken,
    void(std::exception_ptr)> completion(token);

  typename asio::associated_allocator<handler_type>::type a
    = asio::get_associated_allocator(completion.completion_handler);

  typename asio::associated_executor<handler_type>::type ex
    = asio::get_associated_executor(completion.completion_handler);

  if (ok)
  {
    ex.post(
        bindns::bind(
          static_cast<handler_type&&>(completion.completion_handler),
          std::exception_ptr()), a);
  }
  else
  {
    ex.post(
        bindns::bind(
          static_cast<handler_type&&>(completion.completion_handler),
          std::make_exception_ptr(std::runtime_error("blah"))), a);
  }

  return completion.result.get();
}

template <typename CompletionToken>
typename asio::async_result<asio::decay_t<CompletionToken>,
  void(int)>::return_type
async_op_1(ASIO_MOVE_ARG(CompletionToken) token)
{
  typedef typename asio::async_result<asio::decay_t<CompletionToken>,
    void(int)>::completion_handler_type handler_type;

  asio::async_completion<CompletionToken,
    void(int)> completion(token);

  typename asio::associated_allocator<handler_type>::type a
    = asio::get_associated_allocator(completion.completion_handler);

  typename asio::associated_executor<handler_type>::type ex
    = asio::get_associated_executor(completion.completion_handler);

  ex.post(
      bindns::bind(
        static_cast<handler_type&&>(completion.completion_handler),
        42), a);

  return completion.result.get();
}

template <typename CompletionToken>
typename asio::async_result<asio::decay_t<CompletionToken>,
  void(asio::error_code, int)>::return_type
async_op_ec_1(bool ok, ASIO_MOVE_ARG(CompletionToken) token)
{
  typedef typename asio::async_result<asio::decay_t<CompletionToken>,
    void(asio::error_code, int)>::completion_handler_type
      handler_type;

  asio::async_completion<CompletionToken,
    void(asio::error_code, int)> completion(token);

  typename asio::associated_allocator<handler_type>::type a
    = asio::get_associated_allocator(completion.completion_handler);

  typename asio::associated_executor<handler_type>::type ex
    = asio::get_associated_executor(completion.completion_handler);

  if (ok)
  {
    ex.post(
        bindns::bind(
          static_cast<handler_type&&>(completion.completion_handler),
          asio::error_code(), 42), a);
  }
  else
  {
    ex.post(
        bindns::bind(
          static_cast<handler_type&&>(completion.completion_handler),
          asio::error_code(asio::error::operation_aborted),
          0), a);
  }

  return completion.result.get();
}

template <typename CompletionToken>
typename asio::async_result<asio::decay_t<CompletionToken>,
  void(std::exception_ptr, int)>::return_type
async_op_ex_1(bool ok, ASIO_MOVE_ARG(CompletionToken) token)
{
  typedef typename asio::async_result<asio::decay_t<CompletionToken>,
    void(std::exception_ptr, int)>::completion_handler_type
      handler_type;

  asio::async_completion<CompletionToken,
    void(std::exception_ptr, int)> completion(token);

  typename asio::associated_allocator<handler_type>::type a
    = asio::get_associated_allocator(completion.completion_handler);

  typename asio::associated_executor<handler_type>::type ex
    = asio::get_associated_executor(completion.completion_handler);

  if (ok)
  {
    ex.post(
        bindns::bind(
          static_cast<handler_type&&>(completion.completion_handler),
          std::exception_ptr(), 42), a);
  }
  else
  {
    ex.post(
        bindns::bind(
          static_cast<handler_type&&>(completion.completion_handler),
          std::make_exception_ptr(std::runtime_error("blah")), 0), a);
  }

  return completion.result.get();
}

template <typename CompletionToken>
typename asio::async_result<asio::decay_t<CompletionToken>,
  void(int, double)>::return_type
async_op_2(ASIO_MOVE_ARG(CompletionToken) token)
{
  typedef typename asio::async_result<asio::decay_t<CompletionToken>,
    void(int, double)>::completion_handler_type handler_type;

  asio::async_completion<CompletionToken,
    void(int, double)> completion(token);

  typename asio::associated_allocator<handler_type>::type a
    = asio::get_associated_allocator(completion.completion_handler);

  typename asio::associated_executor<handler_type>::type ex
    = asio::get_associated_executor(completion.completion_handler);

  ex.post(
      bindns::bind(
        static_cast<handler_type&&>(completion.completion_handler),
        42, 2.0), a);

  return completion.result.get();
}

template <typename CompletionToken>
typename asio::async_result<asio::decay_t<CompletionToken>,
  void(asio::error_code, int, double)>::return_type
async_op_ec_2(bool ok, ASIO_MOVE_ARG(CompletionToken) token)
{
  typedef typename asio::async_result<asio::decay_t<CompletionToken>,
    void(asio::error_code, int, double)>::completion_handler_type
      handler_type;

  asio::async_completion<CompletionToken,
    void(asio::error_code, int, double)> completion(token);

  typename asio::associated_allocator<handler_type>::type a
    = asio::get_associated_allocator(completion.completion_handler);

  typename asio::associated_executor<handler_type>::type ex
    = asio::get_associated_executor(completion.completion_handler);

  if (ok)
  {
    ex.post(
        bindns::bind(
          static_cast<handler_type&&>(completion.completion_handler),
          asio::error_code(), 42, 2.0), a);
  }
  else
  {
    ex.post(
        bindns::bind(
          static_cast<handler_type&&>(completion.completion_handler),
          asio::error_code(asio::error::operation_aborted),
          0, 0.0), a);
  }

  return completion.result.get();
}

template <typename CompletionToken>
typename asio::async_result<asio::decay_t<CompletionToken>,
  void(std::exception_ptr, int, double)>::return_type
async_op_ex_2(bool ok, ASIO_MOVE_ARG(CompletionToken) token)
{
  typedef typename asio::async_result<asio::decay_t<CompletionToken>,
    void(std::exception_ptr, int, double)>::completion_handler_type
      handler_type;

  asio::async_completion<CompletionToken,
    void(std::exception_ptr, int, double)> completion(token);

  typename asio::associated_allocator<handler_type>::type a
    = asio::get_associated_allocator(completion.completion_handler);

  typename asio::associated_executor<handler_type>::type ex
    = asio::get_associated_executor(completion.completion_handler);

  if (ok)
  {
    ex.post(
        bindns::bind(
          static_cast<handler_type&&>(completion.completion_handler),
          std::exception_ptr(), 42, 2.0), a);
  }
  else
  {
    ex.post(
        bindns::bind(
          static_cast<handler_type&&>(completion.completion_handler),
          std::make_exception_ptr(std::runtime_error("blah")), 0, 0.0), a);
  }

  return completion.result.get();
}

template <typename CompletionToken>
typename asio::async_result<asio::decay_t<CompletionToken>,
  void(int, double, char)>::return_type
async_op_3(ASIO_MOVE_ARG(CompletionToken) token)
{
  typedef typename asio::async_result<asio::decay_t<CompletionToken>,
    void(int, double, char)>::completion_handler_type handler_type;

  asio::async_completion<CompletionToken,
    void(int, double, char)> completion(token);

  typename asio::associated_allocator<handler_type>::type a
    = asio::get_associated_allocator(completion.completion_handler);

  typename asio::associated_executor<handler_type>::type ex
    = asio::get_associated_executor(completion.completion_handler);

  ex.post(
      bindns::bind(
        static_cast<handler_type&&>(completion.completion_handler),
        42, 2.0, 'a'), a);

  return completion.result.get();
}

template <typename CompletionToken>
typename asio::async_result<asio::decay_t<CompletionToken>,
  void(asio::error_code, int, double, char)>::return_type
async_op_ec_3(bool ok, ASIO_MOVE_ARG(CompletionToken) token)
{
  typedef typename asio::async_result<asio::decay_t<CompletionToken>,
    void(asio::error_code, int, double, char)>::completion_handler_type
      handler_type;

  asio::async_completion<CompletionToken,
    void(asio::error_code, int, double, char)> completion(token);

  typename asio::associated_allocator<handler_type>::type a
    = asio::get_associated_allocator(completion.completion_handler);

  typename asio::associated_executor<handler_type>::type ex
    = asio::get_associated_executor(completion.completion_handler);

  if (ok)
  {
    ex.post(
        bindns::bind(
          static_cast<handler_type&&>(completion.completion_handler),
          asio::error_code(), 42, 2.0, 'a'), a);
  }
  else
  {
    ex.post(
        bindns::bind(
          static_cast<handler_type&&>(completion.completion_handler),
          asio::error_code(asio::error::operation_aborted),
          0, 0.0, 'z'), a);
  }

  return completion.result.get();
}

template <typename CompletionToken>
typename asio::async_result<asio::decay_t<CompletionToken>,
  void(std::exception_ptr, int, double, char)>::return_type
async_op_ex_3(bool ok, ASIO_MOVE_ARG(CompletionToken) token)
{
  typedef typename asio::async_result<asio::decay_t<CompletionToken>,
    void(std::exception_ptr, int, double, char)>::completion_handler_type
      handler_type;

  asio::async_completion<CompletionToken,
    void(std::exception_ptr, int, double, char)> completion(token);

  typename asio::associated_allocator<handler_type>::type a
    = asio::get_associated_allocator(completion.completion_handler);

  typename asio::associated_executor<handler_type>::type ex
    = asio::get_associated_executor(completion.completion_handler);

  if (ok)
  {
    ex.post(
        bindns::bind(
          static_cast<handler_type&&>(completion.completion_handler),
          std::exception_ptr(), 42, 2.0, 'a'), a);
  }
  else
  {
    ex.post(
        bindns::bind(
          static_cast<handler_type&&>(completion.completion_handler),
          std::make_exception_ptr(std::runtime_error("blah")),
          0, 0.0, 'z'), a);
  }

  return completion.result.get();
}

} // namespace archetypes

#endif // ARCHETYPES_ASYNC_OPS_HPP
