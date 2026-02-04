#ifndef SLOS_UIRT_H
#define SLOS_UIRT_H

#include <glm/glm.hpp>
#include <functional>
#include <string>

#include <humbletim/opengl.hpp>

class LLRenderTarget;

namespace slos {
    int get_window_cursor_type();
    bool gl_blit_hcursor(intptr_t hcursor);
    bool gl_blit_cursor(int type);
    void tempDebugRender(uint32_t tex_glTextureId, std::function<void()> ui_blit = nullptr);
    
    void gl_blit_side_by_side(glm::ivec2 viewport, glm::ivec2 wh, uint32_t leftFBO, uint32_t rightFBO);
    void gl_blit_functor(glm::ivec2 const& viewport, glm::ivec2 const& extents, std::function<void()> functor);
    void gl_noblend_blit_texture(uint32_t glTextureId, glm::vec2 const& renderSize);
    // void gl_blit_texture(uint32_t glTextureId, glm::vec2 const& renderSize);
    void gl_withUpper(glm::ivec2 const& eye_extents, std::function<void()> const& functor);
    void gl_withWUpper(std::function<void()> const& functor);
    void gl_withFBODrawBinder(uint32_t fboID, glm::ivec2 const& eye_extents, std::function<void()> const& functor);

    namespace xx {
        struct LineSegment { glm::vec3 a,b; glm::vec4 color{}; };
        struct WorldText { std::string text; glm::vec3 pos; glm::vec4 color{}; glm::vec3 dir{}; };
        struct WorldTransforms { std::string text; glm::vec4 color; glm::mat4 transform; };
        void withBlah(std::function<void()> const&);
    }
    void gl_render_world_texts(glm::vec3 const& origin, std::vector<xx::WorldText> const& texts);
    void gl_render_world_transforms(glm::mat4 const& origin, std::vector<xx::WorldTransforms> const& transforms);
    void gl_render_world_text(std::string const& text, glm::vec3 pos, glm::vec4 const& color);
    void gl_render_world_line_segments(glm::vec3 const& origin, std::vector<xx::LineSegment> const& segments);

    void gl_render_all_hud_texts();
    void gl_render_ui_screen_text(std::string const& text, glm::ivec2 mouse, glm::vec4 const& color);
    void gl_render_ui_screen_text_multiline(std::string const& text, glm::ivec2 mouse, glm::vec4 const& color, bool bottom = false);
    bool _saveDepth(std::string const& filename, uint32_t fboID);
    bool _save(std::string const& filename, uint32_t fboID);
    bool _saveTexture(std::string const& filename, uint32_t texID);

    struct MyRenderTarget {
        std::string debug_name;
        MyRenderTarget(const char* name) : debug_name(name) {}
        std::shared_ptr<LLRenderTarget> _prt;
        glm::ivec2 _size{ -1, -1 };
        bool useDepth{ false };
        bool useMip{ false };
        glm::ivec2 rtsize() const;
        virtual bool ensure(int width, int height);
        virtual void reset();
        bool save(std::string const& filename) const;
        bool saveDepth(std::string const& filename) const;
        uint32_t fboID() const;
        uint32_t texID() const;
        uint32_t depthID() const;
    };

    struct UIRenderTarget : MyRenderTarget {
        using MyRenderTarget::MyRenderTarget;
        std::shared_ptr<opengl::PBOCursor> _cursor;
        bool capture();
        bool blit(glm::ivec2 vp) const;
        virtual bool ensure(int width, int height) override;
        virtual void reset() override;
    };

    struct EyeRenderTarget : MyRenderTarget {
        using MyRenderTarget::MyRenderTarget;
        bool capture();
    };

} //ns

#endif //SLOS_UIRT_H

#ifdef SLOS_UIRT_IMPLEMENTATION

#include "llrendertarget.h"
#include "llviewerwindow.h"
#include "pipeline.h"
#include "llviewercontrol.h"
#include "llimagepng.h"
#include "llimagejpeg.h"
#include "lltextbase.h"
namespace asdf {
    extern glm::ivec4 SWIZZLE();
    extern glm::vec3 V3();
}

namespace slos {

    // MyRenderTarget::~MyRenderTarget() {}

    void MyRenderTarget::reset() {
        _prt.reset();
        _size = {-1, -1};
    }

    glm::ivec2 MyRenderTarget::rtsize() const {
        if (!_prt) return glm::ivec2{};
        return { _prt->getWidth(), _prt->getHeight() };
    }
    bool MyRenderTarget::ensure(int width, int height) {
        if (!_prt) {
            _prt = std::make_shared<LLRenderTarget>();
            via_foaf(_prt, { mMipLevels = 0; }); // LLRenderTarget TMG_NONE doesn't initialized this member
        }

        const char* force = nullptr;
        if (_prt->getWidth() != width || _prt->getHeight() != height) force = "~size";
        else if (_prt->getDepth() && !useDepth) force = "-depth";
        else if (via_foaf(_prt, { return mMipLevels; }) && !useMip) force = "-mip";
        else if (!_prt->getDepth() && useDepth) force = "+depth";
        else if (!via_foaf(_prt, { return mMipLevels; }) && useMip) force = "+mip";
        if (force) {
            if (!_prt->allocate(width, height, GL_RGBA, useDepth, LLTexUnit::TT_TEXTURE, useMip ? LLTexUnit::TMG_AUTO : LLTexUnit::TMG_NONE)) {
                fprintf(stdout, "Failed to allocate rendertarget '%s'\n", debug_name.c_str()); fflush(stdout);
                return false;
            }
            else {
                _size = { width, height };
                fprintf(stdout, "[%s] allocated rendertarget '%s' [%dx%d] (texID=%d, depth=%d, mip=%d)\n", force, debug_name.c_str(), _prt->getWidth(), _prt->getHeight(), 
                    texID(), _prt->getDepth(), via_foaf(_prt, { return mMipLevels; })); fflush(stdout);
            }
        }
        return true;
    }

    bool EyeRenderTarget::capture() {
        GLuint srcFBO = slos::get_pipeline_fbo_name();
        // capture current pipeline buffer to local per-eye fbo 
        glBindFramebuffer(GL_READ_FRAMEBUFFER, srcFBO);
        if (srcFBO == 0) glReadBuffer(GL_BACK); // Only needed for window/backbuffer
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fboID());

        glClearColor(0,0,0,1);//if (_eye == -1) glClearColor(1,0,0,1); else glClearColor(0,0,1,1);
        // glColor4f(1,1,1,1);//if (_eye == -1) glClearColor(1,0,0,1); else glClearColor(0,0,1,1);
        glClear(GL_COLOR_BUFFER_BIT | (depthID() ? GL_DEPTH_BUFFER_BIT : 0));

