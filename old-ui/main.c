static void cddnes_load_settings(struct cdd *cdd)
{
	cdd->sample_rate = settings_get_int32(cdd->settings, "sample_rate", 44100);
	cdd->stereo = settings_get_bool(cdd->settings, "stereo", true);
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
	settings_set_int32(cdd->settings, "sampler", cdd->sampler);
	settings_set_int32(cdd->settings, "aspect", cdd->aspect);
	settings_set_int32(cdd->settings, "overscan_top", cdd->overscan.top);
	settings_set_int32(cdd->settings, "overscan_right", cdd->overscan.right);
	settings_set_int32(cdd->settings, "overscan_bottom", cdd->overscan.bottom);
	settings_set_int32(cdd->settings, "overscan_left", cdd->overscan.left);
}
