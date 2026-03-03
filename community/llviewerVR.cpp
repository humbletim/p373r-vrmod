// #include "llviewerprecompiledheaders.h"
#include "llviewerVR.h"
#include "llframetimer.h"
#include "llviewerwindow.h"
#include <string>

#ifdef _WIN32
#include "llwindowwin32.h"
#endif

#include "llviewercontrol.h"
#include "llviewercamera.h"
#include "llagentcamera.h"
#include "pipeline.h"
#include "llagent.h"

#include "llfloatercamera.h"

#ifdef _WIN32
#include "llkeyboardwin32.h"
#endif

#include "llui.h"
#include "llhudtext.h"
#include "llfloaterreg.h"
#include "llviewerVR.vrmod_settings.c++" // gVrModSettings

#include "humbletim/opengl.hpp" // opengl::blit_side_by_side

llviewerVR gVR{}; // global singleton

llviewerVR::llviewerVR()
{
	leftEyeDesc.m_nResolveTextureId = 0;
	rightEyeDesc.m_nResolveTextureId = 0;
	hud_textp = NULL;
	m_fFocusDistance = 1;
	// m_fTextureShift = 0;
	m_fTextureZoom = 0;
	m_fFOV = 100;
}

llviewerVR::~llviewerVR() {}

void llviewerVR::calcUVBounds(xvr::Eye eye, F32 *uMin, F32 *uMax, F32 *vMin, F32 *vMax) {
	F32 gameTanAngleHeight = 2.0f * tan(LLViewerCamera::getInstance()->getView()/2.0f);
	F32 gameTanAngleUp = gameTanAngleHeight/2.0f;
	F32 gameTanAngleDown = -gameTanAngleUp;
	F32 gameTanAngleWidth = gameTanAngleHeight * LLViewerCamera::getInstance()->getAspect();
	F32 gameTanAngleRight = gameTanAngleWidth/2.0f;
	F32 gameTanAngleLeft = -gameTanAngleRight;

	if (!gHMD) return;
	xvr::ProjectionBounds bounds = gHMD->getProjectionBounds(eye);

	F32 vrTanAngleLeft = bounds.min.x;
	F32 vrTanAngleRight = bounds.max.x;
	F32 vrTanAngleDown = bounds.min.y;
	F32 vrTanAngleUp = bounds.max.y;
	
	F32 vrTanAngleWidth = vrTanAngleRight - vrTanAngleLeft;
	F32 vrTanAngleHeight = vrTanAngleUp - vrTanAngleDown;

	*uMin = gameTanAngleLeft * (2.0f / vrTanAngleWidth) - (vrTanAngleRight + vrTanAngleLeft) / vrTanAngleWidth;
	*uMax = gameTanAngleRight * (2.0f / vrTanAngleWidth) - (vrTanAngleRight + vrTanAngleLeft) / vrTanAngleWidth;
	*vMin = gameTanAngleDown * (2.0f / vrTanAngleHeight) - (vrTanAngleUp + vrTanAngleDown) / vrTanAngleHeight;
	*vMax = gameTanAngleUp * (2.0f / vrTanAngleHeight) - (vrTanAngleUp + vrTanAngleDown) / vrTanAngleHeight;

	*uMin = *uMin * 0.5f + 0.5f;
	*uMax = *uMax * 0.5f + 0.5f;
	*vMin = *vMin * 0.5f + 0.5f;
	*vMax = *vMax * 0.5f + 0.5f;
}

F32 llviewerVR::eyeDistance() {
	if (!gHMD) return 0.0f;
	glm::mat4 mat = gHMD->getEyeToHeadTransform(xvr::Eye::Right);
	return 2000.0 * mat[3][0]; // Extract Translation X
}

LLMatrix4 llviewerVR::ConvertGLMToLLMatrix4(const glm::mat4& m)
{
	LLMatrix4 mout;
	const float* src = glm::value_ptr(glm::transpose(m)); // Transpose to match LLMatrix4 layout
	memcpy(mout.mMatrix, src, sizeof(float) * 16);
	return mout;
}

void llviewerVR::UpdateHMDMatrixPose()
{
	if (!gHMD) return;
	
    static LLFrameTimer t;
	gHMD->updatePoses(t.getElapsedTimeF32());

	// m_strPoseClasses = "";
	// for (uint32_t nDevice = 0; nDevice < xvr::MAXDEVICECOUNT; ++nDevice)
	// {
	// 	if (gHMD->getValidDevicePose((xvr::Device)nDevice, m_rmat4DevicePose[nDevice]))
	// 	{
	// 		// Build debug string based on class name
	// 		m_strPoseClasses += gHMD->get_device_class_name((xvr::Device)nDevice)[0]; 
	// 	}
	// }

	gHMD->getValidDevicePose(xvr::HMD, m_mat4HMDPose);
}

