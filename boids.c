/* SPDX-License-Identifier: MIT */
#include "boids.h"

static int get_bg_color(const gchar *color)
{
	int res;

	if (!color)
		return -1;

	switch (*color) {
	case 'r':
	case 'R':
		res = BG_COLOR_REDDISH;
		break;
	case 'g':
	case 'G':
		res = BG_COLOR_GREENISH;
		break;
	case 'b':
	case 'B':
		res = BG_COLOR_BLUISH;
		break;
	case 'w':
	case 'W':
		res = BG_COLOR_WHITE;
		break;
	default:
		res = -1;
		break;
	}

	return res;
}

static void get_boid_rules(gchar *rules, gboolean *avoid, gboolean *align,
			   gboolean *cohesion)
{
	gchar op = 0;
	int i;

	if (!rules)
		return;

	i = 0;
	for (i = 0; rules[i]; i++) {
		if (!op) {
			op = rules[i];
			continue;
		}

		switch (rules[i]) {
		case '+':
		case '-':
			op = rules[i];
			break;
		case 'a':
			*avoid = (op == '+') ? TRUE : FALSE;
			break;
		case 'l':
			*align = (op == '+') ? TRUE : FALSE;
			break;
		case 'c':
			*cohesion = (op == '+') ? TRUE : FALSE;
			break;
		default:
			break;
		}
	}
}

int main(int argc, char **argv)
{
	Swarm *swarm;
	int num_boids = DEFAULT_NUM_BOIDS;
	int seed = 0;
	int bg_color;
	gboolean start = FALSE;
	gboolean walls = FALSE;
	gboolean debug = FALSE;
	gboolean predator = FALSE;
	gboolean rule_avoid = TRUE;
	gboolean rule_align = TRUE;
	gboolean rule_cohesion = TRUE;
	gchar *rules = NULL;
	gchar *bg_color_name = NULL;
	GError *error = NULL;
	GOptionContext *context;
	GOptionEntry entries[] = {
		{ "num-boids", 'n', 0, G_OPTION_ARG_INT, &num_boids,
		  "Number of boids", "VAL" },
		{ "rules", 'l', 0, G_OPTION_ARG_STRING, &rules,
		  "Enable or disable rules. 'a' for avoid, 'l' for align, 'c' for cohesion (i.e. '+a-lc')", "(+|-)(a|l|c)" },
		{ "start", 's', 0, G_OPTION_ARG_NONE, &start,
		  "Start the simulation", NULL },
		{ "predator", 'p', 0, G_OPTION_ARG_NONE, &predator,
		  "Add a predator in the swarm", NULL },
		{ "walls", 'w', 0, G_OPTION_ARG_NONE, &walls,
		  "Add walls to the field", NULL },
		{ "rand-seed", 'r', 0, G_OPTION_ARG_INT, &seed,
		  "Random seed value", "VAL" },
		{ "bg-color", 'b', 0, G_OPTION_ARG_STRING, &bg_color_name,
		  "Background color", "red|green|blue" },
		{ "debug-controls", 'd', 0, G_OPTION_ARG_NONE, &debug,
		  "Enable debug controls", NULL },
		{ NULL }
	};

	context = g_option_context_new("- Boids simulation");
	g_option_context_add_main_entries(context, entries, NULL);
	if (!g_option_context_parse(context, &argc, &argv, &error)) {
		g_fprintf(stderr, "option parsing failed: %s\n", error->message);
		return -1;
	}

	if (seed)
		g_random_set_seed(seed);

	get_boid_rules(rules, &rule_avoid, &rule_align, &rule_cohesion);
	g_free(rules);

	swarm = swarm_alloc();
	swarm_set_debug_controls(swarm, debug);
	swarm_set_num_boids(swarm, num_boids);
	swarm_walls_set_enable(swarm, walls);
	swarm_set_predator_enable(swarm, predator);
	swarm_rule_set_active(swarm, RULE_AVOID, rule_avoid);
	swarm_rule_set_active(swarm, RULE_ALIGN, rule_align);
	swarm_rule_set_active(swarm, RULE_COHESION, rule_cohesion);

	bg_color = get_bg_color(bg_color_name);
	g_free(bg_color_name);

	gui_run(swarm, bg_color, start);

	swarm_free(swarm);

	return 0;
}
