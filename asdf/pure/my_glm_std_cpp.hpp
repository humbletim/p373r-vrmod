#pragma once

// namespace realstd { using namespace std; }

namespace std { template<typename T> struct initializer_list; }

namespace host {
    extern int (*log)(const char*);
    extern int (*vfprintf)(int stream, const char* format, __builtin_va_list&);
    extern void (*exit)(unsigned int);
    inline int fprintf(int stream, const char* format, ...) {
        if (!host::vfprintf) {
            if (host::log) { host::log("!host_vfprintf"); host::log(format); host::log("/!host_vfprintf"); } 
            if (host::exit) host::exit(16);
            return 1; 
        }
        __builtin_va_list args; __builtin_va_start(args, format);
        int result = host::vfprintf(stream, format, args);
        __builtin_va_end(args);
        return result;
    }
    inline int printf(const char* format, ...) {
        if (!host::vfprintf) {
            if (host::log) { host::log("!host_vfprintf"); host::log(format); host::log("/!host_vfprintf"); } 
            if (host::exit) host::exit(16);
            return 1; 
        }
        __builtin_va_list args; __builtin_va_start(args, format);
        int result = host::vfprintf(1, format, args);
        __builtin_va_end(args);
        return result;
    }
}

namespace my_glm_std_emulation {
    // int stdout = 1;
    // int fprintf(int stream, const char* format, ...);
    using size_t = decltype(sizeof(0));
    using int8_t = signed char;
    using uint8_t = unsigned char;
    using int16_t = signed short;
    using uint16_t = unsigned short;
    using int32_t = signed int;
    using uint32_t = unsigned int;
    using int64_t = signed long long;
    using uint64_t = unsigned long long;

    ///////////////////////////
    // // The minimal definition required to satisfy the compiler
    // void* operator new(size_t, void* ptr) noexcept {
    //     return ptr;
    // }

    // // Optionally, define the array version as well
    // void* operator new[](size_t, void* ptr) noexcept {
    //     return ptr;
    // }

    /////////////////////////
    template <typename T> struct numeric_limits {
        static constexpr bool is_specialized = false;
        static constexpr bool is_integer = false;
    };
    template <> struct numeric_limits<float> {
        static constexpr bool is_specialized = true;
        static constexpr bool is_iec559 = true;
        static constexpr bool is_integer = false; // Required: float is not an integer
    };
    template <> struct numeric_limits<double> {
        static constexpr bool is_specialized = true;
        static constexpr bool is_iec559 = true;
        static constexpr bool is_integer = false;
    };
    template <> struct numeric_limits<int> {
        static constexpr bool is_specialized = true;
        static constexpr bool is_iec559 = false;
        static constexpr bool is_integer = true; // Required: int is an integer
    };
    ///////////////////////////
    template <typename T> struct make_unsigned { using type = T; };
    template <> struct make_unsigned<signed char> { using type = unsigned char; };
    template <> struct make_unsigned<short>       { using type = unsigned short; };
    template <> struct make_unsigned<int>         { using type = unsigned int; };
    template <> struct make_unsigned<long>      { using type = unsigned long; };
    template <> struct make_unsigned<long long> { using type = unsigned long long; };
    template <typename T>
    using make_unsigned_t = typename make_unsigned<T>::type;
    ///////////////////////////

    inline float exp(float x) { return __builtin_expf(x); }
    inline float exp2(float x) { return __builtin_exp2f(x); }
    inline float log(float x) { return __builtin_logf(x); }
    inline float log2(float x) { return __builtin_log2f(x); }
    inline float pow(float x, float y) { return __builtin_powf(x, y); }
    inline float sqrt(float x) { return __builtin_sqrtf(x); }
    inline float trunc(float x) { return __builtin_truncf(x); }
    inline float round(float x) { return __builtin_roundf(x); }
    inline float floor(float x) { return __builtin_floorf(x); }
    inline float ceil(float x) { return __builtin_ceilf(x); }
    inline float isnan(float x) { return __builtin_isnan(x); }
    inline float isinf(float x) { return __builtin_isinf(x); }
    inline float sin(float x) { return __builtin_sinf(x); }
    inline float asin(float x) { return __builtin_asinf(x); }
    inline float sinh(float x) { return __builtin_sinhf(x); }
    inline float asinh(float x) { return __builtin_asinhf(x); }
    inline float cos(float x) { return __builtin_cosf(x); }
    inline float acos(float x) { return __builtin_acosf(x); }
    inline float cosh(float x) { return __builtin_coshf(x); }
    inline float acosh(float x) { return __builtin_acoshf(x); }
    inline float tan(float x) { return __builtin_tanf(x); }
    inline float atan(float x) { return __builtin_atanf(x); }
    inline float tanh(float x) { return __builtin_tanhf(x); }
    inline float atanh(float x) { return __builtin_atanhf(x); }
    inline float fma(float x, float y, float z) { return __builtin_fmaf(x,y,z); }
    ///////////////////////////
    namespace chrono {
        template <typename T> struct duration {
            T val;
            T count() const { return val; }
            duration operator-(const duration& other) const { return {val - other.val}; }
        };

