#pragma once



/*** INTERFACE ***/

#if defined GL_EXTERNAL

#define GL_GLEXT_PROTOTYPES
#include "../../GL/glcorearb30.h"

#define gl_dl_global_init() true

#else

#include "matoya.h"

#include "../../GL/glcorearb30.h"

static PFNGLGENFRAMEBUFFERSPROC         glGenFramebuffers;
static PFNGLDELETEFRAMEBUFFERSPROC      glDeleteFramebuffers;
static PFNGLBINDFRAMEBUFFERPROC         glBindFramebuffer;
static PFNGLBLITFRAMEBUFFERPROC         glBlitFramebuffer;
static PFNGLFRAMEBUFFERTEXTURE2DPROC    glFramebufferTexture2D;
static PFNGLENABLEPROC                  glEnable;
static PFNGLISENABLEDPROC               glIsEnabled;
static PFNGLDISABLEPROC                 glDisable;
static PFNGLVIEWPORTPROC                glViewport;
static PFNGLGETINTEGERVPROC             glGetIntegerv;
static PFNGLGETFLOATVPROC               glGetFloatv;
static PFNGLBINDTEXTUREPROC             glBindTexture;
static PFNGLDELETETEXTURESPROC          glDeleteTextures;
static PFNGLGETTEXLEVELPARAMETERIVPROC  glGetTexLevelParameteriv;
static PFNGLTEXPARAMETERIPROC           glTexParameteri;
static PFNGLGENTEXTURESPROC             glGenTextures;
static PFNGLTEXIMAGE2DPROC              glTexImage2D;
static PFNGLTEXSUBIMAGE2DPROC           glTexSubImage2D;
static PFNGLDRAWELEMENTSPROC            glDrawElements;
static PFNGLGETATTRIBLOCATIONPROC       glGetAttribLocation;
static PFNGLSHADERSOURCEPROC            glShaderSource;
static PFNGLBINDBUFFERPROC              glBindBuffer;
static PFNGLVERTEXATTRIBPOINTERPROC     glVertexAttribPointer;
static PFNGLCREATEPROGRAMPROC           glCreateProgram;
static PFNGLUNIFORM1IPROC               glUniform1i;
static PFNGLACTIVETEXTUREPROC           glActiveTexture;
static PFNGLDELETEBUFFERSPROC           glDeleteBuffers;
static PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray;
static PFNGLBUFFERDATAPROC              glBufferData;
static PFNGLDELETESHADERPROC            glDeleteShader;
static PFNGLGENBUFFERSPROC              glGenBuffers;
static PFNGLCOMPILESHADERPROC           glCompileShader;
static PFNGLLINKPROGRAMPROC             glLinkProgram;
static PFNGLGETUNIFORMLOCATIONPROC      glGetUniformLocation;
static PFNGLCREATESHADERPROC            glCreateShader;
static PFNGLATTACHSHADERPROC            glAttachShader;
static PFNGLUSEPROGRAMPROC              glUseProgram;
static PFNGLGETSHADERIVPROC             glGetShaderiv;
static PFNGLDETACHSHADERPROC            glDetachShader;
static PFNGLDELETEPROGRAMPROC           glDeleteProgram;
static PFNGLCLEARPROC                   glClear;
static PFNGLCLEARCOLORPROC              glClearColor;
static PFNGLGETERRORPROC                glGetError;
static PFNGLGETSHADERINFOLOGPROC        glGetShaderInfoLog;
static PFNGLFINISHPROC                  glFinish;
static PFNGLSCISSORPROC                 glScissor;
static PFNGLBLENDFUNCPROC               glBlendFunc;
static PFNGLBLENDEQUATIONPROC           glBlendEquation;
static PFNGLUNIFORMMATRIX4FVPROC        glUniformMatrix4fv;
static PFNGLBLENDEQUATIONSEPARATEPROC   glBlendEquationSeparate;
static PFNGLBLENDFUNCSEPARATEPROC       glBlendFuncSeparate;
static PFNGLGETPROGRAMIVPROC            glGetProgramiv;



/*** RUNTIME OPEN ***/

static MTY_Atomic32 GL_DL_LOCK;
static bool GL_DL_INIT;

static bool gl_dl_global_init(void)
{
	MTY_SpinlockAcquire(&GL_DL_LOCK);

	if (!GL_DL_INIT) {
		bool r = true;

		#define LOAD_SYM(name) \
			name = MTY_GLGetProcAddress(#name); \
			if (!name) {r = false; goto except;}

		LOAD_SYM(glGenFramebuffers);
		LOAD_SYM(glDeleteFramebuffers);
		LOAD_SYM(glBindFramebuffer);
		LOAD_SYM(glBlitFramebuffer);
		LOAD_SYM(glFramebufferTexture2D);
		LOAD_SYM(glEnable);
		LOAD_SYM(glIsEnabled);
		LOAD_SYM(glDisable);
		LOAD_SYM(glViewport);
		LOAD_SYM(glGetIntegerv);
		LOAD_SYM(glGetFloatv);
		LOAD_SYM(glBindTexture);
		LOAD_SYM(glDeleteTextures);
		LOAD_SYM(glGetTexLevelParameteriv);
		LOAD_SYM(glTexParameteri);
		LOAD_SYM(glGenTextures);
		LOAD_SYM(glTexImage2D);
		LOAD_SYM(glTexSubImage2D);
		LOAD_SYM(glDrawElements);
		LOAD_SYM(glGetAttribLocation);
		LOAD_SYM(glShaderSource);
		LOAD_SYM(glBindBuffer);
		LOAD_SYM(glVertexAttribPointer);
		LOAD_SYM(glCreateProgram);
		LOAD_SYM(glUniform1i);
		LOAD_SYM(glActiveTexture);
		LOAD_SYM(glDeleteBuffers);
		LOAD_SYM(glEnableVertexAttribArray);
		LOAD_SYM(glBufferData);
		LOAD_SYM(glDeleteShader);
		LOAD_SYM(glGenBuffers);
		LOAD_SYM(glCompileShader);
		LOAD_SYM(glLinkProgram);
		LOAD_SYM(glGetUniformLocation);
		LOAD_SYM(glCreateShader);
		LOAD_SYM(glAttachShader);
		LOAD_SYM(glUseProgram);
		LOAD_SYM(glGetShaderiv);
		LOAD_SYM(glDetachShader);
		LOAD_SYM(glDeleteProgram);
		LOAD_SYM(glClear);
		LOAD_SYM(glClearColor);
		LOAD_SYM(glGetError);
		LOAD_SYM(glGetShaderInfoLog);
		LOAD_SYM(glFinish);
		LOAD_SYM(glScissor);
		LOAD_SYM(glBlendFunc);
		LOAD_SYM(glBlendEquation);
		LOAD_SYM(glUniformMatrix4fv);
		LOAD_SYM(glBlendEquationSeparate);
		LOAD_SYM(glBlendFuncSeparate);
		LOAD_SYM(glGetProgramiv);

		except:

		GL_DL_INIT = r;
	}

	MTY_SpinlockRelease(&GL_DL_LOCK);

	return GL_DL_INIT;
}

#endif
