#ifndef HUMBLETIM_VRMOD_H
#define HUMBLETIM_VRMOD_H

#include <cstdint>
#include <glm/glm.hpp>

namespace nunja {
    extern void HandleViewerUI();
    extern void Startup();
    extern void Shutdown();
    extern void BeginFrame(uint32_t frame);
    extern void BeginEye(int eye);
    
    extern void EndEye(int eye);
    extern void EndFrame(uint32_t frame);

    extern bool IsEyeReady(int eye);

    extern int FirstEye();
    extern int SecondEye();

    extern bool IsStereo();

    extern glm::mat4 getEyePerspective(int eye, float z_near, float z_far);
    extern glm::mat4 getHeadFloorPose();

    extern int get_world_eye();

    extern glm::mat4 getEyeToHeadTransform(int eye);
    extern bool IsRoomScale();
    namespace cache {
        extern glm::mat4& agentCamera;
        extern glm::mat4& agentCameraRoomScale;
        extern glm::mat4& agentCameraIntegrated;
    }
    extern const char* get_stack_trace();
}

#endif // HUMBLETIM_VRMOD_H

#ifdef HUMBLETIM_VRMOD_IMPLEMENTATION

#include <vector>
#include <string>
#include <sstream>
#include <glm/gtx/string_cast.hpp> // glm::to_string

#include <humbletim/xopenvr.hpp>
#include <humbletim/foaf.hpp>
#include <humbletim/win32.desktop.hpp> // win32::DesktopMonitorCapture

#define CR_HOST CR_UNSAFE
#define CR_DEBUG
#include "cr.h"

#define humbletim_crmath_host
#define humbletim_crmath_implementation
#define humbletim_crmath_host_implementation
#include <vrmod/crmath.hpp>

#define humbletim_hmdmath_host
// #define CR_MAGIC_HOST_IMPLEMENTATION
// #define humbletim_hmdmath_implementation
#include <vrmod/hmdmath.hpp>

// EMBEDDED (implementation details here, not as separate .hpp.obj's))
#define SLOS_IMPLEMENTATION
#include <humbletim/slos.hpp> // slos::on_app_quitting
#define VRMOD_SETTINGS_IMPLEMENTATION
#include <humbletim/slos.settings.hpp>
#define SLOS_UIRT_IMPLEMENTATION
#include <humbletim/slos.uirt.hpp> // slos::EyeRenderTarget
#define NOBAKE_IMPLEMENTATION
#include <humbletim/slos.nobake.hpp> // slos::reenableBaking
slos::BakingDisabler baking_disabler{};
#define humbletim_probe_implementation
#include <humbletim/probe.hpp>
// /EMBEDDED

namespace FSCommon { void report_to_nearby_chat(std::string_view message); }

namespace nunja {
    void _reset_ctx_rendertargets();
    struct VrSettings : vrmod::ExpandoSettings {
        bool handcontrollers{ true };
        bool handlasers{ true };
        bool mousecursor{ true };
        int virtualDesktopMaxDimension{ 512 };
        float virtualDesktopTargetFPS{ 10.0 };
        float uiTargetFPS{ 10.0 };
        float windowScaleFactor { 0.75 };
        // float worldScaleFactor { 1.0 };
            
        float nearClip{ 0.0 };
        float farClip{ 0.0 };
        float ipdScale{ 1.0 };
        bool sbsDebug{ false };
        bool flipEyes{ false };
        bool useDepth{ false };
        bool useMip{ false };
        bool tareYaw{ true };
        bool roomScale{ false };
        glm::vec4 cursors{  1, 2, 1, 0  };
        glm::vec4 _K{  0, 0, 0, 0  };
        glm::vec3 uiOffset{ 0.0, 0.0, 2.0  };
        glm::vec3 _C{  0,0,0  };

        // 2. Register the namespace
        VrSettings() : ExpandoSettings("vrmod") {}

        // 3. Define the bindings
        virtual void declare() override {
            bind("handcontrollers", &handcontrollers);
            bind("handlasers", &handlasers);
            bind("mousecursor", &mousecursor);
            bind("virtualDesktopMaxDimension", &virtualDesktopMaxDimension);
            bind("virtualDesktopTargetFPS", &virtualDesktopTargetFPS);
            bind("uiOffset", &uiOffset);
            bind("uiTargetFPS", &uiTargetFPS);
            bind("windowScaleFactor ", &windowScaleFactor );
            // bind("worldScaleFactor ", &worldScaleFactor );
                
            bind("nearClip", &nearClip);
            bind("farClip", &farClip);
            bind("ipdScale", &ipdScale);
            bind("sbsDebug", &sbsDebug);
            bind("flipEyes", &flipEyes);
            bind("useDepth", &useDepth, "", _reset_ctx_rendertargets);
            bind("useMip", &useMip, "", _reset_ctx_rendertargets);
            bind("tareYaw", &tareYaw);
            bind("roomScale", &roomScale);
            bind("cursors", &cursors);
            bind("_C", &_C);
            bind("_K", &_K);
        }
    };
    
    // Global instance
    static VrSettings settings;
}
static nunja::VrSettings* gVrModSettings = &nunja::settings;

namespace asdf {
    bool pending = false;
}

namespace HMDMath {
    // This rotation matrix moves the default OpenGL reference frame
    // (-Z at, Y up) to Cory's favorite reference frame (X at, Z up)
    const float OGL_TO_CFR_ROTATION[16] = {  0,  0, -1,  0,   // -Z becomes X
                                            -1,  0,  0,  0,   // -X becomes Y
                                             0,  1,  0,  0,   //  Y becomes Z
                                             0,  0,  0,  1 };

    // Transformation from OpenVR/OpenGL (RH, Y-Up, -Z Fwd) to Second Life (RH, Z-Up, +X Fwd)
    // Mapping:
    //   VR Right (+X) -> SL Right   (-Y)
    //   VR Up    (+Y) -> SL Up      (+Z)
    //   VR Back  (+Z) -> SL Back    (-X)
    const glm::mat4 kBasis_Yup_to_Zup = glm::transpose(glm::make_mat4(OGL_TO_CFR_ROTATION));

    // The Inverse (SL -> OpenVR)
    const glm::mat4 kBasis_Zup_to_Yup = glm::inverse(kBasis_Yup_to_Zup);

    // ----------------------------------------------------------------------------
    // CONVERSION FUNCTIONS
    // ----------------------------------------------------------------------------

    static glm::mat4 yup_to_zup(glm::mat4 yupTRS) {
        // Similarity Transform: B * M * B_inverse
        // This converts the 'Language' of the matrix from VR-Dialect to SL-Dialect.
        // It correctly re-orients "Forward is -Z" input to "Forward is +X" output.
        return kBasis_Yup_to_Zup * yupTRS * kBasis_Zup_to_Yup;
    }

    static const glm::mat4 zup_to_yup(glm::mat4 zupTRS) {
        // Similarity Transform: B_inverse * M * B
        return kBasis_Zup_to_Yup * zupTRS * kBasis_Yup_to_Zup;
    }

    static glm::mat4 get_yaw_cancellation(const glm::mat4& hmd_yup_pose) {
        // 1. Get the "Back" vector from the HMD matrix (OpenVR/GLM Column 2)
        //    In OpenVR (Y-Up, -Z Fwd), the 3rd column is the Z-Axis (Back).
        float back_x = hmd_yup_pose[2][0];
        float back_z = hmd_yup_pose[2][2];

        // 2. Calculate the Yaw Angle of that vector on the floor
        //    atan2(x, z) gives us the angle relative to the Z-axis (0,0,1).
        float theta = std::atan2(back_x, back_z);

        // 3. Create a rotation that negates this angle.
        //    We rotate around the Y-Axis (Up) by -theta.
        return glm::rotate(glm::mat4(1.0f), -theta, glm::vec3(0.0f, 1.0f, 0.0f));
    }

