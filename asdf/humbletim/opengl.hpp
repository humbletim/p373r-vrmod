#ifndef HUMBLETIM_OPENGL_H
#define HUMBLETIM_OPENGL_H

#include <cstdint>
#include <vector>
#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#endif
#include <GL/gl.h>
typedef struct __GLsync *GLsync;

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

// TODO: depends on opengl system headers being provided from the outside...
namespace opengl { 
    using GLint = int32_t;
    struct RGBAData {
        GLint id, width, height;
        std::vector<uint8_t> bytes;
    };
    struct DepthDATA {
        GLint id, width, height;
        std::vector<float> floats;
    };
    glm::ivec2 _get_fbo_attachment_extents(uint32_t fboID);
    RGBAData _get_fbo_attachment_rgba(uint32_t fboID);
    RGBAData _get_texture_rgba(uint32_t texID);
    DepthDATA _get_fbo_attachment_depth(uint32_t fboID);

    struct PBOCursor {
        // --- Public Config ---
        GLuint fbo;
        GLbitfield mask; // GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT

        // --- Public Metrics ---
        glm::ivec2 fbsize{ -1, -1 };
        uint64_t serial = 0;   // Increments on successful fresh read
        uint64_t dropped = 0;  // Incremented if cursor moves faster than readback

        // --- Internal State ---
        struct Channel {
            GLuint pbo = 0;
            GLsync fence = 0;
            bool busy = false;
            glm::ivec2 coord_processing = {-1, -1}; 
            
            // Data storage
            glm::vec4 val_color; 
            float val_depth;
        } col, dep;

        glm::ivec2 target_coord = {-1, -1}; 
        GLbitfield ready_flags = 0; 

        // --- API ---
        PBOCursor(GLuint fbo_id = 0, GLbitfield initial_mask = GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        ~PBOCursor();

        void setMask(GLbitfield m);
        
        // Call this when you draw to the FBO and need to ensure previous reads are considered "stale"
        void invalidate();

        // Updates intent. Kicks off read if pipeline is free.
        void setCursorGL(glm::ivec2 gl_coord);
        void setCursor(glm::ivec2 window_coord);

        // Call every frame. Returns vec3(x, y, ready_mask)
        glm::vec3 getCursorGL();
        glm::vec3 getCursor();

        bool read(glm::vec4& out);
        bool read(float& out);

        // Internal helpers
        void _process_channel(Channel& ch, GLbitfield bit, GLenum fmt, GLenum type);
        void _check_channel(Channel& ch, GLbitfield bit);
    };
} //ns

#endif //HUMBLETIM_OPENGL_H

#ifdef HUMBLETIM_OPENGL_IMPLEMENTATION

namespace opengl {

