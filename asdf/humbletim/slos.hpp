#ifndef SLOS_H
#define SLOS_H

#include <string>
#include <cstdint> // uint32_t
#include <functional>
#include <memory>
#include <unordered_map>
// #include <type_traits>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/compatibility.hpp> // glm::isfinite

#ifdef GLM_FORCE_QUAT_DATA_WXYZ
    #error "GLM is configured for WXYZ layout, but LLQuaternion uses XYZW. Conversion will fail!"
#endif

// ----------------------------------------------------------------------------
// CLEAN HEADER (No LL dependencies allowed here)
// ----------------------------------------------------------------------------
namespace slos {
    float performance_now();
    int setInterval(std::function<void()>, std::function<int()>);
    bool clearInterval(int ms);
    int pollIntervals();

    void on_app_quitting(std::function<void()> const& cb);
    glm::vec3 get_agent_world_position();    
    glm::mat4 get_avatarself_world_transform();
    glm::mat4 get_avatarself_pose_transform();
    // glm::quat get_agent_world_orientation();    
    glm::mat4 get_agent_world_transform();    
    // Input & Window State
    bool is_mouselook();
    extern const uint8_t KEY_SHIFT, KEY_CONTROL, KEY_ESCAPE, KEY_F4, KEY_TAB, KEY_CAPSLOCK, KEY_NUMLOCK;
   
    bool is_modifier_active(uint32_t k);
    bool is_key_down(uint32_t k);

    glm::ivec4 get_viewport_extents();
    glm::ivec2 get_bound_fbo_size();

    glm::ivec2 get_pipeline_size();
    // glm::ivec2 get_mUIScreen_size();
    // glm::uint32 get_mUIScreen_fbo_name();
    bool is_ui_visible();
    bool get_pipeline_render_ui_flag();
    bool set_pipeline_render_ui_flag(bool enable);
    glm::uint32 get_pipeline_fbo_name();
    glm::ivec2 get_worldview_size();
    bool get_window_focus();
    glm::ivec2 get_window_size();
    glm::ivec2 get_window_size_raw();
    glm::ivec2 get_mouse_pos_gl(bool clamp = false);
    
    // Direct Window Operations (prefixed with __ to indicate lower-level access)
    intptr_t __get_window_hwnd();
    glm::ivec2 __get_mouse_pos();
    glm::ivec2 __get_window_size();
    void       __set_window_size(glm::ivec2 const& sz);
    bool __force_world_aspect_ratio(glm::ivec2 const& sz, float scale = 0.95f);

    // Avatar Metrics
    float getGroundToEyesDistance();
    float getGroundToHeadDistance();
    
    // Optics
    glm::vec2 get_worldview_tan_half_fov();

    // Camera Interface
    namespace Camera {
        glm::mat4 get_pose();
        void set_pose(const glm::mat4& integrated);
        void set_perspective(float fov_y, float z_near, float z_far);
        // void apply_overrides(float angle, float nearClip);
    }

    // Text Display Wrapper
    // Uses void* to hide LLHUDText* from the public header
    struct HudText {
        std::string buffer;
        void* hudNode = nullptr; 

        std::function<glm::vec3()> anchorfunctor;

        operator bool() const;
        void clear();
        void append(std::string const& text, int n = -1);
        void assign(std::string const& text, bool log = true);
        void hide();
        void show();
        void sync(glm::vec3 const& pos);
    };

    namespace ht {
        // If O is INFINITY, we do nothing (leaving uninitialized (modern GLM default))
        template <float O = INFINITY> struct magic_mat4 : glm::mat4 {
            using glm::mat4::mat4;
            using glm::mat4::operator=;
            constexpr magic_mat4() : glm::mat4() { if constexpr (O < INFINITY) *this = glm::mat4(O); }
        };
        template <float O = INFINITY> struct alignas(16) _PlayerTelemetry {
            using mat4 = magic_mat4<O>;
            mat4 worldToAvatar;
            mat4 mRoot, mPelvis, mHead, mWristLeft, mWristRight, mFootLeft, mFootRight;
            float Lipsync_Aah{O}, Lipsync_Ooh{O};
        };
        using PlayerTelemetry = _PlayerTelemetry<1.0f>;
        struct PlayerPose : PlayerTelemetry {
            std::shared_ptr<PlayerTelemetry> bind;
            mat4 pelvisFixup;
        };
        using UninitializedPlayerTelemetry = _PlayerTelemetry<>;
        PlayerTelemetry get_player_telemetry(void* avatarp);

        namespace blah {
            using TestPT = _PlayerTelemetry<1.0f>;
            // 1. Ensure it's Standard Layout (Safe for memcpy/C-style buffers)
            static_assert(std::is_standard_layout_v<_PlayerTelemetry<1.0f>>,  "Telemetry must be Standard Layout for GPU buffer compatibility!");
            // 2. Ensure no hidden padding or overhead was added by the wrapper
            static_assert(sizeof(_PlayerTelemetry<INFINITY>) == sizeof(glm::mat4) * 8 + 16, "Telemetry size mismatch! Wrapper added unexpected overhead.");
            // 3. Ensure it's still "Trivially Copyable" (Max performance for moves/copies)
            static_assert(std::is_trivially_copyable_v<_PlayerTelemetry<>>, "Telemetry is no longer trivially copyable!");
        }

    }

} // namespace slos