    float _get_optical_shift(glm::vec2 const& eye_extents, glm::mat4 const& matEyeToHead, glm::mat4 const& eyeProj, float ui_distance) {
        glm::vec4 target_head(0.0f, 0.0f, -ui_distance, 1.0f);
        glm::mat4 matHeadToEye = glm::inverse(matEyeToHead);
        glm::vec4 target_eye = matHeadToEye * target_head;
        glm::vec4 target_clip = eyeProj * target_eye;
        glm::vec3 target_ndc = glm::vec3(target_clip) / target_clip.w;
        float center_u = (target_ndc.x * 0.5f) + 0.5f;
        float center_pixel_x = center_u * eye_extents.x;
        return center_pixel_x - (eye_extents.x * 0.5f);
    }
};

namespace xvr {
    extern std::string docapture(xvr::Session session, glm::mat4 hmdPose, glm::mat4 slCamera, glm::mat4 integratedCamera, std::string filename_fmt);
}

namespace nunja {
    bool _oaq = []{ slos::on_app_quitting(nunja::Shutdown); return true; }();
    // ------------------------------------------------------------------------
    // Types
    // ------------------------------------------------------------------------
    struct EyeBuffer {
        xvr::Eye which;
        slos::EyeRenderTarget rt;
        enum State { idle=0, framed, captured, } state;
        std::shared_ptr<opengl::PBOCursor> _cursor;
    };

    struct Context {
        std::string diagnostics;
        slos::HudText hud{};
        slos::UIRenderTarget ui{"UIRenderTarget"};
        win32::DesktopMonitorCapture desktop{ 512, 1.0f };

        // Driver
        xvr::Session    _session{};
        bool            ctrlTabbed = false;
        bool            tabbed = false;
        
        // Visuals
        EyeBuffer       leftEye{ xvr::Eye::Left, "xvr::Eye::Left" };
        EyeBuffer       rightEye{ xvr::Eye::Right, "xvr::Eye::Right" };
        EyeBuffer*      pFirstEye = nullptr;
        EyeBuffer*      pSecondEye = nullptr;
        EyeBuffer*      activeEye = nullptr;
        glm::ivec2      targetResolution{100, 100};
        glm::ivec2      sourceResolution{100, 100};

        // Math
        struct {
            glm::mat4       hmdPose{1.0f};
            glm::mat4       tare{1.0f};
        } yup; // VR Y-up
        struct {
            glm::mat4       agentCamera{1.0f};
            glm::mat4       agentCameraRoomScale{1.0f}; // potentially less avatar height if room scale
            glm::mat4       agentCameraIntegrated{1.0f}; // fully integrated with headPose
            glm::mat4       headPose{1.0f};
            glm::mat4       leftHandPose{1.0f};
            glm::mat4       rightHandPose{1.0f};

            slos::ht::PlayerPose player{};
            slos::HudText headLabel{ "hmd" };
            slos::HudText leftHandLabel{ "left-hand" };
            slos::HudText rightHandLabel{ "right-hand" };

        } zup; // SL Z-up
        glm::ivec2 agentMouseGL{-1, -1};
    };

    static Context ctx;

    // ------------------------------------------------------------------------
    // Utilities
    // ------------------------------------------------------------------------
    EyeBuffer* resolveEyeBuffer(int eye) {
        if (ctx.pFirstEye && eye == ctx.pFirstEye->which) return ctx.pFirstEye;
        if (ctx.pSecondEye && eye == ctx.pSecondEye->which) return ctx.pSecondEye;
        return nullptr;
    }
    bool IsEyeReady(int eye) {
        auto target = resolveEyeBuffer(eye);
        return target && target->state == EyeBuffer::State::captured;
    }
    int FirstEye() { return ctx.pFirstEye ? +ctx.pFirstEye->which : 0; }
    int SecondEye() { return ctx.pSecondEye ? +ctx.pSecondEye->which : 0; }
    bool IsStereo() { return ctx._session && ctx.tabbed; }
    int _get_render_mask(int eye) {
        glm::ivec4 eyecursors = gVrModSettings->cursors;
        return eye == FirstEye() ? eyecursors.x : eye == SecondEye() ? eyecursors.y : eyecursors.z;
    }

    int _get_roled_eye(int role) {
        glm::ivec4 eyecursors = gVrModSettings->cursors;
        const int UI_CURSOR = 1;
        const int WORLD_CURSOR = 2;
        if (eyecursors.x & role) return FirstEye();
        if (eyecursors.y & role) return SecondEye();
        return 0;
    }
    int _get_ui_eye() { return _get_roled_eye(1); }
    int get_world_eye() { return _get_roled_eye(2); }

    static void Log(const std::string& msg, bool commit = false) {
        ctx.hud.append(msg.find('\n') == 0 ? msg : "\n" + msg, 512);
        if (commit) fprintf(stdout, "LOG:'%s'\n", ctx.hud.buffer.c_str());
    }

    void _initBuffers() {
        ctx.targetResolution = ctx._session->recommendedRenderTargetSize();
        slos::__force_world_aspect_ratio(ctx.targetResolution, glm::clamp<float>(gVrModSettings->windowScaleFactor, 0.25f, 0.95f));
        ctx.sourceResolution = slos::get_pipeline_size();

        ctx.ui.useDepth = settings.useDepth;
        ctx.ui.useMip = gVrModSettings->useMip;

        ctx.desktop.setTargetFPS(glm::clamp<float>(gVrModSettings->virtualDesktopTargetFPS, 0.5f, 144.0f));
        ctx.desktop.setMaxDimension(glm::clamp<int32_t>(gVrModSettings->virtualDesktopMaxDimension, 256, 4096));
        
        ctx.leftEye.rt.useDepth = gVrModSettings->useDepth;
        ctx.rightEye.rt.useDepth = gVrModSettings->useDepth;
        ctx.leftEye.rt.ensure(ctx.sourceResolution.x, ctx.sourceResolution.y);
        ctx.rightEye.rt.ensure(ctx.sourceResolution.x, ctx.sourceResolution.y);
    }

    // ------------------------------------------------------------------------
    // Public Logic
    // ------------------------------------------------------------------------
    void _reset_ctx_rendertargets() {
        {
            fprintf(stdout, "_reset_ctx_rendertargets -- temporarily disabled...\n");
            return; // temporarily disable
        }
        ctx.ui.reset();
        ctx.leftEye.rt.reset();
        ctx.rightEye.rt.reset();
    }
    void Shutdown() {
        fprintf(stdout, "nunja::Shutdown tabbed=%d sesion=%p\n", ctx.tabbed, ctx._session.get());fflush(stdout);
        ctx.tabbed = false;
        ctx._session.reset();
        ctx.hud.clear();
        ctx.desktop.pause(true);
        _reset_ctx_rendertargets();
        crmath::cold::shutdown();
        hmdmath::host::shutdown();
    }

    void Startup() {
        if (!ctx.ctrlTabbed) { fprintf(stdout, "nunja::Startup -- !ctrlTabbed\n");fflush(stdout); return; }
        if (ctx._session) { fprintf(stdout, "nunja::Startup -- session already established %p\n", ctx._session.get());fflush(stdout); return; }
        ctx.hud.assign("Initializing VR session!");

        std::string err{};
        const char* cfg = getenv("DUMMYHMD") ? getenv("DUMMYHMD") : XVRWrapper_Version;
        
        auto session = xvr::Init(cfg, &err);

        if (!err.empty() || !session) {
            Log("ERROR: " + err);
            session.reset();
        } else {
            ctx._session = session;
            if (ctx._session->error()) Log(std::string(ctx._session->error()));
            Log(ctx._session->status());
            Log("Driver: " + std::string(ctx._session->get_driver_name()));
            Log("Model: " + std::string(ctx._session->get_model_name()));
            Log("Serial: " + std::string(ctx._session->get_serial_name()));

            _initBuffers();

            ctx._session->setUniform("cycle", gVrModSettings->_K);

            crmath::cold::init();
            Log("crmath.version: " + glm::to_string(crmath::cold::version()));
            Log("crmath.do_version: " + glm::to_string(crmath::warm.version()));

            hmdmath::host::init();
            Log("hmdmath.do_version: " + glm::to_string(hmdmath::api.version()));

            Log("sl-pipeline: " + glm::to_string(slos::get_pipeline_size()) + " aspect: " + std::to_string((float)slos::get_pipeline_size().x/slos::get_pipeline_size().y));
            Log("sl-ui: " + glm::to_string(slos::get_window_size_raw()) + " aspect: " + std::to_string((float)slos::get_window_size_raw().x/slos::get_window_size_raw().y));
            Log("ideal-per-eye: " + glm::to_string(ctx.targetResolution) + " aspect: " + std::to_string((float)ctx.targetResolution.x/ctx.targetResolution.y));
            Log("VR Ready. Press TAB to enter.", true);
        }
        // ctx.hud.sync();
    }

