// xvr openvr wrapper
// 
// copyright (c) 2019-2026 humbletim
// copyright (c) 9000-2026 humbletim + Gemini "HAL6974" Pro 3

#if __INCLUDE_LEVEL__ == 0 && !defined(XVR_IMPLEMENTATION)
    #error "TPVM_RECIPE: -xc++ -DXVR_IMPLEMENTATION"
#endif

#ifndef XOPENVR_H
#define XOPENVR_H

// ===========================================================================

// ----------------------------------------------------------------------------
// Configuration & Exports
// ----------------------------------------------------------------------------

#define XVR_INTERFACE extern "C" XVR_API
#define XVR_API_EXPORT // always export (for now never xopenvr.dll import)

#if defined(_WIN32)
    #ifdef XVR_API_EXPORT
      #define XVR_API __declspec( dllexport )
    #else
      #define XVR_API __declspec( dllimport )
    #endif
    #define XVR_CALLTYPE __cdecl
#elif defined(__GNUC__) || defined(COMPILER_GCC) || defined(__APPLE__)
    #ifdef XVR_API_EXPORT
      #define XVR_API  __attribute__((visibility("default")))
    #else
      #define XVR_API
    #endif
    #define XVR_CALLTYPE 
#else
    #error "Unsupported Platform."
#endif

// ===========================================================================
// ===========================================================================
// ===========================================================================

// ----------------------------------------------------------------------------
// header
// ----------------------------------------------------------------------------

#include <cstdint>
#include <cstring>
#include <memory>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <ostream>
#include <string>

// ----------------------------------------------------------------------------
// Core Types
// ----------------------------------------------------------------------------

struct XVRWrapper;

static const char * const XVRWrapper_Version = "XVRWrapper_001";

namespace xvr {  
  using Session = std::shared_ptr<XVRWrapper>;

  XVR_INTERFACE const uint32_t MAXDEVICECOUNT;

  struct Device {
    enum _Device : uint32_t { Invalid=0xffffffff } value;
    inline explicit Device(uint32_t v) : value((_Device)v) {}
    operator uint32_t() const { return value; }
  };
  XVR_INTERFACE const Device HMD;
  XVR_INTERFACE const Device INVALID;

  struct Role {
    enum _Role : int { LeftHand=-1, Invalid=0, Right=+1 } value;
    inline explicit Role(int v) : value((_Role)v) {}
    inline Role(_Role const& v) : value(v) {}
    inline bool operator==(_Role const& other) const { return value == other; }
  };
  struct Eye {
    enum _Eye : int { Left=-1, Mono=0, Right=+1 } value;
    inline explicit Eye(int v) : value((_Eye)v) {}
    inline Eye(_Eye const& v) : value(v) {}
    inline int operator+() const { return value; }
    inline bool operator==(_Eye const& other) const { return value == other; }
    inline bool operator==(Eye const& other) const { return value == other.value; }
    friend inline bool operator==(int lhs, Eye const& rhs) { return lhs == rhs.value; }
    inline friend std::ostream& operator<<(std::ostream& os, const Eye& eye) {
      return os << (eye.value == Left ? "left" : eye.value == Right ? "right" : eye.value == Mono ? "mono" : "unknown");
    }
  };

  struct ProjectionBounds {
      glm::vec2 min; // {left, bottom} (radians)
      glm::vec2 max; // {right, top} (radians)
      // Assuming bounds are defined at distance z = 1.0
      // Returns full FOV in radians {horizontal, vertical}
      inline glm::vec2 fullFOV() const {
          return glm::atan(max) - glm::atan(min); // For asymmetric: atan(max) - atan(min)
      }
  };

  // ----------------------------------------------------------------------------
  // UNIVERSAL EVENT DEFINITIONS
  // ----------------------------------------------------------------------------
  // The Single Source of Truth for HAL Events.
  // Format: X(Name, Value)

  #define XVR_EVENT_DEFS(X) \
    X(DeviceActivated,         100) \
    X(DeviceDeactivated,       101) \
    X(PlayspaceReset,          102) \
    X(CompositorReset,         103) \
    /* Lifecycle */ \
    X(Quit,                    200) \
    X(ProcessQuit,             201) \
    X(QuitAcknowledged,        202) \
    X(DriverRequestedQuit,     203) \
    X(RestartRequested,        204) \
    /* Power */ \
    X(EnterStandbye,           300) \
    X(LeaveStandbye,           301) \
    /* Data */ \
    X(IpdChanged,              400) \
    X(SettingChanged,          401) \
    X(LensDistortionChanged,   402) \
    /* Chaperone */ \
    X(ChaperoneChanged, 500) \
    /* Misc */ \
    X(DashboardDeactivated,    600) \
    X(ProcessConnected,        601) \
    X(ProcessDisconnected,     602)


  struct Event {
    enum Type : uint32_t {
      INVALID = 0,
      
      #define X_ENUM(Name, Val) Name = Val,
      XVR_EVENT_DEFS(X_ENUM)
      #undef X_ENUM

      UNKNOWN = 0xfffffffe,
    } type{ Type::INVALID };

    virtual inline const char* name() const {
      switch(type) {
        #define X_STR(Name, Val) case Type::Name: return #Name;
        XVR_EVENT_DEFS(X_STR)
        #undef X_STR
        default: return "UNKNOWN";
      }
    }
    
    virtual inline const char* probes() const { return ""; }
    virtual inline const char* description() const { return name(); }
    virtual inline const char* source() const { return ""; }

    // Type-safe property accessors to remove OpenVR dependencies
    virtual inline xvr::Device asDevice() const = 0;
    virtual inline glm::float64 asFloat64() const = 0;
    
    template <typename T> inline T val() const;
    
    inline explicit Event(uint32_t v) : type((Type)v) {}
    inline Event(Type const& v) : type(v) {}
    inline Event() : type(Type::INVALID) {}
    inline operator bool() const { return type != Type::INVALID; }
  };
  
  template<> inline xvr::Device Event::val() const { return asDevice(); }
  template<> inline uint32_t Event::val() const { return (uint32_t)(glm::float32)asFloat64(); }
  template<> inline float Event::val() const { return (glm::float32)asFloat64(); }

  XVR_INTERFACE void* XVR_CALLTYPE XVR_GetGenericInterface( const char *pchInterfaceVersion, std::string *peError = nullptr );
  XVR_INTERFACE bool XVR_CALLTYPE XVR_FreeGenericInterface( const char *pchInterfaceVersion, void* thing, std::string* = nullptr );
  inline Session Init(const char *pchInterfaceVersion = XVRWrapper_Version, std::string* peError = nullptr) {
      const auto iface = pchInterfaceVersion ? pchInterfaceVersion : "";
      auto ptr = ( XVRWrapper * )xvr::XVR_GetGenericInterface(iface, peError );
      return Session{ptr, [iface](XVRWrapper* ptr){ xvr::XVR_FreeGenericInterface(iface, ptr); }};
  }  
}

// ----------------------------------------------------------------------------
// Wrapper Interface (Pure Virtual / Opaque)
// ----------------------------------------------------------------------------

struct XVRWrapper {
  // Pure virtual interface - strict membrane
  
  virtual const char* error() const = 0;
  virtual void clearError() = 0;
  virtual const char* status() const = 0;
  virtual void clearStatus() = 0;
  virtual void AcknowledgeQuit_Exiting() const = 0;

