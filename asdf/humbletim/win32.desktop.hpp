#ifndef HUMBLETIM_WIN32_DESKTOP_H
#define HUMBLETIM_WIN32_DESKTOP_H

#include <cstdint>
#include <memory>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp>

namespace win32 {
    glm::i64vec2 getCursorRange();
    glm::i64vec4 getCursorInfo(intptr_t parent = 0);
    intptr_t getCursorHWND(intptr_t parent = 0);
        
    // Forward declare the Pimpl so we don't need windows.h/gl.h here
    struct DesktopMonitorCaptureInternalState;

    // Simple POD structs for data transfer (no logic)
    struct CaptureStats {
        float averageFPS;
        uint64_t capturedFrames;
        uint64_t droppedFrames;
        float lastUploadTimeMs;
    };

    struct TextureState {
        uint32_t glTextureId; // 0 if invalid
        uint32_t serial;      // Monotonically increasing ID of the frame
        uint64_t timestamp;   // Age/Time of capture (implementation defined epoch)
        glm::ivec2 size;      // Actual pixel dimensions of the texture
        bool valid;           // Quick check if this state is usable
    };

    struct DesktopMonitorCapture {
        std::shared_ptr<DesktopMonitorCaptureInternalState> _d;

        // Constraint: max_dimension (e.g., 2048). 
        // If desktop is larger, we downscale to fit within this box while maintaining aspect ratio.
        // pass 0 to use native desktop resolution.
        DesktopMonitorCapture(int max_dimension = 2048, float target_fps = 15.0f);
        
        // HEARTBEAT: Call this on main thread. 
        // Returns true if a NEW texture was uploaded this frame.
        bool poll(); 
        void pause(bool release_memory = false);
        bool is_paused();
        bool is_monitoring();
        void resume();

        // Get the current valid texture. 
        // If 'max_age_ms' > 0 and the latest frame is older than that, returns an invalid state.
        TextureState acquire(int max_age_ms = -1) const;

        // Configuration
        void setTargetFPS(float fps);
        void setMaxDimension(int pixels);
        
        // Force a re-capture immediately (bypassing FPS limit) on next poll
        void invalidate();

        CaptureStats getStatistics() const;
        std::string getStatisticsString() const;
    };
}

#endif // HUMBLETIM_WIN32_DESKTOP_H

#ifdef HUMBLETIM_WIN32_DESKTOP_IMPLEMENTATION

#include <windows.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <algorithm>
#include <iostream> // for simple debug logging if needed

namespace win32 {

    // -------------------------------------------------------------------------
    // INTERNAL STATE
    // -------------------------------------------------------------------------
    struct DesktopMonitorCaptureInternalState {
        // SETTINGS
        std::atomic<float> target_fps{10.0f};
        std::atomic<int> max_dimension{1920};
        std::atomic<bool> is_running{true};
        std::atomic<bool> please_invalidate{false};
        std::atomic<bool> is_paused{ false };
        std::atomic<bool> please_hibernate{ false }; // New flag

        // THREADING
        std::thread worker_thread;
        std::mutex swap_mutex; 
        
        // BUFFER EXCHANGE
        // 'shared_pixels' is the bridge between threads.
        // Protected by 'swap_mutex'.
        std::vector<uint8_t> shared_pixels; 
        int shared_width = 0;
        int shared_height = 0;
        bool has_new_frame_data = false;

        // OPENGL STATE (Main Thread Only)
        TextureState current_texture_state = {0, 0, 0, {0,0}, false};
        uint64_t frame_serial_counter = 0;
        CaptureStats stats = {0};

        // GDI RESOURCES (Worker Thread Only)
        // We keep these persistent to avoid allocation churn
        HDC desktop_dc = nullptr;
        HDC memory_dc = nullptr;
        HBITMAP memory_bitmap = nullptr;
        void* dib_bits = nullptr; // Raw pointer to the GDI surface
        int current_alloc_w = 0;
        int current_alloc_h = 0;

        DesktopMonitorCaptureInternalState() {
            // Start the worker immediately
            worker_thread = std::thread(&DesktopMonitorCaptureInternalState::workerLoop, this);
        }

        ~DesktopMonitorCaptureInternalState() {
            is_running = false;
            if (worker_thread.joinable()) worker_thread.join();
            cleanupGDI();
        }

        void cleanupGDI() {
            if (memory_bitmap) DeleteObject(memory_bitmap);
            if (memory_dc) DeleteDC(memory_dc);
            if (desktop_dc) ReleaseDC(NULL, desktop_dc);
            memory_bitmap = nullptr;
            memory_dc = nullptr;
            desktop_dc = nullptr;
            dib_bits = nullptr;
        }

