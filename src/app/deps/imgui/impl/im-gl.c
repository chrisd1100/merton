#include "im-gl.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <math.h>

#include "util/glproc.h"

struct im_gl {
	GLuint font;
	GLuint prog;
	GLuint vs;
	GLuint fs;
	GLint locTex;
	GLint locProjMtx;
	GLint locVtxPos;
	GLint locVtxUV;
	GLint locVtxColor;
	GLuint vb;
	GLuint eb;
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

static void im_gl_push_state(struct im_gl_state *s)
{
	glGetIntegerv(GL_ACTIVE_TEXTURE, (GLint *) &s->active_texture);
	glActiveTexture(GL_TEXTURE0);
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
	s->enable_blend = glIsEnabled(GL_BLEND);
	s->enable_cull_face = glIsEnabled(GL_CULL_FACE);
	s->enable_depth_test = glIsEnabled(GL_DEPTH_TEST);
	s->enable_scissor_test = glIsEnabled(GL_SCISSOR_TEST);
}

static void im_gl_pop_state(struct im_gl_state *s)
{
	glUseProgram(s->program);
	glBindTexture(GL_TEXTURE_2D, s->texture);
	glActiveTexture(s->active_texture);
	glBindBuffer(GL_ARRAY_BUFFER, s->array_buffer);
	glBlendEquationSeparate(s->blend_equation_rgb, s->blend_equation_alpha);
	glBlendFuncSeparate(s->blend_src_rgb, s->blend_dst_rgb, s->blend_src_alpha, s->blend_dst_alpha);
	if (s->enable_blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
	if (s->enable_cull_face) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
	if (s->enable_depth_test) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
	if (s->enable_scissor_test) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
	glViewport(s->viewport[0], s->viewport[1], s->viewport[2], s->viewport[3]);
	glScissor(s->scissor_box[0], s->scissor_box[1], s->scissor_box[2], s->scissor_box[3]);
}

void im_gl_render(struct im_gl *ctx, const struct im_draw_data *dd)
{
	int32_t fb_width = lrint(dd->display_size.x * dd->framebuffer_scale.x);
	int32_t fb_height = lrint(dd->display_size.y * dd->framebuffer_scale.y);

	if (fb_width <= 0 || fb_height <= 0 || dd->cmd_list_len == 0)
		return;

	struct im_gl_state state = {0};
	im_gl_push_state(&state);

	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_SCISSOR_TEST);

	glViewport(0, 0, fb_width, fb_height);

	float L = dd->display_pos.x;
	float R = dd->display_pos.x + dd->display_size.x;
	float T = dd->display_pos.y;
	float B = dd->display_pos.y + dd->display_size.y;
	float ortho[4][4] = {
		{2.0f,  0.0f,  0.0f,  0.0f},
		{0.0f,  2.0f,  0.0f,  0.0f},
		{0.0f,  0.0f, -1.0f,  0.0f},
		{0.0f,  0.0f,  0.0f,  1.0f},
	};

	ortho[0][0] /= R - L;
	ortho[1][1] /= T - B;
	ortho[3][0] = (R + L) / (L - R);
	ortho[3][1] = (T + B) / (B - T);

	glUseProgram(ctx->prog);
	glUniform1i(ctx->locTex, 0);
	glUniformMatrix4fv(ctx->locProjMtx, 1, GL_FALSE, &ortho[0][0]);

	glBindBuffer(GL_ARRAY_BUFFER, ctx->vb);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ctx->eb);
	glEnableVertexAttribArray(ctx->locVtxPos);
	glEnableVertexAttribArray(ctx->locVtxUV);
	glEnableVertexAttribArray(ctx->locVtxColor);
	glVertexAttribPointer(ctx->locVtxPos,   2, GL_FLOAT,         GL_FALSE, sizeof(struct im_vtx), (GLvoid *) offsetof(struct im_vtx, pos));
	glVertexAttribPointer(ctx->locVtxUV,    2, GL_FLOAT,         GL_FALSE, sizeof(struct im_vtx), (GLvoid *) offsetof(struct im_vtx, uv));
	glVertexAttribPointer(ctx->locVtxColor, 4, GL_UNSIGNED_BYTE, GL_TRUE,  sizeof(struct im_vtx), (GLvoid *) offsetof(struct im_vtx, col));