#endif // SLOS_H

// ----------------------------------------------------------------------------
// IMPLEMENTATION (LL headers and dirty logic go here)
// ----------------------------------------------------------------------------
#ifdef SLOS_IMPLEMENTATION

#ifdef _WIN32
    #include "llwindowwin32.h"
    #include "llkeyboardwin32.h"
#endif
#include "llagent.h"
#include "llagentcamera.h"
#include "llviewerwindow.h" 
#include "llviewercontrol.h"
#include "llviewercamera.h"
#include "lluiimage.h" 
#include "llhudtext.h"
#include "llvoavatarself.h" // extern bool isAgentAvatarValid();
#include "pipeline.h" // gPipeline
// Internal helper, not exposed in header
static LLHUDText* slos_make_hud_internal(std::string const& text) {
    auto hudNode = (LLHUDText *)LLHUDObject::addHUDObject(LLHUDObject::LL_HUD_TEXT);
    if (hudNode) {
        hudNode->setZCompare(FALSE);
        hudNode->setColor(LLColor4::white);
        hudNode->setMaxLines(-1);
    }
    return hudNode;
}

namespace slos {
    struct Interval {
      std::function<void()> callback;  
      std::function<int()> msfunctor;  
      LLFrameTimer timer{};
      static int N;
      static int poll();
    };
    int Interval::N = 0;
    std::unordered_map<int, std::shared_ptr<Interval>> intervals;
    int setInterval(std::function<void()> callback, std::function<int()> ms) {
        intervals[++Interval::N] = std::make_shared<Interval>(callback, ms);
        return Interval::N;
    }
    bool clearInterval(int id) {
        auto it = intervals.find(id);
        if (it == intervals.end()) return false;
        intervals.erase(it);
        return true;
    }
    int Interval::poll() {
        int n = 0;
        for (const auto& kv : intervals) {
            if (kv.second->timer.getElapsedTimeF32() * 1000.0f > kv.second->msfunctor() ) {
                n++;
                kv.second->timer.reset();
                kv.second->callback();
            }
        }
        return n;
    }
    int pollIntervals() { return Interval::poll(); }

    // struct Forever {
    //     std::shared_ptr<LLFrameTimer> periodic_timer;
    //     float targetFPS{ 1.0f };
    //     std::function<void()> callback;
    //     bool poll() {
    //         if (!periodic_timer || !callback) return false;
    //         float dt = 1.0f/glm::clamp<float>(targetFPS, 0.001f, 144.0f);
    //         if (periodic_timer->getElapsedTimeF32() < dt) return false;
    //         periodic_timer->reset();
    //         callback();
    //         return true;
    //     }
    //     Forever(std::function<void()> callback)

    float performance_now() {
        static LLFrameTimer t;
        return t.getElapsedTimeF32();
    }

    bool __force_world_aspect_ratio(glm::ivec2 const& resolution, float scale) {
        int maxH = GetSystemMetrics(SM_CYSCREEN);
        const glm::ivec2 win = slos::get_window_size();
        const glm::ivec2 wv = slos::get_worldview_size();
        static glm::ivec2 last;
        static float lastscale = 0.0f;
        if (last != win || lastscale != scale) {
            last = win;
            lastscale = scale;
            float aspect = (float)resolution.x / (float)resolution.y;
            if (resolution.y < resolution.x) aspect = 1.0f / aspect;

            glm::fvec2 proportional{ maxH * aspect, maxH };
            glm::ivec2 desiredSize = proportional * scale;
            if (slos::is_ui_visible()) {
                desiredSize.y += win.y - wv.y; // account for world view vs. window with menu bar etc.
            }
            glm::ivec2 currentSize = slos::__get_window_size();
            if (currentSize != desiredSize) {
                slos::__set_window_size(desiredSize);
                return true;
            }
        }
        return false;
    }

    bool is_ui_visible() {return gViewerWindow->getUIVisibility(); }

    glm::vec3 get_agent_world_position() {
        return glm::vec3(gAgent.getPositionAgent());
    }
    glm::mat4 get_avatarself_world_transform() {  
        if (isAgentAvatarValid()) {
            return glm::make_mat4((F32*)gAgentAvatarp->getRenderMatrix().mMatrix);
        }
        return get_agent_world_transform();
    }
    glm::mat4 get_avatarself_pose_transform() {  
        LLJoint* mPelvis = isAgentAvatarValid() ? gAgentAvatarp->getJoint("mPelvis") : nullptr;
        return mPelvis ? glm::make_mat4((F32*)mPelvis->getWorldMatrix().mMatrix) : get_avatarself_world_transform();
    }

    glm::mat4 get_agent_world_transform() {  
        // if (isAgentAvatarValid() && gAgentAvatarp->mRoot) {  
        //     gAgentAvatarp->mRoot->updateWorldMatrixParent();  
        //     const LLMatrix4& avatar_matrix = gAgentAvatarp->mRoot->getWorldMatrix();  
        //     return glm::make_mat4((F32*)avatar_matrix.mMatrix);  
        // }  
        // return glm::translate(glm::vec3(frame.getOrigin())) *   
        //     glm::toMat4(glm::make_quat(gAgent.getQuat().mQ));  
        auto const& frame = gAgent.getFrameAgent();
        glm::mat4 output;  
        frame.getOpenGLTransform(glm::value_ptr(output));  
        return glm::inverse(output);  
    }

