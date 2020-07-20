UNAME_S = $(shell uname -s)
ARCH = $(shell uname -m)
BIN_NAME = merton

.m.o:
	$(CC) $(OCFLAGS)  -c -o $@ $<

OBJS = \
	src/nes/cart.o \
	src/nes/apu.o \
	src/nes/sys.o \
	src/nes/cpu.o \
	src/nes/ppu.o \
	src/app/main.o \
	src/app/ui.o \
	src/app/deps/imgui/im.o

FLAGS = \
	-Wall \
	-Wextra \
	-Wno-unused-function \
	-Wno-unused-result \
	-Wno-unused-value \
	-Wno-unused-parameter

INCLUDES = \
	-I. \
	-Isrc \
	-I../libmatoya/src

DEFS = \
	-D_POSIX_C_SOURCE=200112L

LD_FLAGS = \
	-nodefaultlibs

############
### WASM ###
############
ifdef EMSDK

OBJS := $(OBJS) \
	src/app/deps/imgui/impl/im-gl.o

CC = emcc
CXX = em++
AR = emar

CXXFLAGS = -fno-threadsafe-statics

OS = web
ARCH := wasm

else
#############
### LINUX ###
#############
ifeq ($(UNAME_S), Linux)

OBJS := $(OBJS) \
	src/app/deps/imgui/impl/im-gl.o

LIBS = \
	-ldl \
	-lstdc++ \
	-lm \
	-lc \
	-lgcc_s

OS = linux
LIBS := ../libmatoya/bin/$(OS)/$(ARCH)/libmatoya.a $(LIBS)
endif

#############
### MACOS ###
#############
ifeq ($(UNAME_S), Darwin)

export SDKROOT=$(shell xcrun --sdk macosx --show-sdk-path)

OBJS := $(OBJS) \
	src/app/deps/imgui/impl/im-mtl.o

LIBS = \
	-lc \
	-lc++ \
	-framework AppKit \
	-framework QuartzCore \
	-framework Metal \
	-framework AudioToolbox

OS = macos
LIBS := ../libmatoya/bin/$(OS)/$(ARCH)/libmatoya.a $(LIBS)
endif
endif

ifdef DEBUG
FLAGS := $(FLAGS) -O0 -g
else
FLAGS := $(FLAGS) -O3 -fvisibility=hidden
ifdef LTO
FLAGS := $(FLAGS) -flto
LD_FLAGS := $(LD_FLAGS) -flto
endif
endif

CFLAGS = $(INCLUDES) $(DEFS) $(FLAGS) -std=c99
CXXFLAGS := $(CXXFLAGS) $(INCLUDES) $(DEFS) $(FLAGS) -std=c++11
OCFLAGS = $(CFLAGS) -fobjc-arc

all: clean clear
	make objs -j4

objs: $(OBJS)
	$(CC) -o $(BIN_NAME) $(OBJS) $(LIBS) $(LD_FLAGS)

clean:
	@rm -rf $(OBJS)

clear:
	@clear
