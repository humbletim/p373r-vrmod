/*
    vrmod.settings.hpp - STB-style Header-Only Configuration Library
    
    USAGE:
    
    1. Define your settings struct:
       struct MySettings : vrmod::ExpandoSettings {
           float myVal = 1.0f;
           MySettings() : ExpandoSettings("vrmod") {}
           void declare() override {
               bind("myVal", &myVal, "A cool float value");
           }
       };

    2. In ONE source file:
       #define VRMOD_SETTINGS_IMPLEMENTATION
       #include "humbletim.slos.settings.hpp"
*/

#ifndef VRMOD_SETTINGS_HPP
#define VRMOD_SETTINGS_HPP

#include <string>
#include <vector>
#include <functional>
#include <type_traits>
#include <glm/glm.hpp>

template <class T> class LLPointer;
class LLControlVariable;

namespace vrmod {
    // Helper enum to avoid RTTI in the header
    enum class SettingType { Float, Bool, Int, Vec3, Vec4, String };

    // Base class for defining a settings group
    struct NoDestroy { 
        protected: virtual ~NoDestroy() = default;
    };
    struct ExpandoSettings : NoDestroy {
        LLControlVariable* jsonBlob;
        struct Binding {
            std::string name;
            void* dataPtr;
            SettingType type;
            std::string comment;
            std::function<void()> notify;
        };

        const std::string mNamespace; 
        std::vector<Binding> mPendingBindings;

        // Registers this instance with the global system
        ExpandoSettings(const char* ns);
        virtual ~ExpandoSettings() = default;

        // User must implement this to define bindings
        virtual void declare() = 0; 

        // The Interface: Binds a variable to a setting name
        template <typename T>
        void bind(const char* name, T* member, const char* comment = "", std::function<void()> notify = nullptr) {
            SettingType t;
            if constexpr (std::is_same_v<T, float>) t = SettingType::Float;
            else if constexpr (std::is_same_v<T, bool>) t = SettingType::Bool;
            else if constexpr (std::is_same_v<T, int>) t = SettingType::Int; // typically S32
            else if constexpr (std::is_same_v<T, glm::vec3>) t = SettingType::Vec3;
            else if constexpr (std::is_same_v<T, glm::vec4> || std::is_same_v<T, glm::ivec4>) t = SettingType::Vec4; // Treat ivec4 as vec4 for simplicity if needed
            else if constexpr (std::is_same_v<T, std::string>) t = SettingType::String;
            else static_assert(std::is_same_v<T, void>, "Unsupported setting type");

            mPendingBindings.push_back({ name, member, t, comment, notify });
        }
    };

} // namespace vrmod

#endif // VRMOD_SETTINGS_HPP


// ============================================================================
// IMPLEMENTATION SECTION
// ============================================================================
#ifdef VRMOD_SETTINGS_IMPLEMENTATION

#include "llcontrol.h"
#include "llsd.h"
#include "llsdjson.h" // LlsdFromJson, LlsdToJson
#include "llsdutil.h" // llsd_equals

#include "v3math.h"
#include "llrect.h"

// External globals required by the LL environment
extern LLControlGroup gSavedSettings;
extern LLPounceable<class LLMessageSystem*, LLPounceableStatic> gMessageSystem;

#include <glm/gtx/string_cast.hpp>

namespace {
    static struct _JSON {
        // JSON.stringify(val, 0, 2)
        static std::string stringify(boost::json::value const& jv, std::nullptr_t, int space) {
            if (space < 1) return boost::json::serialize(jv);
            std::string out;
            auto fmt = [&](auto& self, boost::json::value const& v, std::string const& ind) -> void {
                if (v.is_object()) {
                    auto const& obj = v.get_object();
                    if (obj.empty()) { out += "{}"; return; }
                    out += "{\n";
                    for (auto it = obj.begin(); it != obj.end(); ++it) {
                        out += (it != obj.begin() ? ",\n" : "") + ind + std::string(space, ' ') + 
                            boost::json::serialize(it->key()) + ": ";
                        self(self, it->value(), ind + std::string(space, ' '));
                    }
                    out += "\n" + ind + "}";
                } else if (v.is_array()) {
                    auto const& arr = v.get_array();
                    if (arr.empty()) { out += "[]"; return; }
                    out += "[\n";
                    for (auto it = arr.begin(); it != arr.end(); ++it) {
                        out += (it != arr.begin() ? ",\n" : "") + ind + std::string(space, ' ');
                        self(self, *it, ind + std::string(space, ' '));
                    }
                    out += "\n" + ind + "]";
                } else {
                    out += boost::json::serialize(v);
                }
            };
            fmt(fmt, jv, "");
            return out;
        }
        static std::string stringify(LLSD const& llsd, std::nullptr_t na, int space) { return stringify(LlsdToJson(llsd), na, space); }

