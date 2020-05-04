!IF [if /I "%Platform%" EQU "x64" exit 1]
PLATFORM = windows
!ELSE
PLATFORM = windows32
!ENDIF

BIN_NAME = \
	merton.exe

OBJS = \
	src\nes\cart.obj \
	src\nes\apu.obj \
	src\nes\cpu.obj \
	src\nes\sys.obj \
	src\nes\ppu.obj \
	src\app\main.obj \
	src\app\ui.obj \
	src\app\deps\imgui\im.obj \
	src\lib\crypto.obj \
	src\lib\windows\window.obj \
	src\lib\windows\window-quad.obj \
	src\lib\windows\audio.obj \
	src\lib\windows\time.obj \
	src\lib\windows\fs.obj

RESOURCES = \
	assets\windows\icon.res \
	assets\windows\versioninfo.res

RFLAGS = \
	/nologo

CFLAGS = \
	-I. \
	-Isrc \
	-Isrc\lib \
	-DWIN32_LEAN_AND_MEAN \
	-DUNICODE \
	/nologo \
	/wd4201 \
	/Gw \
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

CPPFLAGS = $(CFLAGS) \
	/wd4505

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
	winmm.lib \
	shcore.lib

LD_FLAGS = \
	/subsystem:windows \
	/nodefaultlib \
	/manifest:embed \
	/manifestinput:assets\windows\embed.manifest \
	/nologo

!IFDEF DEBUG
LD_FLAGS = $(LD_FLAGS) /debug
!ELSE
LD_FLAGS = $(LD_FLAGS) /LTCG
!ENDIF

all: clean clear $(OBJS) $(RESOURCES)
	link *.obj $(LIBS) $(RESOURCES) /out:$(BIN_NAME) $(LD_FLAGS)

clean:
	-del $(RESOURCES)
	-del *.obj
	-del *.exe
	-del *.ilk
	-del *.pdb

clear:
	cls
