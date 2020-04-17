static void cddnes_open(char *path, char *name, void *opaque)
{
	struct cdd *cdd = (struct cdd *) opaque;

	if (cdd->crc32[0] != '\0')
		fs_save_sram(cdd->nes, cdd->crc32);

	cddnes_clean_rom_name(cdd->host_cfg.desc, name, HOST_DESC_LEN);

	if (cdd->parsec)
		ParsecHostSetConfig(cdd->parsec, &cdd->host_cfg, NULL);

	char full_path[MAX_FILE_NAME];
	fs_path(full_path, path, name);
	fs_load_rom(cdd->nes, full_path, cdd->crc32);
}

static void cddnes_exit(void *opaque)
{
	struct cdd *cdd = (struct cdd *) opaque;

	cdd->done = true;
}

static void cddnes_reset(void *opaque)
{
	struct cdd *cdd = (struct cdd *) opaque;

	nes_reset(cdd->nes, false);
}

static void cddnes_stereo(void *opaque)
{
	struct cdd *cdd = (struct cdd *) opaque;

	cdd->stereo = !cdd->stereo;
	nes_set_stereo(cdd->nes, cdd->stereo);
}

static void cddnes_sample_rate(uint32_t sample_rate, void *opaque)
{
	struct cdd *cdd = (struct cdd *) opaque;

	cdd->sample_rate = sample_rate;
	nes_set_sample_rate(cdd->nes, sample_rate);

	audio_destroy(&cdd->audio);

	cdd->atimer = audio_timer_init(cdd->args.headless, cdd->sample_rate);
	audio_init(&cdd->audio, sample_rate);
}

static void cddnes_sampler(enum sampler sampler, void *opaque)
{
	struct cdd *cdd = (struct cdd *) opaque;

	cdd->sampler = sampler;
	render_set_sampler(cdd->render, sampler);
}

static void cddnes_mode(enum render_mode mode, void *opaque)
{
	struct cdd *cdd = (struct cdd *) opaque;

	cdd->mode = mode;
	cdd->reset = true;
}

static void cddnes_vsync(bool vsync, void *opaque)
{
	struct cdd *cdd = (struct cdd *) opaque;

	cdd->vsync = vsync;
	cdd->reset = true;
}

static void cddnes_aspect(uint32_t aspect, void *opaque)
{
	struct cdd *cdd = (struct cdd *) opaque;

	cdd->aspect = aspect;
}

static void cddnes_overscan(int32_t index, int32_t crop, void *opaque)
{
	struct cdd *cdd = (struct cdd *) opaque;

	int32_t *overscan = (int32_t *) &cdd->overscan;
	overscan[index] = crop;

	memset(cdd->cropped, 0, 4 * NES_W * NES_H);
}

static void cddnes_load_settings(struct cdd *cdd)
{
	cdd->sample_rate = settings_get_int32(cdd->settings, "sample_rate", 44100);
	cdd->stereo = settings_get_bool(cdd->settings, "stereo", true);
	cdd->vsync = settings_get_bool(cdd->settings, "vsync", true);
	cdd->mode = settings_get_int32(cdd->settings, "mode", RENDER_GL);
	cdd->sampler = settings_get_int32(cdd->settings, "sampler", SAMPLE_NEAREST);
	cdd->aspect = settings_get_int32(cdd->settings, "aspect", ASPECT_PACKED(16, 15));
	cdd->overscan.top = settings_get_int32(cdd->settings, "overscan_top", 8);
	cdd->overscan.right = settings_get_int32(cdd->settings, "overscan_right", 0);
	cdd->overscan.bottom = settings_get_int32(cdd->settings, "overscan_bottom", 8);
	cdd->overscan.left = settings_get_int32(cdd->settings, "overscan_left", 0);
}

static void cddnes_save_settings(struct cdd *cdd)
{
	settings_set_int32(cdd->settings, "sample_rate", cdd->sample_rate);
	settings_set_bool(cdd->settings, "stereo", cdd->stereo);
	settings_set_bool(cdd->settings, "vsync", cdd->vsync);
	settings_set_int32(cdd->settings, "mode", cdd->mode);
	settings_set_int32(cdd->settings, "sampler", cdd->sampler);
	settings_set_int32(cdd->settings, "aspect", cdd->aspect);
	settings_set_int32(cdd->settings, "overscan_top", cdd->overscan.top);
	settings_set_int32(cdd->settings, "overscan_right", cdd->overscan.right);
	settings_set_int32(cdd->settings, "overscan_bottom", cdd->overscan.bottom);
	settings_set_int32(cdd->settings, "overscan_left", cdd->overscan.left);
}