        // JSON.stringify(val)
        std::string stringify(boost::json::value const& jv) const { return boost::json::serialize(jv); }
        std::string stringify(LLSD const& llsd) const { return boost::json::serialize(LlsdToJson(llsd)); }

        // JSON.parse<T>(json)
        template <typename T> inline T parse(std::string const& json_string) const {
            boost::json::error_code ec;
            auto value = boost::json::parse(json_string, ec);
            if constexpr (std::is_same_v<T, boost::json::value>) return value;
            else if constexpr (std::is_same_v<T, boost::json::object>) return value.is_object() ? value.as_object() : boost::json::object{};
            else if constexpr (std::is_same_v<T, LLSD>) return LlsdFromJson(value);
            else static_assert(std::is_same_v<T, void>, "Unsupported setting type");
        }
    } JSON;

    static struct { 
        // Object.keys(val)
        inline std::vector<std::string> keys(const boost::json::value& jv) const {
            std::vector<std::string> keys;
            if (!jv.is_object()) return keys;
            for (const auto& pair : jv.as_object()) keys.push_back(std::string(pair.key()));
            return keys;
        }
        inline std::vector<std::string> keys(const LLSD& _jv) const { auto jv = LlsdToJson(_jv);
            std::vector<std::string> keys;
            if (!jv.is_object()) return keys;
            for (const auto& pair : jv.as_object()) keys.push_back(std::string(pair.key()));
            return keys;
        }
    } Object;

    static struct {
        inline std::string join(auto const& data, std::string const& delim = ", ") const {
            std::stringstream ss;
            for (size_t i = 0; i < data.size(); ++i) {
                ss << data[i];
                if (i < data.size() - 1) ss << delim;
            }
            return ss.str();
        }
    } String;
}
namespace vrmod {

    // ------------------------------------------------------------------------
    // Global Registry
    // ------------------------------------------------------------------------
    static std::vector<ExpandoSettings*>& get_registry() {
        static std::vector<ExpandoSettings*> registry;
        return registry;
    }

    ExpandoSettings::ExpandoSettings(const char* ns) : mNamespace(ns) {
        get_registry().push_back(this);
    }

    // ------------------------------------------------------------------------
    // LLSD Conversion Helpers
    // ------------------------------------------------------------------------

    template <SettingType type> auto read_value(void* ptr) {
        if constexpr (type == SettingType::Float) return *static_cast<float*>(ptr);
        else if constexpr (type == SettingType::Bool) return *static_cast<bool*>(ptr);
        else if constexpr (type == SettingType::Int) return *static_cast<int*>(ptr);
        else if constexpr (type == SettingType::String) return *static_cast<std::string*>(ptr);
        else if constexpr (type == SettingType::Vec3) return *static_cast<glm::vec3*>(ptr);
        else if constexpr (type == SettingType::Vec4) return *static_cast<glm::vec4*>(ptr);
        else static_assert(false, "Unsupported setting type");
    }

    template <SettingType type> void write_value(void* ptr, auto value) {
        if constexpr (type == SettingType::Float) *static_cast<float*>(ptr) = (float)value;
        else if constexpr (type == SettingType::Bool) *static_cast<bool*>(ptr) = (bool)value;
        else if constexpr (type == SettingType::Int) *static_cast<int*>(ptr) = (int)value;
        else if constexpr (type == SettingType::String) *static_cast<std::string*>(ptr) = value;
        else if constexpr (type == SettingType::Vec3) *static_cast<glm::vec3*>(ptr) = value;
        else if constexpr (type == SettingType::Vec4) *static_cast<glm::vec4*>(ptr) = value;
        else static_assert(false, "Unsupported setting type");
    }

