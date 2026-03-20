#pragma once

#include "humbletim/xopenvr.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace FSCommon { void report_to_nearby_chat(std::string_view message); }
class LLHUDText;
#include "llgl.h"
#include "string.h"
#include "llcoord.h"
// #include "llfloater.h"
class LLFloaterCamera; // #include "llfloatercamera.h"
class LLButton;
class LLView;

class llviewerVR
{
public:
	xvr::Session gHMD; // Replaces IVRSystem
	
	std::string gStrDriver;
	std::string gStrDisplay;

	U32 bx = 0;
	U32 by = 0;
	U32 tx = 0;
	U32 ty = 0;

	// KEY	m_kEditKey;
	// KEY	m_kDebugKey;
	// KEY	m_kMenuKey;
	// KEY	m_kPlusKey;
	// KEY	m_kMinusKey;

	template <typename T = bool> struct VocalValue {
		const char* hint;
		T d{};
		operator T() const { return d; }
		VocalValue& operator=(T o){
			if (d != o) FSCommon::report_to_nearby_chat(std::format("[{}] {}={} (was: {})", typeid(T).name(), hint, o, d).c_str());
			d = o;
			return *this; }
		constexpr VocalValue& operator-=(T o) { *this = *this - o; return *this;  }
		constexpr VocalValue& operator+=(T o) { *this = *this + o; return *this;  }
	};

	VocalValue<bool> m_bVrEnabled{"m_bVrEnabled", false };
	VocalValue<bool> m_bVrActive{"m_bVrActive", false };
	VocalValue<bool> m_bVrKeyDown{"m_bVrKeyDown", false};
	bool m_bEditKeyDown = 0;
	bool m_bEditActive = 0;
	bool m_bDebugKeyDown = 0;
	// bool m_bMenuKeyDown = 0;
	// bool m_bPlusKeyDown = 0;
	// bool m_bMinusKeyDown = 0;

	// bool isRenderingLeftEye = 0;
	VocalValue<bool> gVRInitComplete{"gVRInitComplete", 0 };

	// S32 m_iTextureShift = 0;

	struct FramebufferDesc
	{
		// GLuint m_nDepthBufferId;
		// GLuint m_nRenderTextureId;
		// GLuint m_nRenderFramebufferId;
		GLuint m_nResolveTextureId{ 0 };
		GLuint mFBO{ 0 };
		GLuint IsReady{ false };
	};
	FramebufferDesc leftEyeDesc{};
	FramebufferDesc rightEyeDesc{};
	
	VocalValue<U32> m_nRenderWidth{"m_nRenderWidth"};
	VocalValue<U32> m_nRenderHeight{"m_nRenderHeight"};
	// S32 m_iTrackedControllerCount;
	VocalValue<S32> m_iZoomIndex{"m_iZoomIndex",  0 };
	VocalValue<F32> m_fCamRotOffset{"m_fCamRotOffset", 90.0f};
	VocalValue<F32> m_fCamPosOffset{"m_fCamPosOffset", 0 };

	LLVector3 m_vdir_orig;
	LLVector3 m_vup_orig;
	LLVector3 m_vleft_orig;
	LLVector3 m_vpos_orig;

	LLVector3 m_vdir;
	LLVector3 m_vup;
	LLVector3 m_vleft;
	LLVector3 m_vpos;
	
	LLButton  *m_pCamButtonLeft = 0;
	LLButton  *m_pCamButtonRight = 0;
	LLFloaterCamera	*m_pCamera_floater = 0;
	LLView	*m_pCamStack = 0;

	LLCoordWindow m_MousePos;
	LLCoordWindow m_ScrSize;
	// S32 m_iHalfWidth;
	// S32 m_iHalfHeight;
	// S32 m_iThirdWidth;
	// S32 m_iThirdHeight;
	
	F32 m_fFocusDistance{ 1.0f };
	// F32 m_fTextureShift;
	F32 m_fFOV{ 100.0f };
	F32 m_fTextureZoom{ 0 };

	// float m_fNearClip;
	// float m_fFarClip;

	// std::string m_strPoseClasses;
	std::string m_strDriver;
	std::string m_strDisplay;

	glm::mat4 m_mat4HMDPose{ 1.0f };
	// glm::mat4 m_rmat4DevicePose[xvr::MAXDEVICECOUNT];
	// glm::mat4 m_mat4eyePosLeft;
	// glm::mat4 m_mat4eyePosRight;

	// glm::mat4 m_mat4ProjectionCenter;
	// glm::mat4 m_mat4ProjectionLeft;
	// glm::mat4 m_mat4ProjectionRight;

	// LLMatrix4 ConvertGLMToLLMatrix4(const glm::mat4& m);

	// LLVector3 gHMDAxes;
	LLVector3 gCurrentCameraPos;
	// LLQuaternion gHMDQuat;
	// LLQuaternion gCtrlQuat[2];
	LLVector3 gHmdPos;
	LLVector3 gHmdOffsetPos;

	LLVector3 gCtrlPos[xvr::MAXDEVICECOUNT];
	LLVector3 gCtrlOrigin[xvr::MAXDEVICECOUNT];
	LLCoordGL gCtrlscreen[xvr::MAXDEVICECOUNT];

	LLHUDText *hud_textp { nullptr };
	std::string m_strHudText;
	// bool m_bHudTextUpdated=FALSE;

	// bool gRightClick[xvr::MAXDEVICECOUNT];
	// bool gLeftClick[xvr::MAXDEVICECOUNT];
	S32 gCursorDiff{ 0 };
	// uint64_t gPreviousButtonMask;
	// uint64_t gButton;

	// bool m_rbShowTrackedDevice[xvr::MAXDEVICECOUNT];
	// uint32_t gPacketNum = 0;

	void UpdateHMDMatrixPose();
	void SetupCameras();
	bool CreateFrameBuffer(int nWidth, int nHeight, FramebufferDesc &framebufferDesc);
	void vrStartup(bool is_shutdown);
	void vrDisplay();
	bool HandleInput();
	void DrawCursors();
	void ProcessVREvent(const xvr::Event & event);
	// void agentYaw(F32 yaw_inc);
	bool ProcessVRCamera();
	void RenderControllerAxes();
	BOOL posToScreen(const LLVector3 &pos_agent, LLCoordGL &out_point, const BOOL clamp) const;
	void buttonCallbackLeft();
	void buttonCallbackRight();
	void HandleKeyboard();
	void Debug();
	void InitUI();
	void calcUVBounds(xvr::Eye eye, F32 *uMin, F32 *uMax, F32 *vMin, F32 *vMax);
	F32 eyeDistance();

	llviewerVR();
	~llviewerVR();
};
