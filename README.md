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

void f1()
{
    std::lock_guard<std::mutex> lock(iomut);
    std::cout << "f1\n";
}

struct S1
{
    void operator()(const std::string& str) const
    {
        std::lock_guard<std::mutex> lock(iomut);
        std::cout << "S1, str = " << str << '\n';
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
    p.push([]()
    {
        std::lock_guard<std::mutex> lock(iomut);
        std::cout << "lambda\n";
    });
}
```

#### Note:
The STL version is the only one that has been modified from the original. The Boost version is kept only for completeness. I may make similar modifications to that version, however, Boost has its own thread pool library, and it seems a little redundant if you're using Boost anyway. (Though if specifically requested, I should be able to make the required modifications): https://www.boost.org/doc/libs/1_75_0/doc/html/boost_asio/reference/thread_pool.html.

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
- The requirement for the first argument to be an `int` has been dropped. Unfortunately, the pool can only support one or the other (without massive complications), so this will likely break a lot of pre-existing code, however, I believe this to be a worthy break.
- A wait() method was added, this I believe to be a more elegant solution than the ability to stop and restart the threads.

This README is only updated in this branch.

### Final Thoughts:
It definitely needs to be pointed out that this is a fork, not an original library of mine. Most of the code is original or very similar to vit-vit's original version, of course as mentioned there were modifications. The library at the time was very useful to many people, myself included, and I wanted to breathe some new life into it, after the project was seemingly abandoned.
