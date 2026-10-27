/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef LIBSIGROK_FNIRSI_DLA32_TRIGGER_OPTIONS_H
#define LIBSIGROK_FNIRSI_DLA32_TRIGGER_OPTIONS_H

enum dla_trigger_mode { DLA_TRIGGER_DEFAULT, DLA_TRIGGER_PARALLEL, DLA_TRIGGER_SERIAL };
struct dla_trigger_options {
	enum dla_trigger_mode mode;
	uint8_t stages, count[16], continuous[16];
	uint8_t clock, data, falling, bits;
	uint16_t value;
};

/* Parse a complete replacement into a temporary object. Never partially apply
 * invalid, duplicate, unknown, truncated or overflowing configuration fields. */
static inline int dla_trigger_options_parse(GVariant *data, struct dla_trigger_options *out)
{
	struct dla_trigger_options cfg = {0};
	GVariant *mode, *value;
	GVariantIter iter;
	const char *key, *text;
	unsigned int seen = 0, bit, number, expected;
	gboolean enabled;

	if (!data || !out || !g_variant_is_of_type(data, G_VARIANT_TYPE_VARDICT))
		return -1;
	mode = g_variant_lookup_value(data, "mode", G_VARIANT_TYPE_STRING);
	if (!mode) return -1;
	text = g_variant_get_string(mode, NULL);
	if (!strcmp(text, "default")) cfg.mode = DLA_TRIGGER_DEFAULT;
	else if (!strcmp(text, "parallel")) cfg.mode = DLA_TRIGGER_PARALLEL;
	else if (!strcmp(text, "serial")) cfg.mode = DLA_TRIGGER_SERIAL;
	else { g_variant_unref(mode); return -1; }
	g_variant_unref(mode);
	g_variant_iter_init(&iter, data);
	while (g_variant_iter_next(&iter, "{&sv}", &key, &value)) {
		bit = 0;
		if (!strcmp(key, "mode")) bit = 1;
		else if (cfg.mode == DLA_TRIGGER_PARALLEL && !strcmp(key, "stages")) {
			bit = 2;
			if (!g_variant_is_of_type(value, G_VARIANT_TYPE("a(ub)")) ||
					!g_variant_n_children(value) || g_variant_n_children(value) > 16)
				goto invalid;
			cfg.stages = g_variant_n_children(value);
			for (unsigned int i = 0; i < cfg.stages; i++) {
				g_variant_get_child(value, i, "(ub)", &number, &enabled);
				if (!number || number > 255) goto invalid;
				cfg.count[i] = number; cfg.continuous[i] = enabled;
			}
		} else if (cfg.mode == DLA_TRIGGER_SERIAL) {
			if (!strcmp(key, "edge")) {
				bit = 4;
				if (!g_variant_is_of_type(value, G_VARIANT_TYPE_STRING)) goto invalid;
				text = g_variant_get_string(value, NULL);
				if (!strcmp(text, "rising")) cfg.falling = 0;
				else if (!strcmp(text, "falling")) cfg.falling = 1;
				else goto invalid;
			} else {
				if (!g_variant_is_of_type(value, G_VARIANT_TYPE_UINT32)) goto invalid;
				number = g_variant_get_uint32(value);
				if (!strcmp(key, "clock")) { bit = 8; if (number > 31) goto invalid; cfg.clock = number; }
				else if (!strcmp(key, "data")) { bit = 16; if (number > 31) goto invalid; cfg.data = number; }
				else if (!strcmp(key, "bits")) { bit = 32; if (!number || number > 16) goto invalid; cfg.bits = number; }
				else if (!strcmp(key, "value")) { bit = 64; if (number > 65535) goto invalid; cfg.value = number; }
				else goto invalid;
			}
		}
		if (!bit || (seen & bit)) goto invalid;
		seen |= bit;
		g_variant_unref(value);
	}
	expected = cfg.mode == DLA_TRIGGER_DEFAULT ? 1 : cfg.mode == DLA_TRIGGER_PARALLEL ? 3 : 125;
	if (seen != expected || (cfg.mode == DLA_TRIGGER_SERIAL &&
			(cfg.clock == cfg.data || cfg.value >= (1U << cfg.bits))))
		return -1;
	*out = cfg;
	return 0;
invalid:
	g_variant_unref(value);
	return -1;
}

static inline GVariant *dla_trigger_options_get(const struct dla_trigger_options *cfg)
{
	GVariantBuilder dict, stages;
	g_variant_builder_init(&dict, G_VARIANT_TYPE_VARDICT);
	g_variant_builder_add(&dict, "{sv}", "mode", g_variant_new_string(
		cfg->mode == DLA_TRIGGER_PARALLEL ? "parallel" : cfg->mode == DLA_TRIGGER_SERIAL ? "serial" : "default"));
	if (cfg->mode == DLA_TRIGGER_PARALLEL) {
		g_variant_builder_init(&stages, G_VARIANT_TYPE("a(ub)"));
		for (unsigned int i = 0; i < cfg->stages; i++)
			g_variant_builder_add(&stages, "(ub)", cfg->count[i], cfg->continuous[i]);
		g_variant_builder_add(&dict, "{sv}", "stages", g_variant_builder_end(&stages));
	} else if (cfg->mode == DLA_TRIGGER_SERIAL) {
		g_variant_builder_add(&dict, "{sv}", "clock", g_variant_new_uint32(cfg->clock));
		g_variant_builder_add(&dict, "{sv}", "data", g_variant_new_uint32(cfg->data));
		g_variant_builder_add(&dict, "{sv}", "edge", g_variant_new_string(cfg->falling ? "falling" : "rising"));
		g_variant_builder_add(&dict, "{sv}", "bits", g_variant_new_uint32(cfg->bits));
		g_variant_builder_add(&dict, "{sv}", "value", g_variant_new_uint32(cfg->value));
	}
	return g_variant_builder_end(&dict);
}

static inline GVariant *dla_trigger_options_list(void)
{
	GVariantBuilder dict;
	const char *modes[] = {"default", "parallel", "serial", NULL};
	g_variant_builder_init(&dict, G_VARIANT_TYPE_VARDICT);
	g_variant_builder_add(&dict, "{sv}", "modes", g_variant_new_strv(modes, -1));
	g_variant_builder_add(&dict, "{sv}", "max-stages", g_variant_new_uint32(16));
	g_variant_builder_add(&dict, "{sv}", "count-range", g_variant_new("(uu)", 1, 255));
	g_variant_builder_add(&dict, "{sv}", "level-count-unit", g_variant_new_string("ns"));
	g_variant_builder_add(&dict, "{sv}", "serial-max-bits", g_variant_new_uint32(16));
	return g_variant_builder_end(&dict);
}
#endif
