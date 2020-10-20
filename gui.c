/* SPDX-License-Identifier: MIT */
#include <gtk/gtk.h>
#include <glib/gprintf.h>

#include "boids.h"

#define DEBUG_VECT_FACTOR 20

typedef struct {
	GtkWidget *drawing_area;
	cairo_surface_t *surface;
	cairo_t *cr;

	GMutex lock;

	Swarm *swarm;

#ifdef BOIDS_DEBUG
	GtkWidget *timing_label;
	gulong compute_time;
#endif
} BoidsGui;

static void on_draw(GtkDrawingArea *da, cairo_t *cr, BoidsGui *gui)
{
	g_mutex_lock(&gui->lock);

	cairo_set_source_surface(cr, gui->surface, 0, 0);
	cairo_paint(cr);

#ifdef BOIDS_DEBUG
	if (gui->swarm->debug_vectors) {
		int i;

		for (i = 0; i < 10 && i < swarm_get_num_boids(gui->swarm); i++) {
			Boid *b = swarm_get_boid(gui->swarm, i);
			Vector v = b->pos;
			Vector avoid, align, cohes, obst, veloc;

			vector_mult2(&b->avoid, DEBUG_VECT_FACTOR, &avoid);
			vector_mult2(&b->align, DEBUG_VECT_FACTOR, &align);
			vector_mult2(&b->cohesion, DEBUG_VECT_FACTOR, &cohes);
			vector_mult2(&b->obstacle, DEBUG_VECT_FACTOR, &obst);
			vector_mult2(&b->velocity, DEBUG_VECT_FACTOR, &veloc);

			//~ vector_print(&b->pos, "pos");
			//~ vector_print(&avoid, "avoid");
			//~ vector_print(&align, "align");
			//~ vector_print(&cohes, "cohesion");
			//~ vector_print(&obst, "obstacle");
			//~ vector_print(&veloc, "velocity");

			cairo_set_line_width(cr, 2);
			cairo_move_to(cr, v.x, v.y);

			cairo_rel_line_to(cr, avoid.x, avoid.y);
			cairo_set_source_rgba(cr, 1.0, 0.0, 0.0, 1.0);
			cairo_stroke(cr);

			vector_add(&v, &avoid);
			cairo_move_to(cr, v.x, v.y);

			cairo_rel_line_to(cr, align.x, align.y);
			cairo_set_source_rgba(cr, 0.0, 1.0, 0.0, 1.0);
			cairo_stroke(cr);

			vector_add(&v, &align);
			cairo_move_to(cr, v.x, v.y);

			cairo_rel_line_to(cr, cohes.x, cohes.y);
			cairo_set_source_rgba(cr, 0.0, 0.0, 1.0, 1.0);
			cairo_stroke(cr);

			vector_add(&v, &cohes);
			cairo_move_to(cr, v.x, v.y);

			cairo_set_source_rgba(cr, 1.0, 0.0, 1.0, 1.0);
			cairo_rel_line_to(cr, obst.x, obst.y);
			cairo_stroke(cr);

			cairo_move_to(cr, b->pos.x, b->pos.y);
			cairo_rel_line_to(cr, veloc.x, veloc.y);
			cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 1.0);
			cairo_stroke(cr);
		}
	}
#endif

	g_mutex_unlock(&gui->lock);
}

static void draw_obstacles(BoidsGui *gui)
{
	int i;

	cairo_set_source_rgba(gui->cr, 0.3, 0.3, 0.3, 1.0);
	for (i = 0; i < gui->swarm->obstacles->len; i++) {
		Vector *o = swarm_get_obstacle_pos(gui->swarm, i);
		cairo_arc(gui->cr, o->x, o->y, OBSTACLE_RADIUS, 0, 2 * G_PI);
		cairo_fill(gui->cr);
	}
}