  virtual const char* get_driver_name() const = 0;
  virtual const char* get_model_name() const = 0;
  virtual const char* get_serial_name() const = 0;
  virtual const char* get_device_role_name(xvr::Device const& nDevice) const = 0;
  virtual const char* get_device_class_name(xvr::Device const& nDevice) const = 0;
  virtual xvr::Device get_device_index_for_role_name(const char* role_name) const = 0;
  
  inline virtual ~XVRWrapper() { }

  virtual glm::uvec2 recommendedRenderTargetSize() const = 0;

  virtual glm::mat4 getEyePerspectiveMatrix(xvr::Eye const& eye, float zNear, float zFar) const = 0;
  virtual glm::mat4 getEyeToHeadTransform(xvr::Eye const& eye) const = 0;
  virtual xvr::ProjectionBounds getProjectionBounds(xvr::Eye const& eye) const = 0;

  virtual void updatePoses(float tt = 0.0f) = 0;
  virtual xvr::Event const& acceptPendingEvent() = 0;
  virtual glm::mat4 getDevicePose(xvr::Device const& unDevice) const = 0;
  virtual bool getValidDevicePose(xvr::Device const& unDevice, glm::mat4& pose) const = 0;

  virtual glm::mat4 getStandingPose(bool raw = false) const = 0;
  virtual bool submit(xvr::Eye const& eye, uint32_t textureID, xvr::Eye const& eye2 = xvr::Eye::Mono, uint32_t textureID2 = 0) const = 0;
  
  virtual bool _setUniform(const char* name, const void* value, size_t len) { return false; }
  #define declareSetUniform(T) virtual bool setUniform(const char* name, T const& val) { return _setUniform(name, glm::value_ptr(val), sizeof(T)); }
  declareSetUniform(glm::uvec2) declareSetUniform(glm::uvec3) declareSetUniform(glm::uvec4)
  declareSetUniform(glm::ivec2) declareSetUniform(glm::ivec3) declareSetUniform(glm::ivec4)
  declareSetUniform(glm::bvec2) declareSetUniform(glm::bvec3) declareSetUniform(glm::bvec4)
  declareSetUniform(glm::fvec2) declareSetUniform(glm::fvec3) declareSetUniform(glm::fvec4)
  declareSetUniform(glm::dvec2) declareSetUniform(glm::dvec3) declareSetUniform(glm::dvec4)
  declareSetUniform(glm::fmat2) declareSetUniform(glm::fmat3) declareSetUniform(glm::fmat4)
  declareSetUniform(glm::dmat2) declareSetUniform(glm::dmat3) declareSetUniform(glm::dmat4)
  declareSetUniform(glm::quat) declareSetUniform(glm::dquat)
  #undef declareSetUniform
  #define declareSetUniform(T) virtual bool setUniform(const char* name, T const& val) { return _setUniform(name, &val, sizeof(T)); }
  declareSetUniform(glm::float32) declareSetUniform(glm::float64)
  declareSetUniform(glm::int8) declareSetUniform(glm::int16) declareSetUniform(glm::int32) declareSetUniform(glm::int64)
  declareSetUniform(glm::uint8) declareSetUniform(glm::uint16) declareSetUniform(glm::uint32) declareSetUniform(glm::uint64)
  #undef declareSetUniform

};

#endif // XOPENVR_H

// ===========================================================================
// ===========================================================================
// ===========================================================================
// ===========================================================================
// ===========================================================================
// ===========================================================================
// ===========================================================================
// ===========================================================================
// ===========================================================================
// ===========================================================================
// ===========================================================================
// ===========================================================================
// ===========================================================================
// ===========================================================================
// ===========================================================================
// ===========================================================================

// ===========================================================================
// IMPLEMENTATION
// ===========================================================================

// ===========================================================================
// ===========================================================================
// ===========================================================================
// ===========================================================================
// ===========================================================================
// ===========================================================================
// ===========================================================================
// ===========================================================================

#ifdef XVR_IMPLEMENTATION

#define XVR_API_EXPORT
//#pragma comment(lib, "openvr_api")

#define GLM_FORCE_SWIZZLE
#pragma warning(disable:4996)

#include <cmath>
#include <cstring>
#include <sstream>
#include <format>
#include <unordered_map>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/string_cast.hpp>

// ----------------------------------------------------------------------------
// OpenVR Includes & Helpers
// ----------------------------------------------------------------------------

#ifdef interface
#undef interface 
#endif

XVR_INTERFACE const uint32_t xvr::MAXDEVICECOUNT { 64 };
XVR_INTERFACE const xvr::Device xvr::INVALID { xvr::Device::Invalid };
XVR_INTERFACE const xvr::Device xvr::HMD { 0 };

namespace {
    inline glm::mat4 _calculatePerspectiveMatrixFromHalfTangents(glm::vec2 lb, glm::vec2 rt, float n, float f) {
        float l = lb.x, b = lb.y, r = rt.x, t = rt.y;
        l *= n; r *= n; b *= n; t *= n;
        return glm::transpose(glm::mat4(
            2*n/(r-l), 0,          (r+l)/(r-l),  0,
            0,         2*n/(t-b),  (t+b)/(t-b),  0,
            0,         0,          -(f+n)/(f-n), (-2*f*n)/(f-n),
            0,         0,          -1,           0
        ));
    }
} //ns

// Base implementation 
struct XVRBase : public XVRWrapper {
    // ERROR HANDLING / status
    char _status[1024]{ 0 };
    char _error[1024]{ 0 };
    virtual const char* error() const override { return _error; }
    virtual void clearError() override { memset(_error, 0, sizeof(_error)); }
    virtual const char* status() const override { return _status; }
    virtual void clearStatus() override { memset(_status, 0, sizeof(_status)); }
    virtual ~XVRBase() { fprintf(stderr, "~XVRWrapper %p _error=%s\n", this, _error); fflush(stderr); }
    virtual void AcknowledgeQuit_Exiting() const override {}

    // SHARED STUFF
    virtual glm::mat4 getEyePerspectiveMatrix(xvr::Eye const& eye, float zNear, float zFar) const override {
      auto bounds = getProjectionBounds(eye);
      return _calculatePerspectiveMatrixFromHalfTangents(bounds.min, bounds.max, zNear, zFar);
    }
    // virtual float getEyeVerticalFOV(xvr::Eye const& eye) const override {
    //     float l,r,t,b;
    //     _getProjectionRaw(eye,l,r,t,b);
    //     return atan(t)-atan(b);
    // }
};

namespace xvr {
  // --------------------------------------------------------------------------
  // Event Implementation
  // --------------------------------------------------------------------------
  namespace impl {
    struct Event : xvr::Event {
      using xvr::Event::Event;
      xvr::Device _device{ xvr::INVALID };
      double _value{ NAN };
      std::string _probes;

      virtual inline const char* probes() const override { return _probes.c_str(); }
      virtual inline xvr::Device asDevice() const override { return _device; }
      virtual inline glm::float64 asFloat64() const override { return _value; }
    };
  }
} //xvr

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// PROCEDURAL (MOCK) IMPLEMENTATION REGION
// ----------------------------------------------------------------------------
struct ProceduralVRWrapper : public XVRBase {
  struct Pose {
    glm::mat4 mDeviceToAbsoluteTracking{ 1.0f };
    bool bPoseIsValid{ false };
  };
  Pose _devicePoses[xvr::MAXDEVICECOUNT]{};

