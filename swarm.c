/* SPDX-License-Identifier: MIT */
#include "boids.h"

void swarm_move(Swarm *swarm)
{
}

void swarm_set_num_boids(Swarm *swarm, guint num)
{
	GArray *boids = swarm->boids;
	Boid b;

	if (!num || num > MAX_BOIDS)
		num = DEFAULT_NUM_BOIDS;

	if (num > boids->len) {
		while (boids->len < num) {
			b.pos.x = g_random_int_range(0, swarm->width);
			b.pos.y = g_random_int_range(0, swarm->height);

			b.velocity.x = g_random_int_range(-5, 6);
			do {
				b.velocity.y = g_random_int_range(-5, 6);
			} while (vector_is_null(&b.velocity));

			vector_set_mag(&b.velocity, 5);

			g_array_append_val(swarm->boids, b);
		}
	} else if (num < boids->len) {
		g_array_remove_range(boids, num, boids->len - num);
	}
}

void swarm_free(Swarm *swarm)
{
	g_array_free(swarm->boids, TRUE);
	g_free(swarm);
}

Swarm *swarm_alloc(guint num_boids)
{
	Swarm *swarm;

	swarm = g_malloc0(sizeof(Swarm));
	swarm->width = DEFAULT_WIDTH;
	swarm->height = DEFAULT_HEIGHT;

	swarm->boids = g_array_new(FALSE, FALSE, sizeof(Boid));

	swarm_set_num_boids(swarm, num_boids);

	return swarm;
}
