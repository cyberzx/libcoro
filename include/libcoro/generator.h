#pragma once

#if defined(__clang__)
#include <experimental/coroutine>
using namespace std::experimental;
#else
#include <coroutine>
using namespace std;
#endif

#include <exception>
#include <iterator>
#include <utility>

template <typename Promise>
struct GeneratorAwaiter
{
  using promise_type = Promise;
  using handle_type = coroutine_handle<Promise>;

  handle_type *h = nullptr;

  // generators not suspend on co_await
  bool await_ready()
  {
    return true;
  }
  void await_suspend(coroutine_handle<>)
  {
  }

  auto await_resume()
  {
    h->resume();
    h->promise().rethrow_exception();
    return h->promise().get_value();
  }
};

template <class ReturnObject>
class GeneratorPromise
{
protected:
  using value_type = typename ReturnObject::value_type;
  using handle_type = typename ReturnObject::handle_type;

  value_type current_value{};
  std::exception_ptr eptr;

public:
  GeneratorPromise() = default;
  GeneratorPromise(const GeneratorPromise &) = delete;
  GeneratorPromise(GeneratorPromise &&) = delete;
  GeneratorPromise &operator=(const GeneratorPromise &) = delete;
  GeneratorPromise &operator=(GeneratorPromise &&) = delete;

  suspend_always initial_suspend() noexcept { return {}; }
  suspend_always final_suspend() noexcept { return {}; }

  auto get_return_object()
  {
    return ReturnObject{handle_type::from_promise(*this)};
  }

  void return_void() {}

  auto yield_value(const value_type &some_value)
  {
    current_value = some_value;
    return suspend_always{};
  }

  void unhandled_exception()
  {
    eptr = std::current_exception();
  }

  GeneratorAwaiter<GeneratorPromise> await_transform(ReturnObject &gen)
  {
    return {&gen.coro};
  }

  void rethrow_exception() const
  {
    if (eptr)
      std::rethrow_exception(eptr);
  }

  value_type get_value() const
  {
    return current_value;
  }
};

template <typename T>
class Generator
{
public:
  using promise_type = GeneratorPromise<Generator<T>>;
  using value_type = T;
  using handle_type = coroutine_handle<promise_type>;

  friend class GeneratorPromise<Generator<T>>;

protected:
  handle_type coro;

public:
  explicit Generator(handle_type h)
    : coro(h)
  {
  }
  Generator(const Generator &) = delete;
  Generator(Generator &&oth) noexcept
    : coro(std::exchange(oth.coro, nullptr))
  {
  }
  Generator &operator=(const Generator &) = delete;
  Generator &operator=(Generator &&other) noexcept
  {
    coro = std::exchange(other.coro, nullptr);
    return *this;
  }

  ~Generator()
  {
    if (coro)
    {
      coro.destroy();
      coro = nullptr;
    }
  }

  bool next()
  {
    coro.resume();
    coro.promise().rethrow_exception();
    return !coro.done();
  }

  T get_value() const
  {
    return coro.promise().get_value();
  }

  handle_type *handle()
  {
    return &coro;
  }
};

template <class ReturnObject>
class FiniteGeneratorPromise : public GeneratorPromise<ReturnObject>
{
  using typename GeneratorPromise<ReturnObject>::handle_type;
  using typename GeneratorPromise<ReturnObject>::value_type;

public:

  suspend_never initial_suspend() noexcept { return {}; }

  auto get_return_object()
  {
    return ReturnObject{handle_type::from_promise(*this)};
  }

  GeneratorAwaiter<FiniteGeneratorPromise> await_transform(ReturnObject &gen)
  {
    return {&gen.coro};
  }

  template <class RO>
  GeneratorAwaiter<typename RO::promise_type> await_transform(RO &gen)
  {
    return {gen.handle()};
  }
};


template <typename T>
class FiniteGenerator
{
public:
  using promise_type = FiniteGeneratorPromise<FiniteGenerator<T>>;
  using value_type = T;
  using handle_type = coroutine_handle<promise_type>;

  friend class FiniteGeneratorPromise<FiniteGenerator<T>>;

protected:
  handle_type coro;

public:
  explicit FiniteGenerator(handle_type h)
    : coro(h)
  {
  }
  FiniteGenerator(const FiniteGenerator &) = delete;
  FiniteGenerator(FiniteGenerator &&oth) noexcept
    : coro(std::exchange(oth.coro, nullptr))
  {
  }
  FiniteGenerator &operator=(const FiniteGenerator &) = delete;
  FiniteGenerator &operator=(FiniteGenerator &&other) noexcept
  {
    coro = std::exchange(other.coro, nullptr);
    return *this;
  }

  ~FiniteGenerator()
  {
    if (coro)
      coro.destroy();
  }

  bool next()
  {
    coro.resume();
    coro.promise().rethrow_exception();
    return !coro.done();
  }

  bool done() const
  {
    return coro.done();
  }

  value_type get_value() const
  {
    return coro.promise().get_value();
  }

  class iterator : public std::input_iterator_tag
  {
  private:
    friend class FiniteGenerator;
    FiniteGenerator *pgen;
    iterator(FiniteGenerator *gen)
      : pgen(gen)
    {
    }

    bool is_null() const
    {
      return !pgen || pgen->done();
    }

  public:
    bool operator!=(const iterator &oth) const
    {
      return is_null() != oth.is_null();
    }

    iterator operator++()
    {
      pgen->next();
      return *this;
    }

    value_type operator*()
    {
      return pgen->get_value();
    }
  };

  iterator begin()
  {
    return iterator(this);
  }
  iterator end()
  {
    return iterator(nullptr);
  }
};
