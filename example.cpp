#include "ctpl_stl.h"
#include <iostream>
#include <string>
#include <mutex>

std::mutex iomut;

void f1()
{
	std::lock_guard<std::mutex> lock(iomut);
	std::cout << "f1\n";
}

void f2(int par)
{
	std::lock_guard<std::mutex> lock(iomut);
	std::cout << "f2, parameter " << par << '\n';
}

void f3(const std::string & s)
{
	std::lock_guard<std::mutex> lock(iomut);
	std::cout << "f3, '" << s << "'\n";
}

template<class T>
struct S1
{
	T val;
	
	S1(T v) : val(v)
	{
		std::lock_guard<std::mutex> lock(iomut);
		std::cout << "S1 ctor: " << this->val << '\n';
	}
	
	S1(S1 && c) : val(c.val)
	{
		std::lock_guard<std::mutex> lock(iomut);
		std::cout << "S1 move ctor\n";
	}
	
	S1(const S1& c) : val(c.val)
	{
		std::lock_guard<std::mutex> lock(iomut);
		std::cout << "S1 copy ctor\n";
	}
	
	S1& operator=(S1&& c)
	{
		val = c.val;
		std::lock_guard<std::mutex> lock(iomut);
		std::cout << "S1 move assign\n";
		return *this;
	}
	
	S1& operator=(const S1& c)
	{
		val = c.val;
		std::lock_guard<std::mutex> lock(iomut);
		std::cout << "S1 copy assign\n";
		return *this;
	}
	
	~S1()
	{
		std::lock_guard<std::mutex> lock(iomut);
		std::cout << "S1 dtor\n";
	}
	
	void operator()() const
	{
		std::lock_guard<std::mutex> lock(iomut);
		std::cout << "S1 functor, val = " << val << '\n';
	}
};

void f4(S1<int>& s)
{
	std::this_thread::sleep_for(std::chrono::milliseconds(2000));
	std::lock_guard<std::mutex> lock(iomut);
	std::cout << "f4, parameter S1 = " << s.val << '\n';
}

void f5(S1<int> s)
{
	std::lock_guard<std::mutex> lock(iomut);
	std::cout << "f5, parameter S1 = " << s.val << '\n';
}

int main()
{
	// Create pool using all hardware threads
	ctpl::thread_pool p;
	
	{
		std::lock_guard<std::mutex> lock(iomut);
		std::cout << "Pushing functions\n";
	}
	{
		std::future<void> fut = p.push(std::ref(f1));
		p.push(f1);
		p.push(f2, 7);
	}
	
	{
		std::lock_guard<std::mutex> lock(iomut);
		std::cout << "Pushing functors\n";
	}
	{
		S1<int> func(100);
		
		p.push(std::ref(func));  // functor, reference
		
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		
		p.push(const_cast<const S1<int> &>(func));  // functor, copy ctor
		p.push(std::move(func));  // functor, move ctor
		p.push(func);  // functor, move ctor
		p.push(S1<std::string>("string version"));  // functor, move ctor
	}
	
	{
		std::lock_guard<std::mutex> lock(iomut);
		std::cout << "Pushing lambdas\n";
	}
	{
		std::string s = "lambda";
		
		for (unsigned i = 0; i < 16; ++i)
		{
			{
				std::lock_guard<std::mutex> lock(iomut);
				std::cout << "pushing lambda #" << i << '\n';
			}
			p.push([s]
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(1000));
				std::lock_guard<std::mutex> lock(iomut);
				std::cout << s << '\n';
			});
		}
	}
	
	{
		std::lock_guard<std::mutex> lock(iomut);
		std::cout << "Waiting for all lambda tasks to finish...\n";
	}
	
	p.wait();
	
	// Note that this doesn't need to be guarded,
	// since no threads have any tasks now.
	std::cout << "All threads are now idle.\n";
	
	{
		std::lock_guard<std::mutex> lock(iomut);
		std::cout << "Resizing the pool\n";
	}
	p.resize(4);
	
	{
		std::lock_guard<std::mutex> lock(iomut);
		std::cout << "Testing future returns\n";
	}
	{
		auto f1 = p.push([]{ return 5; });
		
		auto result = f1.get();
		
		std::lock_guard<std::mutex> lock(iomut);
		std::cout << "returned " << result << '\n';
	}
	
	{
		std::lock_guard<std::mutex> lock(iomut);
		std::cout << "Testing future exception catching\n";
	}
	{
		auto f2 = p.push([]{ throw std::exception(); });
			
		try
		{
			f2.get();
		}
		catch (std::exception& e)
		{
			std::lock_guard<std::mutex> lock(iomut);
			std::cout << "caught exception\n";
		}
	}
	
	{
		std::lock_guard<std::mutex> lock(iomut);
		std::cout << "Testing perfect forwarding\n";
	}
	{
		S1<int> s{0};
		p.push(f4, s);
		
		p.push(f5, S1<int>{0});
	}
}