        auto t = slos::get_pipeline_size();
        auto self = rtsize();
        glBlitFramebuffer(0, 0, t.x, t.y, // src
                          0, 0, self.x, self.y, // dst
                        GL_COLOR_BUFFER_BIT | (depthID() ? GL_DEPTH_BUFFER_BIT : 0), depthID() ? GL_NEAREST : GL_LINEAR);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return true;
    }

    void UIRenderTarget::reset() {
        MyRenderTarget::reset();
        _cursor.reset();
    }

    bool UIRenderTarget::ensure(int width, int height) {
        auto before = fboID();
        if (!MyRenderTarget::ensure(width, height)) return false;
        if (!_cursor || before != fboID()) _cursor = std::make_shared<opengl::PBOCursor>(fboID());
        return true;
    }
    bool UIRenderTarget::blit(glm::ivec2 sz) const {
        if (!_prt) return false;
        LLGLDepthTest maybedepth(useDepth ? GL_TRUE : GL_FALSE, useDepth ? GL_TRUE : GL_FALSE);
        // LLGLEnable gls_alpha_test(GL_ALPHA_TEST);
        // glAlphaFunc(GL_GREATER, 0.99f); // Or a value just below 1.0

        gGL.getTexUnit(0)->bind(_prt.get());
        if (useMip) {
            glGenerateMipmap(GL_TEXTURE_2D);
            gGL.getTexUnit(0)->setTextureFilteringOption(LLTexUnit::TFO_TRILINEAR);  
        }
        // gGL.color4f(1.f, 1.f, 1.f, 1.f);
        // if (scale != 1.0f && scale > 0.0f) {
        //     glm::vec2 margin = glm::vec2(sz) * (1.0f - scale) / 2.0f;
        //     gGL.translatef(margin.x, margin.y, 0.0f);
        //     gGL.scalef(scale, scale, 1.0f);
        // }
        gl_rect_2d_simple_tex(sz.x, sz.y);
        gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

        return true;
    }

    bool UIRenderTarget::capture() { //std::function<void()> const& postdraw) {
        LLRect r = gViewerWindow->getWindowRectRaw();
        if (!this->ensure(r.getWidth(), r.getHeight())) {
             return false;
        }

        if (!_prt) return false;

        U32 dest_fbo = LLRenderTarget::sCurFBO;    
        {
            gUIProgram.bind();
            LLGLSUIDefault gls_ui;
            {
                auto sz = _size;
                gl_state_for_2d(sz.x, sz.y);
                glViewport(0, 0, sz.x, sz.y);
                _prt->bindTarget();    
                glClearColor(1,1,1,0);
                LLGLDepthTest maybedepth(useDepth ? GL_TRUE : GL_FALSE, useDepth ? GL_TRUE : GL_FALSE);
                glClear(GL_COLOR_BUFFER_BIT | (useDepth ? GL_DEPTH_BUFFER_BIT : 0));
                gGL.blendFunc(LLRender::BF_ONE, LLRender::BF_ZERO, LLRender::BF_ONE, LLRender::BF_ZERO);  
                gGL.pushMatrix();
        gGL.translatef(0.0f, 0.0f, 0.1f);
                gViewerWindow->draw();               
        gGL.popMatrix();

                _prt->flush();    
            }
            gUIProgram.unbind();
        }
        LLRenderTarget::sCurFBO = dest_fbo;    
        return true;
    }

    bool _save_rgba_data(std::string const& filename, opengl::RGBAData const& result) {
        fprintf(stdout, "_save_rgba_data -- width=%d height=%d texID=%d sz=%lu\n", result.width, result.height, result.id, result.bytes.size()); fflush(stdout);
        if (!result.id) return false;
        LLPointer<LLImageRaw> raw = new LLImageRaw( ( const U8*)result.bytes.data(), result.width, result.height, 4);  
        if (raw->isBufferInvalid()) {  
            fprintf(stdout, "Failed to allocate image buffer\n");fflush(stdout);
            return false;
        }  

        LLPointer<LLImageFormatted> image = LLImageFormatted::createFromExtension(filename);  
        if (image.isNull()) {  
            fprintf(stdout, "Unsupported file extension: %s\n", filename.c_str());fflush(stdout);  
            return false;  
        }  
        
        // Handle RGBA to RGB conversion for JPEG if needed  
        if (image->getCodec() == IMG_CODEC_JPEG) {  
            LLPointer<LLImageRaw> raw_rgb = new LLImageRaw(result.width, result.height, 3);  
            raw_rgb->copyUnscaled4onto3(raw);  
            raw = raw_rgb;  
        }  

        if (!image->encode(raw, 0)) { fprintf(stdout, "!encode\n"); fflush(stdout); return false; }
        fprintf(stdout, "saving %s\n", filename.c_str());fflush(stdout);
        if (!image->save(filename)) { fprintf(stdout, "!save\n"); return false; }
        fprintf(stdout, "//saved %s\n", filename.c_str());fflush(stdout);
        return true; 
    }

    bool _save_f32_grayscale_data(std::string const& filename, opengl::DepthDATA const& result) {
        if (!result.id) return false;

        fprintf(stdout, "_save_f32_grayscale_data -- width=%d height=%d depthID=%d sz=%lu\n", result.width, result.height, result.id, result.floats.size()); fflush(stdout);
        
        LLPointer<LLImageRaw> raw = new LLImageRaw( result.width, result.height, 1 );    
        if (raw->isBufferInvalid()) {    
            fprintf(stdout, "Failed to allocate image buffer\n");fflush(stdout);  
            return false;  
        }    
        
        auto nearClip = LLViewerCamera::getInstance()->getNear();
        auto farClip = LLViewerCamera::getInstance()->getFar();
        // Convert depth to grayscale (similar to snapshot code)  
        glm::float32 depth_conversion_factor_1 = (farClip + nearClip) / (2.f * farClip * nearClip);  
        glm::float32 depth_conversion_factor_2 = (farClip - nearClip) / (2.f * farClip * nearClip);  

        auto F32_to_U8 = [](float val, float lower, float upper) -> glm::uint8 {
            val = glm::clamp(val, lower, upper);
            val = (val - lower) / (upper - lower); // Normalize to [0, 1]            
            return static_cast<glm::uint8>(glm::floor(val * 255.0f)); // Scale to [0, 255] and floor to nearest integer
        };
        auto data_ptr = raw->getData();  
        for (size_t i = 0; i < result.floats.size(); i++) {  
            float depth_float = result.floats[i];  
            float linear_depth_float = 1.f / (depth_conversion_factor_1 - (depth_float * depth_conversion_factor_2));  
            glm::uint8 depth_byte = F32_to_U8(linear_depth_float, nearClip, farClip);  
            data_ptr[i] = depth_byte;  
        }  

        LLPointer<LLImagePNG> image = new LLImagePNG;  
        if (image.isNull()) {  
            fprintf(stdout, "Unsupported file extension: %s\n", filename.c_str());fflush(stdout);  
            return false;  
        }  
        
        if (!image->encode(raw, 0.0f)) { fprintf(stdout, "!encode\n"); fflush(stdout); return false; }  
        
        fprintf(stdout, "saving depth %s\n", filename.c_str());fflush(stdout);  
        
        if (!image->save(filename)) { fprintf(stdout, "!save\n"); return false; }  
    
        fprintf(stdout, "//saved depth %s\n", filename.c_str());fflush(stdout);  
        return true;   
    }

    bool _saveTexture(std::string const& filename, uint32_t texID) {
        return _save_rgba_data(filename, opengl::_get_texture_rgba(texID));
    }
    
    bool _save(std::string const& filename, uint32_t fboID) {
        return _save_rgba_data(filename, opengl::_get_fbo_attachment_rgba(fboID));
    }
    bool _saveDepth(std::string const& filename, uint32_t fboID) {
        return _save_f32_grayscale_data(filename, opengl::_get_fbo_attachment_depth(fboID));
    }

    bool MyRenderTarget::save(std::string const& filename) const {
        if (!_prt) {
            fprintf(stdout, "!target\n");fflush(stdout);
            return false;
        }
        if (!_prt->isComplete()) {
            fprintf(stdout, "!target isComplete\n");fflush(stdout);
            return false;
        }
        return _save(filename, fboID());
    }
    bool MyRenderTarget::saveDepth(std::string const& filename) const {  
        if (!_prt) {  
            fprintf(stdout, "!target\n");fflush(stdout);  
            return false;  
        }  
        if (!_prt->isComplete()) {  
            fprintf(stdout, "!target isComplete\n");fflush(stdout);  
            return false;  
        }  
        
        // Check if depth buffer exists  
        if (_prt->getDepth() == 0) {  
            fprintf(stdout, "!no depth buffer\n");fflush(stdout);  
            return false;  
        }  
        return _saveDepth(filename, fboID());
    }

    uint32_t MyRenderTarget::fboID() const {
        return _prt ? via_foaf(_prt.get(), { return mFBO;}) : 0;
    }

    uint32_t MyRenderTarget::texID() const {
        return _prt ? _prt->getTexture(0) : 0;
    }

    uint32_t MyRenderTarget::depthID() const {
        return _prt ? _prt->getDepth() : 0;
    }

}//ns