    // glm::mat4 get_agent_world_transform() {
    //     if (isAgentAvatarValid()) {
    //         return glm::make_mat4((F32*)gAgentAvatarp->getRenderMatrix().mMatrix);
    //     }
    //     return glm::translate(glm::vec3(gAgent.getPositionAgent())) * glm::toMat4(glm::make_quat(gAgent.getQuat().mQ));
    // }

    bool is_mouselook() { 
        return gAgentCamera.getCameraMode() == CAMERA_MODE_MOUSELOOK; 
    }

    const uint8_t KEY_SHIFT{::KEY_SHIFT}, KEY_CONTROL{::KEY_CONTROL}, KEY_ESCAPE{::KEY_ESCAPE}, KEY_F4{::KEY_F4}, KEY_TAB{::KEY_TAB}, KEY_NUMLOCK(VK_NUMLOCK), KEY_CAPSLOCK(::KEY_CAPSLOCK);
    bool is_modifier_active(uint32_t k) {
        if (k == KEY_NUMLOCK) return GetKeyState(VK_NUMLOCK) & 1;
        return false;
    }
    bool is_key_down(uint32_t k) { 
        return gKeyboard->getKeyDown(k); 
    }

    glm::ivec2 get_worldview_size() { 
        return { gViewerWindow->getWorldViewWidthScaled(), gViewerWindow->getWorldViewHeightScaled() }; 
    }

    glm::ivec2 get_window_size() { 
        return { gViewerWindow->getWindowWidthScaled(), gViewerWindow->getWindowHeightScaled() }; 
    }
    glm::ivec2 get_window_size_raw() { 
        return { gViewerWindow->getWindowWidthRaw(), gViewerWindow->getWindowHeightRaw() }; 
    }

    glm::ivec2 get_mouse_pos_gl(bool clamp) { 
        auto mpos = gViewerWindow->getCurrentMouse(); 
        glm::vec2 result{ mpos.mX, mpos.mY };
        return clamp ? glm::clamp(result, glm::vec2(0), glm::vec2(slos::get_window_size())) : result;
    }

    bool get_window_focus() {
        return gFocusMgr.getAppHasFocus();
    }
    intptr_t __get_window_hwnd() {
        LLWindow * win = gViewerWindow->getWindow();
        return (intptr_t)win->getPlatformWindow();
    }
    glm::ivec2 __get_mouse_pos() {
        LLCoordWindow mpos{-1,-1};
        LLWindow * win = gViewerWindow->getWindow();
        win->getCursorPosition(&mpos);
        return { mpos.mX, mpos.mY };
    }

    glm::ivec2 __get_window_size() {
        LLCoordWindow currentSize{-1,-1};
        LLWindow * win = gViewerWindow->getWindow();
        win->getSize(&currentSize);
        return { currentSize.mX, currentSize.mY };
    }

    void __set_window_size(glm::ivec2 const& sz) {
        LLWindow * win = gViewerWindow->getWindow();
        win->setSize(LLCoordWindow{ sz.x, sz.y });
    }

    float getGroundToEyesDistance() {  
        if (!isAgentAvatarValid()) {  
            LL_WARNS() << "Agent avatar is not valid" << LL_ENDL;  
            return 0.0f;  
        }  
        // via_foaf helper assumed to be available in this scope or context
        F32 ground_to_eyes = via_foaf(gAgentAvatarp, { return mPelvisToFoot; }) + gAgentAvatarp->mHeadOffset.mV[VZ];  
        return ground_to_eyes;  
    }  
    
    float getGroundToHeadDistance() {  
        if (!isAgentAvatarValid()) {  
            LL_WARNS() << "Agent avatar is not valid" << LL_ENDL;  
            return 0.0f;  
        }  
        return gAgentAvatarp->mBodySize.mV[VZ] + gAgentAvatarp->mAvatarOffset.mV[VZ];  
    }  

    glm::vec2 get_worldview_tan_half_fov() {
        auto& view = LLViewerCamera::instance();
        float fovY = view.getView();
        float aspect = view.getAspect();
        float tY = tan(fovY / 2.0f); 
        return { tY * aspect, tY };
    }

    // --- Camera Namespace ---
    float get_camera_default_fov() {
        auto& view = LLViewerCamera::instance();
        return view.getDefaultFOV();
    }

    namespace Camera {
        glm::mat4 get_pose() {
            auto& view = LLViewerCamera::instance();
            auto slCamera = glm::toMat4(glm::make_quat(view.getQuaternion().mQ));
            slCamera[3] = glm::vec4(glm::vec3(view.getOrigin()), 1.0f);
            return slCamera;
        }


        void set_pose(const glm::mat4& integrated) {
            auto& view = LLViewerCamera::instance();
            view.setOrigin(LLVector3{ integrated[3] });
            auto axes = glm::mat3(integrated);
            view.setAxes(LLVector3{axes[0]}, LLVector3{axes[1]}, LLVector3{axes[2]});
        }

