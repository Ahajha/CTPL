/*********************************************************
*
*  Copyright (C) 2014 by Vitaliy Vitsentiy
*  Modifications: Copyright (C) 2021 by Alex Trotta
*
*  Licensed under the Apache License, Version 2.0 (the "License");
*  you may not use this file except in compliance with the License.
*  You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
*  Unless required by applicable law or agreed to in writing, software
*  distributed under the License is distributed on an "AS IS" BASIS,
*  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*  See the License for the specific language governing permissions and
*  limitations under the License.
*
*********************************************************/

template <typename T>
bool detail::atomic_queue<T>::push(T const & value)
{
	std::unique_lock<std::mutex> lock(this->mut);
	this->q.push(value);
	return true;
}

template <typename T>
bool detail::atomic_queue<T>::pop(T & value)
{
	std::unique_lock<std::mutex> lock(this->mut);
	if (this->q.empty())
		return false;
	value = this->q.front();
	this->q.pop();
	return true;
}

template <typename T>
bool detail::atomic_queue<T>::empty() const
{
	std::unique_lock<std::mutex> lock(this->mut);
	return this->q.empty();
}

thread_pool::thread_pool()
{
	this->init();
}

thread_pool::thread_pool(std::size_t n_threads)
{
	this->init();
	
	// Starts with 0 threads, resize.
	this->resize(n_threads);
}

thread_pool::~thread_pool()
{
	this->stop(true);
}

std::size_t thread_pool::size() const
{
	return this->threads.size();
}

std::size_t thread_pool::n_idle() const
{
	return this->_n_idle;
}

std::thread& thread_pool::get_thread(std::size_t id)
{
	return *this->threads[id];
}

void thread_pool::resize(std::size_t n_threads)
{
	// One of these will be true if this->stop() has been called,
	// resizing in that case would be pointless at best or incorrect at worst.
	if (!this->stopped && !this->done)
	{
		const std::size_t old_n_threads = this->threads.size();
		
		// If the number of threads stays the same or increases
		if (old_n_threads <= n_threads)
		{
			// Add threads and flags
			this->threads.resize(n_threads);
			this->stop_flags.resize(n_threads);
			
			// Start up all threads into their main loops.
			for (std::size_t i = old_n_threads; i < n_threads; ++i)
			{
				this->stop_flags[i] = std::make_shared<std::atomic<bool>>(false);
				this->start_thread(i);
			}
		}
		else // the number of threads is decreased
		{
			// For each thread to be removed
			for (std::size_t i = n_threads; i < old_n_threads; ++i)
			{
				// Tell the thread to finish its current task
				// (if it has one) and stop. Detach the thread, since
				// there is no need to wait for it to finish (join).
				*this->stop_flags[i] = true;
				this->threads[i]->detach();
			}
			{
				// Stop any detached threads that were waiting.
				// All other detached threads will eventually stop.
				std::unique_lock<std::mutex> lock(this->mut);
				this->signal.notify_all();
			}
			
			// Safe to delete because the threads are detached.
			this->threads.resize(n_threads);
			
			// Safe to delete because the threads have copies of
			// the shared pointers to their respective flags, not originals.
			this->stop_flags.resize(n_threads);
		}
	}
}

void thread_pool::clear_queue()
{
	std::function<void(int id)> * _f;
	while (this->tasks.pop(_f))
		// The functions were allocated with new, so they should be deleted.
		delete _f;
}

std::function<void(int)> thread_pool::pop()
{
	std::function<void(int id)> * _f = nullptr;
	this->tasks.pop(_f);
	
	// At return, delete the function even if an exception occurred
	// This takes over the raw pointer and lets RAII take over cleanup.
	std::unique_ptr<std::function<void(int id)>> func(_f);
	std::function<void(int)> f;
	if (_f)
		// The heap-allocated function will be deleted, so copy it
		// to the stack to be returned.
		f = *_f;
	return f;
}

