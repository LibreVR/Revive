#pragma once

#include <cassert>
#include <shared_mutex>
#include <utility>

/*
	Pointer with implicit locking for implementing a simple RCU pattern.
	The following rules must be followed to use it successfully:
		1. Make a local copy to acquire the pointer from a different thread.
		2. Do not copy the same pointer twice from another thread, you will
		   cause a deadlock.
		3. Only swap from a single writer thread on the original pointer.
*/
template<typename T>
class rcu_ptr
{
public:
	// Null-pointer
	rcu_ptr() : m_ptr(nullptr), m_mutex(nullptr), m_reader() { }

	// Copying implies you acquire a read-lock on the underlying pointer
	rcu_ptr(const rcu_ptr& r) : m_mutex(r.m_mutex), m_ptr(r.m_ptr), m_reader(std::this_thread::get_id())
	{
		if (m_reader != r.m_reader)
			m_mutex->lock_shared();
	}

	// Creating a new pointer doesn't acquire a read-lock, because this thread is the writer
	rcu_ptr(std::shared_ptr<T> ptr) : m_ptr(ptr), m_mutex(std::make_shared<std::shared_mutex>()), m_reader() { }

	// Destroying implies releasing the read-lock on the underlying pointer
	~rcu_ptr()
	{
		// If this thread is a reader, release the lock
		if (m_reader == std::this_thread::get_id())
			m_mutex->unlock_shared();
	}

	const T* operator->() const { assert(m_reader == std::this_thread::get_id()); return m_ptr.get(); }
	const T& operator*() const { assert(m_reader == std::this_thread::get_id()); return *m_ptr.get(); }

	// Swaps out the pointer when all other readers are done with it
	// This function returns the old pointer which is now safe to
	// delete or write to.
	void swap(std::shared_ptr<T>& ptr)
	{
		// Ensure we're not a reader
		assert(m_reader == std::thread::id());

		m_mutex->lock();
		m_ptr.swap(ptr);
		m_mutex->unlock();
	}

private:
	std::shared_ptr<T> m_ptr;
	std::shared_ptr<std::shared_mutex> m_mutex;
	std::thread::id m_reader;
};
