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
	app/main.o \
	app/ui.o \
	app/crypto.o \
	app/unix/fs.o \
	app/unix/time.o

FLAGS = \
	-Iapp \
	-Wall \
	-Wextra \
	-D_POSIX_C_SOURCE=200112L \
	-Wno-unused-value \
	-Wno-unused-result \
	-Wno-unused-parameter

ifeq ($(UNAME), Linux)

OBJS := $(OBJS) \
	app/unix/linux/window.o \
	app/unix/linux/window-quad.o \
	app/unix/linux/audio.o

LIBS = \
	-lm

endif

ifeq ($(UNAME), Darwin)

export SDKROOT=$(shell xcrun --sdk macosx --show-sdk-path)

OBJS := $(OBJS) \
	app/deps/imgui/imgui_impl_metal.o \
	app/unix/macos/window.o \
	app/unix/macos/window-quad.o \
	app/unix/macos/audio.o

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
