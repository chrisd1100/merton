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
	app\main.obj \
	app\ui.obj \
	app\crypto.obj \
	app\windows\window.obj \
	app\windows\window-quad.obj \
	app\windows\audio.obj \
	app\windows\time.obj \
	app\windows\fs.obj

RESOURCES = \
	app\assets\windows\icon.res \
	app\assets\windows\versioninfo.res

RFLAGS = \
	/nologo

CFLAGS = \
	-Iapp \
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
	/manifest:embed \
	/manifestinput:app\assets\windows\embed.manifest \
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