    static glm::vec4 getenv_vec4(const char* name) {
        glm::vec4 v;
        auto str = getenv(name);
        if (!str) return v;
        if (4 == sscanf(str, "%f,%f,%f,%f\n", &v.x, &v.y, &v.z, &v.w )) return v;
        if (3 == sscanf(str, "%f,%f,%f\n", &v.x, &v.y, &v.z )) return glm::vec4(glm::vec3(v), 0.0f);
        if (2 == sscanf(str, "%f,%f\n", &v.x, &v.y )) return glm::vec4(glm::vec2(v), 0.0f, 0.0f);
        if (1 == sscanf(str, "%f\n", &v.x )) return glm::vec4(v.x);
        return v;
    }

  struct Uniforms {
    glm::mat4 hmdOffset{1.0f};
    glm::float32 playerHeight{ (float)atof(getenv("H") ? getenv("H") : "1.5") };
    glm::uvec2 renderTargetSize{ 600, 800 };
    // rest, spin, nod, tilt
    glm::vec4 cycle{ getenv_vec4("K") };
    glm::float32 time{ -1.0f };
  } uniform{};
  
  virtual bool setUniform(const char* name, glm::vec4 const& value) override {
    if (0 == strcmp(name, "cycle")) {
        uniform.cycle = value;
        if (!glm::length2(uniform.cycle)) _devicePoses[xvr::HMD].mDeviceToAbsoluteTracking = glm::mat4(1.0f);
        return true;
    }
    return XVRBase::setUniform(name, value);
  }
  virtual bool setUniform(const char* name, glm::uvec2 const& value) override {
    if (0 == strcmp(name, "renderTargetSize")) {
        uniform.renderTargetSize = value;
        proceduralEvent = { xvr::Event::CompositorReset };
        return true;
    }
    return XVRBase::setUniform(name, value);
  }
  virtual bool setUniform(const char* name, glm::mat4 const& value) override {
    if (0 == strcmp(name, "hmdOffset")) {
        uniform.hmdOffset = value;
        return true;
    }
    return XVRBase::setUniform(name, value);
  }
  virtual bool setUniform(const char* name, glm::float32 const& value) override {
    if (0 == strcmp(name, "playerHeight")) {
        uniform.playerHeight = value;
        return true;
    }
    return XVRBase::setUniform(name, value);
  }

  virtual void updatePoses(float tt = 0.0f) override;

  xvr::impl::Event proceduralEvent{ xvr::Event::INVALID };

  virtual xvr::Event const& acceptPendingEvent() override {
    static xvr::impl::Event event;
    event = {xvr::Event::INVALID};
    if (proceduralEvent.type == xvr::Event::INVALID) {
      static float last = 0.0f;
      if ((uniform.time - last) > 10.0f) {
        last = uniform.time;
        proceduralEvent.type = xvr::Event::IpdChanged;
        static float ipd = 0.060f;
        ipd += 0.001f;
        // proceduralEvent._nativeEventType = typeid(vr::VREvent_IpdChanged).name();
        // proceduralEvent._nativeEvent = { vr::VREvent_IpdChanged };
        // proceduralEvent._nativeEvent.data.ipd.ipdMeters = ipd / 1000.0f;
        proceduralEvent._value = ipd / 1000.0f;
      }
    }
    std::swap(event, proceduralEvent);
    return event;
  }

  explicit ProceduralVRWrapper(std::string const& status) : ProceduralVRWrapper(status.c_str()) {}
  
  ProceduralVRWrapper(const char* _status) {
    #ifdef __GNUC__
        auto strcpy_s = [](char* dest, size_t sz, const char* source) { !source ? strcpy(dest, "ESNULLP") : strnlen(source, sz) >= sz ? strcpy(dest, "ESNOSPC") : strncpy(dest, source, sz); };
    #endif
      strcpy_s(this->_status, sizeof(this->_status), _status);
    updatePoses();
  }
  virtual ~ProceduralVRWrapper() { }

  virtual const char* get_driver_name() const override { return "procedural"; }
  virtual const char* get_model_name() const override { return "procedural"; }
  virtual const char* get_serial_name() const override { return "procedural"; }

  virtual glm::mat4 getDevicePose(xvr::Device const& unDevice) const override {
    if (unDevice < xvr::MAXDEVICECOUNT) return _devicePoses[unDevice].mDeviceToAbsoluteTracking;
    return glm::mat4{1.0f};
  }
  virtual glm::mat4 getStandingPose(bool raw = false) const override { return glm::mat4{1.0f}; }
  virtual bool submit(xvr::Eye const& eye, uint32_t textureID, xvr::Eye const& eye2 = xvr::Eye::Mono, uint32_t textureID2 = 0) const override { return false; }

  virtual bool getValidDevicePose(xvr::Device const& unDevice, glm::mat4& pose) const override {
    if (unDevice < xvr::MAXDEVICECOUNT && _devicePoses[unDevice].bPoseIsValid) {
      pose = _devicePoses[unDevice].mDeviceToAbsoluteTracking;
      return true;
    }
    return false;
  }

  virtual glm::uvec2 recommendedRenderTargetSize() const override { return uniform.renderTargetSize; }

virtual xvr::ProjectionBounds getProjectionBounds(xvr::Eye const& eye) const override {
    static constexpr float DEG_TO_RAD = 0.017453292519943295769236907684886f;
    
    // Baseline: Left Eye (Wide Left, Narrow Right)
    float l = -51.269f * DEG_TO_RAD;
    float r =  46.104f * DEG_TO_RAD;
    float b = -55.024f * DEG_TO_RAD;
    float t =  54.892f * DEG_TO_RAD;

    // If Right Eye, mirror the horizontal bounds
    // The "Inner" (Nose) becomes Left, The "Outer" (Temple) becomes Right
    if (eye == xvr::Eye::Right) {
        float temp = l;
        l = -r;    // New Left is negative of old Right
        r = -temp; // New Right is negative of old Left
    }

    return { {l,b}, {r,t} };
  }

  virtual glm::mat4 getEyeToHeadTransform(xvr::Eye const& eye) const override {
    // TODO: support ipd "uniform"
    return glm::translate(glm::vec3(eye == xvr::Eye::Left ? -.03f : .03f, 0.0f, 0.0f));
  }

  virtual const char* get_device_class_name(xvr::Device const& nDevice) const override {
    if (nDevice == xvr::INVALID) return "invalid";
    if (nDevice == xvr::HMD) return "hmd";
    if (nDevice == xvr::HMD+1) return "controller";
    if (nDevice == xvr::HMD+2) return "controller";
    static std::string buf; buf = "(unhandled device index:"+std::to_string((int)nDevice)+")";
    return buf.c_str();
  }
  virtual const char* get_device_role_name(xvr::Device const& nDevice) const override {
    if (nDevice == xvr::INVALID) return "invalid";
    if (nDevice == xvr::HMD) return "hmd";
    if (nDevice == xvr::HMD+1) return "left-hand";
    if (nDevice == xvr::HMD+2) return "right-hand";
    static std::string buf; buf = "(unhandled device index:"+std::to_string((int)nDevice)+")";
    return buf.c_str();
  }
  virtual xvr::Device get_device_index_for_role_name(const char* role_name) const override {
    if (!role_name) return xvr::INVALID;
    if (0 == strcmp(role_name, "hmd"))        return xvr::HMD;
    if (0 == strcmp(role_name, "left-hand"))  return xvr::Device{ xvr::HMD+1 };
    if (0 == strcmp(role_name, "right-hand")) return xvr::Device{ xvr::HMD+2 };
    return xvr::INVALID;
  }
}; // ProceduralVRWrapper

