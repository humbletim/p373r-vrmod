#ifndef humbletim_llviewerdisplay_h
#define humbletim_llviewerdisplay_h

// NOTE: these are called TWICE per main app render frame
extern bool vrmod_llviewerdisplay_begin_render();
extern bool vrmod_llviewerdisplay_capture_and_continue_render();

#endif // humbletim_llviewerdisplay_h

// ----------------------------------------------------------------------------
#ifdef humbletim_llviewerdisplay_implementation

#include "vrmod/session.hpp"

namespace slos {
    extern bool get_pipeline_render_ui_flag();
    extern bool set_pipeline_render_ui_flag(bool enable);
}

namespace vrmod {
    uint32_t frame = 0; // frame debugging counter
    int eyeIndex = 0; // currently active/rendering eye

    // FIXME: experimentally trying to leave "dominant eye" on the camera stack
    //   so that internal app calculations (picking, angular LOD, etc.) vibe with
    //   primary eye rather than neither / monoscopic camera default setup
    int fallbackEyeIndex = 0; // fallback for eye-ray selection integrity

    // remembers if UI is rendering (ie: not mouselook) when we set to false
    //   because VR Mod renders/captures UI independently
    int prev_draw_ui_state = 0;
    void restore_ui_flag() {
        if (vrmod::prev_draw_ui_state == 0) return;
        slos::set_pipeline_render_ui_flag(vrmod::prev_draw_ui_state == +1);
        vrmod::prev_draw_ui_state = 0;
    }
    void override_ui_flag() {
        bool was_on = slos::set_pipeline_render_ui_flag(false);
        vrmod::prev_draw_ui_state = was_on ? +1 : 0;
    }
}

bool vrmod_llviewerdisplay_begin_render() {
    nunja::HandleViewerUI(); // always process ctrl+tab/tab activations

    if (!nunja::IsStereo()) {
        if (vrmod::prev_draw_ui_state) vrmod::restore_ui_flag();
        if (vrmod::eyeIndex) {
            vrmod::fallbackEyeIndex = 0;
            vrmod::eyeIndex = 0;
        }
        return false;
    }
    const bool newFrame = !nunja::IsEyeReady(nunja::FirstEye());
    if (newFrame) {
        vrmod::frame++;
        nunja::BeginFrame(vrmod::frame);
        vrmod::eyeIndex = nunja::FirstEye();

        // FIXME: this should really be set/reset via VR mode activate/deactivate
        // (assigning here and resetting above is likely +/- 1 frame off)
        vrmod::fallbackEyeIndex = nunja::get_world_eye();

        // Make sure UI rendering is toggled off for VR passess
        vrmod::override_ui_flag();
    } else {
        vrmod::eyeIndex = nunja::SecondEye();
    }
    nunja::BeginEye(vrmod::eyeIndex);
    return true;
}

// called TWICE per main app render frame (after render pass)
bool vrmod_llviewerdisplay_capture_and_continue_render() {
    if (!vrmod::eyeIndex) return false;

    const bool frameComplete = vrmod::eyeIndex == nunja::SecondEye();

    nunja::EndEye(vrmod::eyeIndex);
    vrmod::eyeIndex = 0;

    if (frameComplete) {
        nunja::EndFrame(vrmod::frame);
        vrmod::restore_ui_flag();
    }
    return !frameComplete; // returns true if needing "goto sec" for second pass
}

#elif __INCLUDE_LEVEL__ == 0
    #error "TPVM_RECIPE: -xc++ -Dhumbletim_llviewerdisplay_implementation"
#endif

