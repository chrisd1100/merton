UNAME = $(shell uname -s)
ARCH = $(shell uname -m)

NAME = \
	merton

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
	-I. \
	-Isrc \
	-I../libmatoya/src \
	-Wall \
	-Wextra \
	-D_POSIX_C_SOURCE=200112L \
	-Wno-unused-function \
	-Wno-unused-result \
	-Wno-unused-value \
	-Wno-unused-parameter

ifeq ($(UNAME), Linux)

OBJS := $(OBJS) \
	src/app/deps/imgui/impl/im-gl.o

LIBS = \
	-ldl \
	-lstdc++ \
	-lm \
	-lc \
	-lgcc_s

OS = linux
endif

ifeq ($(UNAME), Darwin)

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
endif

LIBS := ../libmatoya/bin/$(OS)/$(ARCH)/libmatoya.a $(LIBS)

LD_FLAGS = \
	-nodefaultlibs

ifdef DEBUG
FLAGS := $(FLAGS) -O0 -g
else
FLAGS := $(FLAGS) -fvisibility=hidden -O3 -flto
LD_FLAGS := $(LD_FLAGS) -flto
endif

CFLAGS = $(FLAGS) \
	-std=c99

CXXFLAGS = $(FLAGS) \
	-std=c++11

LD_COMMAND = \
	$(CC) \
	$(OBJS) \
	$(LIBS) \
	-o $(NAME) \
	$(LD_FLAGS)

all: clean clear $(OBJS)
	$(LD_COMMAND)

clean:
	rm -rf $(OBJS)

clear:
	clear
