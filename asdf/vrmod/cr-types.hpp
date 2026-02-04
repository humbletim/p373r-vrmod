#ifndef humbletim_crtypes_hpp
#define humbletim_crtypes_hpp

#include "pure/my_glm_std_cpp.hpp"

#include <cstdio>
#include <vector>
#include <string>
#include <map>
#include <chrono>

// extern void ExitProcess(unsigned int code);
namespace crtypes {
    // host helpers
    struct Hosted {
        double (*now)() = nullptr;
        void (*vector_float_resize)(void*& guest, void*& host, float** abc, size_t n) = nullptr;
        void (*vector_float_destroy)(void*& guest, void*& host, float** abc) = nullptr;
        void (*vector_float_push_back)(void*& guest, void*& host, float** abc, float const& n) = nullptr;
        void (*string_resize)(void*& guest, void*& host, char** abc, size_t n) = nullptr;
        void (*string_destroy)(void*& guest, void*& host, char** abc) = nullptr;
        void* (*realloc)(void*& ptr, size_t n) = nullptr;
        void (*free)(void* ptr) = nullptr;
        int (*log)(const char* c_str) = nullptr;
        int (*vfprintf)(int stream, const char* format, __builtin_va_list&) = nullptr;
        void (*exit)(int) = nullptr;
        inline int fprintf(int stream, const char* format, ...) {
            if (!this->vfprintf) return 1; 
            __builtin_va_list args; __builtin_va_start(args, format);
            int result = this->vfprintf(stream, format, args);
            __builtin_va_end(args);
            return result;
        }
        inline int printf(const char* format, ...) {
            if (!this->vfprintf) return 1; 
            __builtin_va_list args; __builtin_va_start(args, format);
            int result = this->vfprintf(1, format, args);
            __builtin_va_end(args);
            return result;
        }
        inline void __sync__(const char* hint) {
            if (log){
                host::log = log;
                host::vfprintf = vfprintf;
                printf("__sync__(%s @ %f)\n", hint, now ? now() : 0.0);
            }
#ifdef CR_MAGIC_HOST
            else ::fprintf(stdout, "--sync-- !log\n");
#endif
        }
    };
    extern Hosted hosted;
    struct WithHostAPIs {
#if defined(CR_MAGIC_GUEST)
        const char* _mode = "CR_MAGIC_GUEST"; 
#elif defined(CR_MAGIC_HOST)
        const char* _mode = "CR_MAGIC_HOST"; 
#else
        const char* _mode = "unknown"; 
#endif
    Hosted* hosted = &crtypes::hosted;
    };
}//ns
namespace host {
    inline int _log(const char* c_str) {
        #ifdef CR_MAGIC_GUEST
            // if (crtypes::hosted.log) crtypes::hosted.log("[~Guest.API.host_log..] %s\n");
            // else 
            _exit(137);
            return -10000;
        #else
            ::fprintf(stdout, "[API.host_log..] %s\n", c_str); ::fflush(stdout);
            return 10000;
        #endif
    }
    inline int _vfprintf(int stream, const char* format, __builtin_va_list& arg) {
        #ifdef CR_MAGIC_GUEST
            static char* queued[16]{};
            if (!crtypes::hosted.vfprintf) {
                for (int i=0; i < 16; i++) if (!queued[i]) { queued[i] = (char*)format; break; }
            }
            if (crtypes::hosted.vfprintf) {
                if (queued[0]) {
                    for (int i=0; i < 16; i++) {
                        if (queued[i]) crtypes::hosted.fprintf(1,"[queued][%d] '%s'\n", i, queued[i]);
                        queued[i] = 0;
                    }
                }
                return crtypes::hosted.vfprintf(stream, format, arg);
            }
            // else if (crtypes::hosted.log) crtypes::hosted.log("!hosted.vfprintf...");
            // else if (host::printf) host::printf("asdfadf\n");
            // else
            return -1;
             _exit(147);
        #else
            return  ::vfprintf(stdout, format, arg); ::fflush(stdout);
        #endif
        return -1;
    }
    inline void _exit(unsigned int rc){
#if HAVE_KERNEL32 || MANUAL_ExitProcess
        ExitProcess(rc);
#else
        ::_exit(rc);
#endif
    }
    template <int ERR, typename ...Args> inline int _abort(Args...) { _exit(ERR); return -1; }
    template <int ERR, typename ...P> inline int __abort(P..., ...) { _exit(ERR); return -1; }

#if defined(CR_MAGIC_GUEST)
    int (*log)(const char* c_str) = _log;// = [](const char* c_str) { return 0; crtypes::hosted.log(c_str); }; 
    int (*vfprintf)(int stream, const char* format, __builtin_va_list&) = _vfprintf;// = _abort<86>;
    void (*exit)(unsigned int);// = _exit;
    // int (*fprintf)(int stream, const char *format, ...) = _fprintf;// = __abort<88>;
    // int (*printf)(const char *format, ...) = _printf;
#elif defined(CR_MAGIC_HOST_IMPLEMENTATION)
    int (*log)(const char* c_str) = _log; 
    int (*vfprintf)(int stream, const char* format, __builtin_va_list&) = _vfprintf;
    void (*exit)(unsigned int) = _exit;
    // int (*fprintf)(int stream, const char *format, ...) = _fprintf;
    // int (*printf)(const char *format, ...) = _printf;
#endif
}

