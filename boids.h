/* SPDX-License-Identifier: MIT */
#ifndef __BOIDS_H__
#define __BOIDS_H__

#include <glib.h>
#include <glib/gprintf.h>

#include "vector.h"

#define DEFAULT_WIDTH  1024
#define DEFAULT_HEIGHT 576

#define BG_COLOR_WHITE    0
#define BG_COLOR_REDDISH  1
#define BG_COLOR_GREENISH 2
#define BG_COLOR_BLUISH   3
#define BG_COLOR_MIN   0
#define BG_COLOR_MAX   3
#define BG_COLOR_RND_MIN   1
#define BG_COLOR_RND_MAX   3

#define DEFAULT_NUM_BOIDS 300
#define MIN_BOIDS 1
#define MAX_BOIDS 1000

#define DEFAULT_DEAD_ANGLE (60)

#define DEFAULT_SPEED 4.5
#define MIN_SPEED 1.0
#define MAX_SPEED 8.0

#define OBSTACLE_RADIUS 20

#define AVOID_DIST_DFLT 30
#define AVOID_DIST_MIN   5
#define AVOID_DIST_MAX  50

#define ALIGN_DIST_DFLT  80
#define ALIGN_DIST_MIN   50
#define ALIGN_DIST_MAX  250

#define COHESION_DIST_DFLT 150
#define COHESION_DIST_MIN   80
#define COHESION_DIST_MAX  450

#define PROXIMITY_DIST 30

typedef struct {
	Vector pos;
	Vector velocity;

	/* For debugging purpose */
	Vector avoid;
	Vector align;
	Vector cohesion;
	Vector obstacle;
} Boid;

typedef enum {
	OBSTACLE_TYPE_IN_FIELD = 0,
	OBSTACLE_TYPE_WALL,
	OBSTACLE_TYPE_SCARY_MOUSE,
	OBSTACLE_TYPE_PREDATOR,
} ObstacleType;

typedef struct {
	ObstacleType type;
	Vector pos;
	/* The predator is a moving obstacle and needs a velocity vector */
	Vector velocity;
	/*
	 * avoid_radius is actually the power of 2 of the avoid radius value
	 * to save a sqrt() call for distance comparison
	 */
	gdouble avoid_radius;
} Obstacle;

typedef struct _Swarm Swarm;

typedef void (*SwarmAnimateFunc)(gpointer userdata, gulong time);

typedef struct _Swarm {
	GArray *boids;
	GArray *obstacles;

	gint width;
	gint height;

	gboolean walls;

	gboolean avoid;
	gboolean align;
	gboolean cohesion;
	gboolean dead_angle;
	gdouble cos_dead_angle;
	gdouble speed;

	guint avoid_dist;
	guint align_dist;
	guint cohesion_dist;

	Vector mouse_pos;
	gboolean scary_mouse;
	gboolean attractive_mouse;

	gboolean predator;

	gboolean debug_controls;
	gboolean debug_vectors;
} Swarm;

typedef enum {
	RULE_AVOID,
	RULE_ALIGN,
	RULE_COHESION,
	RULE_DEAD_ANGLE,
} SwarmRule;

typedef enum {
	MOUSE_MODE_NONE = 0,
	MOUSE_MODE_SCARY,
	MOUSE_MODE_ATTRACTIVE,
} MouseMode;

static inline gdouble deg2rad(gdouble deg)
{
	return deg * G_PI / 180;
}

static inline gdouble rad2deg(gdouble rad)
{
	return rad * 180 / G_PI;
}

#define POW2(v) ((v) * (v))

Swarm *swarm_alloc(void);
void swarm_free(Swarm *swarm);

#define swarm_show_debug_vectors(swarm) ((swarm)->debug_vectors)
#define swarm_set_debug_vectors(swarm, en) ((swarm)->debug_vectors = (en))

#define swarm_show_debug_controls(swarm) ((swarm)->debug_controls)
#define swarm_set_debug_controls(swarm, en) ((swarm)->debug_controls = (en))

void swarm_get_sizes(Swarm *swarm, gint *width, gint *height);
void swarm_set_sizes(Swarm *swarm, guint width, guint height);

gboolean swarm_rule_get_active(Swarm *swarm, SwarmRule rule);
void swarm_rule_set_active(Swarm *swarm, SwarmRule rule, gboolean active);

guint swarm_get_rule_dist(Swarm *swarm, SwarmRule rule);
void swarm_set_rule_dist(Swarm *swarm, SwarmRule rule, guint dist);

gboolean swarm_walls_get_enable(Swarm *swarm);
void swarm_walls_set_enable(Swarm *swarm, gboolean enable);

#define swarm_get_num_boids(swarm) ((swarm)->boids->len)
void swarm_set_num_boids(Swarm *swarm, guint num);

#define swarm_get_boid(swarm, n) (&g_array_index((swarm)->boids, Boid, n))

guint swarm_get_dead_angle(Swarm *swarm);
void swarm_set_dead_angle(Swarm *swarm, guint angle);

gdouble swarm_get_speed(Swarm *swarm);
void swarm_set_speed(Swarm *swarm, gdouble speed);

void swarm_set_mouse_pos(Swarm *swarm, gdouble x, gdouble y);

void swarm_set_mouse_mode(Swarm *swarm, MouseMode mode);

void swarm_add_obstacle(Swarm *swarm, gdouble x, gdouble y, guint flags);
gboolean swarm_remove_obstacle(Swarm *swarm, gdouble x, gdouble y);

void swarm_set_predator_enable(Swarm *swarm, gboolean enable);
gboolean swarm_get_predator_enable(Swarm *swarm);

#define swarm_obstacle_get(swarm, n) (&(g_array_index((swarm)->obstacles, Obstacle, n)))
#define swarm_obstacle_get_pos(swarm, n) (&(g_array_index((swarm)->obstacles, Obstacle, n).pos))
#define swarm_obstacle_get_type(swarm, n) (g_array_index((swarm)->obstacles, Obstacle, n).type)
#define swarm_num_obstacles(swarm) ((swarm)->obstacles->len)
Obstacle *swarm_get_obstacle_by_type(Swarm *swarm, guint type);

void swarm_move(Swarm *swarm);

int gui_run(Swarm *swarm, gint bg_color, gboolean start);

#endif /* __BOIDS_H__ */
