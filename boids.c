/* SPDX-License-Identifier: MIT */
#include <stdlib.h>

#include "boids.h"

int main(int argc, char **argv)
{
	Swarm *swarm;
	int num_boids = DEFAULT_NUM_BOIDS;
	int seed = 0;
	GError *error = NULL;
	GOptionContext *context;
	GOptionEntry entries[] = {
		{ "num-boids", 'n', 0, G_OPTION_ARG_INT, &num_boids,
		  "Number of boids", "VAL" },
		{ "rand-seed", 's', 0, G_OPTION_ARG_INT, &seed,
		  "Random seed value", "VAL" },
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

	swarm = swarm_alloc(num_boids);

	gtk_boids_run(swarm);

	swarm_free(swarm);

	return 0;
}