        template <typename Clock, typename Duration = duration<double>> struct time_point {
            Duration d;
            Duration time_since_epoch() const { return d; }
        };

        struct system_clock {
            using time_point = chrono::time_point<system_clock>;
            static time_point now() {
                // Returns total CPU cycles since boot
                unsigned long long cycles = __builtin_readcyclecounter();
                
                // Typical x86_64 base freq is ~2.4GHz to 3.5GHz. 
                // Using 2.5e9 as a 'magic' divisor gives a "time-like" double.
                return { duration<double>{(double)cycles / 2.5e9} };
            }
        };
    }
    ///////////////////////////
    template <typename T, typename R = void> struct alignas(16) host_buffer {
        using value_type = T;
        using host_type = R;
        void* _padding{ nullptr };
        value_type* _start{ nullptr };
        value_type* _finish{ nullptr };
        value_type* _end_cap{ nullptr };
        host_type* _guest = nullptr;
        host_type* __host = nullptr;
        inline const char* flags() {
            static char flags[5]{};
            flags[0] = __host ? 'h' : ' '; 
            flags[1] = _guest ? 'g' : ' '; 
            flags[2] = false ? 'e' : ' '; 
            flags[3] = _padding ? 'p' : ' '; 
            return flags;
        }

        void reserve(size_t sz);
        void resize(size_t sz);
        void destroy(const char* hint);
        void clear();

        value_type* data() const { return _start; }
        value_type* begin() const { return _start; }
        value_type* end()   const { return _finish; }
        size_t size() const { return (size_t)(_finish - _start); }
        value_type& operator[](size_t i) { return _start[i]; }
        host_buffer() {}
        ~host_buffer() {
            if (_guest || __host) destroy("~host_buffer");
        }

        host_buffer(const std::initializer_list<value_type>& other) { *this = other; }
        host_buffer& operator=(const std::initializer_list<value_type>& other) {
            auto sz = other.size();
            resize(sz);
            if (!_guest) host::exit(154);
            auto src = other.begin();
            auto dst = data();
            for (size_t i = 0; i < sz; ++i) dst[i] = src[i];
            return *this;
        }
         // Constructor accepting range (begin, end)
        template <typename It> host_buffer(It first, It last) noexcept {
            size_t sz = last - first;
            resize(sz);
            if (!_guest) host::exit(163);
            auto src = first;
            auto dst = data();
            for (size_t i = 0; i < sz; ++i) dst[i] = src[i];
        }

        host_buffer(host_buffer const& other) noexcept {
            *this = other;
        }
        // // 3. Move Assignment (Good hygiene, allows 'str = func()')
        host_buffer& operator=(host_buffer const& other) noexcept {
            auto sz = other.size();
            resize(sz);
            auto src = other.begin();
            auto dst = data();
            if (!dst) host::exit(178);
            for (size_t i = 0; i < sz; ++i) dst[i] = src[i];
            return *this;
        };
        host_buffer& operator=(host_buffer&& other) noexcept {
            if (this != &other) {
                // Free our current memory if we have any
                if (this->_guest || this->__host) destroy("move assign"); // or this->~string() logic
                
                // Steal new memory
                this->_padding = other._padding;
                this->_guest = other._guest;
                this->__host = other.__host;
                this->_start = other._start;
                this->_finish = other._finish;
                this->_end_cap = other._end_cap;
                
                // Nullify source
                other._padding = nullptr;
                other._guest = nullptr;
                other.__host = nullptr;
                other._start = nullptr;
                other._finish = nullptr;
                other._end_cap = nullptr;
            }
            return *this;
        }


    };

    template<> void host_buffer<char>::resize(size_t n);
    template<> void host_buffer<char>::destroy(const char*);
    template<> void host_buffer<float>::resize(size_t n);
    template<> void host_buffer<float>::destroy(const char*);
            
    struct string : host_buffer<char> {
        using native = host_buffer<char>;
        using native::begin;
        using native::resize;
        using native::reserve;
        using native::destroy;
        using native::clear;
        using native::data;
        using native::size;
        using native::native;
        constexpr const char* c_str() const { return data(); }

