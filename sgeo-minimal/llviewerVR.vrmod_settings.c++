/*
================================================================================
VR MOD SETTINGS: A Minimalist, Upstream-Netural Approach (humbletim 2025.08.09)
================================================================================

## Philosophy

This implementation provides a persistent settings strategy for the VR Mod with
two primary goals:

1.  **Zero Upstream Footprint:** Avoids altering core systems (like CMake),
.xml settings templates, or core application lifecycle code. This minimizes
potential for future merge or settings conflicts during upstream updates.
2.  **Unified User Experience:** Integrates VR settings directly into the
    viewer's standard `DebugSettings` dialog, so users don't need to manage
    separate configuration files (e.g., `vrconfig.ini`).

## How It Works

### 1. Consolidated Persistence

Instead of adding numerous individual settings to the viewer's configuration,
which would require complex management, we introduce a single, persistent string
setting: `vrmod.json`.

-   This string effectively stores VR-specific settings as JSON within the main
    `user_settings/settings.xml` file.
-   This "setting-within-a-setting" approach neatly compartmentalizes the VR
    mod's configuration, making it portable and easy to back up along with
    standard viewer settings.

### 2. Virtual Debug Settings

We declare individual VR settings (e.g., `vrmod.handlasers`, `vrmod.mousezoom`)
as standard non-persistent viewer controls. This makes them appear and function
natively in the `DebugSettings` dialog, searchable under the "vrmod" prefix.

-   **At Startup:** The system parses the persisted `vrmod.json` string and
    updates the values of these "virtual" settings.
-   **On Change:** When a user modifies a `vrmod.*` setting in the dialog, a
    signal is triggered. We intercept this signal, update the corresponding
    key-value pair in our JSON object, and re-serialize the entire object back
    into the `vrmod.json` string for persistence.

### 3. Early Initialization via `LLPounceable`

To ensure our virtual settings are registered before the `DebugSettings` dialog
is first created, we need to run our initialization code very early in the
application's startup sequence. The `LLPounceable` utility, a legacy mechanism
for deferred calls, allows us to hook into the application lifecycle at the
perfect moment—after essential systems are ready but before UI components like
floaters are initialized.

*/

#include "llsdjson.h" // LlsdFromJson / LlsdToJson
#include "llsdutil.h" // llsd_equals

#include "llcontrol.h"
extern LLControlGroup gSavedSettings;

// Provides a consolidated API for VR Mod settings.
struct VrModSettings {
    // The single, persistent setting that stores all VR config as a JSON string.
    LLControlVariablePtr mJsonBlob;

    // "Virtual" settings that appear in the DebugSettings dialog. They are not
    // persisted directly; their state is managed via the mJsonBlob.
    LLCachedControl<bool> handcontrollers{ gSavedSettings, "vrmod.handcontrollers", DEFAULTS.at("handcontrollers").as_bool(), "Toggle VR hand controller processing." };
    LLCachedControl<bool> handlasers{ gSavedSettings, "vrmod.handlasers", DEFAULTS.at("handlasers").as_bool(), "Toggle VR hand controller laser/cursor rendering." };
    LLCachedControl<bool> mousezoom{ gSavedSettings, "vrmod.mousezoom", DEFAULTS.at("mousezoom").as_bool(), "Toggle the VR mode mouse cursor corner-zooming effect." };
    LLCachedControl<bool> mousecursor{ gSavedSettings, "vrmod.mousecursor", DEFAULTS.at("mousecursor").as_bool(), "Toggle the VR mode mouse cursor rendering." };
    LLCachedControl<F32>  cameraAngle{ gSavedSettings, "vrmod.cameraAngle", DEFAULTS.at("cameraAngle").to_number<float>(),
        "Set to > 0.0 to apply a custom View Angle when entering VR mode.\n\n"
        "NOTE: Search for `CameraAngle` in DebugSettings and use Ctrl-8/9/0 "
        "to experiment and find a useful value."
    };
    LLCachedControl<F32>  nearClip{ gSavedSettings, "vrmod.nearClip", DEFAULTS.at("nearClip").to_number<float>(),
        "Set to > 0.0 value to configure custom VR mode near clipping threshold.\n"
        "0.001 is a good value to try using; 0.015 is a good compromise to avoid z-fighting.\n"
        "Note: When VR Mode is active, this setting can be changed live to test the effect of different near clipping thresholds."
    };

    // Updates a single property within the persisted JSON blob.
    void updateJsonEntry(std::string const& key, LLSD const& newValue);

