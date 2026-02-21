// openvr_api static STB-like single-file header module
// copyright (c) 2026 humbletim

#if !__has_include("openvr_api/headers/openvr.h")
    #error "OpenVR Recipe:"
    #error "git clone https://github.com/ValveSoftware/openvr --branch v1.6.10 --single-branch --depth 1 --filter=blob:none --sparse openvr_api"
    #error "git -C openvr_api sparse-checkout set headers src"
#endif

#if __INCLUDE_LEVEL__ == 0 && !defined(OPENVR_API_IMPLEMENTATION)
    #error "TPVM_RECIPE: -xc++ -DOPENVR_API_IMPLEMENTATION"
#endif

// ===========================================================================
#ifndef OPENVR_API_H
#define OPENVR_API_H
  #define OPENVR_BUILD_STATIC
  #define VR_API_PUBLIC
  #pragma include_alias("openvr.h", "openvr_api/headers/openvr.h")
  #include "openvr.h"
#endif // OPENVR_API_H

// ===========================================================================
#ifdef OPENVR_API_IMPLEMENTATION
  #pragma include_alias("pathtools_public.h",      "openvr_api/src/vrcommon/pathtools_public.h")
  #pragma include_alias("sharedlibtools_public.h", "openvr_api/src/vrcommon/sharedlibtools_public.h")
  #pragma include_alias("envvartools_public.h",    "openvr_api/src/vrcommon/envvartools_public.h")
  #pragma include_alias("hmderrors_public.h",      "openvr_api/src/vrcommon/hmderrors_public.h")
  #pragma include_alias("strtools_public.h",       "openvr_api/src/vrcommon/strtools_public.h")
  #pragma include_alias("vrpathregistry_public.h", "openvr_api/src/vrcommon/vrpathregistry_public.h")
  #pragma include_alias("json/json.h",             "openvr_api/src/json/json.h")

  #include "openvr_api/src/openvr_api_public.cpp"
  #include "openvr_api/src/vrcommon/dirtools_public.cpp"
  #include "openvr_api/src/vrcommon/envvartools_public.cpp"
  #include "openvr_api/src/vrcommon/hmderrors_public.cpp"
  #include "openvr_api/src/vrcommon/pathtools_public.cpp"
  #include "openvr_api/src/vrcommon/sharedlibtools_public.cpp"
  #include "openvr_api/src/vrcommon/strtools_public.cpp"
  #include "openvr_api/src/vrcommon/vrpathregistry_public.cpp"
  #include "openvr_api/src/jsoncpp.cpp"
#endif // OPENVR_API_IMPLEMENTATION
