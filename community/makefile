# Placeholder for your executable name
TARGET = fs-open.exe

# Compiler and Shell script wrapper
CXX = ../experiments/tpvm.sh compile
LD  = ../experiments/tpvm.sh link

# Build flags
CXXFLAGS = -g -O2 -w
LDFLAGS  = -g -O2 -Wl,/demangle:no

override CXXFLAGS += -MMD -MP -I.

VRMOD_SOURCES = \
       vrmod.llviewerdisplay.hpp \
       llviewerVR.cpp

HT_SOURCES = \
       ../asdf/humbletim/opengl.hpp \
       ../asdf/humbletim/xopenvr.hpp

TPV_SOURCES = \
       llviewerdisplay.cpp \
       lldir_win32.cpp

SRCS = $(VRMOD_SOURCES) $(HT_SOURCES) $(TPV_SOURCES)

# Prefix all objects with build/ directory
# This creates paths like: build/llviewercamera.cpp.obj or build/vrmod/session.hpp.obj
# OBJS = $(addprefix build/, $(addsuffix .obj, $(SRCS)))
# Replaces '../' with '__/' in SRCS, then applies suffix and prefix
OBJS = $(addprefix build/, $(addsuffix .obj, $(subst ../,__/,$(SRCS))))

.PHONY: all

all:
	@echo "$(OBJS)" | tr ' ' '\n'

# Link rule
# Depends on the objects in build/
$(TARGET): $(OBJS)
	LDFLAGS="$(LDFLAGS)" $(LD)

# Pattern rule for .cpp files
# FIX: Explicitly match 'build/' in target so '%' matches the source path (e.g. 'pipeline')
build/%.cpp.obj: %.cpp
	CXXFLAGS="$(CXXFLAGS)" $(CXX) $<

# Pattern rule for .hpp files (promoting them to source status)
# FIX: Explicitly match 'build/' in target so '%' matches the source path (e.g. 'vrmod/vrmod.session')
build/%.hpp.obj: %.hpp
	CXXFLAGS="$(CXXFLAGS)" $(CXX) $<

# --- Out-of-tree pattern rules ---
# Maps build/__/... back to ../... for external .cpp files
build/__/%.cpp.obj: ../%.cpp
	CXXFLAGS="$(CXXFLAGS)" $(CXX) $<
# Maps build/__/... back to ../... for external .hpp files (like xopenvr.hpp)
build/__/%.hpp.obj: ../%.hpp
	CXXFLAGS="$(CXXFLAGS)" $(CXX) $<

# Include the generated dependency files (silent if missing)
-include $(OBJS:.obj=.d)

.PHONY:patches
patches:
	@echo 'for x in `find build -name "*.cpp.obj"` ; do ../experiments/tpvm.sh diff $$(basename -s .obj $$x) > patches/$$(basename -s .obj $$x).patch ; done'