void ProceduralVRWrapper::updatePoses(float tt) {
    for (int nDevice = 0; nDevice < xvr::MAXDEVICECOUNT; ++nDevice) _devicePoses[nDevice].bPoseIsValid = false;
    uniform.time = tt;
    if (!glm::length2(uniform.cycle)) return;

    static const auto generateMockPose = [](float time, const glm::vec4& cycle) {
        float total = cycle.x + cycle.y + cycle.z + cycle.w;
        // Safety: Prevent global div-by-zero
        if (total <= 0.0001f) return glm::mat4(1.0f); 

        float t_cursor = std::fmod(time, total);
        if (t_cursor < 0.0f) t_cursor += total;
        glm::quat qTotal(1,0,0,0); 
        // Phase 1: Rest
        if (t_cursor < cycle.x) { 
            /* Rest - qTotal remains identity */ 
        } 
        else {
            t_cursor -= cycle.x; // Consume Phase 1 time

            // Phase 2: Spin
            if (t_cursor < cycle.y) { 
                float t = t_cursor / cycle.y;
                qTotal = glm::angleAxis(glm::radians(t * 360.0f), glm::vec3(0,1,0));
            } else {
                t_cursor -= cycle.y; // Consume Phase 2 time

                // Phase 3: Nod
                if (t_cursor < cycle.z) { 
                    float t = t_cursor / cycle.z;
                    qTotal = glm::angleAxis(glm::radians(std::sin(t * 6.28f) * 15.0f), glm::vec3(1,0,0));
                } 
                else { 
                    // Phase 4: Tilt
                    t_cursor -= cycle.z; // Consume Phase 3 time
                    
                    // FIX: Ternary check protects against div-by-zero if cycle.w is 0.0
                    float t = (cycle.w > 0.0f) ? glm::clamp(t_cursor / cycle.w, 0.0f, 1.0f) : 0.0f;
                    
                    qTotal = glm::angleAxis(glm::radians(std::sin(t * 6.28f) * 15.0f), glm::vec3(0,0,1));
                }
            }
        }
        return glm::toMat4(qTotal);
    };

    glm::mat4 hmdViewPlayspace = glm::translate(glm::mat4(1.0f), glm::vec3(0,uniform.playerHeight,0)) * generateMockPose(tt, uniform.cycle);
    _devicePoses[xvr::HMD].mDeviceToAbsoluteTracking = uniform.hmdOffset * hmdViewPlayspace;
    _devicePoses[xvr::HMD].bPoseIsValid = true;

    auto first = xvr::HMD+1, last = first + 1;
    for (int nDevice = first; nDevice < last+1 && nDevice < xvr::MAXDEVICECOUNT; ++nDevice) {
      auto T = glm::translate(glm::vec3{ nDevice == first ? -.04f : .04f, -.05f, -.15f });
      auto R = glm::toMat4(
        glm::angleAxis(glm::radians(std::sin(tt/8.0f * 6.28f) * 15.0f), glm::vec3(1,0,0))
        * glm::angleAxis(glm::radians(std::cos(tt/8.0f * 6.28f) * 15.0f + 0.0f), glm::vec3(0,nDevice == first ? +1 : -1,0))
      );

      // auto R = glm::toMat4(
      //   glm::quat(glm::radians(glm::vec3(15+-cosf(tt)*15,0,0))) *
      //   glm::quat(glm::radians(glm::vec3(0, (nDevice == first ? -.25f : .25f) * sinf(tt)*30, 0)))
      // );
      _devicePoses[nDevice].mDeviceToAbsoluteTracking = uniform.hmdOffset * hmdViewPlayspace * T * R;
      _devicePoses[nDevice].bPoseIsValid = true;
    }
}


// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------


#define OPENVR_API_IMPLEMENTATION
#include <openvr.h>
#ifdef interface
#undef interface
#endif

// ----------------------------------------------------------------------------
// Utilities (Matrix, Strings)
// ----------------------------------------------------------------------------

namespace vrx {
  inline bool _checkNaN(glm::mat4 const& m, const char* hint) {
    if (std::isnan(m[0][0]) || std::isnan(m[2][2]) || std::isnan(m[3][3])) {
      fprintf(stdout, "xopenvrmatrixstuff isnan!!! %s\n", hint); fflush(stdout);
      return true;
    }
    return false;
  }
  template <typename TO, typename FROM> TO convert(FROM const&);

  template<> inline glm::mat4 convert(vr::HmdMatrix34_t const& matPose) {
      return glm::transpose(glm::mat4(glm::make_mat3x4((float*)&matPose.m[0][0])));
    }
    template<> inline vr::HmdMatrix34_t convert(glm::mat4 const& m4) {
      vr::HmdMatrix34_t matPose;
      memcpy(&matPose.m[0][0], glm::value_ptr(glm::mat3x4(glm::transpose(m4))), sizeof(glm::mat3x4));
      return matPose;
    }

    template<> inline vr::EVREye convert(xvr::Eye const& v) {
        switch(v.value) {
            case xvr::Eye::Left: return vr::EVREye::Eye_Left;
            case xvr::Eye::Mono: return vr::EVREye::Eye_Left;
            case xvr::Eye::Right: return vr::EVREye::Eye_Right;
        }
        return vr::EVREye::Eye_Left;
    }
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// OPENVR IMPLEMENTATION REGION
// ----------------------------------------------------------------------------
struct OpenVRWrapper : public XVRBase {
  vr::IVRSystem* sys = nullptr;
  vr::TrackedDevicePose_t _devicePoses[xvr::MAXDEVICECOUNT]{};

  virtual const char* get_driver_name() const override;
  virtual const char* get_model_name() const override;
  virtual const char* get_serial_name() const override;

  virtual xvr::Device get_device_index_for_role_name(const char* role_name) const override;
  virtual const char* get_device_class_name(xvr::Device const& nDevice) const override;
  virtual const char* get_device_role_name(xvr::Device const& nDevice) const override;

  virtual void updatePoses(float tt = 0.0f) override;
  virtual xvr::Event const& acceptPendingEvent() override;
  
  OpenVRWrapper(vr::IVRSystem* sys = nullptr) { strcpy(this->_status, "<OpenVRWrapper>"); this->sys = sys; }
  virtual void AcknowledgeQuit_Exiting() const override;
  virtual ~OpenVRWrapper() { sys = nullptr; }
  virtual glm::mat4 getDevicePose(xvr::Device const& unDevice) const override;
  virtual bool getValidDevicePose(xvr::Device const& unDevice, glm::mat4& pose) const override;
  virtual glm::mat4 getStandingPose(bool raw = false) const override;
  virtual bool submit(xvr::Eye const& eye, uint32_t textureID, xvr::Eye const& eye2 = xvr::Eye::Mono, uint32_t textureID2 = 0) const override;


  virtual glm::uvec2 recommendedRenderTargetSize() const override {
      glm::uvec2 size{ 768, 1024 };
      if (sys) sys->GetRecommendedRenderTargetSize(&size.x, &size.y);
      return size;
    }
  virtual glm::mat4 getEyeToHeadTransform(xvr::Eye const& eye) const override;
  virtual xvr::ProjectionBounds getProjectionBounds(xvr::Eye const& eye) const override;