        void workerLoop() {
            while (is_running) {
                // 1. HANDLE PAUSE & HIBERNATION
                if (is_paused) {
                    // A. Check if we need to dump cargo
                    if (please_hibernate) {
                        cleanupGDI(); // Worker cleans its own mess. Safe.
                        // We leave please_hibernate true so we don't spam cleanup, 
                        // or we can rely on cleanupGDI being idempotent (checking for nulls).
                    }

                    // B. Sleep and skip
                    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                    continue;
                }

                auto start_time = std::chrono::steady_clock::now();
                
                // 1. ACQUIRE DIMENSIONS
                // We grab the real desktop size every frame to handle resolution changes gracefully
                int real_w = GetSystemMetrics(SM_CXSCREEN);
                int real_h = GetSystemMetrics(SM_CYSCREEN);
                
                // Calculate target size (downscaling if needed)
                int target_w = real_w;
                int target_h = real_h;
                int max_dim = max_dimension.load();
                
                if (max_dim > 0 && (real_w > max_dim || real_h > max_dim)) {
                    float aspect = (float)real_w / (float)real_h;
                    if (real_w > real_h) {
                        target_w = max_dim;
                        target_h = static_cast<int>(max_dim / aspect);
                    } else {
                        target_h = max_dim;
                        target_w = static_cast<int>(max_dim * aspect);
                    }
                }

                // 2. SETUP GDI (Lazy Init)
                if (!desktop_dc) desktop_dc = GetDC(NULL);
                if (!memory_dc) memory_dc = CreateCompatibleDC(desktop_dc);

                // Re-allocate bitmap if size changed
                if (current_alloc_w != target_w || current_alloc_h != target_h || !memory_bitmap) {
                    if (memory_bitmap) DeleteObject(memory_bitmap);
                    
                    BITMAPINFO bmi = {0};
                    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                    bmi.bmiHeader.biWidth = target_w;
                    bmi.bmiHeader.biHeight = target_h; // Top-down DIB not supported well, usually bottom-up
                    bmi.bmiHeader.biPlanes = 1;
                    bmi.bmiHeader.biBitCount = 32;
                    bmi.bmiHeader.biCompression = BI_RGB;

                    // CreateDIBSection gives us direct access to the pixel memory (dib_bits)
                    memory_bitmap = CreateDIBSection(memory_dc, &bmi, DIB_RGB_COLORS, &dib_bits, NULL, 0);
                    SelectObject(memory_dc, memory_bitmap);

                    current_alloc_w = target_w;
                    current_alloc_h = target_h;
                }

                // 3. CAPTURE (Blocking Operation)
                // STRETCH BLT performs the copy + resize in one go.
                // It uses the driver's 2D blitter, so it's reasonably fast.
                SetStretchBltMode(memory_dc, HALFTONE); // Better quality downscaling
                StretchBlt(memory_dc, 0, 0, target_w, target_h, 
                           desktop_dc, 0, 0, real_w, real_h, SRCCOPY);
                
                // GdiFlush ensures the GPU->SystemRAM copy is actually done before we read it
                GdiFlush();

                // 4. PUBLISH (Critical Section)
                {
                    std::lock_guard<std::mutex> lock(swap_mutex);
                    
                    // Resize shared buffer if needed
                    size_t needed_bytes = target_w * target_h * 4;
                    if (shared_pixels.size() != needed_bytes) shared_pixels.resize(needed_bytes);
                    
                    // Copy raw bits. 
                    // Note: DIB Sections are usually BGRA.
                    if (dib_bits) {
                        memcpy(shared_pixels.data(), dib_bits, needed_bytes);
                        // for (size_t i=0; i < needed_bytes-3; i += 4) {
                        //     shared_pixels[i+3] = 0xff;
                        // }
                    }

                    shared_width = target_w;
                    shared_height = target_h;
                    has_new_frame_data = true;
                }

                // 5. THROTTLE
                // Simple sleep to hit target FPS
                float fps = target_fps.load();
                if (please_invalidate.exchange(false)) fps = 1000.0f; // Zoom zoom if invalidated
                
                int frame_ms = (int)(1000.0f / (fps > 0 ? fps : 1.0f));
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time).count();
                if (elapsed < frame_ms) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(frame_ms - elapsed));
                }
            }
        }
    };

    // -------------------------------------------------------------------------
    // PUBLIC API IMPLEMENTATION
    // -------------------------------------------------------------------------

    DesktopMonitorCapture::DesktopMonitorCapture(int max_dimension, float target_fps) {
        _d = std::make_shared<DesktopMonitorCaptureInternalState>();
        _d->max_dimension = max_dimension;
        _d->target_fps = target_fps;
    }

    void DesktopMonitorCapture::setTargetFPS(float fps) {
        _d->target_fps = fps;
    }

    void DesktopMonitorCapture::setMaxDimension(int pixels) {
        _d->max_dimension = pixels;
    }

    void DesktopMonitorCapture::invalidate() {
        _d->please_invalidate = true;
    }

    CaptureStats DesktopMonitorCapture::getStatistics() const {
        return _d->stats;
    }

    std::string DesktopMonitorCapture::getStatisticsString() const {
        auto stats = getStatistics();
        auto tex = acquire();
        return std::format("desktop.stats texID: {}, serial: {}, age: {}, res: {}\n, valid: {}, fps:{}, \ncap: {}\n, drop: {}, \nupload: {}\n",
            tex.glTextureId, tex.serial, tex.timestamp, glm::to_string(tex.size).c_str(), tex.valid, stats.averageFPS, stats.capturedFrames, stats.droppedFrames, stats.lastUploadTimeMs);
    }

    TextureState DesktopMonitorCapture::acquire(int max_age_ms) const {
        // Simple age check could be added here using chrono, 
        // for now just returning the current state.
        return _d->current_texture_state;
    }

    bool DesktopMonitorCapture::is_paused() { return _d->is_paused; }
    bool DesktopMonitorCapture::is_monitoring() { return _d->is_running; }

    void DesktopMonitorCapture::pause(bool release_memory) {
        if (_d->is_paused && _d->please_hibernate == release_memory) return;
        fprintf(stdout, "desktop.pause %s\n", release_memory ? "release_memory" : "");
        _d->is_paused = true;
        _d->please_hibernate = release_memory;
    }

    void DesktopMonitorCapture::resume() {
        if (!_d->is_paused) return;
        fprintf(stdout, "desktop.resume\n");
        _d->please_hibernate = false; 
        _d->is_paused = false;
        // Worker will wake up, see !desktop_dc, and re-run Lazy Init automatically.
    }

    bool DesktopMonitorCapture::poll() {
        if (_d->is_paused) return false;

        // Attempt to lock. If the worker is swapping buffers right now,
        // we can just skip this poll to avoid stalling the main thread.
        // But for low-freq desktop, a blocking lock is usually fine (copy is fast).
        // We'll use try_lock to be strictly non-blocking.
        
        std::vector<uint8_t> upload_buffer;
        int upload_w = 0;
        int upload_h = 0;

        {
            std::unique_lock<std::mutex> lock(_d->swap_mutex, std::try_to_lock);
            if (!lock.owns_lock()) return false; // Worker is busy, try next frame
            
            if (!_d->has_new_frame_data) return false; // Nothing new

            // Swap data out to local scope so we can upload outside the lock
            // Actually, copying is safer to keep lock time tiny.
            upload_buffer = _d->shared_pixels; 
            upload_w = _d->shared_width;
            upload_h = _d->shared_height;
            _d->has_new_frame_data = false;
        }

        // If we got here, we have raw bytes ready for OpenGL
        if (upload_buffer.empty()) return false;

        auto start_upload = std::chrono::steady_clock::now();

        // Lazy Init Texture
        if (_d->current_texture_state.glTextureId == 0) {
            glGenTextures(1, (GLuint*)&_d->current_texture_state.glTextureId);
        }

        glBindTexture(GL_TEXTURE_2D, _d->current_texture_state.glTextureId);

        // Check if size changed, need re-init
        bool size_changed = (upload_w != _d->current_texture_state.size.x || upload_h != _d->current_texture_state.size.y);
        
        if (size_changed) {
            // Initial allocation
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, upload_w, upload_h, 0, GL_BGRA, GL_UNSIGNED_BYTE, upload_buffer.data());
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); 
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_ONE); // negate alpha channel by forcing to 1.0f
        } else {
            // Fast update
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, upload_w, upload_h, GL_BGRA, GL_UNSIGNED_BYTE, upload_buffer.data());
        }

        // Update State
        _d->frame_serial_counter++;
        _d->current_texture_state.serial = _d->frame_serial_counter;
        _d->current_texture_state.size = {upload_w, upload_h};
        _d->current_texture_state.valid = true;
        
        // Stats
        _d->stats.capturedFrames++;
        auto end_upload = std::chrono::steady_clock::now();
        float ms = std::chrono::duration_cast<std::chrono::microseconds>(end_upload - start_upload).count() / 1000.0f;
        _d->stats.lastUploadTimeMs = ms;

        return true;
    }

    glm::i64vec2 getCursorRange() { return { GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) }; }
    glm::i64vec4 getCursorInfo(intptr_t _hwndParent) {
        ::CURSORINFO ci{};
        ci.cbSize = sizeof(CURSORINFO);
        if (::GetCursorInfo(&ci)) {
            if (_hwndParent) ::ScreenToClient((HWND)_hwndParent, &ci.ptScreenPos);
            return { ci.ptScreenPos.x, ci.ptScreenPos.y, (intptr_t)ci.hCursor, ci.flags };
        }
        return {};
    }
    intptr_t getCursorHWND(intptr_t _hWndParent) {
        HWND hWndParent = (HWND)_hWndParent;
        POINT pt;
        if (!::GetCursorPos(&pt)) return 0;
        HWND hWndUnderCursor = ::WindowFromPoint(pt);
        if (!hWndParent || hWndUnderCursor == hWndParent || ::IsChild(hWndParent, hWndUnderCursor)) return (intptr_t)hWndUnderCursor; 
        return 0;
    }

}

#elif __INCLUDE_LEVEL__ == 0
    #error "TPVM_RECIPE: -xc++ -DHUMBLETIM_WIN32_DESKTOP_IMPLEMENTATION"
#endif
