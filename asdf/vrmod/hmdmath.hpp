#ifndef humbletim_hmdmath_hpp
#define humbletim_hmdmath_hpp

#define humbletim_hmdmath_cr "build/hmdmath_guest.dll"

#include "cr-magic.hpp"

#if defined(humbletim_hmdmath_guest_pure)
    #define CR_MAGIC_GUEST
    #define CR_MAGIC_GUEST_PURE
    #include "cr.h" // *without* defining CR_HOST
#elif defined(humbletim_hmdmath_host_implementation)
    #define CR_MAGIC_HOST
    #define CR_MAGIC_HOST_IMPLEMENTATION 
    #include "cr.h" // *without* defining CR_HOST
#elif defined(humbletim_hmdmath_guest)
    #define CR_MAGIC_GUEST
    #include "cr.h" // *without* defining CR_HOST
#elif defined(humbletim_hmdmath_host)
    #define CR_MAGIC_HOST
#endif

#include "vrmod/cr-types.hpp"

#include "humbletim/slos.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp> // angleAxis
#include <glm/gtx/transform.hpp> // translate
#include <glm/gtx/string_cast.hpp>

namespace hmdmath {
    namespace std { using namespace ::abi_std; }
    // static initialization testing...
    // struct GG  {
    //     std::string name;
    //     std::vector<float> G;
    //     // GG(float G) : G(G) {};
    //     ~GG() { fprintf(stdout, "~GG\n"); fflush(stdout); }
    // };
    // static GG g{"!!GG", { 3.14159f, 5.0f }};

    struct Hot {
        static float test() {
            return 1.0f;///*g.G +*/ hosted.now();
        }
        static glm::vec3 version() { return {INFINITY,NAN,glm::pi<float>()}; }
        static std::string str(const char* input) { 
            host::fprintf(1, "BEFORE...\n");
            std::string tmp;
            tmp = std::string("hi thxere! -- ") + input + std::string("--");// + GG{"I"}.name;
            // hosted.log((std::string{"a"}+"Xx"+"y"+std::string{"b"}).c_str());
            std::string final_str = tmp + "########@@@########x";//).c_str();
            // --- MAGICAL INSTRUMENTATION START ---
            crtypes::Inspect(crtypes::get_api_vtable_str(), "GUEST-EXIT", final_str);
            // --- MAGICAL INSTRUMENTATION END ---
            // tmp.resize(0);
            host::fprintf(1, "hmdmat XAFTER %s\n", final_str.c_str());
            return final_str;
        }
        static float vector(std::vector<float>& floats, float a, float b, float c) {
            floats[0] = (float)(long long)&floats[0];
            // = {a,b,a};
            return (float)(long long)&floats;
        }
        static std::vector<float> rvector(float a, float b, float c) {
            std::vector<float> tmp = {a,b,c};
            crtypes::hosted.log("rvector...");
            tmp.push_back(crtypes::hosted.now());
            return tmp;
        }
        static float add(float a, float b) {
            // host::fprintf(1, "sinf %p\n", ::sinf);fflush(stdout);
            return glm::sin(crtypes::hosted.now()*1.0f);
        }
        // struct PlayerTelemetry {
        //     glm::mat4 worldToAvatar;
        //     glm::mat4 mRoot, mPelvis, mHead, mWristLeft, mWristRight, mFootLeft, mFootRight;
        //     float ahh, ooh;
        // };
        static bool pose(slos::ht::PlayerTelemetry* pt, slos::ht::PlayerTelemetry* bind) {
            float t = crtypes::hosted.now()*10.0f;
            static slos::ht::PlayerTelemetry identity{};
            if (!bind) {
                // if (auto tmp = static_cast<slos::ht::PlayerPose*>(pt)) bind = tmp->bind.get();
                // else 
                bind = &identity;
            }
            pt->mHead = glm::translate(glm::vec3(bind->mHead[3]))  * glm::mat4_cast(glm::angleAxis(glm::radians((float)glm::sin(crtypes::hosted.now()/1.0f) * 15.0f), glm::vec3(0,0,1)));
            pt->mFootLeft = glm::translate(glm::vec3(bind->mFootLeft[3])) * glm::mat4_cast(glm::angleAxis(glm::radians((float)glm::sin(crtypes::hosted.now()*16.0f) * -15.0f), glm::vec3(0,0,1)));
            pt->mFootRight = glm::translate(glm::vec3((bind->mFootRight)[3]))  * glm::mat4_cast(glm::angleAxis(glm::radians((float)glm::sin(crtypes::hosted.now()*16.0f) * 15.0f), glm::vec3(0,1,0)));
            pt->mWristRight = bind->mWristRight; //glm::mat4(1);//glm::mat4_cast(glm::angleAxis(glm::radians(glm::sin((float)crtypes::hosted.now()*10.0f) * 105.0f), glm::vec3(1,0,0)));
            // host::fprintf(1, "mWristRight=%s\n", glm::to_string(glm::mat4(pt->mWristRight)).c_str());
            pt->mWristLeft = bind->mWristRight * glm::mat4_cast(glm::angleAxis(glm::radians(95.0f), glm::vec3(0,0,1)));
            return true;
        }
    };

    struct API : Hot, crtypes::WithHostAPIs {
        using hot = Hot;
        int counter = 0;
        decltype(&hot::version) version = hot::version;
        decltype(&hot::test)    test    = hot::test;
        decltype(&hot::str)     str     = hot::str;
        decltype(&hot::vector)  vector  = hot::vector;
        decltype(&hot::rvector) rvector = hot::rvector;
        decltype(&hot::add) add = hot::add;
        decltype(&hot::pose) pose = hot::pose;
    };
    extern API api; 


    namespace host {
        bool init();
        bool poll(bool reloadCheck = true);
        bool shutdown();
        void set_library_path(const char* path);
    }
}
#endif // humbletim_hmdmath_hpp

// =============================================================================
#if defined(CR_MAGIC_HOST) || defined(CR_MAGIC_GUEST)
    #ifdef humbletim_hmdmath_cr
        #define HMDMATH_DLL_PATH humbletim_hmdmath_cr
    #else
        #define HMDMATH_DLL_PATH nullptr
    #endif

    #ifdef CR_MAGIC_HOST_IMPLEMENTATION
        CR_MAGIC_HOST_IMPL(hmdmath, API, HMDMATH_DLL_PATH)
    #endif
    #ifdef CR_MAGIC_GUEST
        CR_MAGIC_GUEST_IMPL(hmdmath, API)
    #endif

    // 4. Cleanup
    #undef CR_MAGIC_HOST
    #undef CR_MAGIC_HOST_IMPLEMENTATION
    #undef CR_MAGIC_GUEST
    #undef CR_MAGIC_GUEST_PURE
    #undef HMDMATH_DLL_PATH

#elif __INCLUDE_LEVEL__ == 0
    #error "TPVM_RECIPE: -xc++ -Dhumbletim_hmdmath_host_implementation"
    #error "GUEST_RECIPE: -xc++ -shared -Dhumbletim_hmdmath_guest -o build/hmdmath_guest.dll -g"
    #error "PURE_GUEST_RECIPE: \
        clang++ -nostdlib -nostdinc -fno-threadsafe-statics \
        --target=x86_64-w64-windows-unknown -fuse-ld=lld -shared \
        -I snapshot/3p/include -I pure/ -I. \
        -Dhumbletim_hmdmath_guest_pure -xc++ -g vrmod/hmdmath.hpp \
        -o build/hmdmath_guest.dll "

#endif // CR_MAGIC_HOST || CR_MAGIC_GUEST