	struct im_vec2 clip_off = dd->display_pos;
	struct im_vec2 clip_scale = dd->framebuffer_scale;

	for (uint32_t n = 0; n < dd->cmd_list_len; n++) {
		struct im_cmd_list *cmd_list = &dd->cmd_list[n];

		glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr) cmd_list->vtx_len * sizeof(struct im_vtx), (const GLvoid *) cmd_list->vtx, GL_STREAM_DRAW);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr) cmd_list->idx_len * sizeof(uint16_t), (const GLvoid *) cmd_list->idx, GL_STREAM_DRAW);

		for (uint32_t cmd_i = 0; cmd_i < cmd_list->cmd_len; cmd_i++) {
			struct im_cmd *pcmd = &cmd_list->cmd[cmd_i];

			struct im_vec4 clip_rect = {0};
			clip_rect.x = (pcmd->clip_rect.x - clip_off.x) * clip_scale.x;
			clip_rect.y = (pcmd->clip_rect.y - clip_off.y) * clip_scale.y;
			clip_rect.z = (pcmd->clip_rect.z - clip_off.x) * clip_scale.x;
			clip_rect.w = (pcmd->clip_rect.w - clip_off.y) * clip_scale.y;

			if (clip_rect.x < fb_width && clip_rect.y < fb_height && clip_rect.z >= 0.0f && clip_rect.w >= 0.0f) {
				glScissor(lrint(clip_rect.x), lrint(fb_height - clip_rect.w), lrint(clip_rect.z - clip_rect.x), lrint(clip_rect.w - clip_rect.y));
				glBindTexture(GL_TEXTURE_2D, (GLuint) (intptr_t) pcmd->texture_id);
				glDrawElements(GL_TRIANGLES, (GLsizei) pcmd->elem_count, GL_UNSIGNED_SHORT, (void *) (intptr_t) (pcmd->idx_offset * sizeof(uint16_t)));
			}
		}
	}

	im_gl_pop_state(&state);
}

bool im_gl_create(const char *version, const void *font, uint32_t width, uint32_t height, struct im_gl **gl)
{
	struct im_gl *ctx = *gl = calloc(1, sizeof(struct im_gl));

	bool r = true;

	const GLchar *vertex_shader[2] = {0};
	vertex_shader[0] = version;
	vertex_shader[1] =
		"\n"
		"uniform mat4 ProjMtx;\n"
		"attribute vec2 Position;\n"
		"attribute vec2 UV;\n"
		"attribute vec4 Color;\n"
		"varying vec2 Frag_UV;\n"
		"varying vec4 Frag_Color;\n"
		"void main()\n"
		"{\n"
		"	Frag_UV = UV;\n"
		"	Frag_Color = Color;\n"
		"	gl_Position = ProjMtx * vec4(Position.xy,0,1);\n"
		"}\n";

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
		"\n"
		"#ifdef GL_ES\n"
		"	precision mediump float;\n"
		"#endif\n"
		"uniform sampler2D Texture;\n"
		"varying vec2 Frag_UV;\n"
		"varying vec4 Frag_Color;\n"
		"void main()\n"
		"{\n"
		"	gl_FragColor = Frag_Color * texture2D(Texture, Frag_UV.st);\n"
		"}\n";

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

	ctx->locTex = glGetUniformLocation(ctx->prog, "Texture");
	ctx->locProjMtx = glGetUniformLocation(ctx->prog, "ProjMtx");
	ctx->locVtxPos = glGetAttribLocation(ctx->prog, "Position");
	ctx->locVtxUV = glGetAttribLocation(ctx->prog, "UV");
	ctx->locVtxColor = glGetAttribLocation(ctx->prog, "Color");

	glGenBuffers(1, &ctx->vb);
	glGenBuffers(1, &ctx->eb);

	GLint prev_texture = 0;
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_texture);

	glGenTextures(1, &ctx->font);
	glBindTexture(GL_TEXTURE_2D, ctx->font);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, font);

	glBindTexture(GL_TEXTURE_2D, prev_texture);

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

	free(ctx);
	*gl = NULL;
}
