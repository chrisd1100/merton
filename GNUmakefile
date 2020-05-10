UNAME = $(shell uname -s)
MACHINE = $(shell uname -m)

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
	src/app/deps/imgui/im.o \
	src/lib/crypto.o \
	src/lib/unix/fs.o \
	src/lib/unix/time.o

FLAGS = \
	-I. \
	-Isrc \
	-Isrc/lib \
	-Wall \
	-Wextra \
	-D_POSIX_C_SOURCE=200112L \
	-Wno-unused-function \
	-Wno-unused-result \
	-Wno-undefined-internal \
	-Wno-unused-value \
	-Wno-unused-parameter

ifeq ($(UNAME), Linux)

OBJS := $(OBJS) \
	src/lib/unix/linux/window.o \
	src/lib/unix/linux/window-quad.o \
	src/lib/unix/linux/audio.o

LIBS = \
	-lm

endif

ifeq ($(UNAME), Darwin)

export SDKROOT=$(shell xcrun --sdk macosx --show-sdk-path)

OBJS := $(OBJS) \
	src/app/deps/imgui/imgui_impl_metal.o \
	src/lib/unix/macos/window.o \
	src/lib/unix/macos/window-quad.o \
	src/lib/unix/macos/audio.o

LIBS = \
	-lc++ \
	-framework AppKit \
	-framework QuartzCore \
	-framework Metal \
	-framework AudioToolbox

endif

LD_FLAGS = \

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

%.o: %.mm
	$(CC) $(CXXFLAGS)   -c -o $@ $<

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
