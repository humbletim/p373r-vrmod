#ifndef vrmod_llviewerdisplay_h
#define vrmod_llviewerdisplay_h

// NOTE: these are called TWICE per main app render frame
extern bool vrmod_llviewerdisplay_begin_render();
extern bool vrmod_llviewerdisplay_capture_and_continue_render();

#endif // vrmod_llviewerdisplay_h

// ----------------------------------------------------------------------------
#ifdef vrmod_llviewerdisplay_implementation

#include "llviewerVR.h"
extern llviewerVR gVR;

#include "llframetimer.h"
#include "llviewerwindow.h" // gViewerWindow

// ----------------------------------------------------------------------------
// Hook 1: Executed at the top of the render loop (replaces manual ProcessVRCamera)
// ----------------------------------------------------------------------------
bool vrmod_llviewerdisplay_begin_render() {
    gVR.HandleKeyboard();
    gVR.ProcessVRCamera();
    gViewerWindow->setup3DRender();
    return true; 
}

// ----------------------------------------------------------------------------
// Hook 2: Executed after UI is rendered, before the screen swap
// ----------------------------------------------------------------------------
bool vrmod_llviewerdisplay_capture_and_continue_render() {
    if (!gVR.m_bVrActive) {
        // VR is off. Return false so the main loop falls through to swap().
        return false; 
    }

    // 1. Draw VR-specific overlays (axes/lasers) into the active buffer BEFORE we capture it.
    // This elegantly eliminates the need for separate injection hooks for these!
    {
        gViewerWindow->setup3DRender();
        gVR.RenderControllerAxes();
        glm::mat4 saved_view = get_current_modelview();
        extern bool gSnapshot;

        if (!gSnapshot)
        {
            gGL.pushMatrix();
            gGL.loadMatrix(gGLLastModelView);
            set_current_modelview(glm::make_mat4(gGLLastModelView));
        }

        gViewerWindow->setup2DRender();
        gVR.DrawCursors();

        if (!gSnapshot)
        {
            set_current_modelview(saved_view);
            gGL.popMatrix();
        }
   }

    // 2. Capture the current framebuffer to the left/right eye texture and submit if ready.
    gVR.vrDisplay();

    // 3. Evaluate if we need a second pass.
    // If Left eye just finished, and Right eye is pending (and we aren't mono):
    if (gVR.leftEyeDesc.IsReady && !gVR.rightEyeDesc.IsReady && gVR.eyeDistance() > 0) {
        return true; // Triggers 'goto sec;' in llviewerdisplay.cpp
    }

    // Both eyes are done (or eyeDistance == 0). Frame is complete.
    // Returning false will let the upstream code naturally hit swap() for the companion window.
    return false; 
}

#elif __INCLUDE_LEVEL__ == 0
    #error "TPVM_RECIPE: -I../asdf -xc++ -Dvrmod_llviewerdisplay_implementation"
#endif