void llviewerVR::SetupCameras()
{
	if (!gHMD) return;
	// m_mat4ProjectionLeft = gHMD->getEyePerspectiveMatrix(xvr::Eye::Left, m_fNearClip, m_fFarClip);
	// m_mat4ProjectionRight = gHMD->getEyePerspectiveMatrix(xvr::Eye::Right, m_fNearClip, m_fFarClip);
	// m_mat4eyePosLeft = gHMD->getEyeToHeadTransform(xvr::Eye::Left);
	// m_mat4eyePosRight = gHMD->getEyeToHeadTransform(xvr::Eye::Right);

	if (gVrModSettings->cameraAngle != 0.0f) {
		gSavedSettings.setF32("CameraAngle", gVrModSettings->cameraAngle);
	}
}

bool llviewerVR::CreateFrameBuffer(int nWidth, int nHeight, FramebufferDesc &framebufferDesc)
{
	if (framebufferDesc.m_nResolveTextureId) {
		glDeleteTextures(1, &framebufferDesc.m_nResolveTextureId);
	} else { 
		glGenFramebuffers(1, &framebufferDesc.mFBO);
	}
	glBindFramebuffer(GL_FRAMEBUFFER, framebufferDesc.mFBO);
	glGenTextures(1, &framebufferDesc.m_nResolveTextureId);
	glBindTexture(GL_TEXTURE_2D, framebufferDesc.m_nResolveTextureId);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, nWidth, nHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, framebufferDesc.m_nResolveTextureId, 0);
	
	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	return status == GL_FRAMEBUFFER_COMPLETE;
}

void llviewerVR::vrStartup(bool is_shutdown)
{
	fprintf(stdout, "vrStartup %d\n", is_shutdown);fflush(stdout);
	if (m_bVrEnabled && !is_shutdown)
	{
		if (!gHMD)
		{
			gVRInitComplete = FALSE;
			std::string err;
	        const char* cfg = getenv("DUMMYHMD") ? getenv("DUMMYHMD") : xvr::XVRWrapper_Version;
			gHMD = xvr::Init(cfg, &err);
			
			m_strHudText = "Initializing VR driver!";
			m_strHudText.append("\n");
			m_strHudText.append(cfg);
			if (!gHMD)
			{
				char buf[1024];
				snprintf(buf, sizeof(buf), "\nERROR Unable to init VR runtime: %s", err.c_str());
				m_strHudText.append(buf);
			}
			else
			{
				m_strDriver = gHMD->get_driver_name();
				m_strDisplay = gHMD->get_serial_name();
				m_strHudText.append("\nDriver = " + m_strDriver);
				m_strHudText.append("\nDisplay = " + m_strDisplay);
				m_strHudText.append("\nVR driver! Initialized");
			}

			if (gHMD && !gVRInitComplete)
			{
				gVRInitComplete = TRUE;
				glm::uvec2 rtSize = gHMD->recommendedRenderTargetSize();
				m_nRenderWidth = rtSize.x;
				m_nRenderHeight = rtSize.y;
				
				CreateFrameBuffer(m_nRenderWidth, m_nRenderHeight, leftEyeDesc);
				CreateFrameBuffer(m_nRenderWidth, m_nRenderHeight, rightEyeDesc);
				SetupCameras();
				m_strHudText.append("\nCreating frame buffers.");
				m_strHudText.append("\neyeDistance() = " + std::to_string(eyeDistance()));
			}
			if(gVRInitComplete)
				m_strHudText.append("\nVR driver ready.\n Press TAB to enter VR mode.");
			
			if (hud_textp) {
				fprintf(stdout, "hud_textp->setString(%s)\n", m_strHudText.c_str());fflush(stdout);
				hud_textp->setString(m_strHudText);
				hud_textp->setDoFade(FALSE);
				hud_textp->setHidden(FALSE);
			}
			// m_strHudText = "";
		}
	}
	else if (gHMD || is_shutdown)
	{
		m_bVrEnabled = FALSE;
		m_bVrActive = FALSE;
		gHMD = nullptr; // Shared ptr cleanup calls shutdown
		gVRInitComplete = FALSE;
		m_strHudText = "Press CTRL+TAB to enable VR mode\n Press TAB to remove this message";
		if (hud_textp) { hud_textp->setString(m_strHudText); hud_textp->setHidden(FALSE); }
		m_strHudText = "";
	}
}

