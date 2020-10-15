#ifndef __VECTOR_H__
#define __VECTOR_H__

#include <glib.h>
#include <math.h>

typedef struct {
	gdouble x;
	gdouble y;
} Vector;

#define vector_is_null(v) ((v)->x == 0.0 && (v)->y == 0.0)

#define vector_init(v) ((v)->x = (v)->y = 0)

static inline void vector_set(Vector *v, gdouble x, gdouble y)
{
	v->x = x;
	v->y = y;
}

static inline void vector_add(Vector *v1, Vector *v2)
{
	v1->x += v2->x;
	v1->y += v2->y;
}

static inline void vector_sub(Vector *v1, Vector *v2)
{
	v1->x -= v2->x;
	v1->y -= v2->y;
}

static inline void vector_div(Vector *v, gdouble scalar)
{
	if (!scalar)
		return;

	v->x /= scalar;
	v->y /= scalar;
}

static inline void vector_mult(Vector *v, gdouble scalar)
{
	v->x *= scalar;
	v->y *= scalar;
}

static inline void vector_mult2(Vector *v, gdouble scalar, Vector *res)
{
	*res = *v;
	vector_mult(res, scalar);
}

static inline gdouble vector_mag(Vector *v)
{
	return sqrt(v->x * v->x + v->y * v->y);
}

static inline gdouble vector_dot(Vector *v1, Vector *v2)
{
	return ((v1->x * v2->x) + (v1->y * v2->y));
}

/*
 * Calculate cos(a) where 'a' is the angle between the 2 vectors v1 and v2
 * cos(a) = v1.v2 / |v1|.|v2|
 */
static inline gdouble vector_cos_angle(Vector *v1, Vector *v2)
{
	return (vector_dot(v1, v2) / (vector_mag(v1) * vector_mag(v2)));
}

static inline void vector_normalize(Vector *v)
{
	gdouble mag;

	mag = vector_mag(v);
	vector_div(v, mag);
}

static inline void vector_set_mag(Vector *v, gdouble mag)
{
	vector_normalize(v);
	vector_mult(v, mag);
}

static inline void vector_print(Vector *v, char *name)
{
	if (!name)
		name = "Vector";

	g_printf("%s (%g, %g)\n", name, v->x, v->y);
}

#endif /* __VECTOR_H__ */
