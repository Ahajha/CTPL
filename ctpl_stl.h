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

#ifndef __ctpl_stl_thread_pool_H__
#define __ctpl_stl_thread_pool_H__

#include <functional>
#include <thread>
#include <atomic>
#include <vector>
#include <memory>
#include <exception>
#include <future>
#include <mutex>
#include <queue>

// thread pool to run user's functors with signature
//      ret func(int id, other_params)
// where id is the index of the thread that runs the functor
// ret is some return type

namespace ctpl
{
	namespace detail
	{
	// A mutex-locked std::queue wrapper, with minor
	// modifications to the functionality of pop().
	template <typename T>
	class atomic_queue
	{
	public:
		bool push(T const & value);
		
		// If there is an item in the queue, pops the first item
		// into 'value' and returns true. Otherwise returns false.
		bool pop(T & value);
		
		bool empty();
	private:
		std::queue<T> q;
		std::mutex mut;
	};
	}
	
	class thread_pool
	{
	public:
		// Creates a thread pool with no threads.
		thread_pool();
		
		// Creates a thread pool with a given number of threads.
		thread_pool(std::size_t n_threads);
		
		// Waits for all the functions in the queue to be finished, then stops.
		~thread_pool();
		
		// Returns the number of running threads in the pool.
		std::size_t size();
		
		// Returns the number of idle threads.
		std::size_t n_idle();
		
		// Returns a reference to the thread with a given ID.
		std::thread& get_thread(std::size_t id);
		
		// Changes the number of threads in the pool. Should be called
		// from one thread, otherwise be careful to not interleave with
		// this or this->stop(). Requires n_threads >= 0.
		void resize(std::size_t n_threads);
		
		// Clears the task queue.
		void clear_queue();
		
		// Pops the next task in the queue, wrapped as a function with
		// signature void(int), which expects the thread ID as an argument.
		std::function<void(int)> pop();
		
		// Wait for all computing threads to finish, stops all threads, and
		// releases all resources. May be called asynchronously to not pause
		// the calling thread while waiting. If finish == true, all the tasks
		// in the queue are run, otherwise the threads will only finish their
		// current tasks, if they have any, and any tasks in the queue are
		// removed. Note that the pool does not support being restarted.
		void stop(bool finish = false);
		
		// Pushes a function and its arguments to the task queue. The function
		// must accept an int as its first argument, the thread ID, all other
		// arguments must be supplied to the call to push. Returns the future
		// result of the function call, which allows the user to get the result
		// when it is ready or manage any caught exceptions.
		template<typename F, typename... Rest>
		auto push(F && f, Rest&&... rest) -> std::future<decltype(f(0, rest...))>;
		template<typename F>
		auto push(F && f) -> std::future<decltype(f(0))>;
		
	private:
		// Copying or moving a thread pool doesn't make
		// much sense, so disable those actions.
		thread_pool(const thread_pool &);// = delete;
		thread_pool(thread_pool &&);// = delete;
		thread_pool & operator=(const thread_pool &);// = delete;
		thread_pool & operator=(thread_pool &&);// = delete;
		
		// Starts a thread at a given index into its main loop.
		void start_thread(int id);
		
		// Helper function for the constructors.
		void init();
		
		// Vector of threads and their stop flags. The stop flags
		// start false, and should be set to true if the thread at
		// the same index should be commanded to stop. These vectors
		// should be the same size at all times, sans during resizing.
		std::vector<std::unique_ptr<std::thread>> threads;
		std::vector<std::shared_ptr<std::atomic<bool>>> stop_flags;
		
		// Queue of tasks to be completed. Note that this queue is managed
		// by a different mutex than the one used by all other thread pool
		// actions.
		detail::atomic_queue<std::function<void(int id)> *> tasks;
		
		// 'Done' is true if this->stop(true) has been called, signals
		// for waiting threads to stop waiting for new jobs.
		// 'Stopped' is true if this->stop(false) has been called, indicates
		// that the thread pool has been stopped.
		// Note that one of these will be true iff this->stop() has been called.
		std::atomic<bool> done, stopped;
		
		// The number of currently idle threads.
		std::atomic<std::size_t> _n_idle;
		
		// Mutex used for most atomic operations in the thread pool, other than
		// those related to the task queue.
		std::mutex mut;
		
		// Used for waking up threads that are waiting for tasks.
		std::condition_variable signal;
	};

#include "tpp/ctpl_stl.tpp"
}

#endif // __ctpl_stl_thread_pool_H__
