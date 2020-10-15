/* SPDX-License-Identifier: MIT */
#ifndef __BOIDS_H__
#define __BOIDS_H__

#include <glib.h>
#include <glib/gprintf.h>

#include "vector.h"

#define DEFAULT_WIDTH  1024
#define DEFAULT_HEIGHT 576

typedef struct {
	Vector pos;
	Vector velocity;
} Boid;

typedef struct {
	gint width;
	gint height;
} Swarm;

Swarm *swarm_alloc(void);
void swarm_free(Swarm *swarm);

int gtk_boids_run(Swarm *swarm);

#endif /* __BOIDS_H__ */