void thread_pool::stop(bool finish)
{
	// Force the threads to stop
	if (!finish)
	{
		// If this->stop(false) has already been called, no need to stop again.
		// If this->stop(true) has alredy been called, still continue, as this
		// will stop the completion of the rest of the tasks in the queue.
		if (this->stopped)
			return;
		this->stopped = true;
		
		// Command all threads to stop
		for (int i = 0, n = this->size(); i < n; ++i)
		{
			*this->stop_flags[i] = true;
		}
		
		// Remove any remaining tasks.
		this->clear_queue();
	}
	else // Let the threads continue
	{
		// If this->stop() has been already been called, no need to stop again.
		if (this->done || this->stopped)
			return;
		
		// Give the waiting threads a command to finish
		this->done = true;
	}
	{
		// Stop all waiting threads
		std::unique_lock<std::mutex> lock(this->mut);
		this->signal.notify_all();
	}
	
	// Wait for the computing threads to finish
	for (int i = 0; i < static_cast<int>(this->threads.size()); ++i)
	{
		if (this->threads[i]->joinable())
			this->threads[i]->join();
	}
	
	// Release all resources.
	this->clear_queue();
	this->threads.clear();
	this->stop_flags.clear();
}

template<typename F, typename... Rest>
auto thread_pool::push(F && f, Rest&&... rest) -> std::future<decltype(f(0, rest...))>
{
	// std::forward is used to ensure perfect forwarding of rvalues where necessary.
	// std::bind is used to make a function with 1 argument (int id) that
	//     simulates calling f with its respective arguments.
	// std::packaged_task is used to get a future out of the function call.
	auto pck = std::make_shared<std::packaged_task<decltype(f(0, rest...))(int)>>(
		std::bind(std::forward<F>(f), std::placeholders::_1, std::forward<Rest>(rest)...)
	);
	
	// Lastly, create a function that wraps the packaged task into a signature of void(int).
	auto task = new std::function<void(int id)>([pck](int id) {
		(*pck)(id);
	});
	
	// Add the task to the queue
	this->tasks.push(task);
	
	// Notify one waiting thread so it can wake up and take this new task.
	std::unique_lock<std::mutex> lock(this->mut);
	this->signal.notify_one();
	
	// Return the future, the user is now responsible for the return value of the task.
	return pck->get_future();
}

template<typename F>
auto thread_pool::push(F && f) -> std::future<decltype(f(0))>
{
	// The main difference here is since there are no parameters to bind, many of
	// the steps can be skipped.
	auto pck = std::make_shared<std::packaged_task<decltype(f(0))(int)>>(std::forward<F>(f));
	
	// Everything below is the same as the other version of push().
	auto task = new std::function<void(int id)>([pck](int id)
	{
		(*pck)(id);
	});
	this->tasks.push(task);
	std::unique_lock<std::mutex> lock(this->mut);
	this->signal.notify_one();
	return pck->get_future();
}

void thread_pool::start_thread(int id)
{
	// A copy of the shared ptr to the flag, note this is captured
	// by value by the lambda.
	std::shared_ptr<std::atomic<bool>> stop_flag(this->stop_flags[id]);
	
	// The main loop for the thread.
	auto loop = [this, id, stop_flag]()
	{
		std::atomic<bool> & stop = *stop_flag;
		
		// Used to store new tasks.
		std::function<void(int id)> * task;
		
		// True if 'task' currently has a runnable task in it.
		bool has_new_task = this->tasks.pop(task);
		while (true)
		{
			// If there is a task to run
			while (has_new_task)
			{
				// At return, delete the function even if an exception occurred.
				// This takes over the raw pointer and lets RAII take care of
				// cleanup.
				std::unique_ptr<std::function<void(int id)>> func(task);
				
				// Run the task
				(*task)(id);
				
				// The thread is wanted to stop, return even
				// if the queue is not empty yet
				if (stop)
					return;
				else
					// Get a new task
					has_new_task = this->tasks.pop(task);
			}
			// At this point the queue has run out of tasks, wait here for more.
			std::unique_lock<std::mutex> lock(this->mut);
			
			// Thread is now idle.
			++this->_n_idle;
			
			// While the following evaluates to true, wait for a signal.
			this->signal.wait(lock, [this, &task, &has_new_task, &stop]()
			{
				// Try to get a new task. This will fail if the thread was
				// woken up for another reason (stopping or resizing), or
				// if another thread happened to grab the task before this
				// one got to it.
				has_new_task = this->tasks.pop(task);
				
				// If there is a new task or the thread is being told to stop,
				// stop waiting.
				return has_new_task || this->done || stop;
			});
			
			// Thread is no longer idle.
			--this->_n_idle;
			
			// if the queue is empty and it was able to stop waiting, then
			// that means the thread was told to stop, so stop.
			if (!has_new_task)
				return;
		}
	};
	// Compilers (at the time the code was written)
	// may not support std::make_unique().
	this->threads[id].reset(new std::thread(loop));
}

void thread_pool::init()
{
	this->_n_idle = 0; this->stopped = false; this->done = false;
}
