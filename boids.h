/* SPDX-License-Identifier: MIT */
#ifndef __BOIDS_H__
#define __BOIDS_H__

#include <glib.h>
#include <glib/gprintf.h>

#include "vector.h"

#define DEFAULT_WIDTH  1024
#define DEFAULT_HEIGHT 576

#define DEFAULT_NUM_BOIDS 300
#define MIN_BOIDS 1
#define MAX_BOIDS 1000

typedef struct {
	Vector pos;
	Vector velocity;
} Boid;

typedef struct {
	GArray *boids;

	gint width;
	gint height;
} Swarm;

Swarm *swarm_alloc(guint num_boids);
void swarm_free(Swarm *swarm);

#define swarm_get_num_boids(swarm) ((swarm)->boids->len)
void swarm_set_num_boids(Swarm *swarm, guint num);

#define swarm_get_boid(swarm, n) (&g_array_index((swarm)->boids, Boid, n))

void swarm_move(Swarm *swarm);

int gtk_boids_run(Swarm *swarm);

#endif /* __BOIDS_H__ */