bool llviewerVR::ProcessVRCamera()
{
	// [UI and hud text code remains exactly identical]
	if (hud_textp == NULL)
	{
		hud_textp = (LLHUDText *)LLHUDObject::addHUDObject(LLHUDObject::LL_HUD_TEXT);
		if (hud_textp != NULL)
		{
			hud_textp->setZCompare(FALSE);
			hud_textp->setColor(LLColor4(1, 1, 1));
			hud_textp->setHidden(FALSE);
			hud_textp->setMaxLines(-1);
			m_strHudText.append("Press CTRL+TAB to enable VR mode\n Press TAB to remove this message");
			hud_textp->setString(m_strHudText);
			m_strHudText = "";
		}
	}
	else
	{
		m_vdir = LLViewerCamera::getInstance()->getAtAxis();
		m_vpos = LLViewerCamera::getInstance()->getOrigin();
		LLVector3 end = m_vpos + (m_vdir)* 1.0f;
		hud_textp->setPositionAgent(end);
	}
		
	if (!gHMD) return FALSE;

	if (m_bVrActive)
	{
		InitUI();

		// passive near clip override logic retained
		if (gVrModSettings->nearClip > 0.0f) 
		{
			static struct { F32 mView; F32 mAspect; F32 mNearPlane; F32 mFarPlane; } frustumProxy{};
			LLViewerCamera::getInstance()->writeFrustumToBuffer(reinterpret_cast<char*>(&frustumProxy));
			frustumProxy.mNearPlane = gVrModSettings->nearClip;
			LLViewerCamera::getInstance()->readFrustumFromBuffer(reinterpret_cast<const char*>(&frustumProxy));
		}

		if (!leftEyeDesc.IsReady && !rightEyeDesc.IsReady)
		{
			LLWindow * WI = gViewerWindow->getWindow();
			WI->getCursorPosition(&m_MousePos);
			
			LLCoordWindow m_ScrSizeOld;
			WI->getSize(&m_ScrSizeOld);
			float mult = (m_nRenderHeight < m_nRenderWidth) ? (float)m_nRenderHeight / (float)m_nRenderWidth : (float)m_nRenderWidth / (float)m_nRenderHeight;
			
			//Set the windows max size and aspect ratio to fit with the HMD.
#ifdef _WIN32
		int scrsize = std::min(GetSystemMetrics(SM_CYSCREEN), GetSystemMetrics(SM_CXSCREEN));
        LLWindow *win = gViewerWindow->getWindow();
        HWND hwnd = (HWND)win->getPlatformWindow();
	    HMONITOR hMonitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
	    MONITORINFO mi = { sizeof(mi) };
	    if (GetMonitorInfo(hMonitor, &mi)) {
    	    int fullHeight = mi.rcMonitor.bottom - mi.rcMonitor.top;
	        int workHeight = mi.rcWork.bottom - mi.rcWork.top;
			scrsize = workHeight;
		}

#else
    int scrsize = 1080;
#endif
			// Quick fallback for window resizing logic
			m_ScrSize.mX = (scrsize*mult)*0.95;
			m_ScrSize.mY = scrsize*0.95;
			if (m_ScrSizeOld.mX != m_ScrSize.mX || m_ScrSizeOld.mY != m_ScrSize.mY) {
				fprintf(stdout, "setSize(%d, %d) was(%d, %d)\n", m_ScrSize.mX, m_ScrSize.mY, m_ScrSizeOld.mX, m_ScrSizeOld.mY); fflush(stdout);
				WI->setSize(m_ScrSize);
			}

			// Store current camera values
			m_vdir_orig = LLViewerCamera::getInstance()->getAtAxis();
			m_vup_orig = LLViewerCamera::getInstance()->getUpAxis();
			m_vleft_orig = LLViewerCamera::getInstance()->getLeftAxis();
			m_vpos_orig = LLViewerCamera::getInstance()->getOrigin();
			
			if (!m_bEditActive)
			{
				glm::vec4 row2 = glm::row(m_mat4HMDPose, 2);
				m_vdir.setVec(row2.x, -row2.z, row2.y);
				
				glm::vec4 row1 = glm::row(m_mat4HMDPose, 1);
				m_vup.setVec(row1.x, -row1.z, row1.y);
				
				glm::vec4 row0 = glm::row(m_mat4HMDPose, 0);
				m_vleft.setVec(row0.x, -row0.z, row0.y);
				
				glm::vec4 row3 = glm::row(m_mat4HMDPose, 3);
				gHmdPos.setVec(row3.x, -row3.z, row3.y);

				if (gHmdOffsetPos.mV[VZ] == 0) gHmdOffsetPos = gHmdPos;

				LLQuaternion qCameraOrig(m_vdir_orig, m_vleft_orig, m_vup_orig);
				float r3, p3, y3;
				qCameraOrig.getEulerAngles(&r3, &p3, &y3);

				LLQuaternion qHMDRot(m_vdir, m_vleft, m_vup);
				float r1, p1, y1;
				qHMDRot.getEulerAngles(&r1, &p1, &y1);

				LLQuaternion qCameraOffset;
				qCameraOffset.setEulerAngles(r3, p3, y3 - (m_fCamRotOffset * DEG_TO_RAD));
				qHMDRot = qHMDRot*qCameraOffset;
				// gHMDQuat = qHMDRot;
				
				LLMatrix3 m3 = qHMDRot.getMatrix3();
				m_vdir = -m3.getFwdRow();
				m_vup = m3.getUpRow();
				m_vleft = m3.getLeftRow();
				m_vdir.normalize();
				m_vup.normalize();
				m_vleft.normalize();

				m_vpos = m_vpos_orig + (((gHmdPos - gHmdOffsetPos))* (qCameraOffset));
			}
			else
			{
				m_vdir = m_vdir_orig;
				m_vup = m_vup_orig;
				m_vleft = m_vleft_orig;
				m_vpos = m_vpos_orig;
			}

			if (m_bDebugKeyDown) Debug();
			else if (m_bVrActive && hud_textp && !hud_textp->getHidden()) {
				fprintf(stdout, "hud_textp->setHidden(TRUE)\n");fflush(stdout);
				hud_textp->setHidden(TRUE);
			}
		}

		LLVector3 new_dir;
		if (m_bEditActive) {
			if (eyeDistance() == 0) LLViewerCamera::getInstance()->lookDir(m_vdir_orig, m_vup_orig);
			new_dir = (m_vleft * (eyeDistance() / 1000));
		} else {
			if (eyeDistance() == 0) LLViewerCamera::getInstance()->lookDir(m_vdir, m_vup);
			new_dir = (-m_vleft * (eyeDistance() / 1000));
		}
			
		if (eyeDistance() != 0)
		{	
			LLVector3 new_fwd_pos = m_vpos + (m_vdir * m_fFocusDistance);
			// fprintf(stdout, "[%s] updateCameraLocation(<%f,%f,%f>)\n",
			// 	!leftEyeDesc.IsReady ? "left" : !rightEyeDesc.IsReady ? "right" : "??",
			// 	(m_vpos + new_dir)[0], (m_vpos + new_dir)[1], (m_vpos + new_dir)[2]);fflush(stdout);
			if (!leftEyeDesc.IsReady) {
				LLViewerCamera::getInstance()->updateCameraLocation(m_vpos + new_dir, m_vup, new_fwd_pos);
			} else if (!rightEyeDesc.IsReady) {
				LLViewerCamera::getInstance()->updateCameraLocation(m_vpos - new_dir, m_vup, new_fwd_pos);
			}
		}
	}
	return TRUE;
}

