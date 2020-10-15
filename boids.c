/* SPDX-License-Identifier: MIT */
#include <stdlib.h>

#include "boids.h"

int main(int argc, char **argv)
{
	Swarm *swarm;

	swarm = swarm_alloc();

	gtk_boids_run(swarm);

	swarm_free(swarm);

	return 0;
}
