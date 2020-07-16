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
	src\app\deps\imgui\impl\im-dx11.obj

RESOURCES = \
	assets\windows\icon.res \
	assets\windows\versioninfo.res

RFLAGS = \
	-Isrc \
	/nologo

CFLAGS = \
	-I. \
	-I..\libmatoya\src \
	-Isrc \
	-DWIN32_LEAN_AND_MEAN \
	-DUNICODE \
	/nologo \
	/wd4201 \
	/W4 \
	/MT \
	/MP

LD_FLAGS = \
	/subsystem:windows \
	/nodefaultlib \
	/manifest:embed \
	/manifestinput:assets\windows\embed.manifest \
	/nologo

!IFDEF DEBUG
CFLAGS = $(CFLAGS) /Oy- /Ob0 /Zi
LD_FLAGS = $(LD_FLAGS) /debug
!ELSE
CFLAGS = $(CFLAGS) /O2 /GS- /Gw /Gy
!IFDEF LTO
CFLAGS = $(CFLAGS) /GL
LD_FLAGS = $(LD_FLAGS) /LTCG
!ENDIF
!ENDIF

CPPFLAGS = $(CFLAGS) \
	/wd4505

LIBS = \
	..\libmatoya\bin\windows\%Platform%\matoya.lib \
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
	shcore.lib \
	bcrypt.lib \
	shlwapi.lib

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
