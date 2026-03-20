// openjpeg 2.5.3 static STB-like single-file header module
// copyright (c) 2026 humbletim

// FIXME: since llimagej2coj.cpp is the only core source including openjpeg.h...
//  ... for now I always assume/include the implementation detail
//  note: this allows this solution to be swapped-in transparently instead of the prebuilt 3p
//  (later this could be moved to a single dedicated openjp2_implementation.cpp code unit)
#define OPENJP2_API_IMPLEMENTATION
/////////////////

#if !__has_include("openjpeg/src/lib/openjp2/opj_includes.h")
    #error "OpenVR Recipe:"
    #error "git clone https://github.com/uclouvain/openjpeg openjpeg && git -C openjpeg checkout 210a8a5"
#endif

#if __INCLUDE_LEVEL__ == 0 && !defined(OPENJP2_API_IMPLEMENTATION)
    #error "TPVM_RECIPE: -xc++ -DOPENJP2_API_IMPLEMENTATION"
#endif

// ===========================================================================
#ifndef OPENJP2_API_H
#define OPENJP2_API_H
#pragma message("hi there OPJ_STATIC")
  #define OPJ_STATIC
  #define OPJ_PACKAGE_VERSION "2.5.3"  
  #define OPJ_HAVE__ALIGNED_MALLOC 1  
  #define MUTEX_win32 1  
  #define OPJ_VERSION_MAJOR 2
  #define OPJ_VERSION_MINOR 5
  #define OPJ_VERSION_BUILD 3
  
  #pragma include_alias("openjpeg.h", "openjpeg/src/lib/openjp2/openjpeg.h")
  #pragma include_alias("opj_config.h", "openjpeg/src/lib/openjp2/openjpeg.h")
  #pragma include_alias("opj_config_private.h", "openjpeg/src/lib/openjp2/openjpeg.h")
  #include "openjpeg/src/lib/openjp2/openjpeg.h"
#endif // OPENJP2_API_H

// ===========================================================================
#ifdef OPENJP2_API_IMPLEMENTATION
#pragma message("hi there OPENJP2_API_IMPLEMENTATION")
  
// #include "openjpeg/src/lib/openjp2/opj_config.h"  
// #include "openjpeg/src/lib/openjp2/opj_config_private.h"  
  
#include "openjpeg/src/lib/openjp2/opj_includes.h"  
#include "openjpeg/src/lib/openjp2/openjpeg.h"  
  
/* Implementation sources (core only; JPIP excluded) */  
#include "openjpeg/src/lib/openjp2/thread.c"  
#include "openjpeg/src/lib/openjp2/bio.c"  
#include "openjpeg/src/lib/openjp2/cio.c"  
#include "openjpeg/src/lib/openjp2/dwt.c"  
#include "openjpeg/src/lib/openjp2/event.c"  
#define opj_t1_allocate_buffers opj_ht_t1_allocate_buffers  
#include "openjpeg/src/lib/openjp2/ht_dec.c"  
#undef opj_t1_allocate_buffers
#include "openjpeg/src/lib/openjp2/image.c"  
#include "openjpeg/src/lib/openjp2/invert.c"  
#include "openjpeg/src/lib/openjp2/j2k.c"  
#include "openjpeg/src/lib/openjp2/jp2.c"  
#include "openjpeg/src/lib/openjp2/mct.c"  
#include "openjpeg/src/lib/openjp2/mqc.c"  
#include "openjpeg/src/lib/openjp2/openjpeg.c"  
#include "openjpeg/src/lib/openjp2/opj_clock.c"  
#include "openjpeg/src/lib/openjp2/pi.c"  
#include "openjpeg/src/lib/openjp2/t1.c"  
#include "openjpeg/src/lib/openjp2/t2.c"  
#include "openjpeg/src/lib/openjp2/tcd.c"  
#include "openjpeg/src/lib/openjp2/tgt.c"  
#include "openjpeg/src/lib/openjp2/function_list.c"  
#include "openjpeg/src/lib/openjp2/opj_malloc.c"  
#include "openjpeg/src/lib/openjp2/sparse_array.c"

#endif // OPENVR_API_IMPLEMENTATION
