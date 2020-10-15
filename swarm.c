/* SPDX-License-Identifier: MIT */
#include "boids.h"

void swarm_free(Swarm *swarm)
{
	g_free(swarm);
}

Swarm *swarm_alloc(void)
{
	Swarm *swarm;

	swarm = g_malloc0(sizeof(Swarm));
	swarm->width = DEFAULT_WIDTH;
	swarm->height = DEFAULT_HEIGHT;

	return swarm;
}