  std::string _probeAdditionalOpenVREvents(vr::VREvent_t const& event);
};

namespace {
  inline const char* _GetCompositorErrorNameFromEnum(uint32_t v) {
      // (Retaining your existing compositor error switch logic here)
      switch(v) {
      case vr::EVRCompositorError::VRCompositorError_None: return "VRCompositorError_None"; break;
      case vr::EVRCompositorError::VRCompositorError_RequestFailed: return "VRCompositorError_RequestFailed"; break;
      case vr::EVRCompositorError::VRCompositorError_IncompatibleVersion: return "VRCompositorError_IncompatibleVersion"; break;
      case vr::EVRCompositorError::VRCompositorError_DoNotHaveFocus: return "VRCompositorError_DoNotHaveFocus"; break;
      case vr::EVRCompositorError::VRCompositorError_InvalidTexture: return "VRCompositorError_InvalidTexture"; break;
      case vr::EVRCompositorError::VRCompositorError_IsNotSceneApplication: return "VRCompositorError_IsNotSceneApplication"; break;
      case vr::EVRCompositorError::VRCompositorError_TextureIsOnWrongDevice: return "VRCompositorError_TextureIsOnWrongDevice"; break;
      case vr::EVRCompositorError::VRCompositorError_TextureUsesUnsupportedFormat: return "VRCompositorError_TextureUsesUnsupportedFormat"; break;
      case vr::EVRCompositorError::VRCompositorError_SharedTexturesNotSupported: return "VRCompositorError_SharedTexturesNotSupported"; break;
      case vr::EVRCompositorError::VRCompositorError_IndexOutOfRange: return "VRCompositorError_IndexOutOfRange"; break;
      case vr::EVRCompositorError::VRCompositorError_AlreadySubmitted: return "VRCompositorError_AlreadySubmitted"; break;
      case vr::EVRCompositorError::VRCompositorError_InvalidBounds: return "VRCompositorError_InvalidBounds"; break;
        default: return "VRCompositorError_Unknown";
      }
  }
  bool _check_submit_error(vr::EVRCompositorError eError) {
    if (eError != vr::VRCompositorError_None && eError != vr::VRCompositorError_DoNotHaveFocus) {
      static vr::VRCompositorError last = vr::VRCompositorError_None;
      if (last != eError) {
        last = eError;
        fprintf(stdout, "[vr compositor error] %d %s %s\n", eError, std::to_string(eError).c_str(), _GetCompositorErrorNameFromEnum(eError)); fflush(stdout);
      }
      return false;
    }
    return true;
  }
}

bool OpenVRWrapper::submit(xvr::Eye const& eye, uint32_t textureID, xvr::Eye const& eye2, uint32_t textureID2) const {
  if (auto compositor = vr::VRCompositor()) {
    if (textureID != 0) {
      const vr::Texture_t t{ (void*)(uintptr_t)textureID, vr::TextureType_OpenGL, vr::ColorSpace_Gamma };
      if (!_check_submit_error(compositor->Submit(vrx::convert<vr::EVREye>(eye), &t, 0, (vr::EVRSubmitFlags)(vr::Submit_Default)))) return false;
    }
    if (textureID2 != 0) {
      const vr::Texture_t t{ (void*)(uintptr_t)textureID2, vr::TextureType_OpenGL, vr::ColorSpace_Gamma };
      if (!_check_submit_error(compositor->Submit(vrx::convert<vr::EVREye>(eye2), &t, 0, (vr::EVRSubmitFlags)(vr::Submit_Default)))) return false;
    }
    if (textureID && textureID2) compositor->PostPresentHandoff();
    return true;
  }
  return false;
}

xvr::Device OpenVRWrapper::get_device_index_for_role_name(const char* role_name) const {
  if (sys) {
    if (0 == strcmp(role_name, "hmd")) return (xvr::Device)xvr::HMD;//vr::k_unTrackedDeviceIndex_Hmd;
    if (0 == strcmp(role_name, "left-hand")) return (xvr::Device)sys->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_LeftHand);
    if (0 == strcmp(role_name, "right-hand")) return (xvr::Device)sys->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_RightHand);
    if (0 == strcmp(role_name, "opt-out")) return (xvr::Device)sys->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_OptOut);
    if (0 == strcmp(role_name, "treadmill")) return (xvr::Device)sys->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_Treadmill);
  }
  return (xvr::Device)xvr::INVALID;//vr::k_unTrackedDeviceIndexInvalid;
}
  
const char* OpenVRWrapper::get_device_role_name(xvr::Device const& nDevice) const {
  if (nDevice == vr::k_unTrackedDeviceIndexInvalid) return "invalid";
  if (nDevice == vr::k_unTrackedDeviceIndex_Hmd) return "hmd";
  auto role = sys ? sys->GetControllerRoleForTrackedDeviceIndex(nDevice) : vr::TrackedControllerRole_Invalid;
  if (role == vr::TrackedControllerRole_LeftHand) return"left-hand";
  if (role == vr::TrackedControllerRole_RightHand) return "right-hand";
  if (role == vr::TrackedControllerRole_OptOut) return "opt-out";
  if (role == vr::TrackedControllerRole_Treadmill) return "treadmill";
  static std::string buf; buf = "(unknown role:"+std::to_string((int)role)+" / device:"+std::to_string((int)nDevice)+")";;
  return buf.c_str();
}
const char* OpenVRWrapper::get_device_class_name(xvr::Device const& nDevice) const {
  auto cls = sys ? sys->GetTrackedDeviceClass(nDevice) : vr::TrackedDeviceClass_Invalid;
  switch(cls) {
    case vr::TrackedDeviceClass_HMD: return "hmd";
    case vr::TrackedDeviceClass_Controller: return "controller";
    case vr::TrackedDeviceClass_GenericTracker: return "tracker";
    case vr::TrackedDeviceClass_TrackingReference: return "reference";
    case vr::TrackedDeviceClass_DisplayRedirect: return "display";
    case vr::TrackedDeviceClass_Invalid: return "invalid";
    case vr::TrackedDeviceClass_Max: return "max";
    default: break;
  }
  static std::string buf; buf = "(unknown class:"+std::to_string((int)cls)+" / device:"+std::to_string((int)nDevice)+")";
  return buf.c_str();
}

glm::mat4 OpenVRWrapper::getDevicePose(xvr::Device const& unDevice) const {
  if (unDevice < vr::k_unMaxTrackedDeviceCount) return vrx::convert<glm::mat4>(_devicePoses[unDevice].mDeviceToAbsoluteTracking);
  return glm::mat4{1.0f};
}
bool OpenVRWrapper::getValidDevicePose(xvr::Device const& unDevice, glm::mat4& pose) const {
  if (unDevice < vr::k_unMaxTrackedDeviceCount && _devicePoses[unDevice].bPoseIsValid) {
    pose = vrx::convert<glm::mat4>(_devicePoses[unDevice].mDeviceToAbsoluteTracking);
    return true;
  }
  return false;
}

glm::mat4 OpenVRWrapper::getStandingPose(bool raw) const  {
  auto sys = this->sys ? vr::VRSystem() : nullptr;
  if (!sys) return glm::mat4(1); 
  if (raw) return vrx::convert<glm::mat4>(sys->GetRawZeroPoseToStandingAbsoluteTrackingPose());
  else return vrx::convert<glm::mat4>(sys->GetSeatedZeroPoseToStandingAbsoluteTrackingPose());
}