        void set_perspective(float fov_y, float z_near, float z_far) {
            auto& view = LLViewerCamera::instance();
            if (z_near <= 0.0f) z_near = MIN_NEAR_PLANE;
            if (z_far <= 0.0f) z_far = glm::min(gAgentCamera.mDrawDistance, gSavedSettings.getF32("RenderFarClip"));
            z_far = glm::clamp(z_far, 16.0f, MAX_FAR_CLIP*2.0f);
            z_near = glm::clamp(z_near, 0.0001f, z_far * .99f);
            // if (z_far != view.getFar()) fprintf(stdout, "z_far=%f, view.getFar()=%f\n", z_far, view.getFar());
            // if (z_near != view.getNear()) fprintf(stdout, "z_near=%f, view.getNear()=%f\n", z_near, view.getNear());

            if (fov_y != 0.0f && fov_y != view.getDefaultFOV()) view.setDefaultFOV(fov_y);
            auto vp = get_pipeline_size();
            // auto mWorldViewRectRaw = gViewerWindow->getWorldViewRectRaw();
            view.setPerspective(NOT_FOR_SELECTION, 0, 0, vp.x, vp.y, false, z_near, z_far);
            gViewerWindow->setup3DViewport();
        }

        // void apply_overrides(float angle, float nearClip) {
        //     auto& view = LLViewerCamera::instance();
        //     if (angle != 0.0f) view.setView(angle);
        //     if (nearClip > 0.0f) {
        //         static struct { F32 m[2]; F32 nearP; F32 farP; } proxy{};
        //         view.writeFrustumToBuffer((char*)&proxy);
        //         proxy.nearP = nearClip;
        //         view.readFrustumFromBuffer((char*)&proxy);
        //     }
        // }
    }

    // --- HudText Implementation ---
    
    HudText::operator bool() const { 
        return hudNode != nullptr; 
    }

    void HudText::clear() { 
        buffer.clear(); 
        hide(); 
    }

    void HudText::append(std::string const& text, int n) {
        buffer += text;
        if (n > 0 && buffer.length() > n) {
            size_t last_nl = buffer.find_last_of('\n', n - 1);
            if (last_nl != std::string::npos) buffer.resize(last_nl + 1); 
            else buffer.resize(n); 
        }
        assign(buffer, n <= 0);
    }

    void HudText::assign(std::string const& text, bool log) {
        buffer = text;
        if (log) fprintf(stdout, "hud:%s\n", buffer.c_str()); fflush(stdout);
        
        LLHUDText* node = static_cast<LLHUDText*>(hudNode);
        if (!node) {
            node = slos_make_hud_internal(buffer);
            hudNode = static_cast<void*>(node);
        }
        
        if (node) node->setString(buffer);
        show();
    }

    void HudText::hide() {
        LLHUDText* node = static_cast<LLHUDText*>(hudNode);
        if (!node) return;
        node->setDoFade(false);
        node->setHidden(true);
    }

    void HudText::show() {
        LLHUDText* node = static_cast<LLHUDText*>(hudNode);
        if (!node || buffer.empty()) return;
        node->setDoFade(false);
        node->setHidden(false);
    }

    void HudText::sync(glm::vec3 const& pos) {
        if (buffer.empty()) return;
        if (!hudNode) assign(buffer, true);
        LLHUDText* node = static_cast<LLHUDText*>(hudNode);
        if (!node) return;
        node->setPositionAgent(LLVector3{pos});
    }

    glm::ivec2 get_pipeline_size() { 
        return { gPipeline.mRT->screen.getWidth(), gPipeline.mRT->screen.getHeight() }; 
    }
    // glm::ivec2 get_mUIScreen_size() { 
    //     return { gPipeline.mUIScreen.getWidth(), gPipeline.mUIScreen.getHeight() }; 
    // }

    // Add this helper
    glm::uint32 get_pipeline_fbo_name() { return via_foaf(&gPipeline.mRT->screen, { return mFBO;}); }
    // glm::uint32 get_mUIScreen_fbo_name() { return via_foaf(&gPipeline.mUIScreen, { return mFBO;}); }
    glm::ivec4 get_viewport_extents() {
        glm::ivec4 viewport;
        glGetIntegerv(GL_VIEWPORT, glm::value_ptr(viewport));
        return viewport;
    }

    bool get_pipeline_render_ui_flag() { return gPipeline.hasRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_UI); }
    bool set_pipeline_render_ui_flag(bool enable) {
        bool prev_draw_ui = get_pipeline_render_ui_flag();
        if (prev_draw_ui != enable) {
            LLPipeline::setRenderDebugFeatureControl(LLPipeline::RENDER_DEBUG_FEATURE_UI, enable);
        }
        return prev_draw_ui;
    }

    glm::ivec2 get_bound_fbo_size() {
        // GLint width, height;
        // glGetFramebufferParameteriv(GL_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_WIDTH, &width);
        // glGetFramebufferParameteriv(GL_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_HEIGHT, &height);
        GLint objName = 0;
        // Get the ID (name) of the object
        glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, 
                                  GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &objName);
        GLint level=0, width=0, height=0;
        glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, 
                                              GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL, &level);

        // Bind to the correct target (e.g., GL_TEXTURE_2D)
        glBindTexture(GL_TEXTURE_2D, objName);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, level, GL_TEXTURE_WIDTH, &width);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, level, GL_TEXTURE_HEIGHT, &height);
        glBindTexture(GL_TEXTURE_2D, 0);
        return { width, height }; //
    }
} // namespace slos