    RGBAData _get_texture_rgba(uint32_t texID) {
        RGBAData result{};
        result.id = texID;
        GLint internalFormat=0;
        glBindTexture(GL_TEXTURE_2D, result.id);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &result.width);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &result.height);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &internalFormat);
        fprintf(stdout, "GL_TEXTURE _get_texture_rgba -- width=%d height=%d internalFormat=%d\n", result.width, result.height, internalFormat); fflush(stdout);
        if (!result.width || !result.height || (internalFormat != GL_RGBA8 && internalFormat != GL_RGBA)) {
            fprintf(stdout, "!saving -- internalFormat=%d is not GL_RGBA=%d GL_RGBA8=%d...\n", internalFormat, GL_RGBA, GL_RGBA8);
            glBindTexture(GL_TEXTURE_2D, 0);
            return result;
        }
        result.bytes.resize(result.width * result.height * 4);
        // glMemoryBarrier(GL_FRAMEBUFFER_BARRIER_BIT | GL_TEXTURE_UPDATE_BARRIER_BIT);
        // glFinish();
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, result.bytes.data());  
        glBindTexture(GL_TEXTURE_2D, 0);
        return result;
    }

    glm::ivec2 _get_fbo_attachment_extents(uint32_t fboID) {
        glm::ivec2 result{-1, -1};
        glBindFramebuffer(GL_FRAMEBUFFER, fboID);
        GLint internalFormat=0;
        GLint type = 0;
        GLint id = 0;
        glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &type);
        glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &id);
        if (type == GL_TEXTURE) {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glBindTexture(GL_TEXTURE_2D, id);
            glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &result.x);
            glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &result.y);
            glBindTexture(GL_TEXTURE_2D, 0);
        } else if (type == GL_RENDERBUFFER) {
            glBindRenderbuffer(GL_RENDERBUFFER, id);
            glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &result.x);
            glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &result.y);
            glBindRenderbuffer(GL_RENDERBUFFER, 0);
        } else {
            fprintf(stdout, "FBO %d: No valid COLOR Attachment (type=%d)\n", fboID, type);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            return result;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return result;
    }

    RGBAData _get_fbo_attachment_rgba(uint32_t fboID) {
        RGBAData result{};
        fprintf(stdout, "_get_fbo_attachment_rgba(%d)\n", fboID); fflush(stdout);
        glBindFramebuffer(GL_FRAMEBUFFER, fboID);
        GLint internalFormat=0;
        GLint type = 0;
        glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &type);
        glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &result.id);
        fprintf(stdout, "_get_fbo_attachment_rgba type=%d name=%d\n", type, result.id); fflush(stdout);
    
        if (type == GL_TEXTURE) {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glBindTexture(GL_TEXTURE_2D, result.id);
            glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &result.width);
            glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &result.height);
            glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &internalFormat);
            fprintf(stdout, "GL_TEXTURE _get_fbo_attachment_rgba -- width=%d height=%d internalFormat=%d\n", result.width, result.height, internalFormat); fflush(stdout);
            if (!result.width || !result.height || (internalFormat != GL_RGBA8 && internalFormat != GL_RGBA)) {
                fprintf(stdout, "!saving -- internalFormat=%d is not GL_RGBA=%d GL_RGBA8=%d...\n", internalFormat, GL_RGBA, GL_RGBA8);
                glBindTexture(GL_TEXTURE_2D, 0);
                return result;
            }
            result.bytes.resize(result.width * result.height * 4);
            // glMemoryBarrier(GL_FRAMEBUFFER_BARRIER_BIT | GL_TEXTURE_UPDATE_BARRIER_BIT);
            // glFinish();
            glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, result.bytes.data());  
            glBindTexture(GL_TEXTURE_2D, 0);
        } else if (type == GL_RENDERBUFFER) {
            glBindRenderbuffer(GL_RENDERBUFFER, result.id);
            glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &result.width);
            glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &result.height);
            glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_INTERNAL_FORMAT, &internalFormat);
            fprintf(stdout, "GL_RENDERBUFFER _get_fbo_attachment_rgba -- width=%d height=%d internalFormat=%d\n", result.width, result.height, internalFormat); fflush(stdout);
            if (!result.width || !result.height || (internalFormat != GL_RGBA8 && internalFormat != GL_RGBA)) {
                fprintf(stdout, "!saving -- internalFormat=%d is not GL_RGBA=%d GL_RGBA8=%d...\n", internalFormat, GL_RGBA, GL_RGBA8);
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                glBindRenderbuffer(GL_RENDERBUFFER, 0);
                return result;
            }
            result.bytes.resize(result.width * result.height * 4);
            // glPixelStorei(GL_PACK_ALIGNMENT, 1); // Prevent alignment corruption
            // glReadBuffer(GL_COLOR_ATTACHMENT0);
            glReadPixels(0, 0, result.width, result.height, GL_RGBA, GL_UNSIGNED_BYTE, result.bytes.data());
            glBindRenderbuffer(GL_RENDERBUFFER, 0);
        } else {
            fprintf(stdout, "FBO %d: No valid COLOR Attachment (type=%d)\n", fboID, type);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            // glBindFramebuffer(GL_READ_FRAMEBUFFER, oldFBO);
            return result;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return result;
    }

    DepthDATA _get_fbo_attachment_depth(uint32_t fboID) {
        DepthDATA result{};
        fprintf(stdout, "_get_fbo_render_depth(%d)\n", fboID); fflush(stdout);
        glBindFramebuffer(GL_FRAMEBUFFER, fboID);
        GLint internalFormat=0;
        GLint type = 0;
        glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &type);
        glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &result.id);
        fprintf(stdout, "_get_fbo_render_depth type=%d name=%d\n", type, result.id); fflush(stdout);

        if (type == GL_TEXTURE) {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glBindTexture(GL_TEXTURE_2D, result.id);
            glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &result.width);
            glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &result.height);
            glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &internalFormat);
            fprintf(stdout, "GL_TEXTURE _get_fbo_render_depth -- width=%d height=%d internalFormat=%d\n", result.width, result.height, internalFormat); fflush(stdout);
            if (!result.width || !result.height || internalFormat != GL_DEPTH_COMPONENT24) {
                fprintf(stdout, "!saving -- internalFormat=%d is not GL_FLOAT=%d GL_DEPTH_COMPONENT24=%d GL_DEPTH_COMPONENT16=%d...\n", internalFormat, GL_FLOAT,GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT16);
                glBindTexture(GL_TEXTURE_2D, 0);
                return result;
            }
            result.floats.resize(result.width * result.height * 1);
            // glMemoryBarrier(GL_FRAMEBUFFER_BARRIER_BIT | GL_TEXTURE_UPDATE_BARRIER_BIT);
            // glFinish();
            glGetTexImage(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, GL_FLOAT, result.floats.data());  
            glBindTexture(GL_TEXTURE_2D, 0);
        } else if (type == GL_RENDERBUFFER) {
            glBindRenderbuffer(GL_RENDERBUFFER, result.id);
            glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &result.width);
            glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &result.height);
            glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_INTERNAL_FORMAT, &internalFormat);
            fprintf(stdout, "GL_RENDERBUFFER _get_fbo_render_depth -- width=%d height=%d internalFormat=%d\n", result.width, result.height, internalFormat); fflush(stdout);
            if (!result.width || !result.height || internalFormat != GL_DEPTH_COMPONENT24) {
                fprintf(stdout, "!saving -- internalFormat=%d is not GL_FLOAT=%d GL_DEPTH_COMPONENT24=%d GL_DEPTH_COMPONENT16=%d...\n", internalFormat, GL_FLOAT,GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT16);
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                glBindRenderbuffer(GL_RENDERBUFFER, 0);
                return result;
            }
            result.floats.resize(result.width * result.height * 1);
            // glPixelStorei(GL_PACK_ALIGNMENT, 1); // Prevent alignment corruption
            // glReadBuffer(GL_DEPTH_COMPONENT);
            glReadPixels(0, 0, result.width, result.height, GL_DEPTH_COMPONENT, GL_FLOAT, result.floats.data());
            glBindRenderbuffer(GL_RENDERBUFFER, 0);
        } else {
            fprintf(stdout, "FBO %d: No valid Depth Attachment (type=%d)\n", fboID, type);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            // glBindFramebuffer(GL_READ_FRAMEBUFFER, oldFBO);
            return result;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return result;
    }

    PBOCursor::PBOCursor(GLuint fbo_id, GLbitfield initial_mask) : fbo(fbo_id), mask(initial_mask),
        fbsize(_get_fbo_attachment_extents(fbo_id)) {
        // Setup Color PBO
        glGenBuffers(1, &col.pbo);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, col.pbo);
        glBufferData(GL_PIXEL_PACK_BUFFER, sizeof(float)*4, NULL, GL_STREAM_READ);

        // Setup Depth PBO
        glGenBuffers(1, &dep.pbo);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, dep.pbo);
        glBufferData(GL_PIXEL_PACK_BUFFER, sizeof(float), NULL, GL_STREAM_READ);
        
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    }

    PBOCursor::~PBOCursor() {
        if (col.pbo) glDeleteBuffers(1, &col.pbo);
        if (dep.pbo) glDeleteBuffers(1, &dep.pbo);
        if (col.fence) glDeleteSync(col.fence);
        if (dep.fence) glDeleteSync(dep.fence);
    }

    void PBOCursor::setMask(GLbitfield m) { 
        mask = m; 
        // If we turned off a bit that was currently ready, clear it so we don't return stale data if turned back on
        ready_flags &= mask; 
    }

    void PBOCursor::invalidate() {
        // 1. Mark current data as invalid
        ready_flags = 0;

        // 2. Orphan any in-flight requests. 
        // By setting processing coord to -1, when the async read finishes, 
        // _check_channel will see the mismatch and discard the result (refusing to update serial/ready_flags).
        col.coord_processing = {-1, -1};
        dep.coord_processing = {-1, -1};
    }

    void PBOCursor::setCursor(glm::ivec2 window_coord) {
        setCursorGL({ window_coord.x, (fbsize.y - 1) - window_coord.y});
    }
    void PBOCursor::setCursorGL(glm::ivec2 gl_coord) {
        if (gl_coord == target_coord) return;
        // fprintf(stdout, "setCursorGL(<%d,%d>)\n", gl_coord.x, gl_coord.y);
        target_coord = gl_coord;
        ready_flags = 0; 
        
        // Try to launch immediately
        _process_channel(col, GL_COLOR_BUFFER_BIT, GL_RGBA, GL_FLOAT);
        _process_channel(dep, GL_DEPTH_BUFFER_BIT, GL_DEPTH_COMPONENT, GL_FLOAT);
    }
    glm::vec3 PBOCursor::getCursor() {
        auto tmp = getCursorGL();
        tmp.y = (fbsize.y-1) - tmp.y;
        return tmp;
    }

    glm::vec3 PBOCursor::getCursorGL() {
        // 1. Check/Finish existing work
        _check_channel(col, GL_COLOR_BUFFER_BIT);
        _check_channel(dep, GL_DEPTH_BUFFER_BIT);

        // 2. If free but not ready (and enabled), start work
        _process_channel(col, GL_COLOR_BUFFER_BIT, GL_RGBA, GL_FLOAT);
        _process_channel(dep, GL_DEPTH_BUFFER_BIT, GL_DEPTH_COMPONENT, GL_FLOAT);

        return glm::vec3(target_coord, (float)ready_flags);
    }

    bool PBOCursor::read(glm::vec4& out) {
        if (ready_flags & GL_COLOR_BUFFER_BIT) {
            out = col.val_color;
            return true;
        }
        return false;
    }

    bool PBOCursor::read(float& out) {
        if (ready_flags & GL_DEPTH_BUFFER_BIT) {
            out = dep.val_depth;
            return true;
        }
        return false;
    }

    void PBOCursor::_process_channel(Channel& ch, GLbitfield bit, GLenum fmt, GLenum type) {
        if (!(mask & bit)) return;      // Masked out
        if (ch.busy) return;            // Already working
        if ((ready_flags & bit) && ch.coord_processing == target_coord) return; // Already have this data

        GLint prev_fbo, prev_pbo;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fbo);
        glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &prev_pbo);

        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, ch.pbo);

        glReadPixels(target_coord.x, target_coord.y, 1, 1, fmt, type, 0);

        if (ch.fence) glDeleteSync(ch.fence);
        ch.fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        
        ch.busy = true;
        ch.coord_processing = target_coord;

        glBindBuffer(GL_PIXEL_PACK_BUFFER, prev_pbo);
        glBindFramebuffer(GL_FRAMEBUFFER, prev_fbo);
    }

    void PBOCursor::_check_channel(Channel& ch, GLbitfield bit) {
        if (!ch.busy) return;

        GLenum status = glClientWaitSync(ch.fence, GL_SYNC_FLUSH_COMMANDS_BIT, 0);
        if (status == GL_ALREADY_SIGNALED || status == GL_CONDITION_SATISFIED) {
            
            glBindBuffer(GL_PIXEL_PACK_BUFFER, ch.pbo);
            float* ptr = (float*)glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
            
            if (ptr) {
                if (bit == GL_COLOR_BUFFER_BIT) ch.val_color = glm::make_vec4(ptr);
                else ch.val_depth = *ptr;
                glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
            }
            glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

            ch.busy = false;
            
            // Only flag as ready if this read matches our current target AND hasn't been invalidated
            if (ch.coord_processing == target_coord) {
                ready_flags |= bit;
                serial++;
            } else {
                dropped++;
            }
        }
    }


} //ns

#elif __INCLUDE_LEVEL__ == 0
    #error "TPVM_RECIPE: -xc++ -includellgl.h -DHUMBLETIM_OPENGL_IMPLEMENTATION"
#endif
