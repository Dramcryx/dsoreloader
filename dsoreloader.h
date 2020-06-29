#ifndef DSORELOADER_H
#define DSORELOADER_H

#include <map>
#include <type_traits>
#include <thread>
#include <mutex>

#include <ctime>

#include "shared_lock_guard.h"

template< class T >
using result_of_t = typename std::result_of<T>::type;

class DSOReloader
{
public:
    DSOReloader() = delete;
    DSOReloader(const std::string & filename);

    ~DSOReloader();

    bool isLoaded();

    void * getFunc(const std::string & fn_name) const;

    std::map<std::string, void *> availableFuncs() const;

    // if void, don't return anything
    template<typename Func, typename ... Args>
    typename std::enable_if<std::is_void<result_of_t<Func(Args...)>>::value, void>::type
    invoke(const char * fn_name, Args &&... args)
    {
        shared_lock_guard<shared_mutex> lk(m_lock);
        ((Func)(m_funcs.at(fn_name)))(args...);
    }

    // if not void, return invoke result
    template<typename Func, typename ... Args>
    typename std::enable_if<!std::is_void<result_of_t<Func(Args...)>>::value,
                            result_of_t<Func(Args...)>>::type
    invoke(const char * fn_name, Args &&... args)
    {
        shared_lock_guard<shared_mutex> lk(m_lock);
        return ((Func)(m_funcs[fn_name]))(args...);
    }

private:
    // filename
    std::string m_name;
    // file handle
    void * m_dl_handle = nullptr;
    // extracted function pointers
    std::map<std::string, void *> m_funcs;
    // mutex needed for safe reload
    shared_mutex m_lock;

    // background thread handle
    std::thread m_bckgrnd;

    // flag to stop background thread working on destruction
    bool shutdown = false;

    // latest modification time
    time_t m_file_time;

    bool load();

    void onFileChanged(const std::string &path);
};

#endif // DSORELOADER_H