namespace slos {
    void on_app_quitting(std::function<void()> const& cb) {
        // In your initialization code  
        LLEventPumps::instance().obtain("LLApp").listen(  
            "MyQuitListener",  
            [cb](const LLSD& event) {  
                std::string status = event["status"];  
                if (status != "running") {  
                    // Application is shutting down  
                    cb();  
                }  
                return false;  
            });    
    }
}

#include "fsposeranimator.h"
#include "lljointdata.h"
#include "llavatarappearance.h"

namespace slos {
    namespace ht {
        FSPosingMotion* poserAnimator_getPosingMotion(LLVOAvatar* avi) {
            if (avi) return nullptr;
            for (auto motion : avi->getMotionController().getActiveMotions()) {  
                FSPosingMotion* posingMotion = dynamic_cast<FSPosingMotion*>(motion);  
                if (posingMotion && !posingMotion->isStopped()) return posingMotion;
            }
            return nullptr;
        }
          
        LLJoint* get_true_true_joint(LLVOAvatar* avi, const char* jointName) {  
            if (!avi || !jointName) return nullptr;  
            // static FSPoserAnimator poserAnimator;  
            auto posingMotion = poserAnimator_getPosingMotion(avi);
            auto jointPose = posingMotion ? posingMotion->getJointPoseByJointName(jointName) : nullptr;
            return jointPose ? jointPose->getJointState()->getJoint() : avi->getJoint(jointName);
        }        
        LLVisualParam* get_param_weight(LLVOAvatar* avi, const char* name, float& weight) {
            auto vp = avi->getVisualParam(name);
            if (!vp) return nullptr;
            weight = vp->getWeight();
            return vp;
        }
        bool set_param_weight(LLVOAvatar* avi, const char* name, float const weight) {
            if (!glm::isfinite(weight)) return false;
            auto vp = avi->getVisualParam(name);
            if (!vp) return false;
            vp->setWeight(weight, false);
            return true;
        }
        LLJoint* get_joint_matrix(LLVOAvatar* avi, const char* name, glm::mat4& dest) {
            LLJoint* joint = avi ? avi->getJoint(name) : nullptr;//get_true_true_joint(avi, name);
            if (!joint) return nullptr;
            joint->updateWorldMatrixParent();  
            dest = glm::make_mat4((F32*)&joint->getWorldMatrix().mMatrix);
            return joint;
        }
        bool set_joint_matrix(LLVOAvatar* avi, const char* name, glm::mat4 const& src) {  
            if (!glm::isfinite(src[0][0])) return false;
            LLJoint* joint = avi ? avi->getJoint(name) : nullptr;  
            if (!joint) return false;  
            LLMatrix4 llmat(glm::value_ptr(src)); // or memcpy into mMatrix  
            joint->setWorldMatrix(llmat);  
            return true;  
        }

    // std::unordered_map<std::string, glm::mat4> oop(LLVOAvatar *avi) {        
    //     // Get raw skeleton info - this is the true source of truth  
    //     LLAvatarSkeletonInfo* skeleton_info = LLAvatarAppearance::sAvatarSkeletonInfo;  
    //     // LLAvatarSkeletonInfo* skeleton_info = avi->getSkeletonInfo();  
        
    //     // Extract bind poses directly from bone info (no joint objects)  
    //     std::unordered_map<std::string, glm::mat4> pelvis_relative_poses;  
    //     glm::mat4 pelvis_matrix = glm::mat4(1.0f);  
        
    //     // First pass: find pelvis and build hierarchy  
    //     std::function<void(LLAvatarBoneInfo*, const glm::mat4&)> extract_poses;  
    //     extract_poses = [&](LLAvatarBoneInfo* bone, const glm::mat4& parent_matrix) {  
    //         // Build local matrix WITHOUT applying scale (mPos is pre-scaled)  
    //         glm::mat4 local_matrix(1.0f);  
    //         local_matrix = glm::rotate(local_matrix, bone->mRot[0], glm::vec3(1, 0, 0));  
    //         local_matrix = glm::rotate(local_matrix, bone->mRot[1], glm::vec3(0, 1, 0));  
    //         local_matrix = glm::rotate(local_matrix, bone->mRot[2], glm::vec3(0, 0, 1));  
    //         local_matrix = glm::translate(local_matrix, glm::vec3(bone->mPos[0], bone->mPos[1], bone->mPos[2]));  
            
    //         glm::mat4 world_matrix = parent_matrix * local_matrix;  
            
    //         if (bone->mName == "mPelvis") {  
    //             pelvis_matrix = world_matrix;  
    //         }  
            
    //         // Store for pelvis-relative calculation  
    //         pelvis_relative_poses[bone->mName] = world_matrix;  
            
    //         for (LLAvatarBoneInfo* child : bone->mChildren) {  
    //             extract_poses(child, world_matrix);  
    //         }  
    //     };  
        