#include "llimagebmp.h"
namespace slos {

    struct HotCursor {
        // ECursorType type;
        LLPointer<LLImageRaw> raw{};
        glm::ivec2 hotspot{};
        glm::ivec3 extents{};
        uint32_t texID = 0;
    };
// Helper struct to define where the cursor comes from
struct CursorResource {
    LPCSTR id;
    bool is_system; // If true, use NULL module; if false, use GetModuleHandle(NULL)
};

// The static mapping function
// Note: We defaulted the legacy logic to the 'false' path for this pure function.
static CursorResource get_cursor_resource_id(ECursorType type) {
    static const std::unordered_map<ECursorType, CursorResource> type_to_res = {
        { UI_CURSOR_ARROW,       { IDC_ARROW, true } },
        { UI_CURSOR_WAIT,        { IDC_WAIT, true } },
        { UI_CURSOR_HAND,        { IDC_HAND, true } },
        { UI_CURSOR_IBEAM,       { IDC_IBEAM, true } },
        { UI_CURSOR_CROSS,       { IDC_CROSS, true } },
        { UI_CURSOR_SIZENWSE,    { IDC_SIZENWSE, true } },
        { UI_CURSOR_SIZENESW,    { IDC_SIZENESW, true } },
        { UI_CURSOR_SIZEWE,      { IDC_SIZEWE, true } },
        { UI_CURSOR_SIZENS,      { IDC_SIZENS, true } },
        { UI_CURSOR_SIZEALL,     { IDC_SIZEALL, true } },
        { UI_CURSOR_NO,          { IDC_NO, true } },
        { UI_CURSOR_WORKING,     { IDC_APPSTARTING, true } },
        
        // Custom Cursors (Module Local)
        { UI_CURSOR_TOOLGRAB,    { TEXT("TOOLGRAB"), false } },
        { UI_CURSOR_TOOLLAND,    { TEXT("TOOLLAND"), false } },
        { UI_CURSOR_TOOLFOCUS,   { TEXT("TOOLFOCUS"), false } },
        { UI_CURSOR_TOOLCREATE,  { TEXT("TOOLCREATE"), false } },
        { UI_CURSOR_ARROWDRAG,   { TEXT("ARROWDRAG"), false } },
        { UI_CURSOR_ARROWCOPY,   { TEXT("ARROWCOPY"), false } },
        { UI_CURSOR_ARROWDRAGMULTI, { TEXT("ARROWDRAGMULTI"), false } },
        { UI_CURSOR_ARROWCOPYMULTI, { TEXT("ARROWCOPYMULTI"), false } },
        { UI_CURSOR_NOLOCKED,    { TEXT("NOLOCKED"), false } },
        { UI_CURSOR_ARROWLOCKED, { TEXT("ARROWLOCKED"), false } },
        { UI_CURSOR_GRABLOCKED,  { TEXT("GRABLOCKED"), false } },
        { UI_CURSOR_TOOLTRANSLATE,{ TEXT("TOOLTRANSLATE"), false } },
        { UI_CURSOR_TOOLROTATE,  { TEXT("TOOLROTATE"), false } },
        { UI_CURSOR_TOOLSCALE,   { TEXT("TOOLSCALE"), false } },
        { UI_CURSOR_TOOLCAMERA,  { TEXT("TOOLCAMERA"), false } },
        { UI_CURSOR_TOOLPAN,     { TEXT("TOOLPAN"), false } },
        { UI_CURSOR_TOOLZOOMIN,  { TEXT("TOOLZOOMIN"), false } },
        { UI_CURSOR_TOOLZOOMOUT, { TEXT("TOOLZOOMOUT"), false } },
        { UI_CURSOR_TOOLPICKOBJECT3, { TEXT("TOOLPICKOBJECT3"), false } },
        { UI_CURSOR_PIPETTE,     { TEXT("TOOLPIPETTE"), false } },
        
        // Non-Legacy Mappings (Defaulting to 'false' block from reference)
        { UI_CURSOR_TOOLSIT,     { TEXT("TOOLSIT"), false } },
        { UI_CURSOR_TOOLBUY,     { TEXT("TOOLBUY"), false } },
        { UI_CURSOR_TOOLOPEN,    { TEXT("TOOLOPEN"), false } },
        // Note: Reference mapped TOOLPAY to TOOLBUY in the else block
        { UI_CURSOR_TOOLPAY,     { TEXT("TOOLBUY"), false } }, 

        { UI_CURSOR_TOOLPATHFINDING, { TEXT("TOOLPATHFINDING"), false } },
        { UI_CURSOR_TOOLPATHFINDING_PATH_START_ADD, { TEXT("TOOLPATHFINDINGPATHSTARTADD"), false } },
        { UI_CURSOR_TOOLPATHFINDING_PATH_START, { TEXT("TOOLPATHFINDINGPATHSTART"), false } },
        { UI_CURSOR_TOOLPATHFINDING_PATH_END, { TEXT("TOOLPATHFINDINGPATHEND"), false } },
        { UI_CURSOR_TOOLPATHFINDING_PATH_END_ADD, { TEXT("TOOLPATHFINDINGPATHENDADD"), false } },
        { UI_CURSOR_TOOLNO,      { TEXT("TOOLNO"), false } },
        
        // Color cursors (treated as standard resources here)
        { UI_CURSOR_TOOLPLAY,       { TEXT("TOOLPLAY"), false } },
        { UI_CURSOR_TOOLPAUSE,      { TEXT("TOOLPAUSE"), false } },
        { UI_CURSOR_TOOLMEDIAOPEN,  { TEXT("TOOLMEDIAOPEN"), false } }
    };

    auto it = type_to_res.find(type);
    if (it != type_to_res.end()) {
        return it->second;
    }
    // Fallback
    return { IDC_ARROW, true };
}

HotCursor get_hcursor_image(HCURSOR hCursor, ECursorType type = UI_CURSOR_ARROW) {
    if (!hCursor) {
        // Absolute failure (shouldn't happen on Windows)
        return {};
    }

    // 3. Get Icon Info (for Hotspot and Bitmaps)
    ICONINFO iconInfo = { 0 };
    if (!GetIconInfo(hCursor, &iconInfo)) {
        return {};
    }

    // cleanup_bitmaps helper to ensure we delete the bitmaps GetIconInfo created
    auto cleanup_bitmaps = [&](bool include_dc = false, HDC hdc = NULL) {
        if (iconInfo.hbmColor) DeleteObject(iconInfo.hbmColor);
        if (iconInfo.hbmMask) DeleteObject(iconInfo.hbmMask);
        if (include_dc && hdc) DeleteDC(hdc);
    };

    // 4. Determine Dimensions
    // If hbmColor is missing, it's a monochrome cursor, and hbmMask contains both AND/XOR masks stacked.
    BITMAP bmp = { 0 };
    if (iconInfo.hbmColor) {
        GetObject(iconInfo.hbmColor, sizeof(bmp), &bmp);
    } else {
        GetObject(iconInfo.hbmMask, sizeof(bmp), &bmp);
        bmp.bmHeight /= 2; // Monochrome masks are double height
    }

    int width = bmp.bmWidth;
    int height = bmp.bmHeight;

    // 5. Create a DIBSection to draw into (Pure 32-bit RGBA container)
    // We use a negative height to force a Top-Down DIB, matching most image buffer expectations
    BITMAPINFO bi = { 0 };
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = width;
    bi.bmiHeader.biHeight = height; // Top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    void* pBits = nullptr;
    HDC hScreenDC = GetDC(NULL);
    HDC hMemDC = CreateCompatibleDC(hScreenDC);
    HBITMAP hDIB = CreateDIBSection(hScreenDC, &bi, DIB_RGB_COLORS, &pBits, NULL, 0);
    ReleaseDC(NULL, hScreenDC);

    if (!hDIB || !pBits) {
        cleanup_bitmaps(true, hMemDC);
        if (hDIB) DeleteObject(hDIB);
        return {};
    }

    // 6. Draw the cursor into the DIB
    // Select the DIB into the DC
    HBITMAP hOldBmp = (HBITMAP)SelectObject(hMemDC, hDIB);
    
    // Clear buffer to transparent (0) just in case
    memset(pBits, 0, width * height * 4);

    // DrawIconEx handles the complex logic of combining Mask and Color bitmaps
    // DI_NORMAL draws the image as usual. DI_IMAGE usually ignores the mask, but for cursors we want the composite.
    // However, drawing a cursor onto empty 0x00000000 memory needs care.
    // Standard DrawIconEx might premultiply alpha or do XOR operations against the 0x00 background.
    DrawIconEx(hMemDC, 0, 0, hCursor, width, height, 0, NULL, DI_NORMAL);

    // 7. Extract and Swizzle Pixels
    // Win32 DIBs are BGRA. LLImageRaw (usually) expects RGBA.
    // Also, we need to ensure the Alpha channel is set correctly.
    // If the source was monochrome (no alpha channel in source), DrawIconEx usually results in 
    // black/white pixels with 0 alpha (if background was 0). We need to fix that.
    
    // We'll read directly from pBits since it's a memory mapped DIB.
    // But since DrawIconEx behavior on alpha can be finicky with legacy cursors, 
    // we iterate to Swizzle and Fix Alpha.
    
    std::vector<U8> rawData;
    rawData.resize(width * height * 4);
    
    U8* src = static_cast<U8*>(pBits);
    U8* dst = rawData.data();
    int numPixels = width * height;

    for (int i = 0; i < numPixels; ++i) {
        U8 b = src[0];
        U8 g = src[1];
        U8 r = src[2];
        U8 a = src[3];

        // If a cursor is drawn and alpha remains 0, but color is not black, 
        // it means it was likely a legacy cursor drawn onto our blank canvas.
        // In that case, we should manually set alpha to 255 for visible pixels.
        // (Simple heuristic: if A=0 but RGB!=0, make opaque).
        if (a == 0 && (r != 0 || g != 0 || b != 0)) {
            a = 255;
        }
        
        // Additional Edge Case: White cursor (255,255,255) on Transparent (0,0,0)
        // DrawIconEx often leaves alpha 0 for the white part of a monochrome cursor.
        // We might want to fill Alpha based on Mask if we wanted perfection, 
        // but this heuristic covers 99% of "Kitbashing".

        dst[0] = r;
        dst[1] = g;
        dst[2] = b;
        dst[3] = a;

        src += 4;
        dst += 4;
    }

    // Clean up drawing resources
    SelectObject(hMemDC, hOldBmp);
    DeleteObject(hDIB);
    cleanup_bitmaps(true, hMemDC); // Deletes iconInfo bitmaps and DC

    // 8. Create LLImageRaw
    // Assuming LLImageRaw constructor: LLImageRaw(U16 width, U16 height, S8 components)
    // and setData(const U8* data, bool for_immediate_use)
    LLPointer<LLImageRaw> image = new LLImageRaw(width, height, 4);
    
    // Depending on your LL version, you might copy data differently. 
    // Standard way:
    U8* img_data = image->getData();
    if (img_data) {
        memcpy(img_data, rawData.data(), rawData.size());
    }

    HotCursor result{};
    // result.type = type;
    result.hotspot = { iconInfo.xHotspot, image->getHeight() - iconInfo.yHotspot };
    result.raw = image;
    result.extents = { image->getWidth(), image->getHeight(), image->getComponents() };
    return result;
}

HotCursor get_cursor_image(ECursorType type) {
    // 1. Resolve Resource ID
    CursorResource res = get_cursor_resource_id(type);
    HMODULE hMod = res.is_system ? NULL : GetModuleHandle(NULL);
    
    // 2. Load the Cursor
    // LR_CREATEDIBSECTION is useful to ensure we get a DIB, though LoadCursor generally handles this.
    HCURSOR hCursor = LoadCursor(hMod, res.id);
    
    if (!hCursor) {
        // Fallback to arrow if custom fails
        hCursor = LoadCursor(NULL, IDC_ARROW);
    }
    return get_hcursor_image(hCursor, type);
}

