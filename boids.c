/* SPDX-License-Identifier: MIT */
#include <stdlib.h>

#include "boids.h"

int main(int argc, char **argv)
{
	Swarm *swarm;
	int num_boids = DEFAULT_NUM_BOIDS;

	swarm = swarm_alloc(num_boids);

	gtk_boids_run(swarm);

	swarm_free(swarm);

	return 0;
}
