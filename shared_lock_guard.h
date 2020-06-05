#ifndef SHARED_LOCK_GUARD_H
#define SHARED_LOCK_GUARD_H

// GCC condition_variable based version
// formatted for easier readability

#include <mutex>
#include <condition_variable>

using std::condition_variable;
using std::unique_lock;
using std::mutex;
using std::lock_guard;

class shared_mutex
{

    // Based on Howard Hinnant's reference implementation from N2406.

    // The high bit of _M_state is the write-entered flag which is set to
    // indicate a writer has taken the lock or is queuing to take the lock.
    // The remaining bits are the count of reader locks.
    //
    // To take a reader lock, block on gate1 while the write-entered flag is
    // set or the maximum number of reader locks is held, then increment the
    // reader lock count.
    // To release, decrement the count, then if the write-entered flag is set
    // and the count is zero then signal gate2 to wake a queued writer,
    // otherwise if the maximum number of reader locks was held signal gate1
    // to wake a reader.
    //
    // To take a writer lock, block on gate1 while the write-entered flag is
    // set, then set the write-entered flag to start queueing, then block on
    // gate2 while the number of reader locks is non-zero.
    // To release, unset the write-entered flag and signal gate1 to wake all
    // blocked readers and writers.
    //
    // This means that when no reader locks are held readers and writers get
    // equal priority. When one or more reader locks is held a writer gets
    // priority and no more reader locks can be taken while the writer is
    // queued.

    // Only locked when accessing _M_state or waiting on condition variables.
    mutex		m_mutex;
    // Used to block while write-entered is set or reader count at maximum.
    condition_variable	m_writers_cv;
    // Used to block queued writers while reader count is non-zero.
    condition_variable	m_readers_cv;
    // The write-entered flag and reader count.
    unsigned		_M_state;

    static constexpr unsigned _S_write_entered = 1U << (sizeof(unsigned)*__CHAR_BIT__ - 1);
    static constexpr unsigned _S_max_readers = ~_S_write_entered;

    // Test whether the write-entered flag is set. _M_mut must be locked.
    bool _M_write_entered() const { return _M_state & _S_write_entered; }

    // The number of reader locks currently held. _M_mut must be locked.
    unsigned _M_readers() const { return _M_state & _S_max_readers; }

public:
    shared_mutex() : _M_state(0) {}

    ~shared_mutex()
    {
        __glibcxx_assert( _M_state == 0 );
    }

    shared_mutex(const shared_mutex&) = delete;
    shared_mutex& operator=(const shared_mutex&) = delete;

    // Exclusive ownership

    void lock()
    {
        unique_lock<mutex> lk(m_mutex);
        // Wait until we can set the write-entered flag.
        m_writers_cv.wait(lk, [=]{ return !_M_write_entered(); });
        _M_state |= _S_write_entered;
        // Then wait until there are no more readers.
        m_readers_cv.wait(lk, [=]{ return _M_readers() == 0; });
    }

    bool try_lock()
    {
        unique_lock<mutex> lk(m_mutex, std::try_to_lock);
        if (lk.owns_lock() && _M_state == 0)
        {
            _M_state = _S_write_entered;
            return true;
        }
        return false;
    }

    void unlock()
    {
        lock_guard<mutex> lk(m_mutex);
        __glibcxx_assert( _M_write_entered() );
        _M_state = 0;
        // call notify_all() while mutex is held so that another thread can't
        // lock and unlock the mutex then destroy *this before we make the call.
        m_writers_cv.notify_all();
    }

    // Shared ownership

    void lock_shared()
    {
        unique_lock<mutex> lk(m_mutex);
        m_writers_cv.wait(lk, [=]{ return _M_state < _S_max_readers; });
        ++_M_state;
    }

    bool try_lock_shared()
    {
        unique_lock<mutex> lk(m_mutex, std::try_to_lock);
        if (!lk.owns_lock())
            return false;
        if (_M_state < _S_max_readers)
        {
            ++_M_state;
            return true;
        }
        return false;
    }

    void unlock_shared()
    {
        lock_guard<mutex> lk(m_mutex);
        __glibcxx_assert( _M_readers() > 0 );
        auto prev = _M_state--;
        if (_M_write_entered())
        {
            // Wake the queued writer if there are no more readers.
            if (_M_readers() == 0)
                m_readers_cv.notify_one();
            // No need to notify gate1 because we give priority to the queued
            // writer, and that writer will eventually notify gate1 after it
            // clears the write-entered flag.
        }
        else
        {
            // Wake any thread that was blocked on reader overflow.
            if (prev == _S_max_readers)
                m_writers_cv.notify_one();
        }
    }
};

template<typename SharedMutex>
class shared_lock_guard
{
private:
    SharedMutex& m;

public:
    typedef SharedMutex mutex_type;
    explicit shared_lock_guard(SharedMutex& m_):
        m(m_)
    {
        m.lock_shared();
    }
    shared_lock_guard(SharedMutex& m_, std::adopt_lock_t):
        m(m_)
    {}
    ~shared_lock_guard()
    {
        m.unlock_shared();
    }
};

#endif // SHARED_LOCK_GUARD_H
