#ifndef humbletim_crmath_h
#define humbletim_crmath_h

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace crmath {
    namespace cold {
        bool init();
        void poll();
        bool shutdown();
        glm::vec3 version();
    } //cold

    struct ReloadableMathAPI {
        static glm::vec3 version() {
            return { 0, 0, 0.99f }; 
        }
        static glm::mat4 calc_a(glm::mat4 const& input) {
            return glm::translate(input, glm::vec3(0,-.5f,0)); 
        }
        static float calc_b(glm::vec3 const& a, glm::vec3 const& b) {
            return glm::dot(a, b);
        }
    };

    struct MathAPI : ReloadableMathAPI {
        using hot = ReloadableMathAPI;
        decltype(&hot::version) version = hot::version;
        decltype(&hot::calc_a)  calc_a  = hot::calc_a;
        decltype(&hot::calc_b)  calc_b  = hot::calc_b;
    } warm{};

    // NOTE to FUTURE SELF: this can work to avoid secondary decls (but is harder to read...)
    // struct MathAPI {
    //     glm::vec3 (*do_version)() = []() -> glm::vec3 { 
    //         return { 0, 0, 0.99f }; 
    //     };
    //     glm::mat4 (*do_calc_a)(glm::mat4 const&) = [](auto const& input) -> glm::mat4 {
    //         return glm::translate(input, glm::vec3(0, .25f, 0));
    //     };
    // };
} //crmath

#endif // humbletim_crmath_h

// =============================================================================
// IMPLEMENTATION ZONES
// =============================================================================

#ifdef humbletim_crmath_implementation

// ---------------------------------------------------------
// THE GUEST RSVP
// ---------------------------------------------------------
#ifdef humbletim_crmath_guest_implementation

#include "cr.h"
#include <cstdio>

CR_EXPORT int cr_main(struct cr_plugin *ctx, enum cr_op operation) {
    crmath::MathAPI* api = (crmath::MathAPI*)ctx->userdata;
    if (!api) {
        fprintf(stdout, "[Guest] CR_LOAD ERROR: Userdata is null!\n");
        return -1;
    }
    static crmath::MathAPI backup = *api;
    switch (operation) {
        case CR_LOAD:
            fprintf(stdout, "[Guest] CR_LOAD: Handshaking pointers... (p=%p, v%d)\n", api, ctx->version);
            *api = crmath::MathAPI{}; // assign hot versions
            break;

        case CR_UNLOAD: // Preparing for hot-reload
            fprintf(stdout, "[Guest] CR_UNLOAD: Preparing to reload...(p=%p)\n", api);
            *api = backup; // restore Host's V-Table
            break;
        case CR_CLOSE:  // Shutting down
            fprintf(stdout, "[Guest] CR_CLOSE: cleaning up...(p=%p)\n", api);
            *api = backup; // restore the Host's V-Table
            break;
            
        case CR_STEP:
            break;
    }
    
    return 0;
}
#endif // humbletim_crmath_guest_implementation

// ---------------------------------------------------------
// THE HOST (The App that runs the DLL)
// ---------------------------------------------------------
#ifdef humbletim_crmath_host_implementation

// Enable CR_DEBUG to see why loading fails (prints to stderr/stdout)
// #define CR_DEBUG 
// #define CR_HOST CR_UNSAFE 
// #include "cr.h"

namespace crmath {
    namespace cold {
        glm::vec3 version() { return { 0, 0, 1 }; }
        cr_plugin g_mathPlugin{};
        bool shutdown() {
            fprintf(stdout, "[Host] closing plugin: (p=%p, api=%p)\n", g_mathPlugin.p, warm.version);
            if (g_mathPlugin.p) {
                cr_plugin_close(g_mathPlugin);
                g_mathPlugin.p = nullptr; // cr_plugin_close doesn't zero this automatically
            }
            warm = {}; // restore initial state
            return true;
        }

        bool init() {
            if (g_mathPlugin.p) {
                return true; // Idempotent
            }

            g_mathPlugin.userdata = &warm; 
            
            // This only sets paths; the actual load happens in the first poll()
            if (!cr_plugin_open(g_mathPlugin, "build/math_guest.dll")) {
                fprintf(stdout, "[Host] ERROR: cr_plugin_open failed (file missing %s?)\n", "build/math_guest.dll");
                return false;
            }
            
            fprintf(stdout, "[Host] Plugin initialized: %p\n", g_mathPlugin.p);
            return true;
        }

        void poll() {
            static unsigned int last_version = 0;
            if (!g_mathPlugin.p) return;

            // CR_DEBUG will now print if this returns -2 (Load Failure)
            int ret = cr_plugin_update(g_mathPlugin);
            
            if (ret < 0) {
                // -2 usually means symbol lookup failed (cr_main missing) or file lock
                fprintf(stdout, "[Host] CR_UPDATE FAILURE: %d\n", ret);
                warm = {}; // restore initial state
            }
            
            if (g_mathPlugin.version != last_version) {
                fprintf(stdout, "[Host] Hot Reload: v%d -> v%d\n", last_version, g_mathPlugin.version);
                last_version = g_mathPlugin.version;
            }
        }
    } // cold
} //crmath 

#endif  // humbletim_crmath_host_implementation

// ---------------------------------------------------------
// RECIPES
// ---------------------------------------------------------
#elif __INCLUDE_LEVEL__ == 0
    #error "TPVM_RECIPE: -xc++ -Dhumbletim_crmath_implementation -Dhumbletim_crmath_host_implementation"
    #error "GUEST_RECIPE: -xc++ -shared -Dhumbletim_crmath_implementation -Dhumbletim_crmath_guest_implementation -o build/math_guest.dll -g"
#endif // humbletim_crmath_implementation