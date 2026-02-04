#ifndef humbletim_crtest_hpp
#define humbletim_crtest_hpp

#define HUMBLETIM_CRTEST_GUEST_DLL "build/crtest_guest.dll"

#include "cr-magic.hpp"

#if defined(humbletim_crtest_guest_pure)
    #define CR_MAGIC_GUEST
    #define CR_MAGIC_GUEST_PURE
    #include "cr.h" // *without* defining CR_HOST
#elif defined(humbletim_crtest_host_implementation)
    #define CR_MAGIC_HOST
    #define CR_MAGIC_HOST_IMPLEMENTATION 
    #include "cr.h" // *without* defining CR_HOST
#elif defined(humbletim_crtest_guest)
    #define CR_MAGIC_GUEST
    #include "cr.h" // *without* defining CR_HOST
#elif defined(humbletim_crtest_host)
    #define CR_MAGIC_HOST
#endif

#include "vrmod/cr-types.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace crtest {
    namespace std { using namespace ::abi_std; }
    // static initialization testing...
    // struct GG  {
    //     std::string name;
    //     std::vector<float> G;
    //     // GG(float G) : G(G) {};
    //     ~GG() { host::printf("~GG\n"); }
    // };
    // static GG g{"!!GG", { 3.14159f, 5.0f }};

    struct Hot {
        static float test() {
            host::printf("Hot::test\n");
            return 2.0f;///*g.G +*/ hosted.now();
        }
        static glm::vec3 version() { return {INFINITY,NAN,glm::pi<float>()}; }
        static std::string str(const char* input) { 
            host::fprintf(1,"BEFORE...\n");
            std::string tmp;
            tmp = std::string("crtest hi thxere! -- ") + input + std::string("--");// + GG{"I"}.name;
            // hosted.log((std::string{"a"}+"Xx"+"y"+std::string{"b"}).c_str());
            std::string final_str = tmp + "###crtest#####@@@########x";//).c_str();
            // --- MAGICAL INSTRUMENTATION START ---
            crtypes::Inspect(crtypes::get_api_vtable_str(), "GUEST-EXIT", final_str);
            // --- MAGICAL INSTRUMENTATION END ---
            // tmp.resize(0);
            host::fprintf(1,"crtest XAFTER... %s\n", final_str.c_str());
            return final_str;
        }
        static float vector(std::vector<float>& floats, float a, float b, float c) {
            floats[0] = (float)(long long)&floats[0];
            // = {a,b,a};
            return (float)(long long)&floats;
        }
        static std::vector<float> rvector(float a, float b, float c) {
            std::string xtmp = "oop";
            host::printf("hxia %s\n", xtmp.c_str());
            std::vector<float> tmp = {a,b,c};
            host::printf("hxib\n");
            tmp.push_back(crtypes::hosted.now());
            host::printf("hxi\n");
/*             crtypes::hosted.log("rvector...");
 */            return tmp;
        }
        // static float add(float a, float b) {
        //     fprintf(stdout, "sinf %p\n", ::sinf);fflush(stdout);
        //     return glm::sin(now()*10.0f);
        // }
    };

    struct API : Hot, crtypes::WithHostAPIs {
        using hot = Hot;
        int counter = 0;
        decltype(&hot::version) version = hot::version;
        decltype(&hot::test)    test    = hot::test;
        decltype(&hot::str)     str     = hot::str;
        decltype(&hot::vector)  vector  = hot::vector;
        decltype(&hot::rvector) rvector = hot::rvector;
    };
    extern API api; 

    namespace host {
        bool init();
        bool poll(bool reloadCheck = true);
        bool shutdown();
        void set_library_path(const char* path);
    }
}
#endif // humbletim_crtest_hpp

// =============================================================================
#if defined(CR_MAGIC_HOST) || defined(CR_MAGIC_GUEST)
    #ifndef HUMBLETIM_CRTEST_GUEST_DLL
        #define HUMBLETIM_CRTEST_GUEST_DLL nullptr
    #endif

    #ifdef CR_MAGIC_HOST_IMPLEMENTATION
        CR_MAGIC_HOST_IMPL(crtest, API, HUMBLETIM_CRTEST_GUEST_DLL)
    #endif
    #ifdef CR_MAGIC_GUEST
        CR_MAGIC_GUEST_IMPL(crtest, API)
    #endif

    // 4. Cleanup
    #undef CR_MAGIC_HOST
    #undef CR_MAGIC_HOST_IMPLEMENTATION
    #undef CR_MAGIC_GUEST
    #undef CR_MAGIC_GUEST_PURE
    #undef HUMBLETIM_CRTEST_GUEST_DLL

#elif __INCLUDE_LEVEL__ == 0
    #error "TPVM_RECIPE: -xc++ -Dhumbletim_crtest_host_implementation"
    #error "GUEST_RECIPE: -xc++ -shared -Dhumbletim_crtest_guest -o build/crtest_guest.dll -g"
    #error "PURE_GUEST_RECIPE: \
        clang++ -nostdlib -nostdinc -fno-threadsafe-statics \
        --target=x86_64-w64-windows-unknown -fuse-ld=lld -shared \
        -I snapshot/3p/include -I pure/ -I. \
        -g \
        -Dhumbletim_crtest_guest_pure -xc++ vrmod/crtest.hpp \
        -o build/crtest_guest.dll "

#endif // humbletim_crtest_host || humbletim_crtest_guest
