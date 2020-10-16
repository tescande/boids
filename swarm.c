/* SPDX-License-Identifier: MIT */
#include "boids.h"

#define PROXIMITY_DIST 30

void swarm_move(Swarm *swarm)
{
	int i;
	int j;
	Boid *b1;
	Boid *b2;
	double dist;
	guint min_dist;
	gdouble dx, dy;

	for (i = 0; i < swarm_get_num_boids(swarm); i++) {
		b1 = swarm_get_boid(swarm, i);

		min_dist = PROXIMITY_DIST;

		for (j = 0; j < swarm_get_num_boids(swarm); j++) {
			b2 = swarm_get_boid(swarm, j);

			if (j == i)
				continue;

			/* Avoid a bunch os useless sqrt */
			dx = b2->pos.x - b1->pos.x;
			dy = b2->pos.y - b1->pos.y;
			dist = (dx * dx) + (dy * dy);
			if (dist >= PROXIMITY_DIST * PROXIMITY_DIST)
				continue;

			/* Do the sqrt only when really needed */
			dist = sqrt(dist);
			if (dist < min_dist)
				min_dist = dist;
		}

		b1->red = 1.0 - ((gdouble)min_dist / PROXIMITY_DIST);

		vector_add(&b1->pos, &b1->velocity);

		b1->pos.x = fmod(b1->pos.x + swarm->width, swarm->width);
		b1->pos.y = fmod(b1->pos.y + swarm->height, swarm->height);
	}
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

void swarm_update_sizes(Swarm *swarm, guint width, guint height)
{
	swarm->width = width;
	swarm->height = height;
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