#if defined(CR_MAGIC_GUEST_PURE)
    namespace {
        void ManualDeinitGlobals();
        void ManualInirtGlobals();
    }
    template<> void std::vector<float>::push_back(float const& value);
    namespace abi_std {
        using namespace ::my_glm_std_emulation;
        namespace chrono { using namespace ::std::chrono; }
    }
#else
    inline void ManualDeinitGlobals() {}
    inline void ManualInitGlobals() {}

    #if defined(CR_MAGIC_HOST) || defined(CR_MAGIC_GUEST)

        #include <set>
        namespace my_glm_std_host_emulation {
            struct string : my_glm_std_emulation::host_buffer<char> {
                using blah = my_glm_std_emulation::host_buffer<char>;
                ::std::string* _native_() { return static_cast<::std::string*>(__host); }
                ::std::string const* _native_() const { return static_cast<::std::string*>(__host);  }
                constexpr const char* c_str() const { return data(); }
                ~string() { destroy("~string"); }
                explicit string() {}
                string(const char* other) {
                    resize(__builtin_strlen(other));
                    auto dst = data();
                    __builtin_memcpy(dst, other, __builtin_strlen(other)+1);
                }
                string(string const& other) noexcept : string(other.c_str()) {}
                string operator+(const char* other) const {
                    if (!_native_()) { host::fprintf(1, "!_native_!! %p %s\n", this, other); fflush(stdout); return other; }
                    return (*_native_() + other).c_str();
                }
                string operator+(string const& other) const {
                    if (!_native_()) { fprintf(stdout, "!_native_!! %p\n", this); fflush(stdout); return "FAIL++"; }
                    return (*_native_() + *other._native_()).c_str();
                }
                operator ::std::string() const { return { begin(), end() }; }
            };

            template <typename T> 
            struct vector : my_glm_std_emulation::host_buffer<T> {
                using blah = my_glm_std_emulation::host_buffer<T>;
                using blah::blah;
                using blah::begin;
                using blah::end;
                void push_back(T const&);
                ::std::vector<float>* _native_() { return static_cast<::std::vector<float>*>(blah::__host); }
                ::std::vector<float> const* _native_() const { return static_cast<::std::vector<float>*>(blah::__host);  }
                // ~vector() { destroy("~vector"); }
                // explicit vector() {}
                operator ::std::vector<float>() const { return { begin(), end() }; }
            };
            template<> void vector<float>::push_back(float const& value);
        }
        namespace abi_std {
            using string = ::my_glm_std_host_emulation::string;
            template <typename T> using vector = ::my_glm_std_host_emulation::vector<T>;
        }
    #endif // HOST || GUEST
