#include "im-gl.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <math.h>

#include "gl-dl.h"

#include "matoya.h"

struct im_gl {
	GLuint font;
	GLuint prog;
	GLuint vs;
	GLuint fs;
	GLuint fb;
	GLint loc_tex;
	GLint loc_proj;
	GLint loc_pos;
	GLint loc_uv;
	GLint loc_col;
	GLuint vb;
	GLuint eb;
};

struct im_gl_state {
	GLint array_buffer;
	GLint fb;
	GLenum active_texture;
	GLint program;
	GLint texture;
	GLint viewport[4];
	GLint scissor_box[4];
	GLfloat color_clear_value[4];
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
	glGetIntegerv(GL_ACTIVE_TEXTURE, (GLint *) &s->active_texture);
	glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &s->array_buffer);
	glGetIntegerv(GL_CURRENT_PROGRAM, &s->program);
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &s->texture);
	glGetIntegerv(GL_VIEWPORT, s->viewport);
	glGetIntegerv(GL_SCISSOR_BOX, s->scissor_box);
	glGetIntegerv(GL_BLEND_SRC_RGB, (GLint *) &s->blend_src_rgb);
	glGetIntegerv(GL_BLEND_DST_RGB, (GLint *) &s->blend_dst_rgb);
	glGetIntegerv(GL_BLEND_SRC_ALPHA, (GLint *) &s->blend_src_alpha);
	glGetIntegerv(GL_BLEND_DST_ALPHA, (GLint *) &s->blend_dst_alpha);
	glGetIntegerv(GL_BLEND_EQUATION_RGB, (GLint *) &s->blend_equation_rgb);
	glGetIntegerv(GL_BLEND_EQUATION_ALPHA, (GLint *) &s->blend_equation_alpha);
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, (GLint *) &s->fb);
	glGetFloatv(GL_COLOR_CLEAR_VALUE, s->color_clear_value);
	s->enable_blend = glIsEnabled(GL_BLEND);
	s->enable_cull_face = glIsEnabled(GL_CULL_FACE);
	s->enable_depth_test = glIsEnabled(GL_DEPTH_TEST);
	s->enable_scissor_test = glIsEnabled(GL_SCISSOR_TEST);
}

static void im_gl_pop_state(struct im_gl *ctx, struct im_gl_state *s)
{
	glUseProgram(s->program);
	glBindTexture(GL_TEXTURE_2D, s->texture);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, s->fb);
	glActiveTexture(s->active_texture);
	glBindBuffer(GL_ARRAY_BUFFER, s->array_buffer);
	glBlendEquationSeparate(s->blend_equation_rgb, s->blend_equation_alpha);
	glBlendFuncSeparate(s->blend_src_rgb, s->blend_dst_rgb, s->blend_src_alpha, s->blend_dst_alpha);
	glClearColor(s->color_clear_value[0], s->color_clear_value[1], s->color_clear_value[2], s->color_clear_value[3]);
	if (s->enable_blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
	if (s->enable_cull_face) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
	if (s->enable_depth_test) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
	if (s->enable_scissor_test) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
	glViewport(s->viewport[0], s->viewport[1], s->viewport[2], s->viewport[3]);
	glScissor(s->scissor_box[0], s->scissor_box[1], s->scissor_box[2], s->scissor_box[3]);
}

