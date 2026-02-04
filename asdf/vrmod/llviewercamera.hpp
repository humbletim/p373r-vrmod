#ifndef humbletim_llviewercamera_h
#define humbletim_llviewercamera_h

#include <glm/glm.hpp>

extern bool vrmod_LLViewerCamera_assignProjection(float fovy, float aspect, float z_near, float z_far, float llm4out[4][4]);
extern glm::mat4 vrmod_LLViewerCamera_perspective(float fovy, float aspect, float z_near, float z_far);

#endif // humbletim_llviewercamera_h

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

#ifdef humbletim_llviewercamera_implementation

#include <glm/gtc/matrix_transform.hpp> // glm::perspective

namespace nunja { extern bool IsStereo(); extern glm::mat4 getEyePerspective(int eye, float z_near, float z_far); }
namespace vrmod { extern int eyeIndex; extern int fallbackEyeIndex; }

// == LLViewerCamera::setPerspective trampoline ==
// PRIMARY: this makes sure the correct eye perspective matrix gets used for rendering;
//  like feeds HMD/Eye into LL opengl matrix setup
glm::mat4 vrmod_LLViewerCamera_perspective(float fovy, float aspect, float z_near, float z_far) {
    int ceye = vrmod::eyeIndex ? vrmod::eyeIndex : vrmod::fallbackEyeIndex;
    if (!ceye || !nunja::IsStereo()) return glm::perspective(fovy, aspect, z_near, z_far);

    return nunja::getEyePerspective(ceye, z_near, z_far);
}

// == LLViewerCamera::calcProjection trampoline ==
// SECONDARY: configures internal projection state for things like raycast picking
bool vrmod_LLViewerCamera_assignProjection(float fovy, float aspect, float z_near, float z_far, float llmat4[4][4]) {
    int ceye = vrmod::eyeIndex ? vrmod::eyeIndex : vrmod::fallbackEyeIndex;
    if (!ceye || !nunja::IsStereo()) return false;

    glm::mat4 perspective = nunja::getEyePerspective(ceye, z_near, z_far);
    memcpy(&llmat4[0][0], &perspective[0][0], sizeof(glm::mat4));
    return true;
}

#elif __INCLUDE_LEVEL__ == 0
    #error "TPVM_RECIPE: -xc++ -Dhumbletim_llviewercamera_implementation"
#endif