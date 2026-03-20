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

} // namespace slos

#endif // SLOS_H

// ----------------------------------------------------------------------------
// IMPLEMENTATION (LL headers and dirty logic go here)
// ----------------------------------------------------------------------------
#ifdef SLOS_IMPLEMENTATION

#include "foaf.hpp"

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
    
#elif __INCLUDE_LEVEL__ == 0
    #error "TPVM_RECIPE: -xc++ -DSLOS_IMPLEMENTATION"
#endif