        // explicit string(decltype(nullptr) b) { static_assert(false, "asdf"); }
        string(const char* b) : string(b, b + __builtin_strlen(b)) {}
        // ;
        //     if (!n) host_log("n");
        //     resize(n);
        //     // if (host_log) host_log("stringctr");
        //     if (!host_buffer::_guest) {
        //         if (host_log)host_log("!_d");
        //         b = "TOOSOON";
        //         _start = (char*)b;
        //         _finish = _end_cap = (char*)b + n;
        //         return;
        //     }
        //      if (!_guest) host_log("!_guest");
        //     if (!_start) host_log("!_start");
        //     if (!data()) host_log("!data()");
        //     if (data()) __builtin_memcpy(data(), b, n + 1);
        // }
        string operator+(const char* other) const {
            string tmp;
            tmp.resize(size() + __builtin_strlen(other));
            __builtin_memcpy(tmp.data(), data(), size());
            __builtin_memcpy(tmp.data() + size(), other, __builtin_strlen(other));
            tmp[tmp.size()] = 0;
            return tmp.c_str();
        }
        string operator+(string const& other) const { return operator+(other.data()).c_str(); }
    };

    template <typename T, typename R = void> struct vector : host_buffer<T, R> {
        using host_type = R;
        using val_type = T;
        using native = host_buffer<T, R>;
        using native::begin;
        using native::resize;
        using native::reserve;
        using native::destroy;
        using native::clear;
        using native::data;
        using native::size;
        using native::native;
        void push_back(T const&);
        // vector(vector const& other) noexcept : native(other) {}
    };



} //ns my_glm_std_emulation

// this must be in namespace "std" or clang/compilers won't recognize as special for compile-level initializers 
// namespace std {
//     template <typename T> using initializer_list = ::my_glm_std_emulation::initializer_list<T>;
//     // using namespace ::my_glm_std_emulation;
// }

#ifdef CR_MAGIC_GUEST_PURE

#define assert(expression) (void)((!!(expression)) || (__builtin_trap(), 0))

namespace std {
    template<typename T>
    struct initializer_list {
        const T* _M_array;
        size_t _M_len;
        constexpr initializer_list(const T* a, size_t l) : _M_array(a), _M_len(l) {}
        constexpr const T* begin() const noexcept { return _M_array; }
        constexpr const T* end()   const noexcept { return _M_array + _M_len; }
        constexpr size_t size()    const noexcept { return _M_len; }
    };
}

#define std my_glm_std_emulation

///////////////////////////

#ifdef HAVE_KERNEL32
    extern "C" void __stdcall ExitProcess(unsigned int);
    #pragma comment(lib, "kernel32.Lib")
#endif

#ifdef MANUAL_ExitProcess
inline void ExitProcess(unsigned int);
#endif

namespace {
    // int stdout = 1;
    // using ::std::fprintf;
    // int fflush(int) { return 0; }
    extern "C" int _fltused = 0; // lld-link: error: undefined symbol: _fltused
    // extern "C" int atexit(void (*function)(void)) { return 0; }
    void _exit(int status) {
#if HAVE_KERNEL32 || MANUAL_ExitProcess
        ::ExitProcess(status);
        return;
#endif
            // x64 Windows Syscall for NtTerminateProcess
            // rcx = ProcessHandle (0xFFFFFFFFFFFFFFFF for current process)
            // rdx = ExitStatus
            __asm__ __volatile__(
                "mov $0x2C, %%eax\n\t"        // Syscall number for NtTerminateProcess (Win10/11)
                "mov $-1, %%rcx\n\t"          // Handle: -1 is the current process
                "mov %0, %%edx\n\t"           // Status: passed from function argument
                "syscall"
                : : "r"(status) : "rax", "rcx", "rdx"
            );
    }

    // extern "C" __declspec(dllimport) void __cdecl _exit(int status);
    extern "C" int __CxxFrameHandler3(...) { host::log("__CxxFrameHandler3"); _exit(3); __builtin_trap(); return 0; }
    extern "C" void __std_terminate() { host::log("__std_terminate"); __builtin_trap(); _exit(4); }     
    // __builtin_memset doesn't work for some reason here...
    extern "C" void memset(void* ptr, int ch, size_t l) { unsigned char* tmp = (unsigned char*)ptr; for (size_t i=0; i < l; i++) tmp[i] = ch; }
    extern "C" void memcpy(void* _dest, const void* _src, size_t l) {
        unsigned char* src = (unsigned char*)_src;
        unsigned char* dst = (unsigned char*)_dest;
        for (size_t i=0; i < l; i++) dst[i] = src[i];
    }
    extern "C" size_t strlen(const char* pch) {
        const char* st = pch;
        while (pch && *pch) pch++;
        return pch - st;
    }

    static constexpr const float NAN = __builtin_nan("");
    static constexpr const float INFINITY = __builtin_inf();

    // --- 1. Standalone Definitions (Replacing windows.h) ---
    #define WINAPI __stdcall
    #define DLL_PROCESS_DETACH 0
    #define DLL_PROCESS_ATTACH 1