    void HandleVRInput();
    
    void BeginFrame(uint32_t frame) {
        if (frame % 900 == 0) fprintf(stdout, "++BeginFrame[%d]...\n", frame);
        if (!ctx._session) return;
        _initBuffers();
        crmath::cold::poll();
        hmdmath::host::poll();

        ctx.activeEye = nullptr;

        {
            ctx._session->updatePoses(slos::performance_now());
            if (ctx._session->getValidDevicePose(xvr::HMD, ctx.yup.hmdPose)) {
                ctx.zup.headPose = HMDMath::yup_to_zup(ctx.yup.tare * ctx.yup.hmdPose);
                auto l = ctx._session->get_device_index_for_role_name("left-hand");
                auto r = ctx._session->get_device_index_for_role_name("right-hand");
                glm::mat4 yup(1.0f);
                if (l != xvr::INVALID && ctx._session->getValidDevicePose(l, yup)) {
                    ctx.zup.leftHandPose = HMDMath::yup_to_zup(ctx.yup.tare * yup);
                }
                if (r != xvr::INVALID && ctx._session->getValidDevicePose(r, yup)) {
                    ctx.zup.rightHandPose = HMDMath::yup_to_zup(ctx.yup.tare * yup);
                }
                ctx.zup.headLabel.sync(ctx.zup.headPose[3]);
                ctx.zup.leftHandLabel.sync(ctx.zup.leftHandPose[3]);
                ctx.zup.rightHandLabel.sync(ctx.zup.rightHandPose[3]);
            } else {
                if (frame %120 == 0) fprintf(stdout, "getValidDevicePose(xvf::HMD) return false...\n");
            }
            slos::ht::fetch_player_pose(gAgentAvatarp, ctx.zup.player, true);
            // fprintf(stdout, "mWristLeft=%s\n", glm::to_string(glm::mat4(ctx.zup.player.mWristLeft)).c_str());
        }

        if (getenv("UI")) {
            auto hwnd = slos::__get_window_hwnd();
            if (hwnd == win32::getCursorHWND(hwnd)) {
                // in bounds AND over self (ie: not obscured by another OS window / popup)
                ctx.agentMouseGL = slos::get_mouse_pos_gl(true);
                if (auto cur = ctx.ui._cursor) {
                    cur->setCursorGL(ctx.agentMouseGL);
                }
            }

            static auto interval = slos::setInterval([]{
                nunja::ctx.ui.capture();
            }, []{
                return 1000.0f/glm::clamp<float>(gVrModSettings->uiTargetFPS, 0.1f, 144.0f);
            });
            slos::pollIntervals();
            if (slos::is_mouselook()) nunja::ctx.desktop.pause();
            else nunja::ctx.desktop.resume();
            nunja::ctx.desktop.poll();
        }

        HandleVRInput();
    }

    float _getEyeVerticalFOV(int eye) {
        if (gVrModSettings->flipEyes) eye = -1 * eye;
        return ctx._session->getProjectionBounds((xvr::Eye)eye).fullFOV().y;
    }
    void _syncEyeToViewer(int eye) {
        slos::Camera::set_pose(ctx.zup.agentCameraIntegrated * nunja::getEyeToHeadTransform(eye));
        slos::Camera::set_perspective(_getEyeVerticalFOV(eye), gVrModSettings->nearClip, gVrModSettings->farClip);
    }
    
    glm::mat4 getEyeToHeadTransform(int eye) {
        if (gVrModSettings->flipEyes) eye = -1 * eye;
        if (eye == 0) return glm::mat4(1);
        glm::mat4 yup_eyeToHead = ctx._session->getEyeToHeadTransform((xvr::Eye)eye);
        glm::float32 ipdScale = glm::clamp<glm::float32>(gVrModSettings->ipdScale, 0.001f, 10.0f);
        if (ipdScale > 0.0f && ipdScale) {
            yup_eyeToHead[3][0] *= ipdScale;
        }
        return HMDMath::yup_to_zup(yup_eyeToHead);
    }

    glm::mat4 getEyePerspective(int eye, float z_near, float z_far) {
        if (gVrModSettings->flipEyes) eye = -1 * eye;
        return nunja::ctx._session->getEyePerspectiveMatrix((xvr::Eye)eye, z_near, z_far);
    }

    glm::mat4 getHeadFloorPose() {
        return ctx.zup.headPose;
    }

    void BeginEye(int eye) {
        if (!ctx._session || !ctx.tabbed) return;
        ctx.activeEye = resolveEyeBuffer(eye);
        if (!ctx.activeEye) { fprintf(stdout, "  Bad Eye %d\n", eye); fflush(stdout); return; }
        _syncEyeToViewer(eye);
        ctx.activeEye->state = EyeBuffer::State::framed;
    }
    
    void RenderUIHMDOverlays(float scale);
    void RenderVirtualDesktopHMDOverlays(float scale, glm::ivec2, uint32_t);
    void RenderHMDRaypickOverlays(glm::ivec2 const& mpos);