void im_gl_render(struct im_gl *ctx, const struct im_draw_data *dd, bool clear, GL_Uint texture)
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

	// Bind texture to draw framebuffer
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, ctx->fb);
	glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

	if (clear) {
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
	}

	// Set viewport based on display size
	glViewport(0, 0, fb_width, fb_height);

	// Set up rendering pipeline
	glUseProgram(ctx->prog);
	glActiveTexture(GL_TEXTURE0);
	glUniform1i(ctx->loc_tex, 0);
	glUniformMatrix4fv(ctx->loc_proj, 1, GL_FALSE, &proj[0][0]);
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_SCISSOR_TEST);
	glBindBuffer(GL_ARRAY_BUFFER, ctx->vb);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ctx->eb);
	glEnableVertexAttribArray(ctx->loc_pos);
	glEnableVertexAttribArray(ctx->loc_uv);
	glEnableVertexAttribArray(ctx->loc_col);
	glVertexAttribPointer(ctx->loc_pos, 2, GL_FLOAT,         GL_FALSE, sizeof(struct im_vtx), (GLvoid *) offsetof(struct im_vtx, pos));
	glVertexAttribPointer(ctx->loc_uv,  2, GL_FLOAT,         GL_FALSE, sizeof(struct im_vtx), (GLvoid *) offsetof(struct im_vtx, uv));
	glVertexAttribPointer(ctx->loc_col, 4, GL_UNSIGNED_BYTE, GL_TRUE,  sizeof(struct im_vtx), (GLvoid *) offsetof(struct im_vtx, col));

	// Draw
	for (uint32_t n = 0; n < dd->cmd_list_len; n++) {
		struct im_cmd_list *cmd_list = &dd->cmd_list[n];

		// Copy vertex, index buffer data
		glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr) cmd_list->vtx_len * sizeof(struct im_vtx), cmd_list->vtx, GL_STREAM_DRAW);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr) cmd_list->idx_len * sizeof(uint16_t), cmd_list->idx, GL_STREAM_DRAW);

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
				glScissor(lrint(r.x), lrint(fb_height - r.w), lrint(r.z - r.x), lrint(r.w - r.y));

				// Optionally sample from a texture (fonts, images)
				glBindTexture(GL_TEXTURE_2D, (GLuint) (size_t) pcmd->texture_id);

				// Draw indexed
				glDrawElements(GL_TRIANGLES, pcmd->elem_count, GL_UNSIGNED_SHORT, (void *) (size_t) (pcmd->idx_offset * sizeof(uint16_t)));
			}
		}
	}

	// Restore previous context state
	im_gl_pop_state(ctx, &state);
}

bool im_gl_create(const char *version, const void *font, uint32_t width, uint32_t height, struct im_gl **gl)
{
	if (!gl_dl_global_init())
		return false;

	struct im_gl *ctx = *gl = calloc(1, sizeof(struct im_gl));

	bool r = true;

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
	ctx->vs = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(ctx->vs, 2, vertex_shader, NULL);
	glCompileShader(ctx->vs);

	glGetShaderiv(ctx->vs, GL_COMPILE_STATUS, &status);
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

	ctx->fs = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(ctx->fs, 2, fragment_shader, NULL);
	glCompileShader(ctx->fs);
	glGetShaderiv(ctx->fs, GL_COMPILE_STATUS, &status);
	if (status == GL_FALSE) {
		fprintf(stderr, "Fragment shader failed to compile\n");
		r = false;
		goto except;
	}

	ctx->prog = glCreateProgram();
	glAttachShader(ctx->prog, ctx->vs);
	glAttachShader(ctx->prog, ctx->fs);
	glLinkProgram(ctx->prog);

	glGetProgramiv(ctx->prog, GL_LINK_STATUS, &status);
	if (status == GL_FALSE) {
		fprintf(stderr, "Program failed to link\n");
		r = false;
		goto except;
	}

	// Store uniform locations
	ctx->loc_proj = glGetUniformLocation(ctx->prog, "proj");
	ctx->loc_pos = glGetAttribLocation(ctx->prog, "pos");
	ctx->loc_uv = glGetAttribLocation(ctx->prog, "uv");
	ctx->loc_col = glGetAttribLocation(ctx->prog, "col");
	ctx->loc_tex = glGetUniformLocation(ctx->prog, "tex");

	// Pre create dynamically resizing vertex, index (element) buffers
	glGenBuffers(1, &ctx->vb);
	glGenBuffers(1, &ctx->eb);

	// Font texture
	GLint prev_texture = 0;
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_texture);

	glGenTextures(1, &ctx->font);
	glBindTexture(GL_TEXTURE_2D, ctx->font);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, font);

	glBindTexture(GL_TEXTURE_2D, prev_texture);

	glGenFramebuffers(1, &ctx->fb);

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

	if (ctx->vb)
		glDeleteBuffers(1, &ctx->vb);

	if (ctx->eb)
		glDeleteBuffers(1, &ctx->eb);

	if (ctx->prog) {
		if (ctx->vs)
			glDetachShader(ctx->prog, ctx->vs);

		if (ctx->fs)
			glDetachShader(ctx->prog, ctx->fs);
	}

	if (ctx->vs)
		glDeleteShader(ctx->vs);

	if (ctx->fs)
		glDeleteShader(ctx->fs);

	if (ctx->prog)
		glDeleteProgram(ctx->prog);

	if (ctx->font)
		glDeleteTextures(1, &ctx->font);

	if (ctx->fb)
		glDeleteFramebuffers(1, &ctx->fb);

	free(ctx);
	*gl = NULL;
}

void im_gl_texture_size(struct im_gl *ctx, GL_Uint texture, float *width, float *height)
{
	GLint w = 0;
	GLint h = 0;

	glBindTexture(GL_TEXTURE_2D, texture);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &w);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &h);

	*width = (float) w;
	*height = (float) h;
}
