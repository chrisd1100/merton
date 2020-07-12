#include "im-gl.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <math.h>

#include "glcorearb.h"
#include "glext.h"

#include "matoya.h"

struct im_gl {
	GLuint font;
	GLuint prog;
	GLuint vs;
	GLuint fs;
	GLint loc_tex;
	GLint loc_proj;
	GLint loc_pos;
	GLint loc_uv;
	GLint loc_col;
	GLuint vb;
	GLuint eb;

	PFNGLENABLEPROC glEnable;
	PFNGLDISABLEPROC glDisable;
	PFNGLISENABLEDPROC glIsEnabled;
	PFNGLVIEWPORTPROC glViewport;
	PFNGLSCISSORPROC glScissor;
	PFNGLGETINTEGERVPROC glGetIntegerv;
	PFNGLBINDTEXTUREPROC glBindTexture;
	PFNGLDELETETEXTURESPROC glDeleteTextures;
	PFNGLBLENDFUNCPROC glBlendFunc;
	PFNGLTEXPARAMETERIPROC glTexParameteri;
	PFNGLGENTEXTURESPROC glGenTextures;
	PFNGLTEXIMAGE2DPROC glTexImage2D;
	PFNGLDRAWELEMENTSPROC glDrawElements;

	PFNGLGETATTRIBLOCATIONPROC glGetAttribLocation;
	PFNGLSHADERSOURCEPROC glShaderSource;
	PFNGLBINDBUFFERPROC glBindBuffer;
	PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer;
	PFNGLCREATEPROGRAMPROC glCreateProgram;
	PFNGLUNIFORM1IPROC glUniform1i;
	PFNGLACTIVETEXTUREPROC glActiveTexture;
	PFNGLDELETEBUFFERSPROC glDeleteBuffers;
	PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray;
	PFNGLBUFFERDATAPROC glBufferData;
	PFNGLDELETESHADERPROC glDeleteShader;
	PFNGLGENBUFFERSPROC glGenBuffers;
	PFNGLCOMPILESHADERPROC glCompileShader;
	PFNGLLINKPROGRAMPROC glLinkProgram;
	PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation;
	PFNGLCREATESHADERPROC glCreateShader;
	PFNGLATTACHSHADERPROC glAttachShader;
	PFNGLUSEPROGRAMPROC glUseProgram;
	PFNGLGETSHADERIVPROC glGetShaderiv;
	PFNGLDETACHSHADERPROC glDetachShader;
	PFNGLDELETEPROGRAMPROC glDeleteProgram;
	PFNGLBLENDEQUATIONPROC glBlendEquation;
	PFNGLUNIFORMMATRIX4FVPROC glUniformMatrix4fv;
	PFNGLBLENDEQUATIONSEPARATEPROC glBlendEquationSeparate;
	PFNGLBLENDFUNCSEPARATEPROC glBlendFuncSeparate;
	PFNGLGETPROGRAMIVPROC glGetProgramiv;
};

struct im_gl_state {
	GLint array_buffer;
	GLenum active_texture;
	GLint program;
	GLint texture;
	GLint viewport[4];
	GLint scissor_box[4];
	GLenum blend_src_rgb;
	GLenum blend_dst_rgb;
	GLenum blend_src_alpha;
	GLenum blend_dst_alpha;
	GLenum blend_equation_rgb;
	GLenum blend_equation_alpha;
	GLboolean enable_blend;
	GLboolean enable_cull_face;
	GLboolean enable_depth_test;
	GLboolean enable_scissor_test;
};

