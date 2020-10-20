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
	Vector avoid_obstacle;
	Vector align;
	Vector cohesion;
	Vector v;

	for (i = 0; i < swarm_get_num_boids(swarm); i++) {
		b1 = swarm_get_boid(swarm, i);

		vector_init(&avoid);
		vector_init(&avoid_obstacle);
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
			dist = POW2(dx) + POW2(dy);
			if (dist >= POW2(COHESION_DIST))
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

		if (!vector_is_null(&align))
			vector_set_mag(&align, 3.5);

		if (cohesion_n) {
			vector_div(&cohesion, cohesion_n);
			vector_sub(&cohesion, &b1->pos);
			vector_set_mag(&cohesion, 0.5);
		}

		vector_add(&b1->velocity, &avoid);
		vector_add(&b1->velocity, &align);
		vector_add(&b1->velocity, &cohesion);

		vector_set_mag(&b1->velocity, 5);

		for (j = 0; j < swarm->obstacles->len; j++) {
			Vector *obs = swarm_obstacle_get_pos(swarm, j);

			dx = obs->x - b1->pos.x;
			dy = obs->y - b1->pos.y;
			dist = POW2(dx) + POW2(dy);
			if (dist >= POW2(OBSTACLE_RADIUS * 1.5))
				continue;

			dist = sqrt(dist);

			v = b1->pos;
			vector_sub(&v, obs);
			vector_div(&v, dist / 4);
			vector_add(&avoid_obstacle, &v);
		}

		if (!vector_is_null(&avoid_obstacle)) {
			vector_add(&b1->velocity, &avoid_obstacle);
			vector_set_mag(&b1->velocity, 5);
		}

		vector_add(&b1->pos, &b1->velocity);

		b1->pos.x = fmod(b1->pos.x + swarm->width, swarm->width);
		b1->pos.y = fmod(b1->pos.y + swarm->height, swarm->height);

#ifdef BOIDS_DEBUG
		if (swarm->debug_vectors) {
			b1->avoid = avoid;
			b1->align = align;
			b1->cohesion = cohesion;
			b1->obstacle = avoid_obstacle;
		}
#endif
	}
}

void swarm_add_obstacle(Swarm *swarm, gdouble x, gdouble y, guint flags)
{
	int i;
	gdouble dx, dy;
	Obstacle *o;
	Obstacle new = {
		.pos.x = x,
		.pos.y = y,
		.flags = flags
	};

	i = swarm->obstacles->len;
	while (i--) {
		o = swarm_obstacle_get(swarm, i);
		dx = o->pos.x - x;
		dy = o->pos.y - y;
		if (abs(dx) < (OBSTACLE_RADIUS >> 1) &&
		    abs(dy) < (OBSTACLE_RADIUS >> 1))
			return;
	}

	g_array_append_val(swarm->obstacles, new);
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
		o = swarm_obstacle_get_pos(swarm, i);
		dist = POW2(o->x - x) + POW2(o->y - y);
		if (dist <= POW2(OBSTACLE_RADIUS)) {
			g_array_remove_index(swarm->obstacles, i);
			return TRUE;
		}
	}

	return FALSE;
}

static void swarm_remove_walls(Swarm *swarm)
{
	GArray *obstacles = swarm->obstacles;
	guint flags;
	gint i;

	i = obstacles->len;
	while (i--) {
		flags = swarm_obstacle_get_flags(swarm, i);

		if ((flags & OBSTACLE_FLAG_WALL) == OBSTACLE_FLAG_WALL)
			g_array_remove_index(obstacles, i);
	}
}