void llviewerVR::vrDisplay()
{
	if (!gHMD || !m_bVrActive) return;

	// [Zoom indexing and FBO blitting logic retained exactly as is]
	if (!leftEyeDesc.IsReady)
	{
		bx = 0;
		by = 0;
		tx = gPipeline.mRT->screen.getWidth();
		ty = gPipeline.mRT->screen.getHeight();

		// m_iTextureShift = ((tx / 2) / 100)* m_fTextureShift;

		S32 halfx = tx / 2;
		S32 halfy = ty / 2;
		S32 div8x = tx / 6;
		S32 div8y = ty / 6;

		S32 thirdx = tx / 3;
		S32 thirdy = ty / 3;
		

		if (m_MousePos.mX > tx - div8x && m_MousePos.mY < div8y)//up right
		{
			m_iZoomIndex = 4;
		}
		else if (m_MousePos.mX > tx - div8x && m_MousePos.mY > ty - div8y)//down right
		{
			m_iZoomIndex = 5;
		}
		else if (m_MousePos.mX < div8x && m_MousePos.mY > ty - div8y)//down left 
		{
			m_iZoomIndex = 6;
		}
		else if (m_MousePos.mX < div8x && m_MousePos.mY < div8y)//up left
		{
			m_iZoomIndex = 7;
		}
		else if (m_MousePos.mX > tx - div8x && m_MousePos.mY > halfy - div8y && m_MousePos.mY < halfy + div8y)//right
		{
			m_iZoomIndex = 10;
		}
		else if (m_MousePos.mY > ty - div8y &&  m_MousePos.mX > halfx - div8x &&  m_MousePos.mX < halfx + div8x)//down
		{
			m_iZoomIndex = 9;
		}
		else if (m_MousePos.mY < div8y &&  m_MousePos.mX >  halfx - div8x &&  m_MousePos.mX < halfx + div8x)//up
		{
			m_iZoomIndex = 8;
		}
		else if (m_MousePos.mX <  div8x && m_MousePos.mY > halfy - div8y && m_MousePos.mY < halfy + div8y)//left
		{
			m_iZoomIndex = 11;
		}
		else if (m_MousePos.mX > halfx - div8x && m_MousePos.mX < halfx + div8x && m_MousePos.mY > halfy - div8y && m_MousePos.mY < halfy + div8y)//center
		{
			m_iZoomIndex = 0;
		}

		///Zoom in
		if (m_iZoomIndex == 0 || !gVrModSettings->mousezoom)
		{
			bx +=   m_fTextureZoom;
			by +=   m_fTextureZoom;
			tx -=   m_fTextureZoom;
			ty -=   m_fTextureZoom;
		}
		else if (m_iZoomIndex == 4)//up right
		{
			bx += thirdx;
			by += thirdy;
			tx += thirdx;
			ty += thirdy;
		}
		else if (m_iZoomIndex == 5)//down right
		{
			bx += thirdx;
			by -= thirdy;
			tx += thirdx;
			ty -= thirdy;
		}
		else if (m_iZoomIndex == 6)//down left 
		{
			bx -= thirdx;
			by -= thirdy;
			tx -= thirdx;
			ty -= thirdy;
		}
		else if (m_iZoomIndex == 7)//up left
		{
			bx -= thirdx;
			by += thirdy;
			tx -= thirdx;
			ty += thirdy;
		}
		else if (m_iZoomIndex == 8)//up 
		{
			by += thirdy;
			ty += thirdy;
		}
		else if (m_iZoomIndex == 9)//down
		{
			by -= thirdy;
			ty -= thirdy;
		}
		else if (m_iZoomIndex == 11)//left
		{
			bx -= thirdx;
			tx -= thirdx;
		}
		else if (m_iZoomIndex == 10)//right
		{
			bx += thirdx;
			tx += thirdx;
		}
	}
	
	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	glReadBuffer(GL_BACK);

	if (!leftEyeDesc.IsReady)
	{
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, leftEyeDesc.mFBO);
		glClear(GL_COLOR_BUFFER_BIT);
		F32 uMin, uMax, vMin, vMax;
		calcUVBounds(xvr::Eye::Left, &uMin, &uMax, &vMin, &vMax);
		glBlitFramebuffer(bx, by, tx, ty, m_nRenderWidth * uMin, m_nRenderHeight * vMin, m_nRenderWidth * uMax, m_nRenderHeight * vMax, GL_COLOR_BUFFER_BIT, GL_LINEAR);
	}
	
	if ((leftEyeDesc.IsReady && !rightEyeDesc.IsReady) || eyeDistance() == 0)
	{
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, rightEyeDesc.mFBO);
		glClear(GL_COLOR_BUFFER_BIT);
		rightEyeDesc.IsReady = TRUE;
		F32 uMin, uMax, vMin, vMax;
		calcUVBounds(xvr::Eye::Right, &uMin, &uMax, &vMin, &vMax);
		glBlitFramebuffer(bx, by, tx, ty, m_nRenderWidth * uMin, m_nRenderHeight * vMin, m_nRenderWidth * uMax, m_nRenderHeight * vMax, GL_COLOR_BUFFER_BIT, GL_LINEAR);
	}
	
	if (!leftEyeDesc.IsReady) leftEyeDesc.IsReady = TRUE;
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	if (leftEyeDesc.IsReady && (rightEyeDesc.IsReady || eyeDistance() == 0))
	{
		rightEyeDesc.IsReady = FALSE;
		leftEyeDesc.IsReady = FALSE;

		// xopenvr: submit both eyes together (handles PostPresentHandoff internally)
		gHMD->submit(xvr::Eye::Left, leftEyeDesc.m_nResolveTextureId, xvr::Eye::Right, rightEyeDesc.m_nResolveTextureId);
		
		UpdateHMDMatrixPose();

		if (int preview = gVrModSettings->preview) {
	        gUIProgram.bind();
    	    LLGLSUIDefault gls_ui;
			if (preview == +2 || preview == -2) {
				opengl::blit_side_by_side(
					{ m_nRenderWidth, m_nRenderHeight },
					{ gViewerWindow->getWindowWidthScaled(), gViewerWindow->getWindowHeightScaled() }, 
					preview == -2 ? rightEyeDesc.mFBO : leftEyeDesc.mFBO,
					preview == -2 ? leftEyeDesc.mFBO : rightEyeDesc.mFBO 
				);
			} else if (preview == +1 || preview == -1) {
				opengl::blit_bordered(
					{ m_nRenderWidth, m_nRenderHeight },
					{ gViewerWindow->getWindowWidthScaled(), gViewerWindow->getWindowHeightScaled() }, 
					preview == -1 ? leftEyeDesc.mFBO : rightEyeDesc.mFBO,
					preview == -1 ? glm::vec4{ 1, 0, 0, 1} : glm::vec4{ 0, 0, 1, 1 }
				);
			}
	        gUIProgram.unbind();
		}
	}
}