    //     // Start from identity (no scaling)  
    //     for (LLAvatarBoneInfo* root_bone : skeleton_info->mBoneInfoList) {  
    //         extract_poses(root_bone, glm::mat4(1.0f));  
    //     }
  
    //     // Make all poses pelvis-relative  
    //     glm::mat4 pelvis_inverse = glm::inverse(pelvis_matrix);  
    //     for (auto& [name, pose] : pelvis_relative_poses) {  
    //         pose = pelvis_inverse * pose;  
    //     }
    //     return pelvis_relative_poses;
    // }

// std::vector<LLJointData> removeScaleFromRestMatrices(const std::vector<LLJointData>& rootJoints)  
// {  
//     std::vector<LLJointData> correctedJoints;  
      
//     // Helper function to recursively process joints  
//     std::function<void(const LLJointData&, LLJointData&)> processJoint;  
//     processJoint = [&](const LLJointData& source, LLJointData& target)  
//     {  
//         // Copy all fields  
//         target = source;  
          
//         // Decompose the rest matrix to remove scale  
//         glm::vec3 scale, translation, skew;  
//         glm::quat rotation;  
//         glm::vec4 perspective;  
//         glm::decompose(source.mRestMatrix, scale, rotation, translation, skew, perspective);  
          
//         // Recompose without scale (normalized to 1,1,1)  
//         target.mRestMatrix = glm::recompose(  
//             glm::vec3(1.0f), rotation, translation, skew, perspective);  
          
//         // Process children recursively  
//         target.mChildren.clear();  
//         for (const auto& child : source.mChildren)  
//         {  
//             LLJointData& correctedChild = target.mChildren.emplace_back();  
//             processJoint(child, correctedChild);  
//         }  
//     };  
      
//     // Process all root joints  
//     for (const auto& rootJoint : rootJoints)  
//     {  
//         LLJointData& correctedRoot = correctedJoints.emplace_back();  
//         processJoint(rootJoint, correctedRoot);  
//     }  
      
//     return correctedJoints;  
// }
// Get pelvis defaults first  

// std::unordered_map<std::string, glm::mat4> getPelvisRelativeBindPoses(LLVOAvatar *avi) {
//     std::unordered_map<std::string, glm::mat4> result;
// LLJoint* pelvis_joint = avi->getJoint("mPelvis");  
// LLVector3 pelvis_pos = pelvis_joint->getDefaultPosition();  
// LLQuaternion pelvis_rot;// = pelvis_joint->getDefaultRotation();  
  
// // Build pelvis matrix (LLMatrix4 then convert to glm)  
// LLMatrix4 pelvis_mat;  
// pelvis_mat.initRotTrans(pelvis_rot, LLVector4(pelvis_pos, 1.0f));  
// glm::mat4 pelvis_matrix = glm::make_mat4(pelvis_mat.mMatrix[0]);  
// glm::mat4 pelvis_inverse = glm::inverse(pelvis_matrix);  
  
// // Process all joints  
// const auto& skeleton = avi->getSkeleton();  
// for (LLJoint* joint : skeleton)  
// {  
//     if (!joint) continue;  
      
//     // Get default bind pose values  
//     LLVector3 bind_pos = joint->getDefaultPosition();  
//     LLQuaternion bind_rot ;//= joint->getDefaultRotation();  
      
//     // Construct local matrix from defaults  
//     LLMatrix4 local_mat;  
//     local_mat.initRotTrans(bind_rot, LLVector4(bind_pos, 1.0f));  
//     glm::mat4 local_matrix = glm::make_mat4(local_mat.mMatrix[0]);  
      
//     // Make pelvis-relative  
//     glm::mat4 pelvis_relative = pelvis_inverse * local_matrix;  
//     result[joint->getName()] = pelvis_relative;    
//     // Store or use pelvis_relative  
// }
// return result;
// }


bool fetch_player_telemetry(LLVOAvatar* avi, PlayerTelemetry& telemetry);
bool fetch_avi_rest_telemetry(LLVOAvatar* avi, PlayerTelemetry& telemetry) {
            if (!avi) return false;
            avi->resetSkeleton(true);
            for (LLVisualParam *param = avi->getFirstVisualParam(); param; param = avi->getNextVisualParam()) param->setWeight(param->getMinWeight(), false);  
            avi->updateVisualParams();
            return fetch_player_telemetry(avi, telemetry);
                        
            // std::vector<LLJointData> rootJointsBad;
            // avi->getJointMatricesAndHierarhy(rootJointsBad);  
            // auto rootJoints = (rootJointsBad);
            
            // struct Element {
            //     static void flatten(const LLJointData& element, std::vector<LLJointData>& result) {
            //         result.push_back(element);
            //         for (const auto& child : element.mChildren) flatten(child, result);
            //     }
            //     static glm::mat4 removeScaleFromRestMatrix(const glm::mat4& rest_matrix)  
            //     {  
            //         // Decompose matrix into components  
            //         glm::vec3 scale, translation, skew;  
            //         glm::quat rotation;  
            //         glm::vec4 perspective;  
            //         glm::decompose(rest_matrix, scale, rotation, translation, skew, perspective);  
                
            //         // Recompose with normalized scale (1,1,1)  
            //         return glm::recompose(glm::vec3(1.0f), rotation, translation, skew, perspective);  
            //     }
            // };

            // std::vector<LLJointData> joints;
            // for (const auto& root : rootJoints) Element::flatten(root, joints);

            // for (const auto& jd : joints) {  
            //     if (jd.mName == "mPelvis") {
            //         telemetry.worldToAvatar = glm::inverse(Element::removeScaleFromRestMatrix(jd.mRestMatrix));  
            //         break;  
            //     }  
            // }  
            
            // std::unordered_map<std::string, glm::mat4> pelvisRelativeMats = getPelvisRelativeBindPoses(avi);  
            // // for (const auto& jd : joints) {  
            // //     pelvisRelativeMats[jd.mName] = telemetry.worldToAvatar * Element::removeScaleFromRestMatrix(jd.mRestMatrix);  
            // //     fprintf(stdout, "fetch_avi_rest_telemetry mName=%s %s\n", jd.mName.c_str(), glm::to_string(pelvisRelativeMats[jd.mName]).c_str());fflush(stdout);
            // // }
                        
            // auto untrack = [&pelvisRelativeMats, &telemetry](const char* name, glm::mat4& out) {
            //     auto it = pelvisRelativeMats.find(name);
            //     if (it == pelvisRelativeMats.end()) return;
            //     out = Element::removeScaleFromRestMatrix(it->second);
            //     fprintf(stdout, "fetch_avi_rest_telemetry name=%s %s\n", name, glm::to_string(out).c_str());fflush(stdout);
            // };
            // untrack("mRoot", telemetry.mRoot);
            // untrack("mPelvis", telemetry.mPelvis);
            // untrack("mHead", telemetry.mHead);
            // untrack("mWristLeft", telemetry.mWristLeft);
            // untrack("mWristRight", telemetry.mWristRight);
            // untrack("mFootLeft", telemetry.mFootLeft);
            // untrack("mFootRight", telemetry.mFootRight);
            // return true;

        }
        bool fetch_player_telemetry(LLVOAvatar* avi, PlayerTelemetry& telemetry) {  
            glm::mat4 avatarToWorld;
            avi->mRoot->updateWorldMatrixChildren();
            auto root = get_joint_matrix(avi, "mPelvis", avatarToWorld);
            if (!root) return false;
            telemetry.worldToAvatar = glm::inverse(avatarToWorld);

            auto track = [avi, &telemetry](const char* name, glm::mat4& dest) {
                LLJoint* joint = avi->getJoint(name);
                if (!joint) return;
                dest = glm::make_mat4((F32*)&joint->getWorldMatrix().mMatrix);
                dest = telemetry.worldToAvatar * dest;
            };
            track("mRoot", telemetry.mRoot);
            track("mPelvis", telemetry.mPelvis);
            track("mHead", telemetry.mHead);
            track("mWristLeft", telemetry.mWristLeft);
            track("mWristRight", telemetry.mWristRight);
            track("mFootLeft", telemetry.mFootLeft);
            track("mFootRight", telemetry.mFootRight);
            return true;  
        }        
        bool fetch_player_pose(LLVOAvatar* avi, PlayerPose& telemetry, bool bind = true) {  
            if (!fetch_player_telemetry(avi, telemetry)) return false;
            float _pelvisFixup = 0.0f;
            if (avi->hasPelvisFixup(_pelvisFixup)) {
                telemetry.pelvisFixup = glm::translate(glm::vec3(0.0f, 0.0f, _pelvisFixup));
            }
            if (bind && !telemetry.bind) {
                telemetry.bind = std::make_shared<PlayerTelemetry>();
                fprintf(stdout, "slos::ht::fetch_player_telemetry(..., bind=true) -- fetch_avi_rest_telemetry(%p) # %s\n", avi, avi->avString().c_str());fflush(stdout);
                fetch_avi_rest_telemetry(avi, *telemetry.bind);
            }

            // auto track = [avi, &telemetry](const char* name, glm::mat4& dest, glm::mat4* bind) {
            //     if (bind) dest = glm::inverse(*bind) * dest;
            // };
            // decltype(telemetry.bind) bindPose = nullptr;//telemetry.bind;
            // track("mRoot", telemetry.mRoot, bindPose ? &bindPose->mRoot : nullptr );
            // track("mPelvis", telemetry.mPelvis, bindPose ? &bindPose->mPelvis : nullptr);
            // track("mHead", telemetry.mHead, bindPose ? &bindPose->mHead : nullptr);
            // track("mWristLeft", telemetry.mWristLeft, bindPose ? &bindPose->mWristLeft : nullptr);
            // track("mWristRight", telemetry.mWristRight, bindPose ? &bindPose->mWristRight : nullptr);
            // track("mFootLeft", telemetry.mFootLeft, bindPose ? &bindPose->mFootLeft : nullptr);
            // track("mFootRight", telemetry.mFootRight, bindPose ? &bindPose->mFootRight : nullptr);
            return true;  
        }        
        // PlayerPose get_player_telemetry(LLVOAvatar* avi, bool bind = true) {  
        //     PlayerPose telemetry;
        //     fetch_player_telemetry(avi, telemetry, bind);
        //     return telemetry;
        // }        
        int apply_player_telemetry(LLVOAvatar* avi, PlayerPose const& telemetry, bool hips = false) {  
            glm::mat4 avatarToWorld;
            auto root = get_joint_matrix(avi, "mPelvis", avatarToWorld);
            if (!root) return false;
            if (hips) avatarToWorld = glm::inverse(telemetry.worldToAvatar);
            int n = 0;
            auto untrack = [avi, &n, &avatarToWorld, &telemetry](const char* name, glm::mat4 const& src, glm::mat4 const* bind) {
                if (!glm::isfinite(src[0][0])) return;
                auto joint = avi ? avi->getJoint(name) : nullptr;
                if (!joint) return;
                glm::mat4 tmp = src;
                // if (bind) tmp = *bind * tmp;
                tmp = avatarToWorld * tmp;
                // return;
                // joint->setWorldMatrix(LLMatrix4{glm::value_ptr(tmp)});
                // fprintf(stdout, "apply_player_telemetry name=%s src=%s tmp=%s\n", name, glm::to_string(src).c_str(), glm::to_string(tmp).c_str());fflush(stdout);
                joint->setWorldRotation(LLQuaternion{glm::value_ptr(glm::quat(tmp))});
                // joint->updateWorldMatrixParent();  
                joint->setWorldPosition(LLVector3{glm::value_ptr(tmp[3])});
                joint->updateWorldMatrix();
                n++;
            };
            // untrack("mRoot", telemetry.mRoot);
            auto bindPose = telemetry.bind;
            // untrack("mRoot", telemetry.mRoot, bindPose ? &bindPose->mRoot : nullptr );
            if (hips) untrack("mPelvis", telemetry.mPelvis, bindPose ? &bindPose->mPelvis : nullptr);
            untrack("mHead", telemetry.mHead, bindPose ? &bindPose->mHead : nullptr);
            untrack("mWristLeft", telemetry.mWristLeft, bindPose ? &bindPose->mWristLeft : nullptr);
            untrack("mWristRight", telemetry.mWristRight, bindPose ? &bindPose->mWristRight : nullptr);
            untrack("mFootLeft", telemetry.mFootLeft, bindPose ? &bindPose->mFootLeft : nullptr);
            untrack("mFootRight", telemetry.mFootRight, bindPose ? &bindPose->mFootRight : nullptr);
            avi->dirtyMesh();  
            return n;

            int jn = n;
            auto untrackvp = [avi, &n, &avatarToWorld, &telemetry](const char* name, float const& src) {
                if (!glm::isfinite(src)) return;
                auto joint = avi ? avi->getVisualParam(name) : nullptr;
                if (!joint) return;
                fprintf(stdout, "slos::ht::apply_player_telemetry -- %s = %f\n", name, src);
                joint->setWeight(src, false);
                n++;
            };
            untrackvp("Lipsync_Ooh", telemetry.Lipsync_Ooh);
            untrackvp("Lipsync_Aah", telemetry.Lipsync_Aah);
            if (n > jn) {
                fprintf(stdout, "slos::ht::apply_player_telemetry -- n=%d jn=%df\n", n, jn);
                avi->LLCharacter::updateVisualParams();  
                avi->dirtyMesh();  
            }
            return n;  
        }
        std::map<LLPointer<LLVOAvatar>,std::pair<std::string,std::shared_ptr<PlayerPose>>> telemetries;
        std::shared_ptr<PlayerPose> overriding_player_telemetry(LLVOAvatar* avi, bool fetch = true) {
            std::shared_ptr<PlayerPose> pt;
            if (!avi) return pt;
            auto it = telemetries.find(avi);
            if (it != telemetries.end()) pt = it->second.second;
            else {
                if (!fetch) return pt;
                pt = std::make_shared<PlayerPose>();
                auto id = avi->avString();
                telemetries[avi] = { id, pt };
                fprintf(stdout, "slos::ht::overriding_player_telemetry(%p) # %s\n", avi, id.c_str());fflush(stdout);
            }
            if (fetch) {
                fetch_player_pose(avi, *pt, true);
            }
            return pt;
        }
        bool prune_stale_player_telemetries() {
            std::erase_if(telemetries, [](const auto& kv) {
                if (!kv.first.get() || !kv.second.second) {
                    fprintf(stdout, "slos::ht::prune_stale_player_telemetries # %s\n", kv.second.first.c_str());fflush(stdout);
                    return true;
                }
                return false;
            });
        }
        bool clear_player_telemetry(LLVOAvatar* avi) {
            auto it = telemetries.find(avi);
            if (it == telemetries.end()) return false;
            fprintf(stdout, "slos::ht::clear_player_telemetry(%p)\n", avi);fflush(stdout);
            telemetries.erase(it);
            return true;
        }

    }
}
void tweak_llvoavatar_pose(LLVOAvatar* avi) {
    auto it = slos::ht::telemetries.find(avi);
    if (it == slos::ht::telemetries.end() || !it->second.second) return;
    slos::ht::apply_player_telemetry(avi, *it->second.second);
}

    
#elif __INCLUDE_LEVEL__ == 0
    #error "TPVM_RECIPE: -xc++ -DSLOS_IMPLEMENTATION"
#endif
