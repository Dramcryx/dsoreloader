# dsoreloader
(C++17) Dynamic linking of Linux shared objects with reloading on it's change

Imagine you have a shared object that you would wish to hotswap without rebuilding or relaunching your app (i.e. you changed 2-3 lines of method).

How to use?
You need to create instance of `DSOReloader` and pass .so path into it.

You can call any function of your library using `DSOReloader::invoke<function_pointer_type>(function_name, args...)`.

There is a background thread that calls a reload procedure once file has changed (`std::filesystem` is being used).

**The file will not be reloaded until there is a function that is being called at that moment!**

