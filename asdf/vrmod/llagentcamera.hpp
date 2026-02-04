#ifndef humbletim_llagentcamera_h
#define humbletim_llagentcamera_h

extern bool vrmod_LLAgentCamera_tweak_agent_camera(float* origin, float* up, float* focus);

#endif // humbletim_llagentcamera_h

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

#ifdef humbletim_llagentcamera_implementation

#include "vrmod/session.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp> // glm::make_vec3
#include <glm/gtx/quaternion.hpp> // glm::toMat4

namespace slos { extern float getGroundToHeadDistance(); }

namespace {
    struct AgentCamera {
        glm::mat4 anchor;
        float focus_dist;
        inline AgentCamera(float* _origin, float* _up, float* _focus) {
            // Import Legacy Vectors (Raw)
            glm::vec3 origin = glm::make_vec3(_origin);
            glm::vec3 up_hint = glm::make_vec3(_up);
            glm::vec3 focus = glm::make_vec3(_focus);

            // Construct the Legacy "Anchor" Matrix manually (RH Z-Up)
            // Basis: X=Forward, Y=Left, Z=Up
            glm::vec3 fwd = focus - origin;
            float focus_dist = glm::length(fwd); // Preserve distance for orbit logic
            fwd = glm::normalize(fwd);           // +X Axis

            // RH Rule: Cross(Up, Fwd) = Left (+Y Axis)
            glm::vec3 left = glm::normalize(glm::cross(up_hint, fwd)); 
            
            // Orthogonal Up (+Z Axis)
            glm::vec3 true_up = glm::cross(fwd, left);

            // Build Matrix (Column-Major)
            anchor[0] = glm::vec4(fwd, 0.0f);     // X (Forward)
            anchor[1] = glm::vec4(left, 0.0f);    // Y (Left)
            anchor[2] = glm::vec4(true_up, 0.0f); // Z (Up)
            anchor[3] = glm::vec4(origin, 1.0f);  // Position
        }
    };
}

bool vrmod_LLAgentCamera_tweak_agent_camera(float* _origin, float* _up, float* _focus) {
    if (!nunja::IsStereo()) return false;
    
    AgentCamera ac{ _origin, _up, _focus };
    auto anchor = ac.anchor;
    auto focus_dist = ac.focus_dist;

    nunja::cache::agentCamera = anchor;

    // Strategy A: Floor Alignment (The Height Fix)
    // Move the Anchor (Eyes) down to the Anchor (Feet)
    if (nunja::IsRoomScale()) {
        float avatar_height = slos::getGroundToHeadDistance();
        //  anchor[3][2] -= avatar_height; 
        // This moves the "feet" back relative to the tilt, keeping the "head" at the origin.
        anchor[3] -= anchor[2] * avatar_height;
    }

    nunja::cache::agentCameraRoomScale = anchor;

    // Apply HEAD Pose (DECOUPLED)
    // Get HEAD Pose in Z-Up Floor Space (No Eye Offset applied)
    //    (raw HMD pose from OpenVR, already converted to Z-Up)
    glm::mat4 head_floor_pose = nunja::getHeadFloorPose();

    // A. ORIENTATION: Inherit FULL rotation (Yaw + Pitch + Roll)
    //    This preserves the critical "looking down" interaction.
    auto rot_anchor = glm::quat(anchor);
    auto rot_head   = glm::quat(head_floor_pose);
    auto final_rot  = rot_anchor * rot_head;

    // B. POSITION: Inherit only YAW for the physical offsets
    //    This prevents the "Head on Stick" from falling forward when looking down.
    
    //    Extract the Head's local movement (Height + Room-scale walk)
    glm::vec3 head_offset = glm::vec3(head_floor_pose[3]);

    //    Build a "Flat" rotation basis from the Anchor (Z-Up safe)
    //    (Project Forward/Left onto the horizon and re-normalize)
    glm::vec3 flat_fwd  = glm::normalize(glm::vec3(anchor[0].x, anchor[0].y, 0.0f));
    glm::vec3 flat_left = glm::normalize(glm::vec3(anchor[1].x, anchor[1].y, 0.0f));
    glm::vec3 flat_up   = glm::vec3(0.0f, 0.0f, 1.0f);

    //    Manually rotate the Head Offset into World Space using the Flat Basis
    glm::vec3 world_offset = 
        (flat_fwd  * head_offset.x) + 
        (flat_left * head_offset.y) + 
        (flat_up   * head_offset.z);

    // C. COMBINE
    glm::mat4 final_pose = glm::toMat4(final_rot);
    final_pose[3]        = anchor[3] + glm::vec4(world_offset, 0.0f);

    nunja::cache::agentCameraIntegrated = final_pose;        

    // Extract New Vectors
    glm::vec3 new_origin = glm::vec3(final_pose[3]);
    glm::vec3 new_fwd    = glm::vec3(final_pose[0]); // X is Forward
    glm::vec3 new_up     = glm::vec3(final_pose[2]); // Z is Up

    // Reconstruct Focus Point
    // Move "focus_dist" along the new forward vector
    glm::vec3 new_focus = new_origin + (new_fwd * focus_dist);

    // Write Back
    memcpy(_origin, glm::value_ptr(new_origin), sizeof(glm::vec3));
    memcpy(_up, glm::value_ptr(new_up), sizeof(glm::vec3));
    memcpy(_focus, glm::value_ptr(new_focus), sizeof(glm::vec3));

    return true;
}

#elif __INCLUDE_LEVEL__ == 0
    #error "TPVM_RECIPE: -xc++ -Dhumbletim_llagentcamera_implementation"
#endif
