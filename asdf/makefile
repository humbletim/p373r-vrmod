# Placeholder for your executable name
TARGET = fs-7.2.2-preview-avx2.exe

# Compiler and Shell script wrapper
CXX = ../experiments/tpvm.sh compile
LD  = ../experiments/tpvm.sh link

# Build flags
CXXFLAGS = -g -w
LDFLAGS  = -g -Wl,/demangle

override CXXFLAGS += -MMD -MP

VRMOD_SOURCES = $(addprefix vrmod/, \
       llviewerdisplay.hpp \
       llviewercamera.hpp \
       llagentcamera.hpp \
       hmdmath.hpp \
       session.hpp \
)
# override CXXFLAGS += -Ivrmod/

HT_SOURCES = $(addprefix humbletim/, \
        xopenvr.hpp \
        win32.desktop.hpp \
        opengl.hpp \
)
# override CXXFLAGS += -Ihumbletim/

TPV_SOURCES = \
       llviewercamera.cpp \
       llagentcamera.cpp \
       llviewerdisplay.cpp \
       llvoavatar.cpp \
       fsdata.cpp \
       llstartup.cpp \
       pipeline.cpp

SRCS = $(VRMOD_SOURCES) $(HT_SOURCES) $(TPV_SOURCES)

# UNUSED_SRCS = lllogininstance.cpp \
#        llappviewerwin32.cpp \
#

# Prefix all objects with build/ directory
# This creates paths like: build/llviewercamera.cpp.obj or build/vrmod/session.hpp.obj
OBJS = $(addprefix build/, $(addsuffix .obj, $(SRCS)))

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

# Include the generated dependency files (silent if missing)
-include $(OBJS:.obj=.d)

.PHONY:patches
patches:
	@echo 'for x in `find build -name "*.cpp.obj"` ; do ../experiments/tpvm.sh diff $$(basename -s .obj $$x) > patches/$$(basename -s .obj $$x).patch ; done'