namespace vr {
  inline const char* __getOpenVRString(vr::IVRSystem* sys, vr::TrackedDeviceIndex_t unDevice, vr::TrackedDeviceProperty prop, vr::ETrackedPropertyError* pError = nullptr) {
      if (!sys) return "!sys";
      static char buf[vr::k_unMaxPropertyStringSize];
      buf[0] = 0;
      static vr::ETrackedPropertyError success;
      if (!pError) {
        success = {vr::ETrackedPropertyError::TrackedProp_Success};
        pError = &success;
      }
      sys->GetStringTrackedDeviceProperty( unDevice, prop, buf, vr::k_unMaxPropertyStringSize, pError);
      return *pError == vr::TrackedProp_Success ? buf : sys->GetPropErrorNameFromEnum(*pError);
  }
}//

const char* OpenVRWrapper::get_driver_name() const {
  return vr::__getOpenVRString(sys, vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_TrackingSystemName_String);
}
const char* OpenVRWrapper::get_model_name() const {
  return vr::__getOpenVRString(sys, vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_ModelNumber_String);
}
const char* OpenVRWrapper::get_serial_name() const {
  return vr::__getOpenVRString(sys, vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_SerialNumber_String);
}

xvr::ProjectionBounds OpenVRWrapper::getProjectionBounds(xvr::Eye const& eye) const {
    float l = -1.0f, r = 1.0f;
    float b = -1.0f, t = 1.0f;
    if (sys) {
      sys->GetProjectionRaw(vrx::convert<vr::EVREye>(eye), &l, &r, &t, &b);
      if (t < 0.0f) std::swap(t, b);
    }
    return { {l,b}, {r,t} };
  }


std::string OpenVRWrapper::_probeAdditionalOpenVREvents(vr::VREvent_t const& event) {
    std::string events;
    switch (event.eventType) {
        case vr::VREvent_SeatedZeroPoseReset: {
            events += std::format("\nPROBE: vr::VREvent_SeatedZeroPoseReset bResetBySystemMenu={}", event.data.seatedZeroPoseReset.bResetBySystemMenu ? "true" : "false");
            auto seated = getStandingPose(false);
            auto raw = getStandingPose(true);
            events += std::format("\nseated={}\nraw={}", glm::to_string(seated).c_str(), glm::to_string(raw).c_str());
            break;
        }
        case 114: break; // skip 114 spamming unknown enum...
        default: if (auto sys = vr::VRSystem()) {
            events += std::format("\nPROBE: enum={} event={}", event.eventType, sys->GetEventTypeNameFromEnum((vr::EVREventType)event.eventType));
        }
        break;
    } // switch
    return events;
}

