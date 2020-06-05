#ifndef DSORELOADER_H
#define DSORELOADER_H

#include <map>
#include <memory>
#include <type_traits>
#include <vector>
#include <filesystem>
#include <mutex>


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
    typename std::enable_if<std::is_void<std::invoke_result_t<Func, Args...>>::value, void>::type
    invoke(const char * fn_name, Args &&... args)
    {
        std::lock_guard<std::mutex> lk(*m_lockers[fn_name]);
        Func callee = (Func)(m_funcs.at(fn_name));
        callee(args...);
    }

    // if not void, return invoke result
    template<typename Func, typename ... Args>
    typename std::enable_if<!std::is_void<std::invoke_result_t<Func, Args...>>::value,
                            std::invoke_result_t<Func, Args...>>::type
    invoke(const char * fn_name, Args &&... args)
    {
        std::lock_guard<std::mutex> lk(*m_lockers[fn_name]);
        return ((Func)(m_funcs[fn_name]))(args...);
    }

private:
    std::string m_name;
    void * m_dl_handle = nullptr;
    std::map<std::string, void *> m_funcs;

    std::filesystem::file_time_type m_file_time;

    std::map<std::string, std::unique_ptr<std::mutex>> m_lockers;

    bool load();

    void onFileChanged(const std::string &path);
};

#endif // DSORELOADER_H
