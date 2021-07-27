#include <stdio.h>
#include <libcoro/generator.h>

template <typename T>
FiniteGenerator<T> takeN(Generator<T> gen, unsigned count)
{
  for (int i = 0; i < count; ++i)
    co_yield co_await gen;
}

template<typename T, class Func>
Generator<T> mapfn(Generator<T> gen, Func func)
{
  while (true)
    co_yield func(co_await gen);
}

Generator<unsigned> fib()
{
  unsigned f0 = 0;
  unsigned f1 = 1;
  co_yield f0;
  co_yield f1;
  while (true)
  {
    unsigned f = f0 + f1;
    f0 = f1;
    f1 = f;
    co_yield f;
  }
}

Generator<unsigned> factorial()
{
  unsigned n = 1;
  unsigned f = 1;
  while (true)
  {
    co_yield f;
    f *= n++;
  }
}

int main()
{
  try
  {
    for (unsigned val : takeN(mapfn(factorial(), [] (auto x) { return x;}), 8))
      printf("%d\n", val);
  }
  catch (std::exception const &)
  {
    printf("catch exception\n");
  }
  return 0;
}