    template <typename T> std::string my_to_string(T const& v) {
        if constexpr (std::is_same_v<T, std::string>) return v;
        else if constexpr (std::is_same_v<T, float> ) return std::to_string(v);
        else if constexpr (std::is_same_v<T, bool>) return v ? "true" : "false";
        else if constexpr (std::is_same_v<T, int>) return std::to_string(v);
        else if constexpr (std::is_same_v<T, glm::vec3>) return glm::to_string(v);
        else if constexpr (std::is_same_v<T, glm::vec4>) return glm::to_string(v);
        else static_assert(std::is_same_v<T, void>, "Unsupported setting type");
    }

    template <SettingType type> std::string my_to_string(void* ptr) {
        if constexpr (type == SettingType::Float) return my_to_string(read_value<SettingType::Float>(ptr));
        else if constexpr (type == SettingType::Bool) return my_to_string(read_value<SettingType::Bool>(ptr));
        else if constexpr (type == SettingType::Int) return my_to_string(read_value<SettingType::Int>(ptr));
        else if constexpr (type == SettingType::String) return my_to_string(read_value<SettingType::String>(ptr));
        else if constexpr (type == SettingType::Vec3) return my_to_string(read_value<SettingType::Vec3>(ptr));
        else if constexpr (type == SettingType::Vec4) return my_to_string(read_value<SettingType::Vec4>(ptr));
        else static_assert(false, "Unsupported setting type");
    }