#endif // !PURE

namespace crtypes {

    template <typename T>
    void Inspect(void* vtable_str_func, const char* label, const T& obj) {
        const unsigned char* bytes = (const unsigned char*)&obj;
        host::fprintf(1, "[@%p] [%s] sizeof: %zu | Addr: %p\n", vtable_str_func, label, sizeof(T), &obj);
        host::fprintf(1,"[@%p] [%s] HEX: ", vtable_str_func, label);
        for (size_t i = 0; i < sizeof(T); i++) {
            host::fprintf(1,"%02X ", bytes[i]);
        }
        host::fprintf(1,"\n");
        
        // SNEAKY PEEK: Let's assume there is a pointer at offset 0 (common for strings)
        // and print what that pointer points to, if valid.
        void* likely_ptr;
        memcpy(&likely_ptr, bytes, sizeof(void*));
        host::fprintf(1,"[@%p] [%s] Offset[0] as Ptr: %p\n", vtable_str_func, label, likely_ptr);

    }

    inline void* get_api_vtable_str() { return 0; }
}

namespace crtypes {
#ifdef CR_MAGIC_HOST
    template <typename T> ::std::set<T*>& track(T* ptr, bool active = false);
#endif
#ifdef CR_MAGIC_HOST_IMPLEMENTATION
    template <typename T> ::std::set<T*>& track(T* ptr, bool active) {
        static ::std::set<T*> emu_managed{};
        static const char* HINT{ []{
            if constexpr (std::is_same_v<T, ::std::string>) return "std::string";
            else if constexpr (std::is_same_v<T, ::std::vector<float>>) return "std::vector<float>";
            else return typeid(T).name();
        }() };
        if (ptr && active && emu_managed.find(ptr) == emu_managed.end()) { fprintf(stdout, "track(%s, %p, %d)\n", HINT, ptr, active);fflush(stdout); }
        if (ptr && !active && emu_managed.find(ptr) != emu_managed.end()) { fprintf(stdout, "~track(%s, %p, %d)\n", HINT, ptr, active);fflush(stdout); }
        if (ptr && active) emu_managed.insert(ptr);
        if (ptr && !active) emu_managed.erase(ptr);
        return emu_managed;
    }
    static void* _host_realloc(void*& native, size_t n) {
        fprintf(stdout, "_host_realloc(%p, %zu)\n", native, n);fflush(stdout);
        if (!native) native = malloc(n);
        else native = realloc(native, n);
        fprintf(stdout, "//_host_realloc(%p, %zu)\n", native, n);fflush(stdout);
        return native;
    }
    static void _host_free(void* native) {
        fprintf(stdout, "_host_free(%p)\n", native);fflush(stdout);
        if (native) free(native);
    }

    static void _host_vector_float_push_back(void*& guest, void*& host, float** abc, float const& n) {
        ::std::vector<float>* v = static_cast<::std::vector<float>*>(guest);
        if (!v) v = new ::std::vector<float>;
        v->push_back(n);
        abc[0] = v->data();
        abc[1] = v->data() + v->size();
        abc[2] = v->data() + v->capacity();
        guest = v;
        track(v, true);
    }
    static void _host_vector_float_destroy(void*& guest, void*& host, float** abc) {
        auto* v = static_cast<::std::vector<float>*>(guest);
        fprintf(stdout, "GUEST=>HOST _host_vector_float_destroy(native=%p, v=%p, v->_d=%p, flags=[%s]) [%p, %p, %p, %p]\n", guest, v, host, "", abc[0], abc[1], abc[2], abc[3] );fflush(stdout);

        if (!guest && host) {
            fprintf(stdout, "_host_vector_float_destroy.... should i delete guested %p!?!?!?\n", host);fflush(stdout);
            ::std::vector<float>* g= (::std::vector<float>*)host;
            track(g, false);
            delete g; 
            host = nullptr;
        }
        ;
        if (v) { track(v, false); delete v; }
        abc[0] = abc[1] = abc[2] = nullptr;
        guest = nullptr;
    }