    void EndFrame(uint32_t frame) {
        ctx.activeEye = nullptr;

        if (asdf::pending) {
            slos::_save("ctx.leftEye.jpg", ctx.leftEye.rt.fboID());
            slos::_saveDepth("ctx.leftEye.depth.png", ctx.leftEye.rt.fboID());
        }

        if (!slos::is_mouselook()) if (getenv("UI")) {
            float scale = glm::clamp(gVrModSettings->_C.x ? gVrModSettings->_C.x : 1.0f, 0.1f, 10.0f);
            RenderUIHMDOverlays(scale);
            auto tex = ctx.desktop.acquire();
            if (tex.valid) RenderVirtualDesktopHMDOverlays(scale, tex.size, tex.glTextureId);
        }

        {
            if (get_world_eye()) {
                // leave "dominant" (world or UI) eye on stack for next frame's "picking" etc.
                _syncEyeToViewer(get_world_eye());
            }
        }

        ctx._session->submit(xvr::Eye::Left, ctx.leftEye.rt.texID(), xvr::Eye::Right, ctx.rightEye.rt.texID());

        bool showSBS = (slos::get_window_focus() || slos::is_modifier_active(slos::KEY_NUMLOCK)) && gVrModSettings->sbsDebug;
        if (showSBS) {
            slos::gl_blit_side_by_side(ctx.sourceResolution, slos::get_window_size(), ctx.leftEye.rt.fboID(), ctx.rightEye.rt.fboID());
        } else {
            slos::gl_blit_functor(ctx.sourceResolution, ctx.ui._size, []{
                gGL.color4f(1.f,1.f,1.f,1.f);
                ctx.ui.blit(ctx.sourceResolution);
            });
        }

        static std::string resolutions;
        if (frame == 2) {
            resolutions = std::format(
                "{} -- os.desktop\n"
                "{} -- HMD.recommended\n"
                "{} -- app.window\n"
                "{} -- app.pipeline\n",
                "{} -- ctx.sourceResolution\n"
                "{}",
                glm::to_string(glm::ivec2(win32::getCursorRange())),
                glm::to_string(glm::ivec2(ctx._session->recommendedRenderTargetSize())),
                glm::to_string(glm::ivec2(slos::get_window_size())),
                glm::to_string(glm::ivec2(slos::get_pipeline_size())),
                glm::to_string(glm::ivec2(ctx.sourceResolution)),
                ""
                // glm::to_string(ctx.targetResolution),
            );
        }

        slos::gl_withUpper(slos::get_window_size(), []{
            slos::gl_withFBODrawBinder(0, slos::get_window_size(), []{
                slos::gl_render_ui_screen_text_multiline(resolutions + "\ndiagnostics:\n" + ctx.diagnostics, { 0, 0 } , { 1, 1, 1, 0.5 }, true);
            });
        });

        ctx.leftEye.state = EyeBuffer::State::idle;
        ctx.rightEye.state = EyeBuffer::State::idle;

        if (asdf::pending) {
            asdf::pending = false;
            slos::_save("ctx.rightEye.jpg", ctx.rightEye.rt.fboID());
            slos::_saveDepth("ctx.rightEye.depth.png", ctx.rightEye.rt.fboID());
            ctx.ui.save("ctx.ui.rgba.png");
            ctx.ui.saveDepth("ctx.ui.depth.png");
        }

        if (frame % 900 == 0) fprintf(stdout, "--EndFrame[%d]...\n", frame);
    }
    bool _get_ui_cursor_hit(glm::ivec2 const& mpos) {
        auto hwnd = slos::__get_window_hwnd();
        if (hwnd != win32::getCursorHWND(hwnd)) return false; // over some other window / out of bounds
        glm::ivec3 status{0};
        glm::vec4 rgba{NAN};
        if (auto cur = ctx.ui._cursor) {
            status = cur->getCursorGL();
            if (glm::ivec2(status) == mpos) cur->read(rgba);
        }
        return !((status.z & GL_COLOR_BUFFER_BIT) && rgba[3] <= 0.01f);
    }
    void EndEye(int _eye) {
        if (!ctx._session || !ctx.tabbed) return;
        auto target = ctx.activeEye;
        ctx.activeEye = nullptr;
        if (!target || target->which != _eye) {
            fprintf(stdout, "  EndEye(%d) activeEye = %d\n", _eye, target ? +target->which : 0);fflush(stdout);
            return;
        }

        _syncEyeToViewer(+target->which);
        slos::gl_withFBODrawBinder(slos::get_pipeline_fbo_name(), slos::get_pipeline_size(), [&]{
            slos::gl_render_all_hud_texts();
            if (slos::is_mouselook() || !nunja::_get_ui_cursor_hit(ctx.agentMouseGL)) RenderHMDRaypickOverlays(ctx.agentMouseGL);

            auto tex = ctx.desktop.acquire();
            slos::tempDebugRender(tex.glTextureId, []{
                gGL.color4f(1.f,1.f,1.f,1.f);
                ctx.ui.blit({1,1});
            });

            if (1) { //getenv("POSE")) {
                if (auto pt = slos::ht::overriding_player_telemetry(gAgentAvatarp)) {
                    slos::ht::PlayerTelemetry* aptr = pt.get();
                    slos::ht::PlayerTelemetry* bptr = &ctx.zup.player;
                    hmdmath::api.pose(aptr, ctx.zup.player.bind.get());
                    *bptr = *aptr;
                }
            }
            
            slos::gl_render_world_text("get_agent_world_position", slos::get_agent_world_position(), glm::vec4(.5,.5,1,1));
            // slos::gl_render_world_text("L", slos::get_agent_world_position() + glm::vec3(ctx.zup.leftHandPose[3]), glm::vec4(1,.5,.5,1));
            auto drawVRcrap = [](glm::vec3 const& origin) {
                std::vector<slos::xx::WorldTransforms> tmp{};
                tmp.emplace_back("mRoot", glm::vec4{1,1,1,1}, ctx.zup.player.mRoot);
                tmp.emplace_back("mPelvis", glm::vec4{1,1,1,1}, ctx.zup.player.mPelvis);
                tmp.emplace_back("crmath::mHead", glm::vec4{1,1,1,1}, crmath::warm.calc_a(ctx.zup.player.mHead));

                auto head = ctx.zup.player.mHead;
                head[3].x = hmdmath::api.add(head[3].x, head[3].x);
                tmp.emplace_back("hmdmath::mHead", glm::vec4{1,0,1,1}, head);

                // hmdmath::api.pose(&ctx.zup.player);


                tmp.emplace_back("bind.mHead", glm::vec4{.5,0,.5,1}, ctx.zup.player.bind->mHead);
                tmp.emplace_back("bind.mWristLeft", glm::vec4{.5,0,.5,1}, ctx.zup.player.bind->mWristLeft);
                tmp.emplace_back("bind.mWristRight", glm::vec4{.2,0,.2,1}, ctx.zup.player.bind->mWristRight);
                tmp.emplace_back("bind.mFootLeft", glm::vec4{.5,0,.5,1}, ctx.zup.player.bind->mFootLeft);
                tmp.emplace_back("bind.mFootRight", glm::vec4{.5,0,.5,1}, ctx.zup.player.bind->mFootRight);

                tmp.emplace_back("mHead", glm::vec4{1,1,1,1}, ctx.zup.player.mHead);
                tmp.emplace_back("mWristLeft", glm::vec4{1,1,1,1}, ctx.zup.player.mWristLeft);
                tmp.emplace_back("mWristRight", glm::vec4{1,1,1,1}, ctx.zup.player.mWristRight);
                tmp.emplace_back("mFootLeft", glm::vec4{1,1,1,1}, ctx.zup.player.mFootLeft);
                tmp.emplace_back("mFootRight", glm::vec4{1,1,1,1}, ctx.zup.player.mFootRight);

                slos::gl_render_world_transforms(slos::get_avatarself_pose_transform(), tmp);
                slos::gl_render_world_transforms(glm::mat4(1.0f), {
                    { "agent", {0,0,0,1}, slos::get_agent_world_transform() },
                    { "avatar.world", {0,0,0,1}, slos::get_avatarself_world_transform() },
                    { "avatar.pose", {0,0,0,1}, slos::get_avatarself_pose_transform() },
                });
                // slos::gl_render_world_line_segments(origin, {
                //     { 
                //         glm::vec3(ctx.zup.leftHandPose[3]),
                //         glm::vec3(ctx.zup.leftHandPose[3]) + glm::vec3(ctx.zup.leftHandPose[0]),
                //         glm::vec4(1,0,0,1)
                //     },
                // });
                slos::gl_render_world_texts(origin, {
                    { "(h)", glm::vec3(ctx.zup.headPose[3]), {0,0,0,1}, glm::vec3(ctx.zup.headPose[0]) },
                    { "(l)", glm::vec3(ctx.zup.leftHandPose[3]), {1,0,0,1}, glm::vec3(ctx.zup.leftHandPose[0]) },
                    { "(r)", glm::vec3(ctx.zup.rightHandPose[3]), {0,1,0,1}, glm::vec3(ctx.zup.rightHandPose[0]) },
                });
                slos::gl_render_world_transforms(glm::translate(glm::mat4(1), origin) * glm::toMat4(glm::quat(nunja::cache::agentCamera)), {
                    { "(ach)", {0,0,0,1}, ctx.zup.headPose,     },
                    { "(acl)", {1,0,0,1}, ctx.zup.leftHandPose, },
                    { "(acr)", {0,1,0,1}, ctx.zup.rightHandPose },
                });
                slos::gl_render_world_transforms(glm::translate(glm::mat4(1), origin), {
                    { "(h)", {0,0,0,1}, ctx.zup.headPose,     },
                    { "(l)", {1,0,0,1}, ctx.zup.leftHandPose, },
                    { "(r)", {0,1,0,1}, ctx.zup.rightHandPose },
                });
            };
            drawVRcrap(slos::get_agent_world_position());
            // static glm::vec3 wp = slos::get_agent_world_position();
            // if (wp != slos::get_agent_world_position()) drawVRcrap(wp);
            // slos::gl_render_world_text("R", slos::get_agent_world_position() + glm::vec3(ctx.zup.rightHandPose[3]), glm::vec4(.5,1,.5,1));
            // slos::gl_render_world_text("H", slos::get_agent_world_position() + glm::vec3(ctx.zup.headPose[3]), glm::vec4(.5,.5,1,1));


        });
        target->rt.capture();
        // {
        //     target->rt._prt->bindTarget();//glBindFramebuffer(GL_FRAMEBUFFER, .fboID());
        //     target->rt._prt->flush();//
        //     glBindFramebuffer(GL_FRAMEBUFFER, 0);
        // }

        target->state = EyeBuffer::State::captured;
     }

