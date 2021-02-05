CTPL
====

Modern and efficient C++ Thread Pool Library

A thread pool is a programming pattern for parallel execution of jobs, http://en.wikipedia.org/wiki/Thread_pool_pattern.

More specifically, there are some threads dedicated to the pool and a container of jobs. The jobs come to the pool dynamically. A job is fetched and deleted from the container when there is an idle thread. The job is then run on that thread.

A thread pool is helpful when you want to minimize time of loading and destroying threads and when you want to limit the number of parallel jobs that run simultaneously. For example, time consuming event handlers may be processed in a thread pool to make UI more responsive.

Features:
- C++20, tested to compile with no warnings (-Wall -Wextra -Wpedantic) on g++ 10.2.0.
- Header only
- Can push any callable object: functions, functors, and lambdas
- Uses std::futures to get returned value or thrown exceptions
- Use for any purpose under Apache license

### Sample usage (more examples in example.cpp)

```C++
#include <iostream>
#include <ctpl_stl>

std::mutex iomut;

void f1(int id)
{
    std::lock_guard<std::mutex> lock(iomut);
    std::cout << "f1, thread #" << id << '\n';
}

struct S1
{
    void operator()(int id, const std::string& str) const
    {
        std::lock_guard<std::mutex> lock(iomut);
        std::cout << "S1, thread #" << id << ", str = " << str << '\n';
    }
} s1;

int main ()
{
    // Automatically determines number of threads based on hardware, can be
    // manually specified if desired.
    ctpl::thread_pool p;
    
    // Can push functions
    p.push(f1);
    
    // Functors
    p.push(s1, "Hello World!");
    
    // And lambdas
    p.push([](int id)
    {
        std::lock_guard<std::mutex> lock(iomut);
        std::cout << "lambda, thread #" << id << '\n';
    });
}
```

### Different branches
The branches were made sequentially, with each modifying specific pieces of the library.

#### reorganization
Separates the implementation from the header for a cleaner to read interface.

#### documentation
Documents how the entire library works. This and reorganization do not modify any original code, other than some variable names.

#### reimplementation
Modifies the implementation of the library, very minor changes to the interface (adding const, changing some `int`s to `std::size_t` in parameters and return types). One minor bug was fixed, the issue is described here: https://github.com/vit-vit/CTPL/issues/32. This is the last branch to remain (mostly) compatible with the original version.

#### finalization (default)
Breaking changes will be made here. Thus far:
- The pop() and get_thread() methods have been removed, there does not seem to be a good reason to use them in my opinion (especially get_thread()).
- The default constructor creates a number of threads equal to the number of hardware threads, this is much more useful than a 0-thread pool.

This README is only updated in this branch.
