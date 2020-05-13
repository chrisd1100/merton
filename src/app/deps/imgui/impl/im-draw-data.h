#pragma once

#include <stdint.h>

#define VTX_INCR (1024 * 5)
#define IDX_INCR (1024 * 10)

struct im_vec2 {
	float x;
	float y;
};

struct im_vec4 {
	float x;
	float y;
	float z;
	float w;
};

struct im_vtx {
	struct im_vec2 pos;
	struct im_vec2 uv;
	uint32_t col;
};

struct im_cmd {
	struct im_vec4 clip_rect;
	void *texture_id;
	uint32_t elem_count;
	uint32_t idx_offset;
	uint32_t vtx_offset;
};

struct im_cmd_list {
	struct im_vtx *vtx;
	uint32_t vtx_len;
	uint32_t vtx_max_len;

	uint16_t *idx;
	uint32_t idx_len;
	uint32_t idx_max_len;

	struct im_cmd *cmd;
	uint32_t cmd_len;
	uint32_t cmd_max_len;
};

struct im_draw_data {
	struct im_cmd_list *cmd_list;
	uint32_t cmd_list_len;
	uint32_t cmd_list_max_len;

	uint32_t idx_len;
	uint32_t vtx_len;

	struct im_vec2 display_size;
	struct im_vec2 display_pos;
	struct im_vec2 framebuffer_scale;
};