    void HandleViewerUI() {
        if (!ctx.hud) {
            ctx.hud.assign("Press CTRL+TAB to toggle VR.\nPress TAB to clear.");
        }
        {
            auto pose = IsStereo() ? nunja::cache::agentCamera : slos::Camera::get_pose();
            ctx.hud.sync(glm::vec3(pose[3]) + glm::vec3(pose[0]));
        }

        {
            static int lastwe = 0xff;
            if (lastwe != nunja::get_world_eye()) {
                lastwe = nunja::get_world_eye();
                fprintf(stdout, "WORLD EYE: %d\n", lastwe);
            }
            static int lastue = 0xff;
            if (lastue != _get_ui_eye()) {
                lastue = _get_ui_eye();
                fprintf(stdout, "UIE EYE: %d\n", lastue);
            }
        }

        // Simplified Edge Detection: Toggle only on the rising edge of the combo
        static std::string was_choord;
        static std::string choord;

        bool shift = slos::is_key_down(slos::KEY_SHIFT);
        bool ctrl = slos::is_key_down(slos::KEY_CONTROL);
        bool tab = slos::is_key_down(slos::KEY_TAB);
        bool esc = slos::is_key_down(slos::KEY_ESCAPE);
        bool f4 = slos::is_key_down(slos::KEY_F4);

        if (ctrl && tab) choord = "ctrl+tab";
        else if (tab) choord = "tab";
        else if (shift && esc) choord = "shift+esc";
        else if (f4) choord = "f4";
        else if (!was_choord.empty()) { was_choord.clear(); return; }
        else return;

        // require full key release between choords
        if (!was_choord.empty()) return;

        fprintf(stdout, "choord: %s (was: %s)\n", choord.c_str(), was_choord.c_str()); fflush(stdout);
        was_choord = choord;

        if (choord == "ctrl+tab") {
            // --- 1. VR Toggle (CTRL+TAB) ---
            if (!ctx.ctrlTabbed) {
                ctx.ctrlTabbed = true;
                ctx.hud.clear();
                Startup();
            } else {
                ctx.ctrlTabbed = false;
                Shutdown();
                ctx.hud.assign("VR shutdown (ctrl+tab to reactivate; tab to this hide message)");
            }
        }
        else if (choord == "tab") {
            // --- 2. Recenter (TAB only) ---
            if (ctx._session) {
                // If VR is running, Tab toggles tabbed (pause/unpause) and recenters
                ctx.tabbed = !ctx.tabbed;
                if (ctx.tabbed) ctx.hud.clear();
                ctx.yup.tare = gVrModSettings->tareYaw ? HMDMath::get_yaw_cancellation(ctx.yup.hmdPose) : glm::mat4(1.0f);
                ctx.pFirstEye = &ctx.leftEye;
                ctx.pSecondEye = &ctx.rightEye;

                if (!ctx.tabbed) slos::ht::clear_player_telemetry(gAgentAvatarp);
                // if (auto control = gSavedSettings.getControl("vrmod.SWIZZLE")) {
                //     control->setComment(glm::to_string(gVrModSettings->_SWIZZLE()));
                // }
            } else {
                // If VR is off, Tab hides the HUD message
                ctx.hud.clear();
            }
        } else if (choord == "shift+esc") {
            asdf::pending = true;
            std::string s = xvr::docapture(ctx._session, ctx.yup.hmdPose, glm::mat4(1)/*ctx.zup.slCamera*/, ctx.zup.headPose, "");
            // fprintf(stdout, "%s\n", s.c_str());fflush(stdout);
            FSCommon::report_to_nearby_chat(s);
        } else if (choord == "f4") {
            FSCommon::report_to_nearby_chat(llformat("baking re-enabled: %d", slos::reenableBaking()));
        }
    }

    void HandleVRInput() {
        if (!ctx._session) return;
        
        std::string events;

        // xopenvr event support is currently limited...
        while (auto const& evt = ctx._session->acceptPendingEvent()) {
            switch(evt.type) {
                case xvr::Event::IpdChanged: {
                    events += std::format("\nxvr::VREvent_IpdChanged {:f}", evt.val<glm::float32>());
                    break;
                }
                case xvr::Event::DeviceActivated: {
                    auto device = evt.val<xvr::Device>();
                    if (device != xvr::INVALID) fprintf(stdout, "acceptPendingEvent xvr::Event::DeviceActivated[%d] %s %s\n",  (int)device , evt.name(), ctx._session->get_device_role_name(device));
                    break;
                }
                case xvr::Event::CompositorReset: {
                    auto sz = ctx._session->recommendedRenderTargetSize();
                    fprintf(stdout, "%s [%dx%d]", evt.name(), sz.x, sz.y);
                    break;
                }
                case xvr::Event::Quit: {
                    events += "\nvr::VREvent_Quit";
                    fprintf(stdout, "xvr::VREvent_Quit %p\n", ctx._session.get()); fflush(stdout);
                    ctx._session->AcknowledgeQuit_Exiting();
                    fprintf(stdout, "xvr::VREvent_Quit %p\n", ctx._session.get()); fflush(stdout);
                    ctx.ctrlTabbed = false;
                    fprintf(stdout, "xvr::VREvent_Quit %p\n", ctx._session.get()); fflush(stdout);
                    Shutdown();
                    fprintf(stdout, "xvr::VREvent_Quit %p\n", ctx._session.get()); fflush(stdout);
                    break;
                }
                default: break;
            }
            // fall back to reaching into underlying openvr driver level stuff
            events += evt.probes();
            if (!ctx._session) break; // exit loop
        } // while loop

        if (!events.empty()) Log(events);
    }
    


    namespace cache {
        glm::mat4& agentCamera = ctx.zup.agentCamera;
        glm::mat4& agentCameraRoomScale = ctx.zup.agentCameraRoomScale;
        glm::mat4& agentCameraIntegrated = ctx.zup.agentCameraIntegrated;
    }
    bool IsRoomScale() { return gVrModSettings->roomScale; }
} // namespace nunja