xvr::Event const& OpenVRWrapper::acceptPendingEvent() {
    vr::VREvent_t event{};
    static xvr::impl::Event result;
    result = {xvr::Event::INVALID};
    result._value = NAN;

    auto sys = vr::VRSystem(); // verify fresh to prevent crashes during shutdown
    if (sys != this->sys) {
        fprintf(stdout, "vr::VRSystem() == %p but this->sys == %p\n", sys, this->sys);fflush(stdout);
        return result;
    }
    if (!sys || !sys->PollNextEvent(&event, sizeof(event))) {
        return result;
    }
    // Map VR event to our HAL enum using local X-Macro to maintain strict decoupling
    // This allows for N:1 projections from Vendor Cruft -> Universal Signal
    
    #define OPENVR_TO_HAL_MAP(X) \
      X(vr::VREvent_TrackedDeviceRoleChanged,   xvr::Event::DeviceActivated) \
      X(vr::VREvent_TrackedDeviceActivated,     xvr::Event::DeviceActivated) \
      X(vr::VREvent_TrackedDeviceDeactivated,   xvr::Event::DeviceDeactivated) \
      X(vr::VREvent_Quit,                       xvr::Event::Quit) \
      X(vr::VREvent_EnterStandbyMode,           xvr::Event::EnterStandbye) \
      X(vr::VREvent_LeaveStandbyMode,           xvr::Event::LeaveStandbye) \
      X(vr::VREvent_SeatedZeroPoseReset,        xvr::Event::PlayspaceReset) \
      X(vr::VREvent_IpdChanged,                 xvr::Event::IpdChanged) \
      X(vr::VREvent_ProcessQuit,                xvr::Event::ProcessQuit) \
      X(vr::VREvent_QuitAcknowledged,           xvr::Event::QuitAcknowledged) \
      X(vr::VREvent_DriverRequestedQuit,        xvr::Event::DriverRequestedQuit) \
      X(vr::VREvent_LensDistortionChanged,      xvr::Event::LensDistortionChanged) \
      X(vr::VREvent_DashboardDeactivated,       xvr::Event::DashboardDeactivated) \
      X(vr::VREvent_ProcessConnected,           xvr::Event::ProcessConnected) \
      X(vr::VREvent_ProcessDisconnected,        xvr::Event::ProcessDisconnected) \
      X(vr::VREvent_ChaperoneDataHasChanged,    xvr::Event::ChaperoneChanged) \
      X(vr::VREvent_ChaperoneUniverseHasChanged, xvr::Event::ChaperoneChanged) \
      X(vr::VREvent_ChaperoneTempDataHasChanged, xvr::Event::ChaperoneChanged) \
      X(vr::VREvent_ChaperoneSettingsHaveChanged, xvr::Event::ChaperoneChanged)

    switch (event.eventType) {
      #define X_SWITCH(Native, Hal) case Native: result.type = Hal; break;
      OPENVR_TO_HAL_MAP(X_SWITCH)
      #undef X_SWITCH

      case vr::VREvent_SteamVRSectionSettingChanged: 
          result.type = xvr::Event::CompositorReset;
          {
            static glm::uvec2 last{};
            auto kv = recommendedRenderTargetSize();
            if (last == kv) result.type = xvr::Event::UNKNOWN; // suppress if no change
            last = kv;
          }
          break;
      
      default: result.type = xvr::Event::UNKNOWN; break;
    }
    
    // Decoupled Data Extraction
    result._device = (xvr::Device)event.trackedDeviceIndex;

    switch (event.eventType) {
        // --- Process & Applications ---
        case vr::VREvent_ProcessConnected:
        case vr::VREvent_ProcessDisconnected:
        case vr::VREvent_InputFocusCaptured: // DEPRECATED
        case vr::VREvent_InputFocusReleased: // DEPRECATED
        case vr::VREvent_SceneFocusLost:
        case vr::VREvent_SceneFocusGained:
        case vr::VREvent_SceneApplicationChanged:
        case vr::VREvent_SceneFocusChanged:
        case vr::VREvent_InputFocusChanged:
        case vr::VREvent_SceneApplicationSecondaryRenderingStarted:
        case vr::VREvent_SceneApplicationUsingWrongGraphicsAdapter:
        case vr::VREvent_ActionBindingReloaded:
        case vr::VREvent_Quit:
        case vr::VREvent_ProcessQuit:
        case vr::VREvent_QuitAborted_UserPrompt:
        case vr::VREvent_QuitAcknowledged:
            result._value = (double)event.data.process.pid;
            // event.data.process.oldPid
            // event.data.process.bForced
             // event.data.process.bConnectionLost
            break;

        case vr::VREvent_ApplicationTransitionNewAppLaunchComplete:
            result._value = (double)event.data.applicationLaunch.pid;
            // event.data.applicationLaunch.unArgsHandle
            break;

        // --- Properties ---
        case vr::VREvent_PropertyChanged:
            result._value = (double)event.data.property.prop; // ETrackedDeviceProperty enum
            // event.data.property.container (PropertyContainerHandle_t)
            break;

        // --- Controller / Buttons ---
        case vr::VREvent_ButtonPress:
        case vr::VREvent_ButtonUnpress:
        case vr::VREvent_ButtonTouch:
        case vr::VREvent_ButtonUntouch:
            result._value = (double)event.data.controller.button; // EVRButtonId enum
            break;

        // --- Mouse / Touchpad (Simulated) ---
        case vr::VREvent_MouseButtonDown:
        case vr::VREvent_MouseButtonUp:
            result._value = (double)event.data.mouse.button; // EVRMouseButton enum
            // event.data.mouse.x
            // event.data.mouse.y
            break;

        case vr::VREvent_MouseMove:
        case vr::VREvent_TouchPadMove:
            // No single "primary" value for 2D vectors, defaulting to X or ignoring.
            // result._value = event.data.mouse.x; 
            // event.data.mouse.y
            // event.data.touchPadMove.bFingerDown;
            // event.data.touchPadMove.flSecondsFingerDown;
            // event.data.touchPadMove.fValueXFirst;
            // event.data.touchPadMove.fValueYFirst;
            // event.data.touchPadMove.fValueXRaw;
            // event.data.touchPadMove.fValueYRaw;
            break;

        case vr::VREvent_ScrollDiscrete:
        case vr::VREvent_ScrollSmooth:
            // result._value = event.data.scroll.ydelta; 
            // event.data.scroll.xdelta
            // event.data.scroll.viewportscale
            break;

        // --- Overlays ---
        case vr::VREvent_FocusEnter:
        case vr::VREvent_FocusLeave:
        case vr::VREvent_OverlayFocusChanged:
        case vr::VREvent_DashboardRequested:
        case vr::VREvent_OverlayShown:
        case vr::VREvent_OverlayHidden:
        case vr::VREvent_OverlayGamepadFocusGained:
        case vr::VREvent_OverlayGamepadFocusLost:
        case vr::VREvent_ImageLoaded:
        case vr::VREvent_ImageFailed:
            result._value = (double)event.data.overlay.overlayHandle;
            // event.data.overlay.devicePath
            break;

        // --- Chaperone / Room ---
        case vr::VREvent_SeatedZeroPoseReset:
            result._value = event.data.seatedZeroPoseReset.bResetBySystemMenu ? 1.0 : 0.0;
            break;

        case vr::VREvent_ChaperoneDataHasChanged:
        case vr::VREvent_ChaperoneUniverseHasChanged:
        case vr::VREvent_ChaperoneTempDataHasChanged:
        case vr::VREvent_ChaperoneSettingsHaveChanged:
        case vr::VREvent_ChaperoneFlushCache:
        case vr::VREvent_ChaperoneRoomSetupStarting:
        case vr::VREvent_ChaperoneRoomSetupFinished:
            result._value = (double)event.data.chaperone.m_nCurrentUniverse;
            // event.data.chaperone.m_nPreviousUniverse
            break;

        case vr::VREvent_IpdChanged:
            result._value = event.data.ipd.ipdMeters;
            break;

        // --- Screenshots ---
        case vr::VREvent_RequestScreenshot:
        case vr::VREvent_ScreenshotTaken:
        case vr::VREvent_ScreenshotFailed:
        case vr::VREvent_SubmitScreenshotToDashboard:
            result._value = (double)event.data.screenshot.handle;
            // event.data.screenshot.type
            break;

        case vr::VREvent_ScreenshotProgressToDashboard:
            result._value = event.data.screenshotProgress.progress;
            break;

        // --- Input & Haptics ---
        case vr::VREvent_Input_HapticVibration:
            result._value = (double)event.data.hapticVibration.componentHandle;
            // event.data.hapticVibration.containerHandle
            // event.data.hapticVibration.fDurationSeconds
            // event.data.hapticVibration.fFrequency
            // event.data.hapticVibration.fAmplitude
            break;

        case vr::VREvent_Input_BindingLoadFailed:
        case vr::VREvent_Input_BindingLoadSuccessful:
            result._value = (double)event.data.inputBinding.ulAppContainer;
            // event.data.inputBinding.pathMessage
            // event.data.inputBinding.pathUrl
            // event.data.inputBinding.pathControllerType
            break;

        case vr::VREvent_Input_ActionManifestLoadFailed:
            result._value = (double)event.data.actionManifest.pathAppKey;
            // event.data.actionManifest.pathMessage
            // event.data.actionManifest.pathMessageParam
            // event.data.actionManifest.pathManifestPath
            break;

        case vr::VREvent_Input_ProgressUpdate:
            result._value = event.data.progressUpdate.fProgress;
            // event.data.progressUpdate.ulApplicationPropertyContainer
            // event.data.progressUpdate.pathDevice
            // event.data.progressUpdate.pathInputSource
            // event.data.progressUpdate.pathProgressAction
            // event.data.progressUpdate.pathIcon
            break;

        // --- Spatial Anchors ---
        case vr::VREvent_SpatialAnchors_PoseUpdated:
        case vr::VREvent_SpatialAnchors_DescriptorUpdated:
        case vr::VREvent_SpatialAnchors_RequestPoseUpdate:
        case vr::VREvent_SpatialAnchors_RequestDescriptorUpdate:
            result._value = (double)event.data.spatialAnchor.unHandle;
            break;

        // --- UI & Tools ---
        case vr::VREvent_ShowUI:
            result._value = (double)event.data.showUi.eType; // EShowUIType enum
            break;

        case vr::VREvent_ShowDevTools:
            result._value = (double)event.data.showDevTools.nBrowserIdentifier;
            break;

        case vr::VREvent_Compositor_HDCPError:
            result._value = (double)event.data.hdcpError.eCode; // EHDCPError enum
            break;

        case vr::VREvent_StatusUpdate:
            result._value = (double)event.data.status.statusState; // EVRState enum
            break;

        // --- Keyboard ---
        case vr::VREvent_KeyboardCharInput:
        case vr::VREvent_KeyboardDone:
            result._value = (double)event.data.keyboard.uUserValue;
            // event.data.keyboard.cNewInput (char array - cannot map)
            break;

        // --- Misc / Complex ---
        case vr::VREvent_DualAnalog_Press:
        case vr::VREvent_DualAnalog_Unpress:
        case vr::VREvent_DualAnalog_Touch:
        case vr::VREvent_DualAnalog_Untouch:
        case vr::VREvent_DualAnalog_Move:
        case vr::VREvent_DualAnalog_ModeSwitch1:
        case vr::VREvent_DualAnalog_ModeSwitch2:
        case vr::VREvent_DualAnalog_Cancel:
            result._value = (double)event.data.dualAnalog.which; // k_EDualAnalog_Left/Right
            // event.data.dualAnalog.x
            // event.data.dualAnalog.y
            // event.data.dualAnalog.transformedX
            // event.data.dualAnalog.transformedY
            break;
            
        case vr::VREvent_PerformanceTest_FidelityLevel:
             result._value = (double)event.data.performanceTest.m_nFidelityLevel;
             break;

        // --- Device Status (The missing block) ---
        // These events primarily rely on result._device (trackedDeviceIndex).
        // Mapping _value to 1.0 ensures the data layer registers a "signal" 
        // rather than 0.0 (which might look like empty data).
        case vr::VREvent_TrackedDeviceActivated:
        case vr::VREvent_TrackedDeviceDeactivated:
        case vr::VREvent_TrackedDeviceUpdated:
        case vr::VREvent_TrackedDeviceUserInteractionStarted:
        case vr::VREvent_TrackedDeviceUserInteractionEnded:
        case vr::VREvent_EnterStandbyMode:
        case vr::VREvent_LeaveStandbyMode:
        case vr::VREvent_WirelessDisconnect:
        case vr::VREvent_WirelessReconnect:
            result._value = 1.0; 
            break;

        case vr::VREvent_TrackedDeviceRoleChanged:
            // No specific data struct, usually implies checking GetControllerRoleForTrackedDeviceIndex
            result._value = 1.0; 
            break;
            
        // --- Notifications ---
        case vr::VREvent_RenderToast:
        case vr::VREvent_Notification_Shown:
        case vr::VREvent_Notification_Hidden:
        case vr::VREvent_Notification_BeginInteraction:
        case vr::VREvent_Notification_Destroyed:
            result._value = (double)event.data.notification.notificationId;
            // event.data.notification.ulUserValue
            break;

        // --- Overlay Messages ---
        case vr::VREvent_MessageOverlay_Closed:
        case vr::VREvent_MessageOverlayCloseRequested:
            result._value = (double)event.data.messageOverlay.unVRMessageOverlayResponse; // vr::VRMessageOverlayResponse enum
            break;

        // --- Web Console ---
        case vr::VREvent_ConsoleOpened:
        case vr::VREvent_ConsoleClosed:
            // These don't have a specific VREvent struct listed in the union, 
            // but VREvent_WebConsole_t exists. Assuming they might use it or generic handles.
            // If strict to header comments, these might just be signals.
            result._value = (double)event.data.webConsole.webConsoleHandle;
            break;

        // --- Tracked Camera ---
        case vr::VREvent_TrackedCamera_EditingSurface:
            result._value = (double)event.data.cameraSurface.overlayHandle;
            // event.data.cameraSurface.nVisualMode
            break;

        case vr::VREvent_TrackedCamera_StartVideoStream:
        case vr::VREvent_TrackedCamera_StopVideoStream:
        case vr::VREvent_TrackedCamera_PauseVideoStream:
        case vr::VREvent_TrackedCamera_ResumeVideoStream:
            // These often rely on the trackedDeviceIndex (result._device)
            result._value = 1.0;
            break;

        // --- Lens / Distortion ---
        case vr::VREvent_LensDistortionChanged:
            // Global event, no specific data payload usually.
            result._value = 1.0;
            break;


    }
    result._probes = _probeAdditionalOpenVREvents(event);
  
    return result;
    }

  void OpenVRWrapper::updatePoses(float tt)  {
    if (auto compositor = vr::VRCompositor()) {
      for (int nDevice = 0; nDevice < xvr::MAXDEVICECOUNT; ++nDevice) {
        _devicePoses[nDevice].bPoseIsValid = false;
      }
      compositor->WaitGetPoses(_devicePoses, vr::k_unMaxTrackedDeviceCount, NULL, 0);
    } else {
      for (int nDevice = 0; nDevice < vr::k_unMaxTrackedDeviceCount; ++nDevice) {
        _devicePoses[nDevice].bPoseIsValid = false;
      }
    }
  }

