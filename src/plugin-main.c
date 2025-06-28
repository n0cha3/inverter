/*
inverter
Copyright (C) 2025 n0cha3

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/
#include <obs-module.h>
#include <plugin-support.h>

#ifdef _WIN32
#include "os\win32.h"
#endif

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

static const char *inverter_name(void *data);
static void *inverter_create(obs_data_t *settings, obs_source_t *source);
static void inverter_destroy(void *data);
static void inverter_render(void *data, gs_effect_t *effect);

typedef struct inverter_param {
	obs_source_t *source;
	gs_texrender_t *texrend;
	gs_texture_t *texture;
} invparam;

struct obs_source_info inverter_funcs = {
	.id = "inverter",
	.type = OBS_SOURCE_TYPE_FILTER,
	.output_flags = OBS_SOURCE_VIDEO,
	.create = inverter_create,
	.destroy = inverter_destroy,
	.video_render = inverter_render,
	.get_name = inverter_name
};

static const char *inverter_name(void *data) {
	UNUSED_PARAMETER(data);
	return "Inverter";
}

static void inverter_render(void *data, gs_effect_t *effect) {
	UNUSED_PARAMETER(effect);

	invparam *inverter_options = data;

	if (inverter_options->source == NULL)
		return;

	obs_source_t *target = obs_filter_get_target(inverter_options->source),
		     *parent = obs_filter_get_parent(inverter_options->source);

	if (parent == NULL || target == NULL) {
		obs_source_skip_video_filter(inverter_options->source);
		return;
	}

	uint32_t x_rend = obs_source_get_base_width(inverter_options->source);
	uint32_t y_rend = obs_source_get_base_height(inverter_options->source);

	gs_texrender_reset(inverter_options->texrend);

	gs_blend_state_push();
	gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);

	if (gs_texrender_begin(inverter_options->texrend, x_rend, y_rend)) {
		uint32_t source_params = obs_source_get_output_flags(target);
		bool custom_draw = (source_params & OBS_SOURCE_CUSTOM_DRAW),
		     async_draw = (source_params & OBS_SOURCE_ASYNC);

		struct vec4 col;

		vec4_zero(&col);
		gs_clear(GS_CLEAR_COLOR, &col, 0.0f, 0);
		gs_ortho(0.0f, (float)x_rend, 0.0f, (float)y_rend, -100.0f, 100.0f);

		if (target == parent && !custom_draw && !async_draw)
			obs_source_default_render(target);
		else
			obs_source_video_render(target);

		gs_texrender_end(inverter_options->texrend);
	}

	gs_blend_state_pop();

	gs_texture_t *source_texture = gs_texrender_get_texture(inverter_options->texrend);

	gs_effect_t *default_effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);
	gs_effect_get_param_by_name(default_effect, "image");

	position cursor_pos;

	gs_texture_t *cursor_texture = get_cursor_bitmap(&cursor_pos);

	if (cursor_texture == NULL) {
		obs_source_skip_video_filter(inverter_options->source);
		return;
	}

	while (gs_effect_loop(default_effect, "Draw")) {
			obs_source_draw(source_texture, 0, 0, x_rend, y_rend, false);
			obs_source_draw(cursor_texture, cursor_pos.x, cursor_pos.y, 0, 0, false);
	}

	obs_enter_graphics();
	gs_texture_destroy(cursor_texture);
	obs_leave_graphics();

}


static void *inverter_create(obs_data_t *settings, obs_source_t *source) {
	UNUSED_PARAMETER(settings);
	invparam *inverter_options = bzalloc(sizeof(invparam));

	inverter_options->source = source;

	obs_enter_graphics();
	inverter_options->texrend = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
	obs_leave_graphics();

	return inverter_options;
}

static void inverter_destroy(void *data) {
	invparam *inverter_options = data;

	obs_enter_graphics();
	gs_texrender_destroy(inverter_options->texrend);
	obs_leave_graphics();
	bfree(data);
}


bool obs_module_load(void) {
	obs_register_source(&inverter_funcs);
	obs_log(LOG_INFO, "Inverter plugin loaded successfully (version %s)", PLUGIN_VERSION);
	return true;
}

void obs_module_unload(void) {
	obs_log(LOG_INFO, "Inverter plugin unloaded");
}
