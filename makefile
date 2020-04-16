!IF [if /I "%Platform%" EQU "x64" exit 1]
PLATFORM = windows
!ELSE
PLATFORM = windows32
!ENDIF

BIN_NAME = \
	merton.exe

OBJS = \
	src\cart.obj \
	src\apu.obj \
	src\cpu.obj \
	src\sys.obj \
	src\ppu.obj \
	ui\main.obj \
	ui\crypto.obj \
	ui\windows\window.obj \
	ui\windows\window-quad.obj \
	ui\windows\audio.obj \
	ui\windows\time.obj \
	ui\windows\fs.obj

RESOURCES = \
	ui\assets\windows\icon.res

RFLAGS = \
	/nologo

CFLAGS = \
	-Iui \
	-DWIN32_LEAN_AND_MEAN \
	-DUNICODE \
	/nologo \
	/wd4201 \
	/GS- \
	/W4 \
	/O2 \
	/MT \
	/MP

!IFDEF DEBUG
CFLAGS = $(CFLAGS) /Oy- /Ob0 /Zi
!ELSE
CFLAGS = $(CFLAGS) /GL
!ENDIF

CPPFLAGS = $(CFLAGS)

LIBS = \
	libvcruntime.lib \
	libucrt.lib \
	libcmt.lib \
	kernel32.lib \
	user32.lib \
	shell32.lib \
	d3d11.lib \
	dxguid.lib \
	ole32.lib \
	uuid.lib \
	winmm.lib

LD_FLAGS = \
	/subsystem:windows \
	/nodefaultlib \
	/nologo

!IFDEF DEBUG
LD_FLAGS = $(LD_FLAGS) /debug
!ELSE
LD_FLAGS = $(LD_FLAGS) /LTCG
!ENDIF

all: clean clear $(OBJS) $(RESOURCES)
	link *.obj $(LIBS) $(RESOURCES) /out:$(BIN_NAME) $(LD_FLAGS)

clean:
	del $(RESOURCES)
	del *.obj
	del *.exe
	del *.ilk
	del *.pdb

clear:
	cls