namespace xvr {
    std::string docapture(xvr::Session session, glm::mat4 hmdPose, glm::mat4 slCamera, glm::mat4 integratedCamera, std::string filename_fmt = "LL.%d.json") {
            // static LLFrameTimer t;
            static int c = 0;
            c++;
            std::string prefix = llformat("capture-%d-", c);
            // auto& viewer = LLViewerCamera::instance();
            // Probe::Instance().Record(llformat("capture-%d-LLViewerCamera-getOrigin", c), viewer.getOrigin());
            // Probe::Instance().Record(llformat("capture-%d-LLViewerCamera-getQuaternion", c), viewer.getQuaternion());
            
            Probe::Instance().Record(prefix+"RenderResolutionMultiplier", gSavedSettings.getF32("RenderResolutionMultiplier"));
            Probe::Instance().Record(prefix+"UIScaleFactor", gSavedSettings.getF32("UIScaleFactor"));
            
            Probe::Instance().Record(prefix+"LLViewerCamera::getInstance()->getViewHeightInPixels()", LLViewerCamera::getInstance()->getViewHeightInPixels());
            Probe::Instance().Record(prefix+"LLRender::sUIGLScaleFactor", LLRender::sUIGLScaleFactor);
            Probe::Instance().Record(prefix+"LLUI::getScaleFactor", LLUI::getScaleFactor());
            Probe::Instance().Record(prefix+"mWindow->getSystemUISize()", gViewerWindow->getWindow()->getSystemUISize());
            Probe::Instance().Record(prefix+"mWindow->getPixelAspectRatio()", gViewerWindow->getWindow()->getPixelAspectRatio());
            Probe::Instance().Record(prefix+"getDisplayScale()", gViewerWindow->getDisplayScale());
            Probe::Instance().Record(prefix+"getWorldViewHeightRaw()", gViewerWindow->getWorldViewHeightRaw());
            Probe::Instance().Record(prefix+"getWorldViewHeightScaled()", gViewerWindow->getWorldViewHeightScaled());
            Probe::Instance().Record(prefix+"getWindowWidthRaw()", gViewerWindow->getWindowWidthRaw());
            Probe::Instance().Record(prefix+"getWindowWidthScaled()", gViewerWindow->getWindowWidthScaled());
            // Probe::Instance().Record(prefix+"slos::get_mUIScreen_size()", slos::get_mUIScreen_size());
            // Probe::Instance().Record(prefix+"slos::get_worldview_size()", slos::get_worldview_size());
            // Probe::Instance().Record(prefix+"slos::get_window_size()", slos::get_window_size());
            // Probe::Instance().Record(prefix+"slos::__get_window_size()", slos::__get_window_size());
            Probe::Instance().Record(prefix+"RenderHiDPI", gSavedSettings.getF32("RenderHiDPI"));
            Probe::Instance().Record(prefix+"FontScreenDPI", gSavedSettings.getF32("FontScreenDPI"));

            Probe::Instance().Record(prefix+"LLViewerCamera", slos::Camera::get_pose());

            if (session) {
                glm::mat4 eyeToHead = session->getEyeToHeadTransform((xvr::Eye)-1);
                glm::mat4 hmdPose = session->getDevicePose(xvr::HMD);
                Probe::Instance().Record(prefix+"-app-getDevicePose(xvr::HMD)", hmdPose);
                Probe::Instance().Record(prefix+"-app-getEyeToHeadTransform(left)", eyeToHead);
            }

            Probe::Instance().Record(prefix+"hmdPose", hmdPose);
            Probe::Instance().Record(prefix+"ctx.zup.headPose", nunja::ctx.zup.headPose);
            Probe::Instance().Record(prefix+"ctx.zup.agentCamera", nunja::ctx.zup.agentCamera);
            Probe::Instance().Record(prefix+"ctx.zup.agentCameraRoomScale", nunja::ctx.zup.agentCameraRoomScale);
            Probe::Instance().Record(prefix+"ctx.zup.agentCameraIntegrated", nunja::ctx.zup.agentCameraIntegrated);
            // Probe::Instance().Record(prefix+"asdf::SWIZZLE", glm::vec4(asdf::SWIZZLE()));
            // Probe::Instance().Record(prefix+"slCamera", slCamera);
            // Probe::Instance().Record(prefix+"fused", integratedCamera);
            
            float sl_avatar_height = slos::getGroundToHeadDistance();
            Probe::Instance().Record(prefix+"sl_avatar_height", sl_avatar_height);


            Probe::Instance().Record(prefix+"hmd-aspect", (float)nunja::ctx.targetResolution.x / nunja::ctx.targetResolution.y);
            Probe::Instance().Record(prefix+"window-aspect", (float)slos::get_window_size().x / slos::get_window_size().y);
            Probe::Instance().Record(prefix+"worldview-aspect", (float)slos::get_worldview_size().x / slos::get_worldview_size().y);
            Probe::Instance().Record(prefix+"slos::get_pipeline_size()", slos::get_pipeline_size());
            Probe::Instance().Record(prefix+"hmd-size", glm::vec2(nunja::ctx.targetResolution));
            Probe::Instance().Record(prefix+"get_worldview_size", slos::get_worldview_size());
            Probe::Instance().Record(prefix+"get_window_size", slos::get_window_size());
            Probe::Instance().Record(prefix+"__get_window_size", slos::__get_window_size());
            Probe::Instance().Record(prefix+"getGroundToEyesDistance", slos::getGroundToEyesDistance());
            Probe::Instance().Record(prefix+"getGroundToHeadDistance", slos::getGroundToHeadDistance());

            if (!filename_fmt.empty()) Probe::Instance().Save(llformat(filename_fmt.c_str(), c));
            else return Probe::Instance().toString();
            return "";
    }
}

namespace nunja {


    std::pair<std::string,bool> get_ui_cursor_status(glm::ivec2 const& mpos) {
        glm::ivec3 status{0};
        glm::vec4 rgba{NAN};
        glm::float32 depth{NAN};
        if (auto cur = ctx.ui._cursor) {
            // cur->setCursor(mpos);
            status = cur->getCursor();
            cur->read(rgba);
            cur->read(depth);
        }
        return { "ui:\n " +
                glm::to_string(status) + "\n " +
                glm::to_string(rgba) + "\n " +
                std::to_string(depth),
                !((status.z & GL_COLOR_BUFFER_BIT) && rgba[3] <= 0.01f) };
    }

