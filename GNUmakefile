UNAME = $(shell uname -s)
MACHINE = $(shell uname -m)

NAME = \
	merton

OBJS = \
	src/cart.o \
	src/apu.o \
	src/sys.o \
	src/cpu.o \
	src/ppu.o \
	ui/main.o \
	ui/crypto.o \
	ui/unix/fs.o \
	ui/unix/time.o

CFLAGS = \
	-Iui \
	-Wall \
	-Wextra \
	-std=c99 \
	-D_POSIX_C_SOURCE=200112L \
	-Wno-unused-value \
	-Wno-unused-result \
	-Wno-unused-parameter

CXXFLAGS = \
	$(CFLAGS) \
	-std=c++11

ifeq ($(UNAME), Linux)

OBJS := $(OBJS) \
	ui/unix/linux/window.o \
	ui/unix/linux/window-quad.o \
	ui/unix/linux/audio.o

LIBS = \
	-lm

endif

ifeq ($(UNAME), Darwin)

export SDKROOT=$(shell xcrun --sdk macosx --show-sdk-path)

OBJS := $(OBJS) \
	ui/unix/macos/window.o \
	ui/unix/macos/window-quad.o \
	ui/unix/macos/audio.o

LIBS = \
	-framework AppKit \
	-framework QuartzCore \
	-framework Metal \
	-framework AudioToolbox

endif

LD_FLAGS = \

ifdef DEBUG
CFLAGS := $(CFLAGS) -O0 -g
else
CFLAGS := $(CFLAGS) -fvisibility=hidden -O3 -flto
LD_FLAGS := $(LD_FLAGS) -flto
endif

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