    template <SettingType type> bool maybe_write_value(std::string const& key, void* ptr, auto value) {
        #define __mwv__(Type, T) constexpr (type == Type) { \
            auto prev = read_value<Type>(ptr); \
            if (value == prev) return false; \
            fprintf(stdout, "write_value %s (%s) = %s (was: %s)\n", key.c_str(), typeid(value).name(), my_to_string(value).c_str(), my_to_string(prev).c_str()); \
            write_value<Type>(ptr, value); \
            return true; \
        }
        if __mwv__(SettingType::Float, float)
        else if __mwv__(SettingType::Bool, bool)
        else if __mwv__(SettingType::Int, int)
        else if __mwv__(SettingType::String, std::string)
        else if __mwv__(SettingType::Vec3, glm::vec3)
        else if __mwv__(SettingType::Vec4, glm::vec4)
        else static_assert(false, "Unsupported setting type");
        #undef __mwv__ 
    }

    static LLSD sync_from_bound_value(void* ptr, SettingType type) {
        switch (type) {
            case SettingType::Float:  return read_value<SettingType::Float>(ptr);
            case SettingType::Bool:  return read_value<SettingType::Bool>(ptr);
            case SettingType::Int:  return read_value<SettingType::Int>(ptr);
            case SettingType::String:  return read_value<SettingType::String>(ptr);
            case SettingType::Vec3: {
                glm::vec3 v = read_value<SettingType::Vec3>(ptr);
                LLSD arr = LLSD::emptyArray();
                arr.append(v.x); arr.append(v.y); arr.append(v.z);
                return arr;
            }
            case SettingType::Vec4: {
                glm::vec4 v = read_value<SettingType::Vec4>(ptr);
                LLSD arr = LLSD::emptyArray();
                arr.append(v.x); arr.append(v.y); arr.append(v.z); arr.append(v.w);
                return arr;
            }
            default: fprintf(stdout, "unsupported type: %d\n", type); fflush(stdout); break;
        }
        return LLSD();
    }

    // The Interface: Binds a variable to a setting name
    static void sync_to_bound_value(std::string const& key, const LLSD& val, void* ptr, SettingType type) {
        bool updated = false;
        switch (type) {
            case SettingType::Float: updated = maybe_write_value<SettingType::Float>(key, ptr, (float)val.asReal()); break;
            case SettingType::Bool: updated = maybe_write_value<SettingType::Bool>(key, ptr, (bool)val.asBoolean()); break;
            case SettingType::Int: updated = maybe_write_value<SettingType::Int>(key, ptr, (int)val.asInteger()); break;
            case SettingType::String: updated = maybe_write_value<SettingType::String>(key, ptr, val.asString()); break;
            case SettingType::Vec3: if (val.isArray() && val.size() >= 3) updated = maybe_write_value<SettingType::Vec3>(key, ptr, glm::vec3((float)val[0].asReal(), (float)val[1].asReal(), (float)val[2].asReal())); break;
            case SettingType::Vec4: if (val.isArray() && val.size() >= 4) updated = maybe_write_value<SettingType::Vec4>(key, ptr, glm::vec4((float)val[0].asReal(), (float)val[1].asReal(), (float)val[2].asReal(), (float)val[3].asReal())); break;
            default: return;
        }
        if (updated) fprintf(stdout, "sync_bound_value: %s bind.ptr updated\n", key.c_str());
    }

    // ------------------------------------------------------------------------
    // Instantiation Logic
    // ------------------------------------------------------------------------
    static void InstantiateSettings(void*) {
        fprintf(stdout, "vrmod.settings: Initializing...\n");

        for (auto* settings : get_registry()) {
            
            // 1. Hydrate Bindings (User declares their C++ defaults here)
            settings->declare();

            // 2. Build "C++ Defaults" Map
            // This represents the "Factory Reset" state.
            LLSD cppDefaults = LLSD::emptyMap();
            for (const auto& bind : settings->mPendingBindings) {
                cppDefaults[bind.name] = sync_from_bound_value(bind.dataPtr, bind.type);
            }

            // 3. Declare Persistent JSON Blob
            // This is the only thing that actually saves to disk.
            std::string jsonKey = settings->mNamespace + ".json";
            settings->jsonBlob = gSavedSettings.declareControl(
                jsonKey,
                eControlType::TYPE_STRING,
                JSON.stringify(cppDefaults), // Default is C++ state
                "Configuration for " + settings->mNamespace,
#if !__secondlife__ 
                eSanityType::SANITY_TYPE_NONE, LLSD(), "",
#endif
                LLControlVariable::PERSIST_NONDFT,
#if !__secondlife__
                true, // can_backup
#endif
                true // Hidden from editor
            );

            // 4. Load Saved State
            // Read what is currently on disk.

            static const auto reup = [](ExpandoSettings* settings) -> LLSD {
                LLSD savedConfig;
                if (!settings->jsonBlob->isDefault()) savedConfig = JSON.parse<LLSD>(settings->jsonBlob->getSaveValue().asString());
                if (!savedConfig.isMap()) savedConfig = LLSD::emptyMap();

                // 5. Calculate Sets (Bound vs Passthru)
                LLSD boundConfig = LLSD::emptyMap();
                LLSD passthruConfig = LLSD::emptyMap();
                
                // Start with everything in saved, then filter
                LLSD::map_iterator iter = savedConfig.beginMap();
                LLSD::map_iterator end  = savedConfig.endMap();
                for(; iter != end; ++iter) {
                    bool isBound = false;
                    for(const auto& bind : settings->mPendingBindings) {
                        if(bind.name == iter->first) { isBound = true; break; }
                    }
                    if(isBound) boundConfig[iter->first] = iter->second;
                    else        passthruConfig[iter->first] = iter->second;
                }

                // 6. Logging
                fprintf(stdout, "--- [%s] ---\n", settings->mNamespace.c_str());
                fprintf(stdout, "Incoming Saved Settings: %s\n", String.join(Object.keys(savedConfig)).c_str());
                fprintf(stdout, "Passthru (ignored):      %s\n", String.join(Object.keys(passthruConfig)).c_str());
                fprintf(stdout, "Bound Settings:          %s\n", String.join(Object.keys(boundConfig)).c_str());
                return savedConfig;
            };

            fprintf(stdout, "C++ Declared Settings:   %s\n", JSON.stringify(cppDefaults).c_str());
            LLSD savedConfig = reup(settings);

            // handle JSON updates splat to unit settings
            settings->jsonBlob->getSignal()->connect([savedConfig, settings](LLControlVariable*, const LLSD&, const LLSD&) {
                LLSD blob = JSON.parse<LLSD>(settings->jsonBlob->getValue().asString());
                if (!blob.isMap()) blob = LLSD::emptyMap();
                for (const auto& bind : settings->mPendingBindings) {
                    std::string fullKey = settings->mNamespace + "." + bind.name;
                    LLControlVariable* control = gSavedSettings.getControl(fullKey);
                    if (!control) { fprintf(stdout, "skippping %s\n", fullKey.c_str()); continue; };
                    LLSD unitValue = control->getValue();
                    LLSD defaultValue = control->getDefault();
                    LLSD groupValue = blob.has(bind.name) ? blob[bind.name] : defaultValue;
                    bool unchanged = llsd_equals(unitValue, groupValue);
                    bool def = llsd_equals(groupValue, defaultValue);
                    if (def) {
                        blob.erase(bind.name);
                        continue;
                    }
                    if (unchanged) continue;
                    fprintf(stdout, "assign %s=%s\n", fullKey.c_str(), JSON.stringify(groupValue).c_str());
                    control->setValue(groupValue);
                }
            });
            // 7. Wire up Virtual Controls
            for (const auto& bind : settings->mPendingBindings) {
                std::string fullKey = settings->mNamespace + "." + bind.name;
                LLControlVariable* control = nullptr;
                auto persistMode = LLControlVariable::PERSIST_NO; 
                
                // A. DECLARE with C++ Default (Crucial for "Reset" to work)
                // Note: bind.dataPtr currently holds the C++ default value
                switch (bind.type) {
                    case SettingType::Float:
                        control = gSavedSettings.declareF32(fullKey, *static_cast<float*>(bind.dataPtr), bind.comment, persistMode);
                        break;
                    case SettingType::Bool:
                        control = gSavedSettings.declareBOOL(fullKey, *static_cast<bool*>(bind.dataPtr), bind.comment, persistMode);
                        break;
                    case SettingType::Int:
                        control = gSavedSettings.declareS32(fullKey, *static_cast<int*>(bind.dataPtr), bind.comment, persistMode);
                        break;
                    case SettingType::String:
                        control = gSavedSettings.declareString(fullKey, *static_cast<std::string*>(bind.dataPtr), bind.comment, persistMode);
                        break;
                    case SettingType::Vec3: {
                        LLVector3 v(*static_cast<glm::vec3*>(bind.dataPtr));
                        control = gSavedSettings.declareVec3(fullKey, v, bind.comment, persistMode);
                        break;
                    }
                    case SettingType::Vec4: {
                        LLRect c; 
                        glm::vec4 v = *static_cast<glm::vec4*>(bind.dataPtr);
                        c.mLeft = v[0];
                        c.mTop = v[1];
                        c.mRight = v[2];
                        c.mBottom = v[3];
                        control = gSavedSettings.declareRect(fullKey, c, bind.comment, persistMode);
                        break;
                    }
                }

                if (control) {
                    // B. OVERRIDE with Saved Value (if exists)
                    if (savedConfig.has(bind.name)) {
                        control->setValue(savedConfig[bind.name]);
                        // Update the C++ memory immediately so it matches
                        sync_to_bound_value(bind.name, savedConfig[bind.name], bind.dataPtr, bind.type);
                    } else {
                        control->setValue(cppDefaults[bind.name]);
                        // Update the C++ memory immediately so it matches
                        sync_to_bound_value(bind.name, cppDefaults[bind.name], bind.dataPtr, bind.type);
                    }

                    // C. CONNECT Signal for future updates
                    control->getSignal()->connect([bind, settings, bindName=bind.name]
                    (LLControlVariable* self, const LLSD& newVal, const LLSD&) {
                        
                        // 1. Update C++ Memory "Live"
                        sync_to_bound_value(bind.name, newVal, bind.dataPtr, bind.type);

                        // 2. Update Persistence
                        // Re-fetch fresh blob (to catch updates from other controls)
                        LLSD blob = JSON.parse<LLSD>(settings->jsonBlob->getValue().asString());
                        LLSD oldBlob = blob;
                        if (!blob.isMap()) blob = LLSD::emptyMap();

                        if (llsd_equals(newVal, self->getDefault()))
                            blob.erase(bindName);
                        else
                            blob[bindName] = newVal;

                        if (!llsd_equals(blob, oldBlob)) {
                            fprintf(stdout, "Outgoing Settings: %s\n", JSON.stringify(blob, 0, 2).c_str());
                            settings->jsonBlob->setValue(JSON.stringify(blob));
                        }
                    });
                }
            }
            fprintf(stdout, "----------------------\n");
        }
    }

    // ------------------------------------------------------------------------
    // Pounce Registration
    // ------------------------------------------------------------------------
    static bool g_AutoInit = []{
        gMessageSystem.callWhenReady(InstantiateSettings);
        return true;
    }();

} // namespace vrmod

#elif __INCLUDE_LEVEL__ == 0
    #error "TPVM_RECIPE: -xc++ -DVRMOD_SETTINGS_IMPLEMENTATION"
#endif