    static void _host_vector_float_resize(void*& guest, void*& host, float** abc, size_t n) {
        ::std::vector<float>* v = static_cast<::std::vector<float>*>(guest);
        // fprintf(stdout, "_host_string_resize(native=%p,v=%p, n=%zu, abc=%p, abc[0]=%p)\n", native, v, n, abc, abc[0]);fflush(stdout);
        if (!v) v = new ::std::vector<float>;
        v->resize(n); 
        track(v, true);
        abc[0] = v->data();
        abc[1] = v->data() + v->size();
        abc[2] = v->data() + v->capacity();
        guest = v;
    }

    static void _host_string_destroy(void*& guest, void*& host, char** abc) {
        auto* v = static_cast<::std::string*>(guest);
        fprintf(stdout, "GUEST=>HOST _host_string_destroy(native=%p, v=%p, v->_d=%p, flags=[%s]) [%p, %p, %p, %p]\n", guest, v, host, "", abc[0], abc[1], abc[2], abc[3] );fflush(stdout);

        if (!guest && host) {
            fprintf(stdout, "_host_string_destroy.... should i delete guested %p!?!?!?\n", host);fflush(stdout);
            ::std::string* g= (::std::string*)host;
            track(g, false);
            delete g; 
            host = nullptr;
        }
        if (v) { track(v, false); delete v; }
        abc[0] = abc[1] = abc[2] = nullptr;
        guest = nullptr;
    }
    static void _host_string_resize(void*& guest, void*& host, char** abc, size_t n) {
        ::std::string* v = static_cast<::std::string*>(guest);
        host::fprintf(1,"_host_string_resize(native=%p,v=%p, n=%zu, abc=%p, abc[0]=%p)\n", guest, v, n, abc, abc[0]);
        if (!v) v = new ::std::string;
        v->resize(n); 
        track(v, true);
        abc[0] = v->data();
        abc[1] = v->data() + v->size();
        abc[2] = v->data() + v->capacity();
        guest = v;
    }

    static int host_vfprintf_impl(int stream, const char* format, __builtin_va_list& args) {
        FILE* file = (long long)stream == 1 ? stdout : (long long)stream == 2 ? stderr : static_cast<FILE*>((void*)(long long)stream);
        return vfprintf(file, format, args);
    }
    static int _host_log(const char* c_str) {
        fprintf(stdout, "[API.host_log..] %s\n", c_str);fflush(stdout);
        return 10000;
    }
    static double _host_now() {
        // static auto st = __builtin_readcyclecounter();
            // unsigned long long cycles = __builtin_readcyclecounter() - st;
            // return (double)cycles / 2.5e9;

        static std::chrono::duration<double> start = std::chrono::system_clock::now().time_since_epoch();
        std::chrono::duration<double> cur = std::chrono::system_clock::now().time_since_epoch();
        return (cur-start).count();
    }
    crtypes::Hosted hosted{
        _host_now, 
        _host_vector_float_resize, _host_vector_float_destroy, _host_vector_float_push_back, 
        _host_string_resize, _host_string_destroy,
        _host_realloc, _host_free,
        _host_log, host_vfprintf_impl,};
    int blah = []{ hosted.__sync__("host"); return 5; }();
#endif
#ifdef CR_MAGIC_GUEST
    crtypes::Hosted hosted{};
    int blah = []{ hosted.__sync__("guest"); return 5; }(); // will be triggered after cr_load but during static init 
#endif

} //ns