void llviewerVR::ProcessVREvent(const xvr::Event & event) 
{
	if (event.type == xvr::Event::Quit)
	{
		m_bVrActive = FALSE;
		m_bVrEnabled = FALSE;
		gHMD->AcknowledgeQuit_Exiting();
		gHMD = nullptr;
	}
}

// void llviewerVR::agentYaw(F32 yaw_inc)  
// {
// 	if (gAgentCamera.cameraMouselook()  && gSavedSettings.getBOOL("JoystickMouselookYaw"))
// 		gAgent.rotate(-yaw_inc, gAgent.getReferenceUpVector());
// 	else {
// 		if (yaw_inc < 0) gAgent.setControlFlags(AGENT_CONTROL_YAW_POS);
// 		else if (yaw_inc > 0) gAgent.setControlFlags(AGENT_CONTROL_YAW_NEG);
// 		gAgent.yaw(-yaw_inc);
// 	}
// }

bool llviewerVR::HandleInput()
{
	if (!gHMD || !m_bVrActive) return FALSE;

	while (auto const& event = gHMD->acceptPendingEvent())
	{
		ProcessVREvent(event);
	}

	// TODO: xopenvr input (axis/buttons) migration needed here for SteamVR controller state.
	// Previously polled gHMD->GetControllerState.

	return false;
}