    // --- 2. Existing Init Logic ---
    typedef void (*pfn_init)(void);
    #pragma section(".CRT$XCA", read)
    #pragma section(".CRT$XCZ", read)
    __declspec(allocate(".CRT$XCA")) pfn_init __start_initializers = nullptr;
    __declspec(allocate(".CRT$XCZ")) pfn_init __end_initializers = nullptr;

    void ManualInitGlobals() {
        pfn_init* p = &__start_initializers + 1;
        host::fprintf(1, "ManualInitGlobals #%zu\n", &__end_initializers - p);
        while (p < &__end_initializers) {
            if (*p) (*p)();
            p++;
        }
    }

    // --- 3. Destructor Logic (Static Buffer) ---

    // Define a static stack size. 
    // If you have more than 64 global objects, increase this number.
    #define ATEXIT_MAX_STACK 64

    typedef void (__cdecl *pfn_atexit)(void);

    // The stack itself
    static pfn_atexit g_atexit_stack[ATEXIT_MAX_STACK];
    static int g_atexit_count = 0;

    // Standard atexit implementation
    extern "C" int __cdecl atexit(void (__cdecl *func)(void)) {
        if (g_atexit_count < ATEXIT_MAX_STACK) {
            g_atexit_stack[g_atexit_count++] = func;
            return 0; 
        }
        return -1; // Fail: Stack full
    }

    // MSVC C++ compiler maps destructors to _onexit, not atexit
    extern "C" pfn_atexit __cdecl _onexit(pfn_atexit func) {
        if (atexit(func) == 0) {
            return func;
        }
        return nullptr;
    }

    void ManualDeinitGlobals() {
        // Execute LIFO (Last-In, First-Out)
        host::fprintf(1, "//ManualDeinitGlobals #%zu\n", g_atexit_count);
        while (g_atexit_count > 0) {
            --g_atexit_count;
            if (g_atexit_stack[g_atexit_count]) {
                g_atexit_stack[g_atexit_count]();
            }
        }
    }

    // --- 4. Entry Point ---

    extern "C" int WINAPI _DllMainCRTStartup(void* hinstDLL, unsigned int fdwReason, void* lpvReserved) {
        if (fdwReason == DLL_PROCESS_ATTACH) {
            host::fprintf(1, "DLL_PROCESS_ATTACH\n");
            // ManualInitGlobals();
        }
        else if (fdwReason == DLL_PROCESS_DETACH) {
            host::fprintf(1,"DLL_PROCESS_DETACH\n");
            // ManualDeinitGlobals();
        }
        return 1;
    }
} //ns

#endif // CR_MAGIC_GUEST_PURE

#ifdef MANUAL_ExitProcess
// Pure C++: No headers, no libs, x64 Win10/11 congruent
inline void ExitProcess(unsigned int code) {
    unsigned __int64 __readgsqword(unsigned long Offset);
    // 1. Access PEB via GS register
    unsigned char* peb = (unsigned char*)__readgsqword(0x60);
    
    // 2. PEB -> Ldr -> InLoadOrderModuleList
    unsigned char* ldr = *(unsigned char**)(peb + 0x18);
    unsigned char* list_entry = *(unsigned char**)(ldr + 0x10); // First entry (usually the EXE)

    while (list_entry) {
        unsigned char* base_address = *(unsigned char**)(list_entry + 0x30);
        if (!base_address) break;

        // 3. Parse PE Header
        unsigned int lfanew = *(unsigned int*)(base_address + 0x3C);
        unsigned char* nt_headers = base_address + lfanew;
        unsigned int export_rva = *(unsigned int*)(nt_headers + 0x88);
        
        if (export_rva != 0) {
            unsigned char* export_dir = base_address + export_rva;
            unsigned int num_names = *(unsigned int*)(export_dir + 0x18);
            unsigned int name_rva_table = *(unsigned int*)(export_dir + 0x20);
            unsigned int* names = (unsigned int*)(base_address + name_rva_table);

            for (unsigned int i = 0; i < num_names; i++) {
                const char* name = (const char*)(base_address + names[i]);
                
                // 4. Robust manual comparison for "ExitProcess"
                if (name[0] == 'E' && name[4] == 'P' && name[10] == 's' && name[11] == '\0') {
                    unsigned int func_rva_table = *(unsigned int*)(export_dir + 0x1C);
                    unsigned int ord_rva_table = *(unsigned int*)(export_dir + 0x24);
                    unsigned short* ordinals = (unsigned short*)(base_address + ord_rva_table);
                    unsigned int* functions = (unsigned int*)(base_address + func_rva_table);

                    typedef void (__stdcall *ExitProcessPtr)(unsigned int);
                    ExitProcessPtr fnExitProcess = (ExitProcessPtr)(base_address + functions[ordinals[i]]);
                    
                    fnExitProcess(code); // Should never return
                }
            }
        }
        // Move to next module in list
        list_entry = *(unsigned char**)list_entry; 
    }
}
#endif
