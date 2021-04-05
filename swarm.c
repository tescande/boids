/* SPDX-License-Identifier: MIT */
#include "boids.h"

static gboolean swarm_avoid_obstacles(Swarm *swarm, Boid *boid, Vector *direction)
{
	int i;
	gdouble dx, dy;
	gdouble dist;
	Obstacle *obs;
	Vector v;

	vector_init(direction);

	for (i = 0; i < swarm->obstacles->len; i++) {
		obs = swarm_obstacle_get(swarm, i);

		dx = obs->pos.x - boid->pos.x;
		dy = obs->pos.y - boid->pos.y;
		dist = POW2(dx) + POW2(dy);
		if (dist >= obs->avoid_radius)
			continue;

		dist = sqrt(dist);

		v = boid->pos;
		vector_sub(&v, &obs->pos);
		vector_div(&v, dist / 4);
		vector_add(direction, &v);
	}

	if (vector_is_null(direction))
		return FALSE;

	vector_set_mag(direction, 5);

	return TRUE;
}

static void swarm_move_predator(Swarm *swarm)
{
	Obstacle *predator;
	Vector cohesion;
	Boid *b;
	int i;
	int cohesion_n;
	gdouble dx, dy;
	gdouble dist;

	if (!swarm->predator)
		return;

	predator = swarm_get_obstacle_by_type(swarm, OBSTACLE_TYPE_PREDATOR);

	vector_init(&cohesion);
	cohesion_n = 0;

	for (i = 0; i < swarm_get_num_boids(swarm); i++) {
		b = swarm_get_boid(swarm, i);

		dx = predator->pos.x - b->pos.x;
		dy = predator->pos.y - b->pos.y;
		dist = POW2(dx) + POW2(dy);
		if (dist >= POW2(swarm->cohesion_dist))
			continue;

		cohesion_n++;
		vector_add(&cohesion, &b->pos);
	}

	if (cohesion_n) {
		vector_div(&cohesion, cohesion_n);
		vector_sub(&cohesion, &predator->pos);
		vector_set_mag(&cohesion, 0.5);
	} else {
		cohesion = predator->velocity;
	}

	vector_add(&predator->velocity, &cohesion);
	vector_set_mag(&predator->velocity,
		       swarm->speed * (cohesion_n ? 1.2 : 0.8));
	vector_add(&predator->pos, &predator->velocity);

	predator->pos.x = fmod(predator->pos.x + swarm->width, swarm->width);
	predator->pos.y = fmod(predator->pos.y + swarm->height, swarm->height);
}

void swarm_move(Swarm *swarm)
{
	int i;
	int j;
	Boid *b1;
	Boid *b2;
	gdouble dist;
	guint min_dist;
	gdouble dx, dy;
	gdouble cos_angle;
	int cohesion_n;
	Vector avoid;
	Vector avoid_obstacle;
	Vector align;
	Vector cohesion;
	Vector v;

	swarm_move_predator(swarm);

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
			dist = POW2(dx) + POW2(dy);
			if (dist >= POW2(swarm->cohesion_dist))
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

			if (swarm->avoid && dist < swarm->avoid_dist) {
				v = b1->pos;
				vector_sub(&v, &b2->pos);
				vector_div(&v, dist);
				vector_add(&avoid, &v);
			} else if (swarm->align && dist < swarm->align_dist) {
				v = b2->velocity;
				vector_div(&v, dist);
				vector_add(&align, &v);
			} else if (swarm->cohesion && dist < swarm->cohesion_dist) {
				cohesion_n++;
				vector_add(&cohesion, &b2->pos);
			}
		}

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

		if (swarm->attractive_mouse && swarm->mouse_pos.x >= 0) {
			Vector attract;

			dx = swarm->mouse_pos.x - b1->pos.x;
			dy = swarm->mouse_pos.y - b1->pos.y;

			vector_set(&attract, dx, dy);
			vector_normalize(&attract);
			vector_add(&b1->velocity, &attract);
		}

		vector_set_mag(&b1->velocity, swarm->speed);

		if (swarm_avoid_obstacles(swarm, b1, &avoid_obstacle)) {
			vector_add(&b1->velocity, &avoid_obstacle);
			vector_set_mag(&b1->velocity, swarm->speed);
		}

		vector_add(&b1->pos, &b1->velocity);

		b1->pos.x = fmod(b1->pos.x + swarm->width, swarm->width);
		b1->pos.y = fmod(b1->pos.y + swarm->height, swarm->height);

		if (swarm->debug_vectors) {
			b1->avoid = avoid;
			b1->align = align;
			b1->cohesion = cohesion;
			b1->obstacle = avoid_obstacle;
		}
	}
}