void llviewerVR::HandleKeyboard()
{
	bool ctrl = gKeyboard->getKeyDown(KEY_CONTROL);
	bool tab = gKeyboard->getKeyDown(KEY_TAB);

	if (tab && !m_bVrKeyDown) m_bVrKeyDown = TRUE;
	else if (!tab && m_bVrKeyDown)
	{
		fprintf(stdout, "HandleKeyboard gHMD=%p ctrl=%d, tab=%d, ctrltab=%d\n", gHMD.get(), ctrl, tab, ctrl&tab);fflush(stdout);

		m_bVrKeyDown = FALSE;
		if (ctrl)
		{
			m_bVrEnabled = !m_bVrEnabled;
			m_bVrActive = FALSE;
			vrStartup(FALSE);
		}
		else if (!ctrl && gHMD)
		{
			m_bVrActive = !m_bVrActive;
			gHmdOffsetPos.mV[2] = 0;
			if (!m_bVrActive) {
				m_strHudText = "";
				m_strHudText.append("\nVR driver ready.\n Press TAB to enter VR mode.");
				if (hud_textp) {
					hud_textp->setString(m_strHudText);
					hud_textp->setHidden(FALSE);
				}
			}
		}
		else if (!ctrl && hud_textp && !hud_textp->getHidden())
		{
			m_strHudText = "";
			fprintf(stdout, "hud_textp->setString(%s)\n", m_strHudText.c_str());fflush(stdout);
			hud_textp->setString(m_strHudText);
			hud_textp->setDoFade(FALSE);
			hud_textp->setHidden(TRUE);
		}
	}

	if (!gHMD) return;

	if (gKeyboard->getKeyDown(KEY_F4) && !m_bEditKeyDown) m_bEditKeyDown = TRUE;
	else if (!gKeyboard->getKeyDown(KEY_F4) && m_bEditKeyDown)
	{
		m_bEditKeyDown = FALSE;
		m_bEditActive = !m_bEditActive;
	}

	if (gKeyboard->getKeyDown(KEY_F3) && !m_bDebugKeyDown) m_bDebugKeyDown = TRUE;
	else if (!gKeyboard->getKeyDown(KEY_F3) && m_bDebugKeyDown)
	{
		m_bDebugKeyDown = FALSE;
		if (m_iZoomIndex > 7) m_iZoomIndex = 0;
	}
}

void llviewerVR::DrawCursors()
{
	if (!m_bVrActive || (!gVrModSettings->handlasers && !gVrModSettings->mousecursor)) return;
	
	// [Cursor rendering retained, controller iterating updated to xopenvr indexes]
	gUIProgram.bind();
	gGL.pushMatrix();
	S32 half_width = (gViewerWindow->getWorldViewWidthScaled() / 2);
	S32 half_height = (gViewerWindow->getWorldViewHeightScaled() / 2);

	gGL.translatef((F32)half_width, (F32)half_height, 0.f);
	gGL.color4fv(LLColor4::white.mV);

	if (!gVrModSettings->handlasers) {
		for (uint32_t unTrackedDevice = xvr::HMD + 1; unTrackedDevice < xvr::MAXDEVICECOUNT; ++unTrackedDevice)
		{
			if (gCtrlscreen[unTrackedDevice].mX > -1)
				gl_circle_2d(gCtrlscreen[unTrackedDevice].mX - half_width, gCtrlscreen[unTrackedDevice].mY - half_height + gCursorDiff, half_width / 200, 8, TRUE);
		}
	}

	if (gVrModSettings->mousecursor)
	if (gAgentCamera.getCameraMode() != CAMERA_MODE_MOUSELOOK)
	{
		LLColor4 cl;
		cl = LLColor4::black.mV;

		S32 wwidth = gViewerWindow->getWindowWidthScaled();
		S32 wheight = gViewerWindow->getWindowHeightScaled();
		LLCoordGL mpos = gViewerWindow->getCurrentMouse();
		S32 mx = mpos.mX - half_width;
		S32 my = mpos.mY - (half_height);
		if (mpos.mX < 0 || mpos.mX > wwidth)
			mx = half_width;
		if (mpos.mY < 0 || mpos.mY > wheight)
			my = half_height;

		gl_triangle_2d(mx, my, mx + 8, my - 15, mx + 15, my - 8, cl, TRUE);
		cl = LLColor4::white.mV;
		gl_triangle_2d(mx+2, my-2, mx + 9, my - 13, mx + 12, my - 8, cl, TRUE);
	}

	//gl_circle_2d(mpos.mX - half_width, mpos.mY - (half_height)  /*+ gVR.gCursorDiff)*/, half_width / 200, 8, TRUE);

	//glEnable(GL_DEPTH_TEST);
	gGL.popMatrix();
	gUIProgram.unbind();
}