    // HotCursor get_cursor_image_sdl(ECursorType type) {  
    //     // Map cursor types to BMP filenames (from initCursors patterns)
    //     static std::unordered_map<ECursorType, HotCursor> loaded{};
    //     {
    //         auto it = loaded.find(type);
    //         if (it != loaded.end()) return it->second;
    //     }
    //     static const std::unordered_map<ECursorType, std::pair<std::string, std::pair<int, int>>> cursor_files = {  
    //         {UI_CURSOR_ARROW, {"llarrow.BMP", {0, 0}}},  
    //         {UI_CURSOR_WAIT, {"wait.BMP", {12, 15}}},  
    //         {UI_CURSOR_HAND, {"hand.BMP", {7, 10}}},  
    //         {UI_CURSOR_IBEAM, {"ibeam.BMP", {15, 16}}},  
    //         {UI_CURSOR_CROSS, {"cross.BMP", {16, 14}}},  
    //         {UI_CURSOR_SIZENWSE, {"sizenwse.BMP", {14, 17}}},  
    //         {UI_CURSOR_SIZENESW, {"sizenesw.BMP", {17, 17}}},  
    //         {UI_CURSOR_SIZEWE, {"sizewe.BMP", {16, 14}}},  
    //         {UI_CURSOR_SIZENS, {"sizens.BMP", {17, 16}}},  
    //         {UI_CURSOR_SIZEALL, {"sizeall.BMP", {17, 17}}},  
    //         {UI_CURSOR_NO, {"llno.BMP", {8, 8}}},  
    //         {UI_CURSOR_WORKING, {"working.BMP", {12, 15}}},  
    //         {UI_CURSOR_TOOLGRAB, {"lltoolgrab.BMP", {2, 13}}},  
    //         {UI_CURSOR_TOOLLAND, {"lltoolland.BMP", {1, 6}}},  
    //         {UI_CURSOR_TOOLFOCUS, {"lltoolfocus.BMP", {8, 5}}},  
    //         {UI_CURSOR_TOOLCREATE, {"lltoolcreate.BMP", {7, 7}}},  
    //         {UI_CURSOR_ARROWDRAG, {"arrowdrag.BMP", {0, 0}}},  
    //         {UI_CURSOR_ARROWCOPY, {"arrowcop.BMP", {0, 0}}},  
    //         {UI_CURSOR_ARROWDRAGMULTI, {"llarrowdragmulti.BMP", {0, 0}}},  
    //         {UI_CURSOR_ARROWCOPYMULTI, {"arrowcopmulti.BMP", {0, 0}}},  
    //         {UI_CURSOR_NOLOCKED, {"llnolocked.BMP", {8, 8}}},  
    //         {UI_CURSOR_ARROWLOCKED, {"llarrowlocked.BMP", {0, 0}}},  
    //         {UI_CURSOR_GRABLOCKED, {"llgrablocked.BMP", {2, 13}}},  
    //         {UI_CURSOR_TOOLTRANSLATE, {"lltooltranslate.BMP", {0, 0}}},  
    //         {UI_CURSOR_TOOLROTATE, {"lltoolrotate.BMP", {0, 0}}},  
    //         {UI_CURSOR_TOOLSCALE, {"lltoolscale.BMP", {0, 0}}},  
    //         {UI_CURSOR_TOOLCAMERA, {"lltoolcamera.BMP", {7, 5}}},  
    //         {UI_CURSOR_TOOLPAN, {"lltoolpan.BMP", {7, 5}}},  
    //         {UI_CURSOR_TOOLZOOMIN, {"lltoolzoomin.BMP", {7, 5}}},  
    //         {UI_CURSOR_TOOLZOOMOUT, {"lltoolzoomout.BMP", {7, 5}}},  
    //         {UI_CURSOR_TOOLPICKOBJECT3, {"toolpickobject3.BMP", {0, 0}}},  
    //         {UI_CURSOR_TOOLPLAY, {"toolplay.BMP", {0, 0}}},  
    //         {UI_CURSOR_TOOLPAUSE, {"toolpause.BMP", {0, 0}}},  
    //         {UI_CURSOR_TOOLMEDIAOPEN, {"toolmediaopen.BMP", {0, 0}}},  
    //         {UI_CURSOR_PIPETTE, {"lltoolpipette.BMP", {2, 28}}},  
    //         {UI_CURSOR_TOOLSIT, {"toolsit.BMP", {20, 15}}},  
    //         {UI_CURSOR_TOOLBUY, {"toolbuy.BMP", {20, 15}}},  
    //         {UI_CURSOR_TOOLOPEN, {"toolopen.BMP", {20, 15}}},  
    //         {UI_CURSOR_TOOLPAY, {"toolbuy.BMP", {20, 15}}},  
    //         {UI_CURSOR_TOOLPATHFINDING, {"lltoolpathfinding.BMP", {16, 16}}},  
    //         {UI_CURSOR_TOOLPATHFINDING_PATH_START, {"lltoolpathfindingpathstart.BMP", {16, 16}}},  
    //         {UI_CURSOR_TOOLPATHFINDING_PATH_START_ADD, {"lltoolpathfindingpathstartadd.BMP", {16, 16}}},  
    //         {UI_CURSOR_TOOLPATHFINDING_PATH_END, {"lltoolpathfindingpathend.BMP", {16, 16}}},  
    //         {UI_CURSOR_TOOLPATHFINDING_PATH_END_ADD, {"lltoolpathfindingpathendadd.BMP", {16, 16}}},  
    //         {UI_CURSOR_TOOLNO, {"llno.BMP", {8, 8}}}  
    //     };  
    