glm::mat4 OpenVRWrapper::getEyeToHeadTransform(xvr::Eye const& eye) const {
  glm::mat4 eyeToHeadPlayspace{ 1.0f };
  if (sys) {
    eyeToHeadPlayspace = vrx::convert<glm::mat4>(sys->GetEyeToHeadTransform(vrx::convert<vr::EVREye>(eye)));
  }
  return eyeToHeadPlayspace;
}

void OpenVRWrapper::AcknowledgeQuit_Exiting() const { if (auto sys = this->sys ? vr::VRSystem() : nullptr) sys->AcknowledgeQuit_Exiting(); }
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------




// ----------------------------------------------------------------------------
// FACTORY
// ----------------------------------------------------------------------------
#include <unordered_map>
namespace xvr {
  int instances = 0;
  XVR_INTERFACE void *XVR_CALLTYPE XVR_GetGenericInterface( const char *pchInterfaceVersion, std::string* peError ) {
    instances++;
    std::string interface = pchInterfaceVersion ? pchInterfaceVersion : "";
    int width, height;
    if (2 == sscanf(interface.c_str(), "%dx%d\n", &width, &height )) {
      auto ptr = new ProceduralVRWrapper("<ProceduralVRWrapper: [renderTargetSize: '" + interface + "']>");
      ptr->setUniform("renderTargetSize", glm::uvec2{ width, height });
      return ptr;
    }
  
    if (interface != "XVRWrapper_001") return new ProceduralVRWrapper("<ProceduralVRWrapper: [invalid interface: '" + interface + "']>");
    
    auto err2str = [](vr::EVRInitError const& eError) {
        return std::string("ERROR: ")
            + std::string(vr::VR_GetVRInitErrorAsSymbol( eError )) 
            + " " + vr::VR_GetVRInitErrorAsEnglishDescription( eError );
    };
    vr::EVRInitError eError{ vr::VRInitError_None };
    vr::IVRSystem *sys = vr::VR_Init(&eError, vr::VRApplication_Scene);
    if (eError != vr::VRInitError_None || !sys) {
        if (eError == vr::VRInitError_None) eError = vr::VRInitError_Compositor_Failed;
        if (peError) *peError = err2str(eError);
        return nullptr;
    }
    auto compositor = vr::VRCompositor();
    if (!compositor) {
        if (peError) *peError = "!vr::VRCompositor";
        vr::VR_Shutdown();
        return nullptr;
    }
    compositor->SetTrackingSpace(vr::TrackingUniverseSeated);
    auto wrapper = new OpenVRWrapper(sys);
    {
      std::ostringstream log;
      #define VRLOG() log
      VRLOG() << "instance = " << instances << "\n";
      VRLOG() << "ptr = " << (wrapper != nullptr ? "yes" : "(nil)") << "\n";
      VRLOG() << "Driver = " << wrapper->get_driver_name() << "\n";
      VRLOG() << "Display = " << wrapper->get_serial_name() << "\n";
      VRLOG() << "VR driver! Initialized" << "\n";
      VRLOG() << "HMD: " << wrapper->get_model_name() << "\n";
      #undef VRLOG
      #ifdef __GNUC__
      auto strcpy_s = [](char* dest, size_t sz, const char* source) { !source ? strcpy(dest, "ESNULLP") : strnlen(source, sz) >= sz ? strcpy(dest, "ESNOSPC") : strncpy(dest, source, sz); };
      #endif
      strcpy_s(wrapper->_status, sizeof(wrapper->_status), log.str().c_str());
    }
    return wrapper;
  }
  XVR_INTERFACE bool XVR_CALLTYPE XVR_FreeGenericInterface( const char *pchInterfaceVersion, void* thing, std::string* peError) {
    std::string interface = pchInterfaceVersion ? pchInterfaceVersion : "";
    auto reterr = [&](std::string msg) {
        if (peError) *peError = msg;
        return false;
    };

    // TODO: proper cleanup for ProceduralVRWrapper (used only for debugging / development right now)

    if (interface != "XVRWrapper_001") {
        return reterr(std::string("unrecognized interface : ") + pchInterfaceVersion);
    }
    OpenVRWrapper* wrapper = dynamic_cast<OpenVRWrapper*>((XVRBase*)thing);
    if (!wrapper) return reterr("!wrapper " + std::to_string((intptr_t)thing));

    delete wrapper;
    instances--;
    fprintf(stdout, "[xvr] instances=%d (%s)\n", instances, instances == 0 ? "(shutting down)" : ""); fflush(stdout);
    if (instances == 0) {
        vr::VR_Shutdown();
    }
    return true;
  }

}

#endif // XVR_IMPLEMENTATION