static void draw(BoidsGui *gui)
{
	int i;

	cairo_set_source_rgba(gui->cr, 0.8, 0.8, 0.8, 1.0);
	cairo_paint(gui->cr);

	draw_obstacles(gui);

	for (i = 0; i < swarm_get_num_boids(gui->swarm); i++) {
		Boid *b = swarm_get_boid(gui->swarm, i);
		Vector velocity = b->velocity;
		Vector orth;
		Vector top = b->pos;
		Vector bottom1 = b->pos;
		Vector bottom2 = b->pos;

		vector_set_mag(&velocity, 5);
		vector_add(&top, &velocity);
		vector_sub(&bottom1, &velocity);
		vector_sub(&bottom2, &velocity);

		/* Using 'orth.x = -velocity.y;' gives a nice 3D effect */
		orth.x = velocity.y;
		orth.y = -velocity.x;
		vector_set_mag(&orth, 3);

		vector_sub(&bottom1, &orth);
		vector_add(&bottom2, &orth);

		cairo_set_line_width(gui->cr, 3);
		cairo_set_source_rgba(gui->cr, b->red, 0.0, 1.0 - b->red, 1.0);

		cairo_move_to(gui->cr, bottom1.x, bottom1.y);
		cairo_line_to(gui->cr, top.x, top.y);
		cairo_line_to(gui->cr, bottom2.x, bottom2.y);
		cairo_set_line_join(gui->cr, CAIRO_LINE_JOIN_ROUND);
		cairo_stroke(gui->cr);
	}
}

#define DELAY 20000

static void animate_cb(BoidsGui *gui, gulong time)
{
	g_mutex_lock(&gui->lock);
	draw(gui);
	g_mutex_unlock(&gui->lock);

#ifdef BOIDS_DEBUG
	gui->compute_time = time;
#endif

	if (time < DELAY)
		g_usleep(DELAY - time);
}

static gboolean queue_draw(BoidsGui *gui)
{
#ifdef BOIDS_DEBUG
	gchar label[32];
	g_snprintf(label, 32, "%2ldms", gui->compute_time / 1000);
	gtk_label_set_text(GTK_LABEL(gui->timing_label), label);
#endif
	gtk_widget_queue_draw(gui->drawing_area);

	return swarm_thread_running(gui->swarm);
}

static void cairo_init(BoidsGui *gui)
{
	gint width;
	gint height;

	swarm_get_sizes(gui->swarm, &width, &height);

	g_mutex_lock(&gui->lock);

	cairo_destroy(gui->cr);
	cairo_surface_destroy(gui->surface);

	gui->surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
						  width, height);
	gui->cr = cairo_create(gui->surface);

	draw(gui);

	g_mutex_unlock(&gui->lock);
}

static void on_start_clicked(GtkButton *button, BoidsGui *gui)
{
	if (swarm_thread_running(gui->swarm)) {
		swarm_thread_stop(gui->swarm);
		gtk_button_set_label(button, "Start");
		return;
	}

	gtk_button_set_label(button, "Stop");
	swarm_thread_start(gui->swarm, (SwarmAnimateFunc)animate_cb, gui);
	g_timeout_add(40, (GSourceFunc)queue_draw, gui);
}

static void on_step_clicked(GtkButton *button, BoidsGui *gui)
{
	if (swarm_thread_running(gui->swarm))
		return;

	swarm_move(gui->swarm);
	draw(gui);
	gtk_widget_queue_draw(gui->drawing_area);
}

static void on_avoid_clicked(GtkToggleButton *button, BoidsGui *gui)
{
	gui->swarm->avoid = gtk_toggle_button_get_active(button);
}

static void on_align_clicked(GtkToggleButton *button, BoidsGui *gui)
{
	gui->swarm->align = gtk_toggle_button_get_active(button);
}

static void on_cohesion_clicked(GtkToggleButton *button, BoidsGui *gui)
{
	gui->swarm->cohesion = gtk_toggle_button_get_active(button);
}

