/* SPDX-License-Identifier: MIT */
#include "boids.h"

#define AVOID_DIST 25
#define ALIGN_DIST 120
#define COHESION_DIST 200
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
	gdouble cos_angle;
	int cohesion_n;
	Vector avoid;
	Vector align;
	Vector cohesion;
	Vector v;

	for (i = 0; i < swarm_get_num_boids(swarm); i++) {
		b1 = swarm_get_boid(swarm, i);

		vector_init(&avoid);
		vector_init(&align);
		vector_init(&cohesion);
		cohesion_n = 0;
		min_dist = PROXIMITY_DIST;

		for (j = 0; j < swarm_get_num_boids(swarm); j++) {
			b2 = swarm_get_boid(swarm, j);

			if (j == i)
				continue;

			/* Avoid a bunch os useless sqrt */
			dx = b2->pos.x - b1->pos.x;
			dy = b2->pos.y - b1->pos.y;
			dist = (dx * dx) + (dy * dy);
			if (dist >= COHESION_DIST * COHESION_DIST)
				continue;

			if (swarm->dead_angle) {
				vector_set(&v, dx, dy);

				cos_angle = vector_cos_angle(&b1->velocity, &v);
				if (cos_angle < swarm->cos_dead_angle)
					continue;
			}

			/* Do the sqrt only when really needed */
			dist = sqrt(dist);
			if (dist < min_dist)
				min_dist = dist;

			if (swarm->avoid && dist < AVOID_DIST) {
				v = b1->pos;
				vector_sub(&v, &b2->pos);
				vector_div(&v, dist);
				vector_add(&avoid, &v);
			} else if (swarm->align && dist < ALIGN_DIST) {
				v = b2->velocity;
				vector_div(&v, dist);
				vector_add(&align, &v);
			} else if (swarm->cohesion && dist < COHESION_DIST) {
				cohesion_n++;
				vector_add(&cohesion, &b2->pos);
			}
		}

		b1->red = 1.0 - ((gdouble)min_dist / PROXIMITY_DIST);

		if (cohesion_n) {
			vector_div(&cohesion, cohesion_n);
			vector_sub(&cohesion, &b1->pos);
			vector_set_mag(&cohesion, 0.5);
		}

		vector_add(&b1->velocity, &avoid);
		vector_add(&b1->velocity, &align);
		vector_add(&b1->velocity, &cohesion);

		vector_set_mag(&b1->velocity, 5);

		vector_add(&b1->pos, &b1->velocity);

		b1->pos.x = fmod(b1->pos.x + swarm->width, swarm->width);
		b1->pos.y = fmod(b1->pos.y + swarm->height, swarm->height);
	}
}

void swarm_add_obstacle(Swarm *swarm, gdouble x, gdouble y, guint flags)
{
	Obstacle o = {
		.pos.x = x,
		.pos.y = y,
		.flags = flags
	};

	g_array_append_val(swarm->obstacles, o);
}

gboolean swarm_remove_obstacle(Swarm *swarm, gdouble x, gdouble y)
{
	Vector *o;
	gdouble dist;
	int i;

	if (!swarm->obstacles->len)
		return FALSE;

	i = swarm->obstacles->len;
	while (i--) {
		o = swarm_get_obstacle_pos(swarm, i);
		dist = pow(o->x - x, 2) + pow(o->y - y, 2);
		if (dist <= OBSTACLE_RADIUS * OBSTACLE_RADIUS) {
			g_array_remove_index(swarm->obstacles, i);
			return TRUE;
		}
	}

	return FALSE;
}

guint swarm_get_dead_angle(Swarm *swarm)
{
	return ceil(rad2deg((G_PI - acos(swarm->cos_dead_angle)) * 2));
}

void swarm_set_dead_angle(Swarm *swarm, guint angle)
{
	if (angle > 360)
		angle = 360;

	swarm->cos_dead_angle = cos(G_PI - deg2rad((gdouble)angle / 2));
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
	g_array_free(swarm->obstacles, TRUE);
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
	swarm_set_dead_angle(swarm, DEFAULT_DEAD_ANGLE);

	swarm->obstacles = g_array_new(FALSE, FALSE, sizeof(Obstacle));

	return swarm;
}