#ifdef CR_MAGIC_GUEST
        #ifdef CR_MAGIC_GUEST_PURE
            template<> void my_glm_std_emulation::host_buffer<float>::resize(size_t n) {
                crtypes::hosted.vector_float_resize(_guest, __host, &this->_start, n);
            }
            template<> void my_glm_std_emulation::host_buffer<float>::destroy(const char* hint) {
                crtypes::hosted.vector_float_destroy(_guest, __host, &this->_start);
            }
            template<> void my_glm_std_emulation::vector<float>::push_back(float const& value) {
                crtypes::hosted.vector_float_push_back(_guest, __host, &this->_start, value);
            }

            template<> void my_glm_std_emulation::host_buffer<char>::resize(size_t n) {
                if (!crtypes::hosted.string_resize) return;
                crtypes::hosted.string_resize(_guest, __host, &this->_start, n);
            }
            template<> void my_glm_std_emulation::host_buffer<char>::destroy(const char* hint) {
                host::fprintf(1, "GUEST::host_string::destroy(%s, %p, _guest=%p, __host=%p)\n", hint, this, _guest, __host);
                if (!crtypes::hosted.string_destroy) return;
                crtypes::hosted.string_destroy(_guest, __host, &this->_start);
            }
        #else
            template<> void my_glm_std_emulation::host_buffer<float>::resize(size_t n) {
                crtypes::hosted.vector_float_resize(_guest, __host, &this->_start, n);
            }
            template<> void my_glm_std_emulation::host_buffer<float>::destroy(const char* hint) {
                crtypes::hosted.vector_float_destroy(_guest, __host, &this->_start);
            }
            template<> void my_glm_std_host_emulation::vector<float>::push_back(float const& value) {
                crtypes::hosted.vector_float_push_back(_guest, __host, &this->_start, value);
            }

            template<> void my_glm_std_emulation::host_buffer<char>::resize(size_t n) {
                if (!crtypes::hosted.string_resize) return;
                crtypes::hosted.string_resize(_guest, __host, &this->_start, n);
            }
            template<> void my_glm_std_emulation::host_buffer<char>::destroy(const char* hint) {
                crtypes::hosted.fprintf(1, "GUEST::host_string::destroy(%s, %p, _guest=%p, __host=%p)\n", hint, this, __host, _guest);
                if (!crtypes::hosted.string_destroy) return;
                crtypes::hosted.string_destroy(_guest, __host, &this->_start);
            }
        #endif
#endif

#ifdef CR_MAGIC_HOST_IMPLEMENTATION
    template<> void my_glm_std_emulation::host_buffer<float>::resize(size_t n) {
        crtypes::_host_vector_float_resize(_guest, __host, &this->_start, n);
    }
    template<> void my_glm_std_emulation::host_buffer<float>::destroy(const char* hint) {
        fprintf(stdout, "[%p] HOST::vector::_destroy %s %p _guest=%p, __host=%p, flags=[%s]\n", crtypes::get_api_vtable_str(), hint, this, _guest, __host, flags());fflush(stdout);
        crtypes::_host_vector_float_destroy(_guest, __host, &this->_start);
    }
    template<> void my_glm_std_host_emulation::vector<float>::push_back(float const& value) {
        crtypes::_host_vector_float_push_back(_guest, __host, &this->_start, value);
    }
    template<> void my_glm_std_emulation::host_buffer<char>::resize(size_t n) {
        crtypes::_host_string_resize(_guest, __host, &this->_start, n);
    }
    template<> void my_glm_std_emulation::host_buffer<char>::destroy(const char* hint) {
        fprintf(stdout, "[%p] HOST::string::_destroy %s %p _guest=%p, __host=%p, flags=[%s]\n", crtypes::get_api_vtable_str(), hint, this, _guest, __host, flags());fflush(stdout);
        crtypes::_host_string_destroy(__host, _guest, &this->_start);
    }
#endif

#endif // humbletim_crtypes_hpp