static void on_walls_clicked(GtkToggleButton *button, BoidsGui *gui)
{
	gui->swarm->walls = gtk_toggle_button_get_active(button);

	swarm_add_walls(gui->swarm);

	if (!swarm_thread_running(gui->swarm)) {
		draw(gui);
		gtk_widget_queue_draw(gui->drawing_area);
	}
}

static void on_num_boids_changed(GtkSpinButton *spin, BoidsGui *gui)
{
	swarm_set_num_boids(gui->swarm, gtk_spin_button_get_value_as_int(spin));

	if (!swarm_thread_running(gui->swarm)) {
		draw(gui);
		gtk_widget_queue_draw(gui->drawing_area);
	}
}

static void on_dead_angle_clicked(GtkToggleButton *button, BoidsGui *gui)
{
	gui->swarm->dead_angle = gtk_toggle_button_get_active(button);
}

static void on_dead_angle_changed(GtkSpinButton *spin, BoidsGui *gui)
{
	swarm_set_dead_angle(gui->swarm, gtk_spin_button_get_value_as_int(spin));
}

static gboolean on_mouse_clicked(GtkWidget *da, GdkEventButton *event,
				 BoidsGui *gui)
{
	gboolean redraw = TRUE;

	if (!(event->state & GDK_CONTROL_MASK))
		swarm_add_obstacle(gui->swarm, event->x, event->y, 0);
	else
		redraw = swarm_remove_obstacle(gui->swarm, event->x, event->y);

	if (!swarm_thread_running(gui->swarm) && redraw) {
		draw(gui);
		gtk_widget_queue_draw(gui->drawing_area);
	}

	return TRUE;
}

#ifdef BOIDS_DEBUG
static void on_debug_vectors_clicked(GtkToggleButton *button, BoidsGui *gui)
{
	gui->swarm->debug_vectors = gtk_toggle_button_get_active(button);
}
#endif

static void on_destroy(GtkWindow *win, BoidsGui *gui)
{
	swarm_thread_stop(gui->swarm);

	gtk_main_quit();
}

static gboolean on_configure_event(GtkWidget *widget, GdkEventConfigure *event,
				   BoidsGui *gui)
{
	swarm_update_sizes(gui->swarm, event->width, event->height);
	cairo_init(gui);

	return FALSE;
}

