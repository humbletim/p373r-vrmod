// =============================================================================
// SECTION B: IMPLEMENTATION GENERATOR
// =============================================================================
#if 1 //defined(CR_MAGIC_HOST) || defined(CR_MAGIC_GUEST)
    // 1. GUEST LOGIC
    // #if defined(CR_MAGIC_GUEST)
        // #include "cr.h" // Guest needs standard symbols
    #define CR_MAGIC_GUEST_IMPL(NS, API, ...) \
        namespace NS { constexpr static const auto CR_MODE{ "CR_MAGIC_GUEST" }; API api{}; } \
        CR_EXPORT int cr_main(struct cr_plugin *ctx, enum cr_op operation) { \
            NS::API* ptr = (NS::API*)ctx->userdata; \
            if (!ptr) return -1; \
            int pp = ptr->counter; \
            /* if (ptr->hosted && ptr->hosted->log && ptr->hosted->log("cr_main") < 0) ;  */\
            static NS::API backup = *ptr; \
            switch (operation) { \
                case CR_LOAD: { \
                    /* if (0) if (backup.hosted->log) backup.hosted->log("[~Guest "#NS"] CR_LOAD"); */ \
                    host::printf("[Guest "#NS"] CR_LOAD ptr=%p version=%d\n", ptr, ctx->version); \
                    /* if (!ptr->sanity.check()) { host::printf("[Guest "#NS"] CR_LOAD -- sanity check failed; ptr=%p\n", ptr); return -1; } */ \
                    *ptr = NS::API{}; \
                    ptr->counter = pp + 1; \
                    crtypes::hosted = *backup.hosted; \
                    crtypes::hosted.log("[~Guest "#NS"] ManualInitGlobals"); \
                    crtypes::hosted.fprintf(1,"[Guest "#NS"] CR_LOAD ptr=%p counter=%d -- calling ManualInitGlobals\n", ptr, ptr->counter); \
                    /*);*/ \
                    ManualInitGlobals(); \
                    /* if(0)crtypes::hosted.log("[~Guest "#NS"] //CR_LOAD"); */ \
                    /* if(0)host::fprintf(1, "[Guest "#NS"] //CR_LOAD ptr=%p counter=%d\n", ptr, ptr->counter); */ \
                    } break; \
                case CR_CLOSE: ManualDeinitGlobals(); /* if(0)backup.hosted->log("[~Guest "#NS"] CR_CLOSE"); if(0)host::fprintf(1, "[Guest "#NS"] CR_CLOSE ptr=%p\n", ptr); */ *ptr = backup; ptr->counter = pp; break; \
                case CR_UNLOAD: ManualDeinitGlobals(); /* if(0)backup.hosted->log("[~Guest "#NS"] CR_UNLOAD"); if(0)host::fprintf(1, "[Guest "#NS"] CR_UNLOAD ptr=%p\n", ptr); */ *ptr = backup; ptr->counter = pp; break; \
                case CR_STEP: break; \
            } \
            return 0; \
        }
    // #else
    //     #define CR_MAGIC_GUEST_IMPL(NS, API)
    // #endif

    // 2. HOST LOGIC (The Static Shim)
    // #if defined(CR_MAGIC_HOST_IMPLEMENTATION)

        // FIXME: hard coded extractions to allow multiple "cr hosted modules" at same time
        // (which specifying CR_HOST would create conflicts since only one unit can "implement" these)
        // #include "cr.h"
        // #ifndef CR_HOST
        // extern "C" bool cr_plugin_open(cr_plugin &ctx, const char *fullpath);
        // extern "C" void cr_plugin_close(cr_plugin &ctx);
        // extern "C" int cr_plugin_update(cr_plugin &ctx, bool reloadCheck = true);
        // #endif
        // GENERATOR
    #define CR_MAGIC_HOST_IMPL(NS, API, DEFAULT_PATH) \
        extern "C" bool cr_plugin_open(cr_plugin &ctx, const char *fullpath); \
        extern "C" void cr_plugin_close(cr_plugin &ctx); \
        extern "C" int cr_plugin_update(cr_plugin &ctx, bool reloadCheck); \
        namespace NS { constexpr static const auto CR_MODE{ "CR_MAGIC_HOST" }; API api{}; namespace host { \
            const char* _dll_path = DEFAULT_PATH; \
            cr_plugin _plugin{}; \
            void set_library_path(const char* path) { _dll_path = path; } \
            bool ready() { return _plugin.p; } \
            bool init() { \
                fprintf(stdout, "[Host "#NS"] init() _dll_path='%s' _plugin.p=%p\n", _dll_path ? _dll_path : "(nil)", _plugin.p); \
                /* NS::api.hosted->log("init..."); */ \
                if (!_dll_path) return false; \
                if (_plugin.p) return true; \
                _plugin.userdata = &NS::api; \
                if (!cr_plugin_open(_plugin, _dll_path)) return false; \
                /* NS::api.hosted->log("//init"); */ \
                return true; \
            } \
            bool shutdown() { \
                if(1)NS::api.hosted->log("shutdown..."); \
                fprintf(stdout, "[Host "#NS"] shutdown() _dll_path='%s' _plugin.p=%p\n", _dll_path ? _dll_path : "(nil)", _plugin.p); \
                if (_plugin.p) { cr_plugin_close(_plugin); _plugin.p = nullptr; } \
                NS::api = NS::API{}; \
                return true; \
            } \
            bool poll(bool reloadCheck) { \
                if (!_plugin.p) return false; \
                /* if(1)NS::api.hosted->log("...poll...");  */ \
                static unsigned int last_version = 0; \
                if (auto rc = cr_plugin_update(_plugin, reloadCheck); rc < 0) { \
                    fprintf(stdout, "[Host "#NS"] poll() _dll_path='%s' _plugin.p=%p update failed... %d\n", _dll_path ? _dll_path : "(nil)", _plugin.p, rc); \
                    if (!reloadCheck && _plugin.p) { cr_plugin_close(_plugin); _plugin.p = nullptr; } \
                    NS::api = NS::API{}; \
                    return false; \
                } \
                if (last_version != _plugin.version) fprintf(stdout, "[Host "#NS"] poll() _dll_path='%s' _plugin.p=%p counter=%d version=%d\n", _dll_path ? _dll_path : "(nil)", _plugin.p, NS::api.counter, last_version = _plugin.version); \
                /* if(1)NS::api.hosted->log("//poll..."); */ \
                return true; \
            } \
        }}
    // #else
    //     #define CR_MAGIC_HOST_IMPL(NS, API, DEFAULT_PATH)
    // #endif

    // // 3. THE UNIT MACRO
    // #define CR_MAGIC_UNIT(NS, API, DEFAULT_PATH) \
    //     CR_MAGIC_GUEST_IMPL(NS, API) \
    //     CR_MAGIC_HOST_IMPL(NS, API, DEFAULT_PATH)

#else
    // #define CR_MAGIC_UNIT(NS, API, DEFAULT_PATH) asdfasdf
#endif