    //     auto it = cursor_files.find(type);  
    //     if (it == cursor_files.end()) {  
    //         return {}; // Unknown cursor type  
    //     }  
    
    //     const std::string& filename = it->second.first;  
    //     auto path_buffer = llformat("%s/res-sdl/%s", gDirUtilp->getAppRODataDir().c_str(), filename.c_str()); 
    //     LLPointer<LLImageBMP> bmp_image = new LLImageBMP;  
    //     if (!bmp_image->load(path_buffer)) {
    //         fprintf(stdout, "Failed to load cursor BMP: %s (%s)\n", filename.c_str(), path_buffer.c_str());
    //         return type == UI_CURSOR_ARROW ? HotCursor{} : get_cursor_image(UI_CURSOR_ARROW);  
    //     }  
    
    //     LLPointer<LLImageRaw> raw_image = new LLImageRaw;  
    //     if (!bmp_image->decode(raw_image, 0.0f)) {  
    //         fprintf(stdout, "Failed to decode cursor BMP: %s\n", filename.c_str());
    //         return {};  
    //     }  
    
    //     // Convert to 4-component RGBA for proper alpha blending  
    //     if (raw_image->getComponents() == 3) {  
    //         // Create a new 4-component image from your 3-component source  
    //         LLPointer<LLImageRaw> rgba_image = new LLImageRaw(raw_image->getWidth(), raw_image->getHeight(), 4);  
    //         rgba_image->copyUnscaled3onto4(raw_image);
    //         raw_image = rgba_image; // Add alpha channel  
    //     }  
    //     HotCursor& result = loaded[type];
    //     result.type = type;
    //     result.hotspot = { it->second.second.first, it->second.second.second };
    //     result.raw = raw_image;
    //     result.extents = { raw_image->getWidth(), raw_image->getHeight(), raw_image->getComponents() };
    