static void gui_show(BoidsGui *gui)
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *drawing_area;
	GtkWidget *button;
	GtkWidget *spin;
	GtkWidget *label;
	GtkWidget *separator;
	GtkWidget *check;
	gint width;
	gint height;

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(window), "Boids");
	g_signal_connect(G_OBJECT(window), "destroy",
			 G_CALLBACK(on_destroy), gui);

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
	gtk_box_set_spacing(GTK_BOX(vbox), 5);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	drawing_area = gtk_drawing_area_new();
	gui->drawing_area = g_object_ref(drawing_area);
	swarm_get_sizes(gui->swarm, &width, &height);
	gtk_widget_set_size_request(drawing_area, width, height);
	gtk_box_pack_start(GTK_BOX(vbox), drawing_area, TRUE, TRUE, 0);
	g_signal_connect(G_OBJECT(drawing_area), "draw",
			 G_CALLBACK(on_draw), gui);
	gtk_widget_add_events(drawing_area, GDK_STRUCTURE_MASK |
					    GDK_BUTTON_PRESS_MASK |
					    GDK_BUTTON1_MOTION_MASK);
	g_signal_connect(G_OBJECT(drawing_area), "configure-event",
			 G_CALLBACK(on_configure_event), gui);
	g_signal_connect(G_OBJECT(drawing_area), "button-press-event",
			 G_CALLBACK(on_mouse_clicked), gui);
	g_signal_connect(G_OBJECT(drawing_area), "motion-notify-event",
			 G_CALLBACK(on_mouse_clicked), gui);

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_set_spacing(GTK_BOX(hbox), 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	button = gtk_button_new_with_label("Start");
	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(on_start_clicked), gui);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);

	button = gtk_button_new_with_label("Step");
	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(on_step_clicked), gui);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);

	separator = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
	gtk_box_pack_start(GTK_BOX(hbox), separator, FALSE, FALSE, 5);

	label = gtk_label_new("Boids:");
	gtk_label_set_xalign(GTK_LABEL(label), 1.0);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	spin = gtk_spin_button_new_with_range(MIN_BOIDS, MAX_BOIDS, 100);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), swarm_get_num_boids(gui->swarm));
	g_signal_connect(G_OBJECT(spin), "value-changed",
			 G_CALLBACK(on_num_boids_changed), gui);
	gtk_box_pack_start(GTK_BOX(hbox), spin, FALSE, FALSE, 0);

	separator = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
	gtk_box_pack_start(GTK_BOX(hbox), separator, FALSE, FALSE, 5);

	check = gtk_check_button_new_with_label("Walls");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), gui->swarm->walls);
	g_signal_connect(G_OBJECT(check), "toggled",
			 G_CALLBACK(on_walls_clicked), gui);
	gtk_box_pack_start(GTK_BOX(hbox), check, FALSE, FALSE, 0);

	separator = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
	gtk_box_pack_start(GTK_BOX(hbox), separator, FALSE, FALSE, 5);

	check = gtk_check_button_new_with_label("Avoid");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), gui->swarm->avoid);
	g_signal_connect(G_OBJECT(check), "toggled",
			 G_CALLBACK(on_avoid_clicked), gui);
	gtk_box_pack_start(GTK_BOX(hbox), check, FALSE, FALSE, 0);

	check = gtk_check_button_new_with_label("Align");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), gui->swarm->align);
	g_signal_connect(G_OBJECT(check), "toggled",
			 G_CALLBACK(on_align_clicked), gui);
	gtk_box_pack_start(GTK_BOX(hbox), check, FALSE, FALSE, 0);

	check = gtk_check_button_new_with_label("Cohesion");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), gui->swarm->cohesion);
	g_signal_connect(G_OBJECT(check), "toggled",
			 G_CALLBACK(on_cohesion_clicked), gui);
	gtk_box_pack_start(GTK_BOX(hbox), check, FALSE, FALSE, 0);

	check = gtk_check_button_new_with_label("FoV Dead Angle");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), gui->swarm->cohesion);
	g_signal_connect(G_OBJECT(check), "toggled",
			 G_CALLBACK(on_dead_angle_clicked), gui);
	gtk_box_pack_start(GTK_BOX(hbox), check, FALSE, FALSE, 0);

	spin = gtk_spin_button_new_with_range(0, 360, 10);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), swarm_get_dead_angle(gui->swarm));
	g_signal_connect(G_OBJECT(spin), "value-changed",
			 G_CALLBACK(on_dead_angle_changed), gui);
	gtk_box_pack_start(GTK_BOX(hbox), spin, FALSE, FALSE, 0);

#ifdef BOIDS_DEBUG
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_set_spacing(GTK_BOX(hbox), 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	label = gtk_label_new("Debug:");
	gtk_label_set_xalign(GTK_LABEL(label), 1.0);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);

	check = gtk_check_button_new_with_label("Vectors");
	g_signal_connect(G_OBJECT(check), "toggled",
			 G_CALLBACK(on_debug_vectors_clicked), gui);
	gtk_box_pack_start(GTK_BOX(hbox), check, FALSE, FALSE, 0);

	label = gtk_label_new("");
	gui->timing_label = g_object_ref(label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
#endif

	gtk_widget_show_all(window);
}

int gtk_boids_run(Swarm *swarm)
{
	BoidsGui *gui;

	gui = g_malloc0(sizeof(*gui));
	gui->swarm = swarm;
	g_mutex_init(&gui->lock);

	gtk_init(0, NULL);
	gui_show(gui);

	gtk_main();

#ifdef BOIDS_DEBUG
	g_object_unref(gui->timing_label);
#endif
	g_object_unref(gui->drawing_area);
	cairo_destroy(gui->cr);
	cairo_surface_destroy(gui->surface);

	g_free(gui);

	return 0;
}
