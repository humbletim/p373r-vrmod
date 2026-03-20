#ifndef PROBE_H
#define PROBE_H

#endif //PROBE_H

#ifdef humbletim_probe_implementation
// ============================================================================
// COORDINATE PROBE: The Forensic Apparatus
// ============================================================================
// Philosophy: Capture raw evidence. Zero assumptions. Zero conversions.
// usage:
//    Probe::Instance().Record("Reference_Forward", raw_openvr_matrix);
//    Probe::Instance().Record("Reference_Forward", ll_camera_quaternion);
//    Probe::Instance().Save();
// ============================================================================

#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <cstring>
#include <iostream>

// ----------------------------------------------------------------------------
// 1. TYPE DETECTION (No headers required if types are forward declared)
// ----------------------------------------------------------------------------
// We use templates to capture data regardless of what headers are included.
// This allows you to drop this into any file without fixing include hell.

class Probe {
public:
    struct Evidence {
        std::string label;      // e.g., "Physical_LookLeft"
        std::string type_name;  // e.g., "vr::HmdMatrix34_t"
        std::string hex_dump;   // Raw memory view
        std::string float_dump; // Floating point view (if applicable)
        size_t size_bytes;
    };

private:
    std::vector<Evidence> logbook;
    
    // Internal: Hex Dumper
    static std::string Hex(const void* ptr, size_t size) {
        std::stringstream ss;
        const unsigned char* b = (const unsigned char*)ptr;
        for(size_t i=0; i<size; ++i) {
            ss << std::hex << std::setw(2) << std::setfill('0') << (int)b[i] << " ";
        }
        return ss.str();
    }

    // Internal: Float Dumper (Assumes floats are contiguous)
    static std::string Floats(const void* ptr, size_t size) {
        std::stringstream ss;
        const float* f = (const float*)ptr;
        size_t count = size / sizeof(float);
        ss << std::fixed << std::setprecision(4);
        for(size_t i=0; i<count; ++i) {
            if (i > 0) ss << ", ";
            ss << f[i];
        }
        return ss.str();
    }

public:
    static Probe& Instance() {
        static Probe instance;
        return instance;
    }

    // ------------------------------------------------------------------------
    // THE CAPTURE API
    // ------------------------------------------------------------------------
    
    // Generic Catch-All: Dumps raw memory of ANY struct/class.
    template <typename T>
    void Record(const std::string& label, const T& obj) {
        Evidence e;
        e.label = label;
        e.type_name = typeid(T).name();
        e.size_bytes = sizeof(T);
        e.hex_dump = Hex(&obj, sizeof(T));
        e.float_dump = Floats(&obj, sizeof(T)); // Blindly assume it might be floats
        
        logbook.push_back(e);
        
        // Console Beep / Visual Confirmation
        std::cout << "[PROBE] Captured: " << label << " (" << e.size_bytes << " bytes)" << std::endl;
        std::cout << "\a"; // SYSTEM BEEP
    }

    void Record(const std::string& label, const glm::int32& obj) { return Record(label, glm::float32(obj)); }
    void Record(const std::string& label, const glm::uint32& obj) { return Record(label, glm::float32(obj)); }
    void Record(const std::string& label, const glm::ivec2& obj) { return Record(label, glm::vec2(obj)); }
    void Record(const std::string& label, const glm::ivec3& obj) { return Record(label, glm::vec3(obj)); }
    void Record(const std::string& label, const glm::ivec4& obj) { return Record(label, glm::vec4(obj)); }
    void Record(const std::string& label, const glm::uvec2& obj) { return Record(label, glm::vec2(obj)); }
    void Record(const std::string& label, const glm::uvec3& obj) { return Record(label, glm::vec3(obj)); }
    void Record(const std::string& label, const glm::uvec4& obj) { return Record(label, glm::vec4(obj)); }
    // ------------------------------------------------------------------------
    // EXPORT
    // ------------------------------------------------------------------------
    std::string toString(bool clear = true) {
        std::ostringstream f;
        f << "{\n  \"evidence\": [\n";
        for (size_t i = 0; i < logbook.size(); ++i) {
            const auto& e = logbook[i];
            f << "    {\n";
            f << "      \"label\": \"" << e.label << "\",";
            f << " \"type\": \"" << e.type_name << "\",";
            // f << " \"size\": " << e.size_bytes << ",";
            f << " \"raw_floats\": [" << e.float_dump << "],";
            // f << "      \"raw_hex\": \"" << e.hex_dump << "\"";
            f << "    }" << (i < logbook.size() - 1 ? "," : "") << "\n";
        }
        f << "  ]\n}\n";
        
        if (clear) logbook.clear();
        return f.str();
    }
    void Save(const std::string& filename = "coordinate_evidence.json", bool clear = true) {
        std::ofstream f(filename);
        if (!f.is_open()) {
            std::cerr << "[PROBE] Failed to open " << filename << std::endl;
            return;
        }
        f << toString(clear);
        std::cout << "[PROBE] Evidence saved to " << filename << std::endl;
    }
};

#endif // humbletim_probe_implementation