static void im_gl_push_state(struct im_gl *ctx, struct im_gl_state *s)
{
	ctx->glGetIntegerv(GL_ACTIVE_TEXTURE, (GLint *) &s->active_texture);
	ctx->glActiveTexture(GL_TEXTURE0);
	ctx->glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &s->array_buffer);
	ctx->glGetIntegerv(GL_CURRENT_PROGRAM, &s->program);
	ctx->glGetIntegerv(GL_TEXTURE_BINDING_2D, &s->texture);
	ctx->glGetIntegerv(GL_VIEWPORT, s->viewport);
	ctx->glGetIntegerv(GL_SCISSOR_BOX, s->scissor_box);
	ctx->glGetIntegerv(GL_BLEND_SRC_RGB, (GLint *) &s->blend_src_rgb);
	ctx->glGetIntegerv(GL_BLEND_DST_RGB, (GLint *) &s->blend_dst_rgb);
	ctx->glGetIntegerv(GL_BLEND_SRC_ALPHA, (GLint *) &s->blend_src_alpha);
	ctx->glGetIntegerv(GL_BLEND_DST_ALPHA, (GLint *) &s->blend_dst_alpha);
	ctx->glGetIntegerv(GL_BLEND_EQUATION_RGB, (GLint *) &s->blend_equation_rgb);
	ctx->glGetIntegerv(GL_BLEND_EQUATION_ALPHA, (GLint *) &s->blend_equation_alpha);
	s->enable_blend = ctx->glIsEnabled(GL_BLEND);
	s->enable_cull_face = ctx->glIsEnabled(GL_CULL_FACE);
	s->enable_depth_test = ctx->glIsEnabled(GL_DEPTH_TEST);
	s->enable_scissor_test = ctx->glIsEnabled(GL_SCISSOR_TEST);
}

static void im_gl_pop_state(struct im_gl *ctx, struct im_gl_state *s)
{
	ctx->glUseProgram(s->program);
	ctx->glBindTexture(GL_TEXTURE_2D, s->texture);
	ctx->glActiveTexture(s->active_texture);
	ctx->glBindBuffer(GL_ARRAY_BUFFER, s->array_buffer);
	ctx->glBlendEquationSeparate(s->blend_equation_rgb, s->blend_equation_alpha);
	ctx->glBlendFuncSeparate(s->blend_src_rgb, s->blend_dst_rgb, s->blend_src_alpha, s->blend_dst_alpha);
	if (s->enable_blend) ctx->glEnable(GL_BLEND); else ctx->glDisable(GL_BLEND);
	if (s->enable_cull_face) ctx->glEnable(GL_CULL_FACE); else ctx->glDisable(GL_CULL_FACE);
	if (s->enable_depth_test) ctx->glEnable(GL_DEPTH_TEST); else ctx->glDisable(GL_DEPTH_TEST);
	if (s->enable_scissor_test) ctx->glEnable(GL_SCISSOR_TEST); else ctx->glDisable(GL_SCISSOR_TEST);
	ctx->glViewport(s->viewport[0], s->viewport[1], s->viewport[2], s->viewport[3]);
	ctx->glScissor(s->scissor_box[0], s->scissor_box[1], s->scissor_box[2], s->scissor_box[3]);
}

