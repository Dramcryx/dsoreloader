#include "dsoreloader.h"

#include <dlfcn.h>
#include <link.h>

#include <thread>

DSOReloader::DSOReloader(const std::string &filename):
    m_name(filename),
    m_file_time(std::filesystem::last_write_time(
                    std::filesystem::path(filename)))
{
    this->load();
    std::thread([this]()
    {
        while (true)
        {
            auto new_time = std::filesystem::last_write_time(std::filesystem::path(m_name));
            if (new_time != m_file_time)
            {
                onFileChanged(m_name);
                m_file_time = new_time;
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }).detach();
}

DSOReloader::~DSOReloader()
{
    dlclose(m_dl_handle);
}

bool DSOReloader::isLoaded()
{
    return this->m_dl_handle;
}

void *DSOReloader::getFunc(const std::string &fn_name) const
{
    return dlsym(m_dl_handle, fn_name.c_str());
}

std::map<std::string, void *> DSOReloader::availableFuncs() const
{
    return this->m_funcs;
}

bool DSOReloader::load()
{
    if (!this->m_dl_handle)
    {
        this->m_dl_handle = dlopen(m_name.c_str(), RTLD_LAZY | RTLD_GLOBAL);
        link_map * map = nullptr;
        dlinfo(m_dl_handle, RTLD_DI_LINKMAP, &map);

        Elf64_Sym * symtab = nullptr;
        const char * strtab = nullptr;
        int symentries = 0;
        for (auto section = map->l_ld; section->d_tag != DT_NULL; ++section)
        {
            if (section->d_tag == DT_SYMTAB)
            {
                symtab = (Elf64_Sym *)section->d_un.d_ptr;
            }
            if (section->d_tag == DT_STRTAB)
            {
                strtab = (char*)section->d_un.d_ptr;
            }
            if (section->d_tag == DT_SYMENT)
            {
                symentries = section->d_un.d_val;
            }
        }

        int size = strtab - (char *)symtab;
        for (int k = 0; k < size / symentries; ++k)
        {
            auto sym = &symtab[k];
            if (ELF64_ST_TYPE(sym->st_info) == STT_FUNC)
            {
                if (m_lockers.find(&strtab[sym->st_name]) == m_lockers.end())
                {
                    m_lockers[&strtab[sym->st_name]].reset(new std::mutex{});
                }
                m_funcs[&strtab[sym->st_name]] = getFunc(&strtab[sym->st_name]);
            }
        }
    }
    return this->m_dl_handle;
}

void DSOReloader::onFileChanged(const std::string &path)
{
    if (path.find(m_name) != path.npos)
    {
        std::vector<std::unique_ptr<std::lock_guard<std::mutex>>> lockers;
        for (auto & i: m_lockers)
        {
            lockers.emplace_back(new std::lock_guard<std::mutex>(*i.second));
        }
        dlclose(m_dl_handle);
        m_dl_handle = nullptr;
        m_name = path;
        load();
    }
}