    //     fprintf(stdout, "loaded cursor BMP: %s type=%d [%dx%dx%d @ <%d,%d>]\n", filename.c_str(), type, raw_image->getWidth(), raw_image->getHeight(), raw_image->getComponents(),
    //         result.hotspot.x, result.hotspot.y);        
    //     return result;  
    // }    
    int get_window_cursor_type() { return gViewerWindow->getWindow()->getCursor(); }
    uint32_t load_image_to_texture(LLPointer<LLImageRaw> const& raw) {
        // Generate texture name - do once  
        U32 tex_name = 0;  
        if (!raw) return tex_name;
        LLImageGL::generateTextures(1, &tex_name);  
        
        // Set format and upload initial data - do once or when image changes  
        gGL.getTexUnit(0)->bindManual(LLTexUnit::TT_TEXTURE, tex_name);  
        LLImageGL::setManualImage(  
            LLTexUnit::getInternalType(LLTexUnit::TT_TEXTURE),  
            0, GL_RGBA, raw->getWidth(), raw->getHeight(),  
            GL_RGBA, GL_UNSIGNED_BYTE, raw->getData(), false  
        );  
        
        // Set texture parameters - do once  
        gGL.getTexUnit(0)->setTextureFilteringOption(LLTexUnit::TFO_BILINEAR);  
        gGL.getTexUnit(0)->setTextureAddressMode(LLTexUnit::TAM_CLAMP);  
        gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);      
        return tex_name;
    }
    bool gl_blit_hcursor(intptr_t type) {
        static std::unordered_map<intptr_t, HotCursor> cursors{};
        HotCursor cursor{};
        auto it = cursors.find(type);
        if (it != cursors.end()) cursor = it->second;
        else {
            // fprintf(stdout, "loading Hcursor: type=%p \n", type);
            cursor = get_hcursor_image((HCURSOR)type);
            cursor.texID = load_image_to_texture(cursor.raw);
            fprintf(stdout, "//loaded Hcursor: type=%p texID=%d hotspot=%s wh=%s\n", type, cursor.texID, glm::to_string(cursor.hotspot).c_str(), glm::to_string(cursor.extents).c_str());
            // if (cursor.texID) {
                // slos::_saveTexture(llformat("ctx.cursor.%d.png", _type), cursor.texID);
                cursors[type] = cursor;
            // }
        }
        if (!cursor.texID) return false;
        gGL.getTexUnit(0)->bindManual(LLTexUnit::TT_TEXTURE, cursor.texID);
        // Setup blending for alpha  
        LLGLDisable no_depth(GL_DEPTH_TEST);
        LLGLDisable no_cull(GL_CULL_FACE);
        LLGLEnable blend(GL_BLEND);
        gGL.setSceneBlendType(LLRender::BT_ALPHA);  
        gGL.color4f(1.f, 1.f, 1.f, 1.f);
        gGL.pushMatrix();
        gGL.translatef(-cursor.hotspot.x, -cursor.hotspot.y + 1, 0.0f);
        gl_rect_2d_simple_tex(cursor.extents.x, cursor.extents.y);  
        gGL.popMatrix();
        gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
        return true;
    }

    bool gl_blit_cursor(int _type) {
        ECursorType type = (ECursorType)_type;
        static std::unordered_map<ECursorType, HotCursor> cursors{};
        HotCursor cursor{};
        auto it = cursors.find(type);
        if (it != cursors.end()) cursor = it->second;
        else {
            // fprintf(stdout, "loading cursor: type=%d \n", type);
            cursor = get_cursor_image(type);
            cursor.texID = load_image_to_texture(cursor.raw);
            fprintf(stdout, "//loaded cursor: type=%d texID=%d hotspot=%s wh=%s\n", type, cursor.texID, glm::to_string(cursor.hotspot).c_str(), glm::to_string(cursor.extents).c_str());
            // if (cursor.texID) {
                // slos::_saveTexture(llformat("ctx.cursor.%d.png", _type), cursor.texID);
                cursors[type] = cursor;
            // }
        }
        if (!cursor.texID) return false;
        gGL.getTexUnit(0)->bindManual(LLTexUnit::TT_TEXTURE, cursor.texID);
        // Setup blending for alpha  
        LLGLDisable no_depth(GL_DEPTH_TEST);
        LLGLDisable no_cull(GL_CULL_FACE);
        LLGLEnable blend(GL_BLEND);
        gGL.setSceneBlendType(LLRender::BT_ALPHA);  
        gGL.color4f(1.f, 1.f, 1.f, 1.f);
        gGL.pushMatrix();
        gGL.translatef(-cursor.hotspot.x, -cursor.hotspot.y + 1, 0.0f);
        gl_rect_2d_simple_tex(cursor.extents.x, cursor.extents.y);  
        gGL.popMatrix();
        gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
        return true;
    }

}//ns

#include "llhudrender.h"
namespace slos {
    namespace xx {
        void withBlah(std::function<void()> const& functor) {
            LLGLSLShader* shader = LLGLSLShader::sCurBoundShaderPtr;
            gUIProgram.bind();  
            LLGLDisable no_depth(GL_DEPTH_TEST);
            LLGLDisable no_cull(GL_CULL_FACE);
            LLGLDisable noclamp(GL_DEPTH_CLAMP);
            LLGLDisable noscissor(GL_SCISSOR_TEST);
            LLGLDepthTest maybedepth(GL_FALSE, GL_FALSE);
            LLGLEnable blend(GL_BLEND);
            gGL.pushMatrix();
            gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
            functor();
            gGL.popMatrix();

            gGL.flush();
            gUIProgram.unbind();
            if (shader) shader->bind();
        }
    }
    void gl_render_all_hud_texts() {
        xx::withBlah(LLHUDText::renderAll);
    }
    void gl_render_world_line_segments(glm::vec3 const& origin, std::vector<xx::LineSegment> const& segments) {
        xx::withBlah([&]{
            LLGLDisable blend(GL_BLEND);
            gGL.setLineWidth(2.0f);
            gGL.begin(LLRender::LINES);
            for (const auto& seg : segments) {
                gGL.color4fv(glm::value_ptr(glm::length2(seg.color) ? seg.color : glm::vec4(1)));
                gGL.vertex3fv(glm::value_ptr(origin + seg.a));
                gGL.vertex3fv(glm::value_ptr(origin + seg.b));
            }
            gGL.setLineWidth(1.0f);
            gGL.end();
        });
    }
    void gl_render_world_transforms(glm::mat4 const& origin, std::vector<xx::WorldTransforms> const& transforms) {
        xx::withBlah([&]{
            auto pfont = LLFontGL::getFontSansSerif();
            for (const auto& seg : transforms) {
                hud_render_utf8text(seg.text, LLVector3(glm::value_ptr(glm::vec3((origin * seg.transform)[3]))), *pfont,   
                    LLFontGL::NORMAL, LLFontGL::DROP_SHADOW, 0, 0,   
                    LLColor4{glm::value_ptr(glm::length2(seg.color) ? seg.color : glm::vec4(1))}, true);
            }
            {
                LLGLDisable blend(GL_BLEND);
                gGL.setLineWidth(2.0f);
                gGL.begin(LLRender::LINES);
                for (const auto& seg : transforms) {
                    auto pt = origin * seg.transform;
                    gGL.color4fv(glm::value_ptr(glm::length2(seg.color) ? seg.color : glm::vec4(1)));
                    gGL.vertex3fv(glm::value_ptr(pt[3]));
                    gGL.vertex3fv(glm::value_ptr(pt[3] + pt[0]));
                }
                gGL.setLineWidth(1.0f);
                gGL.end();
            }
        });
    }