void im_gl_render(struct im_gl *ctx, const struct im_draw_data *dd)
{
	int32_t fb_width = lrint(dd->display_size.x * dd->framebuffer_scale.x);
	int32_t fb_height = lrint(dd->display_size.y * dd->framebuffer_scale.y);

	// Prevent rendering under invalid scenarios
	if (fb_width <= 0 || fb_height <= 0 || dd->cmd_list_len == 0)
		return;

	// Update the vertex shader's proj data based on the current display size
	float L = dd->display_pos.x;
	float R = dd->display_pos.x + dd->display_size.x;
	float T = dd->display_pos.y;
	float B = dd->display_pos.y + dd->display_size.y;
	float proj[4][4] = {
		{2.0f,  0.0f,  0.0f,  0.0f},
		{0.0f,  2.0f,  0.0f,  0.0f},
		{0.0f,  0.0f, -1.0f,  0.0f},
		{0.0f,  0.0f,  0.0f,  1.0f},
	};

	proj[0][0] /= R - L;
	proj[1][1] /= T - B;
	proj[3][0] = (R + L) / (L - R);
	proj[3][1] = (T + B) / (B - T);

	// Store current context state
	struct im_gl_state state = {0};
	im_gl_push_state(ctx, &state);

	// Set viewport based on display size
	ctx->glViewport(0, 0, fb_width, fb_height);

	// Set up rendering pipeline
	ctx->glUseProgram(ctx->prog);
	ctx->glUniform1i(ctx->loc_tex, 0);
	ctx->glUniformMatrix4fv(ctx->loc_proj, 1, GL_FALSE, &proj[0][0]);
	ctx->glEnable(GL_BLEND);
	ctx->glBlendEquation(GL_FUNC_ADD);
	ctx->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	ctx->glDisable(GL_CULL_FACE);
	ctx->glDisable(GL_DEPTH_TEST);
	ctx->glEnable(GL_SCISSOR_TEST);
	ctx->glBindBuffer(GL_ARRAY_BUFFER, ctx->vb);
	ctx->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ctx->eb);
	ctx->glEnableVertexAttribArray(ctx->loc_pos);
	ctx->glEnableVertexAttribArray(ctx->loc_uv);
	ctx->glEnableVertexAttribArray(ctx->loc_col);
	ctx->glVertexAttribPointer(ctx->loc_pos, 2, GL_FLOAT,         GL_FALSE, sizeof(struct im_vtx), (GLvoid *) offsetof(struct im_vtx, pos));
	ctx->glVertexAttribPointer(ctx->loc_uv,  2, GL_FLOAT,         GL_FALSE, sizeof(struct im_vtx), (GLvoid *) offsetof(struct im_vtx, uv));
	ctx->glVertexAttribPointer(ctx->loc_col, 4, GL_UNSIGNED_BYTE, GL_TRUE,  sizeof(struct im_vtx), (GLvoid *) offsetof(struct im_vtx, col));

	// Draw
	for (uint32_t n = 0; n < dd->cmd_list_len; n++) {
		struct im_cmd_list *cmd_list = &dd->cmd_list[n];

		// Copy vertex, index buffer data
		ctx->glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr) cmd_list->vtx_len * sizeof(struct im_vtx), cmd_list->vtx, GL_STREAM_DRAW);
		ctx->glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr) cmd_list->idx_len * sizeof(uint16_t), cmd_list->idx, GL_STREAM_DRAW);

		for (uint32_t cmd_i = 0; cmd_i < cmd_list->cmd_len; cmd_i++) {
			struct im_cmd *pcmd = &cmd_list->cmd[cmd_i];

			// Use the clip_rect to apply scissor
			struct im_vec4 r = {0};
			r.x = (pcmd->clip_rect.x - dd->display_pos.x) * dd->framebuffer_scale.x;
			r.y = (pcmd->clip_rect.y - dd->display_pos.y) * dd->framebuffer_scale.y;
			r.z = (pcmd->clip_rect.z - dd->display_pos.x) * dd->framebuffer_scale.x;
			r.w = (pcmd->clip_rect.w - dd->display_pos.y) * dd->framebuffer_scale.y;

			// Make sure the rect is actually in the viewport
			if (r.x < fb_width && r.y < fb_height && r.z >= 0.0f && r.w >= 0.0f) {
				ctx->glScissor(lrint(r.x), lrint(fb_height - r.w), lrint(r.z - r.x), lrint(r.w - r.y));

				// Optionally sample from a texture (fonts, images)
				ctx->glBindTexture(GL_TEXTURE_2D, (GLuint) (size_t) pcmd->texture_id);

				// Draw indexed
				ctx->glDrawElements(GL_TRIANGLES, pcmd->elem_count, GL_UNSIGNED_SHORT, (void *) (size_t) (pcmd->idx_offset * sizeof(uint16_t)));
			}
		}
	}

	// Restore previous context state
	im_gl_pop_state(ctx, &state);
}