    void RenderHMDRaypickOverlays(glm::ivec2 const& mpos) {
        LLGLSLShader* shader = LLGLSLShader::sCurBoundShaderPtr;
        LLGLDisable no_depth(GL_DEPTH_TEST);
        LLGLDisable no_cull(GL_CULL_FACE);
        LLGLDisable noclamp(GL_DEPTH_CLAMP);
        LLGLDisable noscissor(GL_SCISSOR_TEST);
        // LLGLDisable noblend(GL_BLEND);
        LLGLEnable withblend(GL_BLEND);
        gUIProgram.bind();  
        gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
        gGL.color4f(1.f, 1.f, 1.f, 1.f);
        gGL.pushMatrix();
        glm::vec2 eye_extents = ctx.sourceResolution;
        glm::vec2 window_size = slos::get_window_size();
        // Get camera position and mouse direction  
        // LLVector3 cam_pos = LLViewerCamera::getInstance()->getOrigin();  
        LLVector3 cam_pos  = LLVector3(nunja::cache::agentCameraIntegrated[3]);//
        LLVector3 mouse_dir = gViewerWindow->mouseDirectionGlobal(mpos.x, mpos.y);  

        {
            auto pick = gViewerWindow->pickImmediate(mpos.x, mpos.y, false /*pick_transparent*/, false /*pick_rigged*/, false /*pick_particle*/, true /*pick_unselectable*/, false /*pick_reflection_probe*/);
            gGL.setLineWidth(4.0f);
            gGL.flush();
            gGL.begin(LLRender::LINES);
            {
                if (pick.mObjectID.notNull()) {  
                    gGL.color4f(0.f, 0.f, 0.f, 0.f);
                    gGL.vertex3fv( cam_pos.mV );
                    gGL.color4f(1.f, 1.f, 1.f, 0.75f);
                    gGL.vertex3fv( pick.mIntersection.mV );
                } else {
                    auto blah = (cam_pos + mouse_dir);
                    gGL.color4f(0.f, 0.f, 0.f, 0.f);
                    gGL.vertex3fv( cam_pos.mV );
                    gGL.color4f(0.f, 1.f, 0.f, .75);
                    gGL.vertex3fv( blah.mV );
                }
            }

if (gVrModSettings->handlasers)
{
    // --------------------------------------------------------------------------
    // STEP 1: Reconstruct the "Room Basis" (The Matrix you built in Camera Code)
    // --------------------------------------------------------------------------

    // Get the Anchor (which includes the "Feet" height fix you applied earlier)
    glm::mat4 anchor = nunja::cache::agentCameraRoomScale;

    // 1. Re-derive the Flat Forward/Left/Up vectors (Exact copy of your camera logic)
    //    This ensures the "Forward" direction for the hand matches the "Forward" for the head.
    glm::vec3 flat_up   = glm::vec3(0.0f, 0.0f, 1.0f); // World Up
    glm::vec3 flat_fwd  = glm::normalize(glm::vec3(anchor[0].x, anchor[0].y, 0.0f));
    
    // Safety check: if looking straight down (gimbal lock), default to X-fwd
    if (glm::length(glm::vec3(anchor[0].x, anchor[0].y, 0.0f)) < 0.001f) flat_fwd = glm::vec3(1.0f, 0.0f, 0.0f);

    // Re-calculate Left (assuming Right-Handed coordinate system: Cross(Up, Fwd) = Left? Check your math)
    // In your code: Left = normalize(vec3(anchor[1].x, anchor[1].y...)). 
    // Let's rely on the Cross Product to ensure it is orthogonal 90 degrees.
    glm::vec3 flat_left = glm::normalize(glm::cross(flat_up, flat_fwd)); 

    // --------------------------------------------------------------------------
    // STEP 2: Transform the Hand (Manually, component by component)
    // --------------------------------------------------------------------------

    // Get Raw Hand Data (Physical Space)
    auto handPoseRaw = ctx.zup.rightHandPose;
    glm::vec3 handPhysPos = glm::vec3(handPoseRaw[3]);
    glm::quat handPhysRot = glm::quat(handPoseRaw);

    // 1. Position: Rotate the physical offset by our Flat Basis, then add to Anchor
    //    This is the exact same math used in 'glm::vec3 world_offset = ...' in your camera code.
    glm::vec3 worldPos = glm::vec3(anchor[3]) + 
                         (flat_fwd  * handPhysPos.x) + 
                         (flat_left * handPhysPos.y) + 
                         (flat_up   * handPhysPos.z);

    // 2. Rotation: Create a quaternion for the Flat Basis and combine with Hand Rotation
    glm::mat3 basisMat(flat_fwd, flat_left, flat_up);
    glm::quat basisRot = glm::quat(basisMat);
    glm::quat worldRot = basisRot * handPhysRot;

    // --------------------------------------------------------------------------
    // STEP 3: Render
    // --------------------------------------------------------------------------
    
    // Build the final ModelView matrix for the hand
    glm::mat4 handToWorld = glm::toMat4(worldRot);
    handToWorld[3] = glm::vec4(worldPos, 1.0f);

    // Visualization Config
    glm::vec3 localLaserEnd = glm::vec3(10.0f, 0.0f, 0.0f); // 10m Laser along X (Adjust if fwd is -Z)
    glm::vec3 localOrigin   = glm::vec3(0.0f);

    // Draw Laser (MAGENTA)
    gGL.color4f(1.0f, 0.0f, 1.0f, 0.8f);
    glm::vec3 pStart = glm::vec3(handToWorld * glm::vec4(localOrigin, 1.0f));
    glm::vec3 pEnd   = glm::vec3(handToWorld * glm::vec4(localLaserEnd, 1.0f));
    gGL.vertex3fv(glm::value_ptr(pStart));
    gGL.vertex3fv(glm::value_ptr(pEnd));

    // Draw Tripod (RGB = XYZ)
    // X - Red
    gGL.color4f(1.0f, 0.0f, 0.0f, 1.0f);
    gGL.vertex3fv(glm::value_ptr(pStart));
    gGL.vertex3fv(glm::value_ptr(glm::vec3(handToWorld * glm::vec4(0.2f,0,0,1))));
    // Y - Green
    gGL.color4f(0.0f, 1.0f, 0.0f, 1.0f);
    gGL.vertex3fv(glm::value_ptr(pStart));
    gGL.vertex3fv(glm::value_ptr(glm::vec3(handToWorld * glm::vec4(0,0.2f,0,1))));
    // Z - Blue
    gGL.color4f(0.0f, 0.0f, 1.0f, 1.0f);
    gGL.vertex3fv(glm::value_ptr(pStart));
    gGL.vertex3fv(glm::value_ptr(glm::vec3(handToWorld * glm::vec4(0,0,0.2f,1))));
}
            if (0 && gVrModSettings->handlasers)
            {
                gGL.multMatrix(glm::value_ptr(nunja::cache::agentCameraRoomScale));
                // gGL.color4f(0.f, 1.f, 0.f, .75);
                // gGL.vertex3fv( glm::value_ptr(ctx.zup.headPose[3]) );
                // gGL.vertex3fv( glm::value_ptr(glm::vec3(ctx.zup.headPose[3])+glm::normalize(glm::vec3(ctx.zup.headPose[0]))) );

                    gGL.color4f(1.f, 0.f, 0.f, .25);
                    gGL.vertex3fv( glm::value_ptr(ctx.zup.leftHandPose[3]) );
                    gGL.color4f(1.f, 0.f, 0.f, .75);
                    gGL.vertex3fv( glm::value_ptr(glm::vec3(ctx.zup.leftHandPose[3])+glm::normalize(glm::vec3(ctx.zup.leftHandPose[0]))) );

                    auto before = glm::inverse(nunja::cache::agentCameraRoomScale) * nunja::cache::agentCameraIntegrated;
                    auto after = glm::mat4(1);
                    auto rightHandPose = ctx.zup.rightHandPose;
                    gGL.color4f(0.f, 0.f, 1.f, .25);
                    gGL.vertex3fv( glm::value_ptr(rightHandPose[3]) );
                    gGL.color4f(0.f, 0.f, 1.f, .75);
                    gGL.vertex3fv( glm::value_ptr(glm::vec3(rightHandPose[3])+glm::normalize(glm::vec3(rightHandPose[0]))) );

                    gGL.color4f(0.f, 1.f, 1.f, .25);
                    gGL.vertex3fv( glm::value_ptr(rightHandPose[3]) );
                    gGL.color4f(0.f, 1.f, 1.f, .75);
                    gGL.vertex3fv( glm::value_ptr(glm::vec3(rightHandPose[3])+glm::normalize(glm::vec3(rightHandPose[1]))) );

                    gGL.color4f(1.f, 1.f, 0.f, .25);
                    gGL.vertex3fv( glm::value_ptr(rightHandPose[3]) );
                    gGL.color4f(1.f, 1.f, 0.f, .75);
                    gGL.vertex3fv( glm::value_ptr(glm::vec3(rightHandPose[3])+glm::normalize(glm::vec3(rightHandPose[2]))) );

            }
            gGL.end();
            gGL.setLineWidth(1.0f);

            // gl_line_3d(cam_pos, pick.mIntersection, LLColor4(1.f, 1.f, 1.f, 1.f));  
            // gl_line_3d(cam_pos, pick.mIntersection, LLColor4(1.f, 0.f, 0.f, 1.f));  
        }
        gGL.flush();
        gUIProgram.unbind();
        gGL.popMatrix();
        if (shader) shader->bind();
    }