    void gl_render_world_texts(glm::vec3 const& origin, std::vector<xx::WorldText> const& texts) {
        xx::withBlah([&]{
            auto pfont = LLFontGL::getFontSansSerif();
            bool needLines = false;
            for (const auto& seg : texts) {
                hud_render_utf8text(seg.text, LLVector3(glm::value_ptr(origin + seg.pos)), *pfont,   
                    LLFontGL::NORMAL, LLFontGL::DROP_SHADOW, 0, 0,   
                    LLColor4{glm::value_ptr(glm::length2(seg.color) ? seg.color : glm::vec4(1))}, true);
                if (glm::length2(seg.dir)) needLines = true;
            }
            if (needLines) {
                LLGLDisable blend(GL_BLEND);
                gGL.setLineWidth(2.0f);
                gGL.begin(LLRender::LINES);
                for (const auto& seg : texts) {
                    if (!glm::length2(seg.dir)) continue;
                    gGL.color4fv(glm::value_ptr(glm::length2(seg.color) ? seg.color : glm::vec4(1)));
                    gGL.vertex3fv(glm::value_ptr(origin + seg.pos));
                    gGL.vertex3fv(glm::value_ptr(origin + seg.pos + seg.dir));
                }
                gGL.setLineWidth(1.0f);
                gGL.end();
            }
        });
    }
    void gl_render_world_text(std::string const& text, glm::vec3 world_pos, glm::vec4 const& color) {
        xx::withBlah([&]{
            hud_render_utf8text(text, LLVector3(world_pos), *LLFontGL::getFontMonospace(),   
                            LLFontGL::NORMAL, LLFontGL::NO_SHADOW, 0, 0,   
                            LLColor4{glm::value_ptr(color)}, true);
        });
    }
    void gl_render_ui_screen_text(std::string const& text, glm::ivec2 mouse, glm::vec4 const& color) {
        LLFontGL::getFontMonospace()->renderUTF8(text, 0, mouse.x, mouse.y, LLColor4{glm::value_ptr(color)}, LLFontGL::LEFT, LLFontGL::TOP);
    }

    void gl_render_ui_screen_text_multiline(std::string const& text, glm::ivec2 mouse, glm::vec4 const& color, bool bottom_align) {  
        LLFontGL* font = LLFontGL::getFontMonospace();  
        S32 line_height = font->getLineHeight();  
        LLColor4 col{glm::value_ptr(color)};
        
        S32 current_y = mouse.y;  

        // If aligning to bottom, shift the starting Y up by the total height of the block.
        // We keep the render logic Top-to-Bottom because that handles newlines naturally.
        if (bottom_align) {
            // Count lines: 1 + number of newlines
            size_t line_count = std::count(text.begin(), text.end(), '\n') + 1;
            
            // If we want the *bottom* of the last line to be at mouse.y:
            // Start Y = mouse.y + (Total Lines * Height)
            // Note: You might need to adjust this depending on if mouse.y is 
            // intended to be the baseline or the bottom bounding box.
            // Assuming mouse.y is the visual bottom anchor:
            current_y += (line_count * line_height);
            
            // If your text rendering uses Top-Left origin for glyphs, 
            // you usually don't need to change the V-Align enum (LLFontGL::TOP),
            // just the position you pass in.
        }

        size_t start = 0;  
        size_t pos = text.find('\n');  
        
        while (pos != std::string::npos) {  
            std::string line = text.substr(start, pos - start);
            
            // Note: Kept LLFontGL::TOP for consistency. 
            // It's usually safer to adjust 'current_y' than to flip the alignment enum,
            // which might change how the font engine interprets the coordinate.
            font->renderUTF8(line, 0, mouse.x, current_y, col, LLFontGL::LEFT, LLFontGL::TOP, LLFontGL::NORMAL, LLFontGL::DROP_SHADOW);  
                            
            current_y -= line_height;  
            start = pos + 1;  
            pos = text.find('\n', start);  
        }  
        
        std::string last_line = text.substr(start);  
        font->renderUTF8(last_line, 0, mouse.x, current_y, col, LLFontGL::LEFT, LLFontGL::TOP, LLFontGL::NORMAL, LLFontGL::DROP_SHADOW);  
    }
} //ns


namespace slos {
// extern bool ll_get_stack_trace(std::vector<std::string>& lines);
// const char* nunja::get_stack_trace() {
//     std::vector<std::string> stack_lines;  
//     ll_get_stack_trace(stack_lines);  
//     // Convert to single string for fprintf  
//     std::stringstream ss;  
//     for (const auto& line : stack_lines) {  
//         ss << line << "\n";  
//     }
//     static std::string tmp = ss.str();
//     return tmp.c_str();
// }
// class LLGLSLShader;
// extern LLGLSLShader         gSolidColorProgram;
// extern LLGLSLShader         gObjectPreviewProgram;
// extern LLGLSLShader         gHUDFullbrightAlphaMaskAlphaProgram;
// extern LLGLSLShader         gHUDAlphaProgram;
// extern LLGLSLShader         gGLTFPBRMetallicRoughnessProgram;
// extern LLGLSLShader         gHUDPBROpaqueProgram;

    void tempDebugRender(uint32_t tex_glTextureId, std::function<void()> ui_blit) {
        // gGL.loadIdentity();
        LLGLSLShader* shader = LLGLSLShader::sCurBoundShaderPtr;
        gUIProgram.bind();  
        {
            LLGLDisable no_depth(GL_DEPTH_TEST);
            LLGLDisable no_cull(GL_CULL_FACE);
            LLGLDisable noclamp(GL_DEPTH_CLAMP);
            LLGLDisable noscissor(GL_SCISSOR_TEST);
            LLGLDepthTest maybedepth(GL_FALSE, GL_FALSE);
            LLGLEnable blend(GL_BLEND);
            // LLGLDisable noblend(GL_BLEND);
            // gGL.setAmbientLightColor(LLColor4::black);
            gGL.setSceneBlendType(LLRender::BT_ALPHA);  
            gGL.color4f(1.f, 1.f, 1.f, 1.f);
            // auto& view = LLViewerCamera::instance();
            // F32 ground_height_at_camera = land.resolveHeightGlobal( gAgentCamera.getCameraPositionGlobal() )
            static auto world_position = slos::get_agent_world_position();
            // auto world_position = gAgentview.getOrigin() + view.getAtAxis();
            {
                gGL.pushMatrix();
                gGL.translatef(world_position.x, world_position.y, world_position.z);  
            // float mx = 0.5, my=0.5;
            // auto col = LLColor4::green; 
            // gl_triangle_2d(mx, my, mx + 8, my - 15, mx + 15, my - 8, LLColor4::black, TRUE);
            //         gl_triangle_2d(mx+2, my-2, mx + 9, my - 13, mx + 12, my - 8, col, TRUE);
            // for (auto& eye : { &ctx.leftEye, &ctx.rightEye }) {
                // glBindFramebuffer(GL_DRAW_FRAMEBUFFER, eye->fbo);
                // glViewport(0, 0, ctx.resolution.x, ctx.resolution.y);
                ui_blit();
                // glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
                gGL.popMatrix();
            }
            // }
            if (tex_glTextureId) {
                // for (auto& eye : { &ctx.leftEye, &ctx.rightEye }) 
                {
                    // glBindFramebuffer(GL_DRAW_FRAMEBUFFER, eye->fbo);
                    // glViewport(0, 0, ctx.resolution.x, ctx.resolution.y);
                    {
                        LLGLDisable no_depth(GL_DEPTH_TEST);
                        LLGLDisable no_cull(GL_CULL_FACE);
                        LLGLDisable noclamp(GL_DEPTH_CLAMP);
                        LLGLDisable noscissor(GL_SCISSOR_TEST);
                        LLGLDisable noblend(GL_BLEND);
                        gGL.getTexUnit(0)->bindManual(LLTexUnit::TT_TEXTURE, tex_glTextureId);
                        gGL.color4f(1.f, 1.f, 1.f, 1.f);
                        // glm::vec2 scaleFactors = glm::vec2(ctx.resolution) / glm::vec2(tex.size);
                        // float scale = glm::min(scaleFactors.x, scaleFactors.y);
                        // glm::ivec2 renderSize = glm::ivec2(glm::vec2(tex.size) * scale);
                        // glm::vec2 renderPos = (ctx.resolution - renderSize) / 2;
                        // gGL.loadIdentity();
                        // gGL.translatef(renderPos.x, renderPos.y, 0);
                        gGL.pushMatrix();
                        gGL.translatef(world_position.x, world_position.y, world_position.z);  
                        gGL.multMatrix(glm::value_ptr(glm::rotate(glm::mat4(1), glm::radians(90.0f), glm::vec3(1,0,0))));
                        gl_rect_2d_simple_tex(1, 1);
                        gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
                        gGL.popMatrix();
                    }
                    // glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
                }
            }
            gGL.flush();
        }
        if (shader) shader->bind();
    }