Obstacle *swarm_get_obstacle_by_type(Swarm *swarm, guint type)
{
	Obstacle *o;
	int i;

	for (i = 0; i < swarm->obstacles->len; i++) {
		o = swarm_obstacle_get(swarm, i);
		if (o->type == type)
			return o;
	}

	return NULL;
}

static void swarm_remove_obstacle_by_type(Swarm *swarm, guint type)
{
	Obstacle *o;
	gint i;

	i = swarm->obstacles->len;
	while (i--) {
		o = swarm_obstacle_get(swarm, i);
		if (o->type == type)
			g_array_remove_index(swarm->obstacles, i);
	}
}

void swarm_add_obstacle(Swarm *swarm, gdouble x, gdouble y, guint type)
{
	int i;
	gdouble dx, dy;
	Obstacle *o;
	Obstacle new = {
		.pos.x = x,
		.pos.y = y,
		.type = type,
		.avoid_radius = POW2(OBSTACLE_RADIUS * 1.5),
	};

	/* Only one scary mouse obstacle as first element in the array. */
	if (type == OBSTACLE_TYPE_SCARY_MOUSE) {
		o = swarm_get_obstacle_by_type(swarm, type);

		if (!o) {
			/* The scary mouse obstacle is much bigger */
			new.avoid_radius = POW2(OBSTACLE_RADIUS * 10);
			g_array_prepend_val(swarm->obstacles, new);
		}

		return;
	}

	if (swarm->predator)
		return;

	if (type == OBSTACLE_TYPE_PREDATOR) {
		if (swarm_get_obstacle_by_type(swarm, type) != NULL)
			return;

		/* Remove obstacles and walls */
		swarm_remove_obstacle_by_type(swarm, OBSTACLE_TYPE_WALL);
		swarm_remove_obstacle_by_type(swarm, OBSTACLE_TYPE_IN_FIELD);

		new.velocity.x = 1;
		new.velocity.y = 0;
		new.avoid_radius = POW2(OBSTACLE_RADIUS * 5);
		g_array_prepend_val(swarm->obstacles, new);

		swarm->predator = TRUE;

		return;
	}

	i = swarm->obstacles->len;

	while (i--) {
		o = swarm_obstacle_get(swarm, i);

		/* Ignore the scary mouse obstacle */
		if (o->type == OBSTACLE_TYPE_SCARY_MOUSE)
			continue;

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
	Obstacle *o;
	gdouble dist;
	int i;

	if (!swarm->obstacles->len)
		return FALSE;

	i = swarm->obstacles->len;
	while (i--) {
		o = swarm_obstacle_get(swarm, i);
		/* Remove only field obstacle */
		if (o->type != OBSTACLE_TYPE_IN_FIELD)
			continue;

		dist = POW2(o->pos.x - x) + POW2(o->pos.y - y);
		if (dist <= POW2(OBSTACLE_RADIUS)) {
			g_array_remove_index(swarm->obstacles, i);
			return TRUE;
		}
	}

	return FALSE;
}

static void swarm_remove_walls(Swarm *swarm)
{
	swarm_remove_obstacle_by_type(swarm, OBSTACLE_TYPE_WALL);
}

static void swarm_add_walls(Swarm *swarm)
{
	gint x, y;

	swarm_remove_walls(swarm);

	y = OBSTACLE_RADIUS;
	for (x = 0; x < swarm->width + OBSTACLE_RADIUS; x += (OBSTACLE_RADIUS >> 1)) {
		swarm_add_obstacle(swarm, x, -y, OBSTACLE_TYPE_WALL);
		swarm_add_obstacle(swarm, x, swarm->height + y, OBSTACLE_TYPE_WALL);
	}

	x = y;
	for (y = 0; y < swarm->height + OBSTACLE_RADIUS; y += (OBSTACLE_RADIUS >> 1)) {
		swarm_add_obstacle(swarm, -x, y, OBSTACLE_TYPE_WALL);
		swarm_add_obstacle(swarm, swarm->width + x, y, OBSTACLE_TYPE_WALL);
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

void swarm_set_mouse_pos(Swarm *swarm, gdouble x, gdouble y)
{
	Obstacle *o;

	vector_set(&swarm->mouse_pos, x, y);

	/* Update the scary mouse obstacle position */
	if (swarm->scary_mouse && swarm_num_obstacles(swarm)) {
		o = swarm_obstacle_get(swarm, 0);
		if (o->type == OBSTACLE_TYPE_SCARY_MOUSE)
			vector_set(&o->pos, x, y);
	}
}

void swarm_set_mouse_mode(Swarm *swarm, MouseMode mode)
{
	gboolean scary = FALSE;
	gboolean attractive = FALSE;

	switch (mode) {
	case MOUSE_MODE_SCARY:
		scary = TRUE;
		break;
	case MOUSE_MODE_ATTRACTIVE:
		attractive = TRUE;
		break;
	default:
		break;
	}

	if (scary)
		swarm_add_obstacle(swarm,
				   swarm->mouse_pos.x, swarm->mouse_pos.y,
				   OBSTACLE_TYPE_SCARY_MOUSE);
	else
		swarm_remove_obstacle_by_type(swarm, OBSTACLE_TYPE_SCARY_MOUSE);

	swarm->scary_mouse = scary;
	swarm->attractive_mouse = attractive;
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

gdouble swarm_get_speed(Swarm *swarm)
{
	return swarm->speed;
}

void swarm_set_speed(Swarm *swarm, gdouble speed)
{
	if (speed < MIN_SPEED)
		speed = MIN_SPEED;
	else if (speed > MAX_SPEED)
		speed = MAX_SPEED;

	swarm->speed = speed;
}

void swarm_init_boid(Swarm *swarm, Boid *boid)
{
	memset(boid, 0, sizeof(Boid));

	boid->pos.x = g_random_int_range(0, swarm->width);
	boid->pos.y = g_random_int_range(0, swarm->height);

	boid->velocity.x = g_random_int_range(-5, 6);
	do {
		boid->velocity.y = g_random_int_range(-5, 6);
	} while (vector_is_null(&boid->velocity));

	vector_set_mag(&boid->velocity, 5);
}

void swarm_set_num_boids(Swarm *swarm, guint num)
{
	GArray *boids = swarm->boids;
	Boid b;

	if (!num || num > MAX_BOIDS)
		num = DEFAULT_NUM_BOIDS;

	if (num > boids->len) {
		while (boids->len < num) {
			swarm_init_boid(swarm, &b);
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

void swarm_set_rule_dist(Swarm *swarm, SwarmRule rule, guint dist)
{
	guint d = dist;

	switch (rule) {
	case RULE_AVOID:
		if (d < AVOID_DIST_MIN)
			d = AVOID_DIST_MIN;
		else if (d > AVOID_DIST_MAX)
			d = AVOID_DIST_MAX;

		swarm->avoid_dist = d;
		break;
	case RULE_ALIGN:
		if (d < ALIGN_DIST_MIN)
			d = ALIGN_DIST_MIN;
		else if (d > ALIGN_DIST_MAX)
			d = ALIGN_DIST_MAX;

		swarm->align_dist = dist;
		break;
	case RULE_COHESION:
		if (d < COHESION_DIST_MIN)
			d = COHESION_DIST_MIN;
		else if (d > COHESION_DIST_MAX)
			d = COHESION_DIST_MAX;

		swarm->cohesion_dist = d;
		break;
	default:
		break;
	}
}

guint swarm_get_rule_dist(Swarm *swarm, SwarmRule rule)
{
	guint dist = 0;

	switch (rule) {
	case RULE_AVOID:
		dist = swarm->avoid_dist;
		break;
	case RULE_ALIGN:
		dist = swarm->align_dist;
		break;
	case RULE_COHESION:
		dist = swarm->cohesion_dist;
		break;
	default:
		break;
	}

	return dist;
}

void swarm_set_bg_color(Swarm *swarm, int bg_color)
{
	if (bg_color < BG_COLOR_MIN || bg_color > BG_COLOR_MAX)
		bg_color = g_random_int_range(BG_COLOR_RND_MIN,
					      BG_COLOR_RND_MAX + 1);

	swarm->bg_color = bg_color;
}

int swarm_get_bg_color(Swarm *swarm)
{
	return swarm->bg_color;
}

void swarm_predator_enable(Swarm *swarm, gboolean enable)
{
	if (enable)
		swarm_add_obstacle(swarm, swarm->width >> 1, swarm->height >> 1,
				   OBSTACLE_TYPE_PREDATOR);
	else
		swarm_remove_obstacle_by_type(swarm, OBSTACLE_TYPE_PREDATOR);

	swarm->predator = enable;
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
	swarm_set_speed(swarm, DEFAULT_SPEED);

	swarm->avoid_dist = AVOID_DIST_DFLT;
	swarm->align_dist = ALIGN_DIST_DFLT;
	swarm->cohesion_dist = COHESION_DIST_DFLT;

	swarm->debug_controls = FALSE;

	return swarm;
}