    void RenderUIHMDOverlays(float scale) {
        auto hwnd = slos::__get_window_hwnd();
        glm::vec2 mpos = win32::getCursorInfo(hwnd);//slos::get_mouse_pos(true);
        auto kv = get_ui_cursor_status(mpos);
        auto render_ui_cursor = kv.second;
        auto cursor_status = kv.first;

        glm::vec2 eye_extents = ctx.sourceResolution;
        glm::vec2 window_size = slos::get_window_size();
        glm::vec3 offset = gVrModSettings->uiOffset;

        bool mousecursor = gVrModSettings->mousecursor;
        
        scale = glm::clamp(scale, 0.1f, 10.0f);
        
        const int UI_CURSOR = 1;

        slos::gl_withUpper(eye_extents, [&]{
            for (auto& eye : { &ctx.leftEye, &ctx.rightEye }) {
                if (!(_get_render_mask(+eye->which) & UI_CURSOR)) continue;

                slos::gl_withFBODrawBinder(eye->rt.fboID(), eye_extents, [&]{
                    // auto col = _cursor_color(slos::get_window_cursor_type(), eye->which == FirstEye() ? LLColor4::red : eye->which == SecondEye() ? LLColor4::blue : LLColor4::white);
                    
                    // 1. Calculate Target Center (The Fixed Point on the HMD Screen)
                    float barn_door_close_dir = +eye->which;
                    glm::vec2 stereo_offset = glm::vec2(
                        HMDMath::_get_optical_shift(eye_extents, nunja::getEyeToHeadTransform(+eye->which), nunja::getEyePerspective(+eye->which, 0.001f, 10.0f), offset.z) + offset.x * barn_door_close_dir, 
                        offset.y
                    );
                    glm::vec2 target_center = (eye_extents / 2.0f) + stereo_offset;

                    // 2. Calculate "Sliding" Margin
                    // We need the vector from the Texture Origin -> Mouse Position (in Screen Pixels)
                    
                    // A. Convert Mouse to GL coords (Flip Y)
                    float mouse_gl_y = (window_size.y - 1) - mpos.y;
                    glm::vec2 mouse_gl = glm::vec2(mpos.x, mouse_gl_y);
                    // B. Determine the total size of the UI texture as it appears on screen
                    // The UI blit normally fills 'eye_extents', then we apply 'scale'.
                    glm::vec2 final_screen_size = eye_extents * scale;
                    
                    // C. Calculate Mouse Offset within that screen rect
                    // Map Window Coords -> Normalized UV -> Screen Size
                    glm::vec2 mouse_uv = mouse_gl / window_size;
                    glm::vec2 mouse_offset_screen = mouse_uv * final_screen_size;
                    
                    // D. Solve for Margin (Texture Origin)
                    glm::vec2 margin = target_center - mouse_offset_screen;

                    // 3. Draw Sliding UI Texture
                    // We apply the calculated margin and the user's zoom scale
                    {
                        slos::xx::TS ts{ glm::vec3(margin, 0.0f), glm::vec3(scale, scale, 1.0f) };
                        if (hwnd != win32::getCursorHWND(hwnd)) {
                            gGL.color4f(.5f,.5f,.5f,.5f);
                            ctx.ui.blit(eye_extents);
                        } else {
                            gGL.color4f(1.f,1.f,1.f,1.f);
                            ctx.ui.blit(eye_extents);
                        }
                    }
                    
                    // 4. Draw Cursor (Fixed at Target Center)
                    if (mousecursor) {

                        // CRITICAL: Translation is locked to target_center.
                        // Scale is kept as eye_extents / window_size to preserve cursor aspect/size relative to resolution.
                        slos::xx::TS ts{ glm::vec3(target_center, 0.0f), glm::vec3(eye_extents / window_size, 1.0f) };
                        
                        if (render_ui_cursor) {
                            slos::gl_blit_cursor(slos::get_window_cursor_type());
                        }
                        // if (handlasers) {
                        //     // Handlasers also originate from the fixed center now
                        //     gl_triangle_2d(0, 0, 0 + 8, 0 - 15, 0 + 15, 0 - 8, LLColor4::black, TRUE);
                        //     gl_triangle_2d(0+2, 0-2, 0 + 9, 0 - 13, 0 + 12, 0 - 8, LLColor4::white, TRUE);
                        // }
                        slos::gl_render_ui_screen_text_multiline(cursor_status, {16,0}, { 1, 1, 1, 1 });
                    }
                });
            }
        });
    }
    void RenderVirtualDesktopHMDOverlays(float scale, glm::ivec2 tex_size, uint32_t glTextureId) {
        {
            auto hwnd = slos::__get_window_hwnd();
            static auto ptr = win32::getCursorHWND(hwnd);
            if (ptr != win32::getCursorHWND(hwnd)) {
                fprintf(stdout, "win32::getCursorHWND() = %p\n", ptr = win32::getCursorHWND(hwnd));
            }
        }
        auto hwnd = slos::__get_window_hwnd();
        // if (hwnd == win32::getCursorHWND(hwnd)) return;

        glm::vec2 eye_extents = ctx.sourceResolution;
        glm::vec3 offset = gVrModSettings->uiOffset;
        
        const int WORLD_CURSOR = 2;

        slos::gl_withUpper(eye_extents, [&]{
            for (auto& eye : { &ctx.leftEye, &ctx.rightEye }) {
                if (!(_get_render_mask(+eye->which) & WORLD_CURSOR)) continue;

                auto mpos = win32::getCursorInfo();
                auto window_size = win32::getCursorRange();
                    
                slos::gl_withFBODrawBinder(eye->rt.fboID(), eye_extents, [&]{
                    slos::gl_withWUpper([&]{

                        // 1. Calculate Target Center (Screen Center + Stereo Offset)
                        // The cursor will be LOCKED here. The world slides underneath it.
                        glm::vec2 eye_center = eye_extents / 2.0f;
                        float barn_door_close_dir = +eye->which; 
                        glm::vec2 stereo_offset = glm::vec2(
                            HMDMath::_get_optical_shift(eye_extents, nunja::getEyeToHeadTransform(+eye->which), nunja::getEyePerspective(+eye->which, 0.001f, 10.0f), offset.z) + offset.x * barn_door_close_dir, 
                            offset.y
                        );
                        glm::vec2 target_center = eye_center + stereo_offset;

                        // 2. Calculate "Sliding" Margin (Using verified Diagnostic Math)
                        // Mouse (Top-Left 0,0) -> GL (Bottom-Left 0,0) conversion
                        float mouse_gl_y = (window_size.y - 1) - mpos.y; 
                        glm::vec2 mouse_gl = glm::vec2(mpos.x, mouse_gl_y);
                        
                        // Calculate Render Sizes
                        glm::vec2 scaleFactors = glm::vec2(eye_extents) / glm::vec2(tex_size);
                        float render_scale = glm::min(scaleFactors.x, scaleFactors.y);
                        glm::vec2 renderSize = (glm::vec2(tex_size) * render_scale); // Base size before user-scale
                        glm::vec2 final_screen_size = renderSize * scale; // Final on-screen size
                        
                        // Calculate Mouse Offset within that screen rect
                        glm::vec2 mouse_uv = mouse_gl / glm::vec2(window_size);
                        glm::vec2 mouse_offset_screen = mouse_uv * final_screen_size;
                        
                        // Shift the texture origin so the Mouse Point lands on Target Center
                        glm::vec2 margin = target_center - mouse_offset_screen;

                        // 3. Draw Texture at the Calculated Margin
                        {
                            slos::xx::TS ts{ { margin, 0.0f }, { scale, scale, 1.0f } };
                            if (hwnd == win32::getCursorHWND(hwnd)) {
                                gGL.color4f(.5f,.5f,.5f,.5f);
                                slos::gl_blit_texture(glTextureId, renderSize);
                            } else {
                                gGL.color4f(1,1,1,1);
                                slos::gl_noblend_blit_texture(glTextureId, renderSize);
                            }

                        }

                        // 4. Draw Cursor Fixed at Target Center
                        // CRITICAL: We do NOT use 'mpos' for position here. The cursor is geometrically locked to the center.
                        {
                            slos::xx::TS ts{ { target_center, 0.0f }, { 4.0, 4.0, 1.0f } };
                            if (mpos.w & CURSOR_SHOWING)
                                slos::gl_blit_hcursor(mpos.z);
                            else
                                slos::gl_blit_cursor(ECursorType::UI_CURSOR_ARROW);
                            slos::gl_render_ui_screen_text_multiline(nunja::ctx.desktop.getStatisticsString(), {16,0}, { 1, 1, 1, 0.5 });
                        }
                        
                    });                    
                    
                });
            }
        });
    }
} //ns

#elif __INCLUDE_LEVEL__ == 0
    #error "TPVM_RECIPE: -xc++ -UUNICODE -DHUMBLETIM_VRMOD_IMPLEMENTATION"
#endif
