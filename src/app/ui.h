#pragma once

#include "config.h"

#include "nes/nes.h"

#define UI_LOG_LEN  128

enum ui_event_type {
	UI_EVENT_NONE     = 0,
	UI_EVENT_CONFIG   = 1,
	UI_EVENT_QUIT     = 2,
	UI_EVENT_PAUSE    = 3,
	UI_EVENT_OPEN_ROM = 4,
	UI_EVENT_RESET    = 5,
};

struct ui_args {
	const struct config *cfg;
	uint32_t crc32;
	bool paused;
	bool show_menu;
	NES *nes;
};

struct ui_event {
	enum ui_event_type type;
	struct config cfg;
	const char *rom_name;
};

void ui_component_root(const struct ui_args *args,
	void (*event_callback)(struct ui_event *event, void *opaque), const void *opaque);
void ui_component_message(const char *msg, int32_t timeout);
void ui_component_destroy(void);
void ui_component_log(const char *msg, int32_t timeout);
void ui_component_clear_log(void);