static void swarm_add_walls(Swarm *swarm)
{
	gint x, y;

	swarm_remove_walls(swarm);

	y = OBSTACLE_RADIUS;
	for (x = 0; x < swarm->width + OBSTACLE_RADIUS; x += OBSTACLE_RADIUS) {
		swarm_add_obstacle(swarm, x, -y, OBSTACLE_FLAG_WALL);
		swarm_add_obstacle(swarm, x, swarm->height + y, OBSTACLE_FLAG_WALL);
	}

	x = y;
	for (y = 0; y < swarm->height + OBSTACLE_RADIUS; y += OBSTACLE_RADIUS) {
		swarm_add_obstacle(swarm, -x, y, OBSTACLE_FLAG_WALL);
		swarm_add_obstacle(swarm, swarm->width + x, y, OBSTACLE_FLAG_WALL);
	}
}

gboolean swarm_walls_get_enable(Swarm *swarm)
{
	return swarm->walls;
}

void swarm_walls_set_enable(Swarm *swarm, gboolean enable)
{
	swarm->walls = enable;

	if (enable)
		swarm_add_walls(swarm);
	else
		swarm_remove_walls(swarm);
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

void swarm_get_sizes(Swarm *swarm, gint *width, gint *height)
{
	*width = swarm->width;
	*height = swarm->height;
}

void swarm_set_sizes(Swarm *swarm, guint width, guint height)
{
	if (swarm->width == width && swarm->height == height)
		return;

	swarm->width = width;
	swarm->height = height;

	if (swarm->walls) {
		swarm_remove_walls(swarm);
		swarm_add_walls(swarm);
	}
}

void swarm_rule_set_active(Swarm *swarm, SwarmRule rule, gboolean active)
{
	switch (rule) {
	case RULE_AVOID:
		swarm->avoid = active;
		break;
	case RULE_ALIGN:
		swarm->align = active;
		break;
	case RULE_COHESION:
		swarm->cohesion = active;
		break;
	case RULE_DEAD_ANGLE:
		swarm->dead_angle = active;
		break;
	}
}

gboolean swarm_rule_get_active(Swarm *swarm, SwarmRule rule)
{
	gboolean active = FALSE;

	switch (rule) {
	case RULE_AVOID:
		active = swarm->avoid;
		break;
	case RULE_ALIGN:
		active = swarm->align;
		break;
	case RULE_COHESION:
		active = swarm->cohesion;
		break;
	case RULE_DEAD_ANGLE:
		active = swarm->dead_angle;
		break;
	}

	return active;
}

static int swarm_move_thread(Swarm *swarm)
{
	GTimer *timer = g_timer_new();
	gulong elapsed;

	while (swarm->move_th_running) {
		g_timer_start(timer);
		swarm_move(swarm);
		g_timer_elapsed(timer, &elapsed);

		swarm->animate_cb(swarm->animate_cb_userdata, elapsed);
	}

	return 0;
}

void swarm_thread_start(Swarm *swarm, SwarmAnimateFunc cb, gpointer userdata)
{
	if (swarm->move_th_running)
		return;

	swarm->animate_cb = cb;
	swarm->animate_cb_userdata = userdata;
	swarm->move_th_running  = TRUE;
	swarm->move_th = g_thread_new("move", (GThreadFunc)swarm_move_thread, swarm);
}

void swarm_thread_stop(Swarm *swarm)
{
	if (!swarm->move_th_running)
		return;

	swarm->move_th_running = FALSE;
	g_thread_join(swarm->move_th);
}

gboolean swarm_thread_running(Swarm *swarm)
{
	return swarm->move_th_running;
}

void swarm_free(Swarm *swarm)
{
	g_array_free(swarm->boids, TRUE);
	g_array_free(swarm->obstacles, TRUE);
	g_free(swarm);
}

Swarm *swarm_alloc(void)
{
	Swarm *swarm;

	swarm = g_malloc0(sizeof(Swarm));

	swarm->width = DEFAULT_WIDTH;
	swarm->height = DEFAULT_HEIGHT;

	swarm->boids = g_array_new(FALSE, FALSE, sizeof(Boid));
	swarm->obstacles = g_array_new(FALSE, FALSE, sizeof(Obstacle));

	swarm_set_num_boids(swarm, DEFAULT_NUM_BOIDS);
	swarm_set_dead_angle(swarm, DEFAULT_DEAD_ANGLE);

	return swarm;
}