void llviewerVR::RenderControllerAxes()
{
	if (!gHMD) return;
	HandleInput();

	if (!m_bVrActive || !gVrModSettings->handcontrollers) return;

	gUIProgram.bind();

	for (uint32_t unTrackedDevice = xvr::HMD + 1; unTrackedDevice < xvr::MAXDEVICECOUNT; ++unTrackedDevice)
	{
		gCtrlscreen[unTrackedDevice].set(-1, -1);
		
		glm::mat4 mat;
		if (!gHMD->getValidDevicePose((xvr::Device)unTrackedDevice, mat)) continue;
		
		glm::vec4 row2 = glm::row(mat, 2);
		LLVector3 dir(row2.x, -row2.z, row2.y);

		glm::vec4 row1 = glm::row(mat, 1);
		LLVector3 up(row1.x, -row1.z, row1.y);

		glm::vec4 row0 = glm::row(mat, 0);
		LLVector3 left(row0.x, -row0.z, row0.y);

		glm::vec4 row3 = glm::row(mat, 3);
		gCtrlOrigin[unTrackedDevice].setVec(row3.x, -row3.z, row3.y);

		LLQuaternion q1(dir, left, up);
		LLQuaternion qCameraOrig(m_vdir_orig, m_vleft_orig, m_vup_orig);
		float r3, p3, y3;
		qCameraOrig.getEulerAngles(&r3, &p3, &y3);

		LLQuaternion q3;
		q3.setEulerAngles(0, 0, y3 - (m_fCamRotOffset * DEG_TO_RAD));
		q1 = (q1)*q3;

		LLMatrix3 m3 = q1.getMatrix3();
		dir = m3.getFwdRow();
		dir.normalize();

		gCtrlOrigin[unTrackedDevice] -= gHmdPos;
		gCtrlOrigin[unTrackedDevice] = m_vpos + gCtrlOrigin[unTrackedDevice] * q3;
		gCtrlPos[unTrackedDevice] = gCtrlOrigin[unTrackedDevice]  - (dir * 10.0f);

		posToScreen(gCtrlPos[unTrackedDevice], gCtrlscreen[unTrackedDevice], FALSE);
		
		S32 height = gViewerWindow->getWorldViewHeightScaled();
		gCursorDiff= gViewerWindow->getWindowHeightScaled();
		gCursorDiff = gCursorDiff - height;
		gCtrlscreen[unTrackedDevice].mY -= gCursorDiff;
	
		if (gVrModSettings->handlasers) {
			LLGLSUIDefault gls_ui;
			gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
			LLVector3 v = gCurrentCameraPos;	
			glClear(GL_DEPTH_BUFFER_BIT);
			glDisable(GL_DEPTH_TEST);
			gGL.pushMatrix();
			gGL.translatef(v.mV[VX], v.mV[VY], v.mV[VZ]);
			gGL.begin(LLRender::LINES);
			gGL.color3f(1.0f, 0.0f, 0.0f);
			gGL.vertex3f(gCtrlOrigin[unTrackedDevice].mV[VX], gCtrlOrigin[unTrackedDevice].mV[VY], gCtrlOrigin[unTrackedDevice].mV[VZ]);
			gGL.vertex3f(gCtrlPos[unTrackedDevice].mV[VX], gCtrlPos[unTrackedDevice].mV[VY], gCtrlPos[unTrackedDevice].mV[VZ]);
			gGL.end();
			gGL.popMatrix();
			glEnable(GL_DEPTH_TEST);
		}

		// TODO: xopenvr input integration for analog stick rotation and buttons
	}
	gUIProgram.unbind();
}