    // Populates all virtual settings from the persisted JSON blob on startup.
    void applyJsonToSettings();

    // Default values for all VR settings.
    static const boost::json::object DEFAULTS;
};

/*static*/ const boost::json::object VrModSettings::DEFAULTS{
    { "handcontrollers", true },
    { "handlasers",      true },
    { "mousezoom",       true },
    { "mousecursor",     true },
    { "cameraAngle",     0.0f },
    { "nearClip",        0.0f },
};

namespace {
    // Safely parses a string into a boost::json::object, returning an empty object on failure.
    boost::json::object safe_json_parse(std::string const& json_string) {
        boost::json::error_code ec;
        auto parsed_value = boost::json::parse(json_string, ec);
        if (ec || !parsed_value.is_object()) {
            return boost::json::object();
        }
        return parsed_value.as_object();
    }
} // namespace

void VrModSettings::updateJsonEntry(std::string const& key, LLSD const& newValue) {
    auto current_json_string = mJsonBlob->getValue().asString();
    auto json_obj = safe_json_parse(current_json_string);

    json_obj[key] = LlsdToJson(newValue);

    auto new_json_string = boost::json::serialize(json_obj);
    if (current_json_string == new_json_string) return;

    LL_WARNS() << "VRMOD: Updating '" << key << "' ==> " << newValue << LL_ENDL;
    mJsonBlob->setValue(new_json_string);
}

void VrModSettings::applyJsonToSettings() {
    auto json_obj = safe_json_parse(mJsonBlob->getValue().asString());

    for (const auto& kv_pair : json_obj) {
        const std::string key { kv_pair.key() };
        if (auto setting = gSavedSettings.getControl("vrmod." + key)) {
            auto current_value = setting->getValue();
            auto new_value = LlsdFromJson(kv_pair.value());
            if (!llsd_equals(current_value, new_value)) {
                LL_WARNS() << "VRMOD: Applying vrmod.json ==> " << key << " = " << new_value << LL_ENDL;
                setting->setValue(new_value);
            }
        }
    }
}

// Global singleton instance, accessed from the main VR mod code.
std::shared_ptr<VrModSettings> gVrModSettings{};

// Initializes the settings system, creates the JSON blob, and wires up signals.
void instrument_vrmod_settings() {
    gVrModSettings = std::make_shared<VrModSettings>();

    gVrModSettings->mJsonBlob = gSavedSettings.declareControl(
        "vrmod.json", eControlType::TYPE_STRING, boost::json::serialize(VrModSettings::DEFAULTS),
        "VR Mod Advanced Configuration.\n\n"
        "NOTE: Please use the individual `vrmod.*` settings for tuning.\n\n"
        "Editing this JSON string directly may cause settings to reset if invalid.",
        eSanityType::SANITY_TYPE_NONE, LLSD(), "",
        LLControlVariable::PERSIST_NONDFT,
        true, // can_backup
        true // hide from DebugSettings (in favor of individual vrmod.* settings)
    );

    // 1. On startup, apply the persisted JSON values to the virtual settings.
    gVrModSettings->applyJsonToSettings();

    // 2. If vrmod.json is ever changed directly (e.g., manual edit, settings restore), re-apply.
    gVrModSettings->mJsonBlob->getSignal()->connect([](LLControlVariable*, const LLSD&, const LLSD&) {
        gVrModSettings->applyJsonToSettings();
    });

    // 3. For each virtual setting, connect its change signal to update the JSON blob.
    auto connect_signal = [](std::string const& key) {
        if (auto setting = gSavedSettings.getControl("vrmod." + key)) {
            setting->getSignal()->connect([key](LLControlVariable*, const LLSD& newValue, const LLSD&) {
                gVrModSettings->updateJsonEntry(key, newValue);
            });
        } else {
            LL_ERRS() << "VRMOD: Could not find virtual setting to instrument: " << key << LL_ENDL;
        }
    };

    for (const auto& kv_pair : VrModSettings::DEFAULTS) {
        connect_signal(kv_pair.key());
    }
}

// Use LLPounceable to schedule our settings initialization at the right time.
// A statically initialized constant forces this registration to run once at program launch.
#include "llpounceable.h"
extern LLPounceable<class LLMessageSystem*, LLPounceableStatic> gMessageSystem;
const bool gVrmodSettingsPending = []{
    gMessageSystem.callWhenReady([](auto) { instrument_vrmod_settings(); });
    return true;
}();