#define GL_PROC(cast, func) \
	ctx->func = (cast) MTY_GLGetProcAddress(#func); \
	if (!ctx->func) {r = false; goto except;}

bool im_gl_create(const char *version, const void *font, uint32_t width, uint32_t height, struct im_gl **gl)
{
	struct im_gl *ctx = *gl = calloc(1, sizeof(struct im_gl));

	bool r = true;

	// Load functions
	GL_PROC(PFNGLENABLEPROC, glEnable);
	GL_PROC(PFNGLDISABLEPROC, glDisable);
	GL_PROC(PFNGLISENABLEDPROC, glIsEnabled);
	GL_PROC(PFNGLVIEWPORTPROC, glViewport);
	GL_PROC(PFNGLSCISSORPROC, glScissor);
	GL_PROC(PFNGLGETINTEGERVPROC, glGetIntegerv);
	GL_PROC(PFNGLBINDTEXTUREPROC, glBindTexture);
	GL_PROC(PFNGLDELETETEXTURESPROC, glDeleteTextures);
	GL_PROC(PFNGLBLENDFUNCPROC, glBlendFunc);
	GL_PROC(PFNGLTEXPARAMETERIPROC, glTexParameteri);
	GL_PROC(PFNGLGENTEXTURESPROC, glGenTextures);
	GL_PROC(PFNGLTEXIMAGE2DPROC, glTexImage2D);
	GL_PROC(PFNGLDRAWELEMENTSPROC, glDrawElements);

	GL_PROC(PFNGLGETATTRIBLOCATIONPROC, glGetAttribLocation);
	GL_PROC(PFNGLSHADERSOURCEPROC, glShaderSource);
	GL_PROC(PFNGLBINDBUFFERPROC, glBindBuffer);
	GL_PROC(PFNGLVERTEXATTRIBPOINTERPROC, glVertexAttribPointer);
	GL_PROC(PFNGLCREATEPROGRAMPROC, glCreateProgram);
	GL_PROC(PFNGLUNIFORM1IPROC, glUniform1i);
	GL_PROC(PFNGLACTIVETEXTUREPROC, glActiveTexture);
	GL_PROC(PFNGLDELETEBUFFERSPROC, glDeleteBuffers);
	GL_PROC(PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray);
	GL_PROC(PFNGLBUFFERDATAPROC, glBufferData);
	GL_PROC(PFNGLDELETESHADERPROC, glDeleteShader);
	GL_PROC(PFNGLGENBUFFERSPROC, glGenBuffers);
	GL_PROC(PFNGLCOMPILESHADERPROC, glCompileShader);
	GL_PROC(PFNGLLINKPROGRAMPROC, glLinkProgram);
	GL_PROC(PFNGLGETUNIFORMLOCATIONPROC, glGetUniformLocation);
	GL_PROC(PFNGLCREATESHADERPROC, glCreateShader);
	GL_PROC(PFNGLATTACHSHADERPROC, glAttachShader);
	GL_PROC(PFNGLUSEPROGRAMPROC, glUseProgram);
	GL_PROC(PFNGLGETSHADERIVPROC, glGetShaderiv);
	GL_PROC(PFNGLDETACHSHADERPROC, glDetachShader);
	GL_PROC(PFNGLDELETEPROGRAMPROC, glDeleteProgram);
	GL_PROC(PFNGLBLENDEQUATIONPROC, glBlendEquation);
	GL_PROC(PFNGLUNIFORMMATRIX4FVPROC, glUniformMatrix4fv);
	GL_PROC(PFNGLBLENDEQUATIONSEPARATEPROC, glBlendEquationSeparate);
	GL_PROC(PFNGLBLENDFUNCSEPARATEPROC, glBlendFuncSeparate);
	GL_PROC(PFNGLGETPROGRAMIVPROC, glGetProgramiv);

	// Create vertex, fragment shaders
	const GLchar *vertex_shader[2] = {0};
	vertex_shader[0] = version;
	vertex_shader[1] =
		"                                             \n"
		"uniform mat4 proj;                           \n"
		"                                             \n"
		"attribute vec2 pos;                          \n"
		"attribute vec2 uv;                           \n"
		"attribute vec4 col;                          \n"
		"                                             \n"
		"varying vec2 frag_uv;                        \n"
		"varying vec4 frag_col;                       \n"
		"                                             \n"
		"void main()                                  \n"
		"{                                            \n"
		"    frag_uv = uv;                            \n"
		"    frag_col = col;                          \n"
		"    gl_Position = proj * vec4(pos.xy, 0, 1); \n"
		"}                                            \n";

	GLint status = 0;
	ctx->vs = ctx->glCreateShader(GL_VERTEX_SHADER);
	ctx->glShaderSource(ctx->vs, 2, vertex_shader, NULL);
	ctx->glCompileShader(ctx->vs);

	ctx->glGetShaderiv(ctx->vs, GL_COMPILE_STATUS, &status);
	if (status == GL_FALSE) {
		fprintf(stderr, "Vertex shader failed to compile\n");
		r = false;
		goto except;
	}

	const GLchar *fragment_shader[2] = {0};
	fragment_shader[0] = version;
	fragment_shader[1] =
		"                                                          \n"
		"#ifdef GL_ES                                              \n"
		"	precision mediump float;                               \n"
		"#endif                                                    \n"
		"                                                          \n"
		"uniform sampler2D tex;                                    \n"
		"                                                          \n"
		"varying vec2 frag_uv;                                     \n"
		"varying vec4 frag_col;                                    \n"
		"                                                          \n"
		"void main()                                               \n"
		"{                                                         \n"
		"    gl_FragColor = frag_col * texture2D(tex, frag_uv.st); \n"
		"}                                                         \n";

	ctx->fs = ctx->glCreateShader(GL_FRAGMENT_SHADER);
	ctx->glShaderSource(ctx->fs, 2, fragment_shader, NULL);
	ctx->glCompileShader(ctx->fs);
	ctx->glGetShaderiv(ctx->fs, GL_COMPILE_STATUS, &status);
	if (status == GL_FALSE) {
		fprintf(stderr, "Fragment shader failed to compile\n");
		r = false;
		goto except;
	}

	ctx->prog = ctx->glCreateProgram();
	ctx->glAttachShader(ctx->prog, ctx->vs);
	ctx->glAttachShader(ctx->prog, ctx->fs);
	ctx->glLinkProgram(ctx->prog);

	ctx->glGetProgramiv(ctx->prog, GL_LINK_STATUS, &status);
	if (status == GL_FALSE) {
		fprintf(stderr, "Program failed to link\n");
		r = false;
		goto except;
	}

	// Store uniform locations
	ctx->loc_proj = ctx->glGetUniformLocation(ctx->prog, "proj");
	ctx->loc_pos = ctx->glGetAttribLocation(ctx->prog, "pos");
	ctx->loc_uv = ctx->glGetAttribLocation(ctx->prog, "uv");
	ctx->loc_col = ctx->glGetAttribLocation(ctx->prog, "col");
	ctx->loc_tex = ctx->glGetUniformLocation(ctx->prog, "tex");

	// Pre create dynamically resizing vertex, index (element) buffers
	ctx->glGenBuffers(1, &ctx->vb);
	ctx->glGenBuffers(1, &ctx->eb);

	// Font texture
	GLint prev_texture = 0;
	ctx->glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_texture);

	ctx->glGenTextures(1, &ctx->font);
	ctx->glBindTexture(GL_TEXTURE_2D, ctx->font);
	ctx->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	ctx->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	ctx->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, font);

	ctx->glBindTexture(GL_TEXTURE_2D, prev_texture);

	except:

	if (!r)
		im_gl_destroy(gl);

	return r;
}

GL_Uint im_gl_font_texture(struct im_gl *ctx)
{
	return ctx->font;
}

void im_gl_destroy(struct im_gl **gl)
{
	if (!gl || !*gl)
		return;

	struct im_gl *ctx = *gl;

	if (ctx->glDeleteBuffers) {
		if (ctx->vb)
			ctx->glDeleteBuffers(1, &ctx->vb);

		if (ctx->eb)
			ctx->glDeleteBuffers(1, &ctx->eb);
	}

	if (ctx->glDetachShader && ctx->prog) {
		if (ctx->vs)
			ctx->glDetachShader(ctx->prog, ctx->vs);

		if (ctx->fs)
			ctx->glDetachShader(ctx->prog, ctx->fs);
	}

	if (ctx->glDeleteShader) {
		if (ctx->vs)
			ctx->glDeleteShader(ctx->vs);

		if (ctx->fs)
			ctx->glDeleteShader(ctx->fs);
	}

	if (ctx->glDeleteProgram && ctx->prog)
		ctx->glDeleteProgram(ctx->prog);

	if (ctx->glDeleteTextures && ctx->font)
		ctx->glDeleteTextures(1, &ctx->font);

	free(ctx);
	*gl = NULL;
}
