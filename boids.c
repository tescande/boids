/* SPDX-License-Identifier: MIT */
#include "boids.h"

int main(int argc, char **argv)
{
	Swarm *swarm;
	int num_boids = DEFAULT_NUM_BOIDS;
	int seed = 0;
	gboolean walls = FALSE;
	gboolean debug = FALSE;
	GError *error = NULL;
	GOptionContext *context;
	GOptionEntry entries[] = {
		{ "num-boids", 'n', 0, G_OPTION_ARG_INT, &num_boids,
		  "Number of boids", "VAL" },
		{ "walls", 'w', 0, G_OPTION_ARG_NONE, &walls,
		  "Add walls to the field", NULL },
		{ "rand-seed", 's', 0, G_OPTION_ARG_INT, &seed,
		  "Random seed value", "VAL" },
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

	swarm = swarm_alloc();
	swarm_set_debug_controls(swarm, debug);
	swarm_set_num_boids(swarm, num_boids);
	swarm_walls_set_enable(swarm, walls);
	swarm_rule_set_active(swarm, RULE_AVOID, TRUE);
	swarm_rule_set_active(swarm, RULE_ALIGN, TRUE);
	swarm_rule_set_active(swarm, RULE_COHESION, TRUE);

	gtk_boids_run(swarm);

	swarm_free(swarm);

	return 0;
}