    void gl_noblend_blit_texture(uint32_t glTextureId, glm::vec2 const& renderSize) {
        LLGLDisable noblend(GL_BLEND);
        gGL.getTexUnit(0)->bindManual(LLTexUnit::TT_TEXTURE, glTextureId);
        gl_rect_2d_simple_tex(renderSize.x, renderSize.y);
        gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
    }
    void gl_blit_texture(uint32_t glTextureId, glm::vec2 const& renderSize) {
        LLGLDisable noalphatest(GL_ALPHA_TEST);
        gGL.getTexUnit(0)->bindManual(LLTexUnit::TT_TEXTURE, glTextureId);
        gl_rect_2d_simple_tex(renderSize.x, renderSize.y);
        gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
    }

    void gl_blit_functor(glm::ivec2 const& viewport, glm::ivec2 const& extents, std::function<void()> functor) {
        gUIProgram.bind();
        LLGLSUIDefault gls_ui;
        gl_state_for_2d(viewport.x, viewport.y);
        glViewport(0, 0, viewport.x, viewport.y);
        LLGLDisable noclamp(GL_DEPTH_CLAMP);
        LLGLDisable noscissor(GL_SCISSOR_TEST);
        // LLGLDisable noblend(GL_BLEND);
        LLGLDisable nodepth(GL_DEPTH_TEST);
        gGL.pushMatrix();
        gGL.pushUIMatrix();  
        gGL.loadUIIdentity();  
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glm::vec2 eye_extents = viewport;
        glm::vec2 window_size = extents;
        glm::vec2 eye_to_window = window_size / eye_extents;
        gGL.scalef(eye_to_window.x, eye_to_window.y, 1.0f);
        functor();
        gGL.flush();
        gGL.popUIMatrix();  
        gGL.popMatrix();
        gUIProgram.unbind();
    }
    void gl_blit_side_by_side(glm::ivec2 viewport, glm::ivec2 wh, uint32_t leftFBO, uint32_t rightFBO) {
        gUIProgram.bind();
        LLGLSUIDefault gls_ui;
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
        S32 w = wh.x;
        S32 h = wh.y;
        
        S32 y_bottom = h / 4;
        S32 y_top = h * 3 / 4;
        S32 b = 4; // Border thickness in pixels

        #if 1
            // --- Draw Borders (Backgrounds) ---
            // We use Scissor+Clear because it is robust and ignores matrix states
            glEnable(GL_SCISSOR_TEST);

            // Left Eye: Red
            glScissor(0, y_bottom, w/2, h/2); 
            glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            // Right Eye: Blue
            glScissor(w/2, y_bottom, w/2, h/2); 
            glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            glScissor(0, 0, w, h); 
            glDisable(GL_SCISSOR_TEST);
        #endif
        glClearColor(0.5f, .5f, .5f, .5f);
        // glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        // --- Blit Images (Inset by b) ---

        glBindFramebuffer(GL_READ_FRAMEBUFFER, leftFBO);
        // Destination: 0 to w/2, inset by b
        glBlitFramebuffer(0, 0, viewport.x, viewport.y, 
                            0 + b, y_bottom + b, (w/2) - b, y_top - b, 
                            GL_COLOR_BUFFER_BIT, GL_LINEAR);
        
        glBindFramebuffer(GL_READ_FRAMEBUFFER, rightFBO);
        // Destination: w/2 to w, inset by b
        glBlitFramebuffer(0, 0, viewport.x, viewport.y, 
                            (w/2) + b, y_bottom + b, w - b, y_top - b, 
                            GL_COLOR_BUFFER_BIT, GL_LINEAR);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        
        gUIProgram.unbind();
    }

    namespace xx {
        struct Upper {
            LLGLSLShader* shader { LLGLSLShader::sCurBoundShaderPtr };
            LLGLSUIDefault gls_ui;
            LLGLDisable noclamp{ GL_DEPTH_CLAMP };
            LLGLDisable noscissor {GL_SCISSOR_TEST };
            Upper(glm::ivec2 const& eye_extents) {
                gUIProgram.bind();
                gl_state_for_2d(eye_extents.x, eye_extents.y);
                gGL.pushMatrix();
                gGL.pushUIMatrix();  
                gGL.loadUIIdentity();  
            }
            ~Upper() {
                gGL.popUIMatrix();  
                gGL.popMatrix();
                gUIProgram.unbind();
                if (shader) shader->bind();
                stop_glerror();
            }
        };
        struct WUpper {
            LLGLSLShader* shader { LLGLSLShader::sCurBoundShaderPtr };
            LLGLDisable no_depth { GL_DEPTH_TEST };
            LLGLDisable no_cull { GL_CULL_FACE };
            LLGLEnable blend { GL_BLEND };
            WUpper() {
                gGL.setSceneBlendType(LLRender::BT_ALPHA);  
                gGL.color4f(1.f, 1.f, 1.f, 1.f);
            }
            ~WUpper() {
                gGL.flush();
                if (shader && shader != LLGLSLShader::sCurBoundShaderPtr) shader->bind();
            }
        };
        struct FBODrawBinder {
            FBODrawBinder(uint32_t fboID, glm::ivec2 const& viewport) {
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fboID);
                glViewport(0, 0, viewport.x, viewport.y);
            }
            ~FBODrawBinder() {
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
            }
        };        
        struct TS {
            TS(glm::vec3 T, glm::vec3 S) {
                gGL.pushMatrix();
                gGL.translatef(T.x, T.y, T.z);
                gGL.scalef(S.x, S.y, S.z);
            }
            ~TS() { gGL.popMatrix(); }
        };
    } //ns xx
    void gl_withUpper(glm::ivec2 const& eye_extents, std::function<void()> const& functor) {
        xx::Upper upper{ eye_extents };
        functor();
    }
    void gl_withWUpper(std::function<void()> const& functor) {
        xx::WUpper upper{ };
        functor();
    }
    void gl_withFBODrawBinder(uint32_t fboID, glm::ivec2 const& eye_extents, std::function<void()> const& functor) {
        xx::FBODrawBinder binder{ fboID, eye_extents };
        functor();
    }                

} //ns

#elif __INCLUDE_LEVEL__ == 0
    #error "TPVM_RECIPE: -xc++ -DSLOS_IMPLEMENTATION"
#endif