// [posToScreen, button callbacks, Debug, and InitUI remain largely unchanged]

BOOL llviewerVR::posToScreen(const LLVector3 &pos_agent, LLCoordGL &out_point, const BOOL clamp) const
{
	LLRect world_view_rect = gViewerWindow->getWorldViewRectRaw();
	S32	viewport[4] = { world_view_rect.mLeft, world_view_rect.mBottom, world_view_rect.getWidth(), world_view_rect.getHeight() };
	F64 mdlv[16]; F64 proj[16];
	for (U32 i = 0; i < 16; i++) { mdlv[i] = (F64)gGLModelView[i]; proj[i] = (F64)gGLProjection[i]; }

	GLdouble x, y, z;
	if (GL_TRUE == gluProject(pos_agent.mV[VX], pos_agent.mV[VY], pos_agent.mV[VZ], mdlv, proj, (GLint*)viewport, &x, &y, &z)) {
		x /= gViewerWindow->getDisplayScale().mV[VX];
		y /= gViewerWindow->getDisplayScale().mV[VY];
		out_point.mX = lltrunc(x); out_point.mY = lltrunc(y);
		return TRUE;
	}
	return FALSE;
}

void llviewerVR::buttonCallbackLeft() { if (m_pCamStack) { m_fCamRotOffset -= 5; if (m_fCamRotOffset > 360) m_fCamRotOffset = 0; } }
void llviewerVR::buttonCallbackRight() { 
	if (m_pCamStack) { 
		m_fCamRotOffset += 5; if (m_fCamRotOffset > 360) m_fCamRotOffset = 0; 
		LLRect rc; rc.setCenterAndSize(80, 80, 160, 80); m_pCamStack->setRect(rc);
		if (m_pCamera_floater) {
			rc = m_pCamera_floater->getRect();
			rc.setCenterAndSize(rc.getCenterX(), rc.getCenterY(), 200, 120);
			m_pCamera_floater->handleReshape(rc, TRUE);
		}
	} 
}

void llviewerVR::Debug() {
	if (!gHMD || !hud_textp) return;
	std::string str = "Debug View...\n";
	hud_textp->setString(str);
	LLVector3 end = m_vpos + (m_vdir)* 1.0f;
	hud_textp->setPositionAgent(end);
	hud_textp->setDoFade(FALSE);
	hud_textp->setHidden(FALSE);
}

void llviewerVR::InitUI()
{
		if (!m_pCamButtonLeft)
		{
			/*LLPanel* panelp = NULL;
			panelp=LLPanel::createFactoryPanel("vr_controlls");

			panelp->setRect(rc);
			LLColor4 color(1, 1, 1);
			panelp->setOrigin(700, 700);
			panelp->setColor(color);
			panelp->setVisible(TRUE);
			panelp->setEnabled(TRUE);*/
			LLRect rc;
			rc.setCenterAndSize(500, 500, 200, 200);


			m_pCamera_floater = LLFloaterReg::findTypedInstance<LLFloaterCamera>("camera");
			if (m_pCamera_floater)
			{
				LLStringExplicit lb("keks");
				//LLRect rc;
				//LLButton  *m_pButton1;
				LLButton  *m_pButton;
				for (int i = 0; i < 2; i++)
				{

					LLButton::Params p;
					if (i == 0)
					{
						p.name("rot_left");
						p.label("<<");
					}
					else
					{
						p.name("rot_right");
						p.label(">>");
					}


					m_pButton = LLUICtrlFactory::create<LLButton>(p);


					m_pCamera_floater->addChild(m_pButton);
					//panelp->addChild(m_pButton);
					if (i == 0)
						rc.setCenterAndSize(20, 20, 30, 20);
					else
						rc.setCenterAndSize(50, 20, 30, 20);
					m_pButton->setRect(rc);
					m_pButton->setVisible(TRUE);
					m_pButton->setEnabled(TRUE);
					if (i == 0)
						m_pCamButtonLeft = m_pButton;
					else
						m_pCamButtonRight = m_pButton;
				}
				m_pCamButtonLeft->setCommitCallback(boost::bind(&llviewerVR::buttonCallbackLeft, this));
				m_pCamButtonRight->setCommitCallback(boost::bind(&llviewerVR::buttonCallbackRight, this));

				/*m_pButton1 = m_pCamera_floater->findChild<LLButton>("rot_left");
				//lb.assign("<");
				if (m_pButton1)
				{
				m_pButton1->setLabel(LLStringExplicit("<"));
				m_pButton1->setCommitCallback(boost::bind(&llviewerVR::buttonsCallback, this));
				}*/


				m_pCamStack = m_pCamera_floater->findChild<LLView>("camera_view_layout_stack");


				//m_pCamera_floater->getChildList();
				//m_pButton1->
			}
		}
}
