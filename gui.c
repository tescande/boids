/* SPDX-License-Identifier: MIT */
#include <gtk/gtk.h>
#include <glib/gprintf.h>

#include "boids.h"

#define DEBUG_VECT_FACTOR 20

typedef struct {
	GtkApplication *app;
	GtkWidget *window;
	GtkWidget *controls_vbox;
	GtkWidget *drawing_area;
	GtkToggleButton *walls_check;
	cairo_surface_t *surface;
	cairo_surface_t *boids_surface;
	cairo_t *boids_cr;
	cairo_operator_t boids_cr_operator;
	gdouble boids_cr_alpha;
	cairo_surface_t *bg_surface;
	cairo_t *cr;
	gint bg_color;
	guint inhibit_cookie;

	gboolean running;

	Swarm *swarm;

	GtkWidget *timing_label;
	gulong compute_time;
	gulong draw_time;
	gint64 update_label_time;
} BoidsGui;

static void gui_draw_obstacles(BoidsGui *gui)
{
	int i;

	cairo_set_source_rgba(gui->cr, 0.3, 0.3, 0.3, 1.0);
	for (i = 0; i < swarm_num_obstacles(gui->swarm); i++) {
		Obstacle *o = swarm_get_obstacle(gui->swarm, i);

		if (o->type == OBSTACLE_TYPE_WALL ||
		    o->type == OBSTACLE_TYPE_SCARY_MOUSE ||
		    o->type == OBSTACLE_TYPE_PREDATOR)
			continue;

		cairo_arc(gui->cr, o->pos.x, o->pos.y, OBSTACLE_RADIUS, 0, 2 * G_PI);
		cairo_fill(gui->cr);
	}
}

static void gui_draw_boid(cairo_t *cr, Boid *b)
{
	Vector top;
	Vector bottom;
	Vector length;

	top = bottom = b->pos;
	length = b->velocity;
	vector_set_mag(&length, 2);
	vector_add(&top, &length);
	vector_sub(&bottom, &length);

	cairo_set_line_width(cr, 4);
	cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
	cairo_move_to(cr, top.x, top.y);
	cairo_line_to(cr, bottom.x, bottom.y);
	cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
	cairo_stroke(cr);
}

static void gui_draw_predator(BoidsGui *gui)
{
	gdouble predator_rgb[][3] = {
		[BG_COLOR_WHITE]    = { 0.6, 0.6, 0.6 },
		[BG_COLOR_REDDISH]  = { 0.0, 1.0, 1.0 },
		[BG_COLOR_GREENISH] = { 1.0, 0.0, 0.8 },
		[BG_COLOR_BLUISH]   = { 1.0, 1.0, 0.0 },
	};
	Obstacle *predator;
	Vector top;
	Vector bottom;
	Vector length;
	gdouble *rgb;

	predator = swarm_get_obstacle_by_type(gui->swarm, OBSTACLE_TYPE_PREDATOR);
	if (!predator)
		return;

	rgb = predator_rgb[gui->bg_color];

	top = bottom = predator->pos;
	length = predator->velocity;
	vector_set_mag(&length, 4);
	vector_add(&top, &length);
	vector_sub(&bottom, &length);

	cairo_set_line_width(gui->boids_cr, 7);
	cairo_set_line_cap(gui->boids_cr, CAIRO_LINE_CAP_ROUND);
	cairo_move_to(gui->boids_cr, top.x, top.y);
	cairo_line_to(gui->boids_cr, bottom.x, bottom.y);
	cairo_set_source_rgb(gui->boids_cr, rgb[0], rgb[1], rgb[2]);
	cairo_stroke(gui->boids_cr);
}

static void gui_draw_background(BoidsGui *gui)
{
	gdouble rgb[3];
	int full_color;
	int x_color;
	int y_color;
	int bg_color;
	cairo_t *bg_cr;
	cairo_pattern_t *pattern = NULL;
	int width, height;

	swarm_get_sizes(gui->swarm, &width, &height);

	bg_cr = cairo_create(gui->bg_surface);

	/*
	 * Get the dominant color
	 * The 2 others vary with x and y, from .2 to .7
	 */
	bg_color = gui->bg_color;

	if (bg_color == BG_COLOR_WHITE) {
		cairo_set_source_rgb(bg_cr, 1, 1, 1);
	} else {
		pattern = cairo_pattern_create_mesh();
		cairo_mesh_pattern_begin_patch(pattern);

		/* Define pattern corners to fill the entire area */
		cairo_mesh_pattern_move_to(pattern, 0, 0);
		cairo_mesh_pattern_line_to(pattern, width, 0);
		cairo_mesh_pattern_line_to(pattern, width, height);
		cairo_mesh_pattern_line_to(pattern, 0, height);

		switch (bg_color) {
		case BG_COLOR_REDDISH:
			full_color = 0;
			x_color = 1;
			y_color = 2;
			break;
		case BG_COLOR_GREENISH:
			x_color = 0;
			full_color = 1;
			y_color = 2;
			break;
		default:
		case BG_COLOR_BLUISH:
			y_color = 0;
			x_color = 1;
			full_color = 2;
			break;
		}

		#define SET_RGB(_rgb, _x_val, _y_val) \
				(_rgb)[full_color] = 1.0; \
				(_rgb)[x_color] = _x_val; \
				(_rgb)[y_color] = _y_val;
		#define MIN_VAL 0.2
		#define MAX_VAL 0.7

		SET_RGB(rgb, MIN_VAL, MIN_VAL);
		cairo_mesh_pattern_set_corner_color_rgb(pattern, 0,
							rgb[0], rgb[1], rgb[2]);
		SET_RGB(rgb, MAX_VAL, MIN_VAL);
		cairo_mesh_pattern_set_corner_color_rgb(pattern, 1,
							rgb[0], rgb[1], rgb[2]);
		SET_RGB(rgb, MAX_VAL, MAX_VAL);
		cairo_mesh_pattern_set_corner_color_rgb(pattern, 2,
							rgb[0], rgb[1], rgb[2]);
		SET_RGB(rgb, MIN_VAL, MAX_VAL);
		cairo_mesh_pattern_set_corner_color_rgb(pattern, 3,
							rgb[0], rgb[1], rgb[2]);

		#undef SET_RGB
		#undef MIN_VAL
		#undef MAX_VAL

		cairo_mesh_pattern_end_patch(pattern);

		cairo_set_source(bg_cr, pattern);
	}

	cairo_rectangle(bg_cr, 0, 0, width, height);
	cairo_fill(bg_cr);
	cairo_pattern_destroy(pattern);

	cairo_destroy(bg_cr);
}

static void gui_draw(BoidsGui *gui)
{
	int i;

	cairo_set_source_surface(gui->cr, gui->bg_surface, 0, 0);
	cairo_paint(gui->cr);

	gui_draw_obstacles(gui);

	/*
	 * Draw the boid trail effect.
	 * This is done by partially erasing the boids previously drawn by
	 * painting the entrire the boids surface using the cairo operator
	 * CAIRO_OPERATOR_DEST_OUT with an alpha value of 0.5. The color doesn't
	 * matter as the DEST_OUT operator only affects the destination, i.e.
	 * the already painted boids on the surface. Then the boids are drawn at
	 * their new positions.
	 * If the swarm is not running, the operator is set to CLEAR with full
	 * opacity. This will erase the boid trails when the swarm is stopped.
	 * See gui_set_boids_draw_operator()
	 */
	cairo_save(gui->boids_cr);
	cairo_set_operator(gui->boids_cr, gui->boids_cr_operator);
	cairo_set_source_rgba(gui->boids_cr, 1.0, 1.0, 1.0, gui->boids_cr_alpha);
	cairo_paint(gui->boids_cr);
	cairo_restore(gui->boids_cr);

	for (i = 0; i < swarm_get_num_boids(gui->swarm); i++) {
		Boid *b = swarm_get_boid(gui->swarm, i);
		gui_draw_boid(gui->boids_cr, b);
	}

	gui_draw_predator(gui);

	cairo_set_source_surface(gui->cr, gui->boids_surface, 0, 0);
	cairo_paint(gui->cr);
}

static void gui_set_boids_draw_operator(BoidsGui *gui)
{
	if (gui->running) {
		gui->boids_cr_operator = CAIRO_OPERATOR_DEST_OUT;
		gui->boids_cr_alpha = 0.5;
	} else {
		gui->boids_cr_operator = CAIRO_OPERATOR_CLEAR;
		gui->boids_cr_alpha = 1.0;
	}
}

static void gui_set_bg_color(BoidsGui *gui, gint bg_color)
{
	if (bg_color < BG_COLOR_MIN || bg_color > BG_COLOR_MAX)
		bg_color = g_random_int_range(BG_COLOR_RND_MIN,
					      BG_COLOR_RND_MAX + 1);

	gui->bg_color = bg_color;
}

static void gui_update(BoidsGui *gui)
{
	if (!gui->running) {
		gui_draw(gui);
		gtk_widget_queue_draw(gui->drawing_area);
	}
}

#define DELAY 20000

static gboolean gui_animate(BoidsGui *gui)
{
	static gint64 last_time = 0;
	gint64 now;
	gint64 compute_time;
	gint64 draw_time;
	gint64 curr_time;
	gint64 total_time;

	now = g_get_monotonic_time();
	if (now - last_time < DELAY) {
		g_usleep(100);
		return TRUE;
	}

	last_time = now;

	swarm_move(gui->swarm);
	compute_time = g_get_monotonic_time() - now;

	gui_draw(gui);
	draw_time = g_get_monotonic_time() - now - compute_time;

	if (swarm_show_debug_controls(gui->swarm)) {
		curr_time = g_get_monotonic_time();
		total_time = compute_time + draw_time;

		if (curr_time - gui->update_label_time > G_USEC_PER_SEC ||
		    total_time > gui->compute_time + gui->draw_time) {
			gchar label[32];

			gui->update_label_time = curr_time;
			gui->compute_time = compute_time;
			gui->draw_time = draw_time;

			g_snprintf(label, 32, "c: %2ldms d: %2ldms %ld fps",
				   compute_time / 1000,
				   draw_time / 1000,
				   total_time ? 1000000 / total_time : 0);

			gtk_label_set_text(GTK_LABEL(gui->timing_label), label);
		}
	}

	gtk_widget_queue_draw(gui->drawing_area);

	return TRUE;
}

static void gui_init(BoidsGui *gui)
{
	gint width;
	gint height;

	swarm_get_sizes(gui->swarm, &width, &height);

	cairo_destroy(gui->cr);
	cairo_surface_destroy(gui->surface);

	cairo_destroy(gui->boids_cr);
	cairo_surface_destroy(gui->boids_surface);

	cairo_surface_destroy(gui->bg_surface);

	gui->surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24,
						  width, height);
	gui->cr = cairo_create(gui->surface);

	gui->boids_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
							width, height);

	gui->boids_cr = cairo_create(gui->boids_surface);

	gui->bg_surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24,
						     width, height);

	gui_set_boids_draw_operator(gui);

	gui_draw_background(gui);

	gui_draw(gui);
}

static void gui_set_fullscreen(BoidsGui *gui, gboolean fullscreen)
{
	if (fullscreen) {
		gtk_widget_hide(gui->controls_vbox);
		gtk_window_fullscreen(GTK_WINDOW(gui->window));
	} else {
		gtk_window_unfullscreen(GTK_WINDOW(gui->window));
		gtk_widget_show(gui->controls_vbox);
	}
}

static void gui_simulation_start(BoidsGui *gui)
{
	gui->running = TRUE;
	gui_set_boids_draw_operator(gui);
	g_idle_add(G_SOURCE_FUNC(gui_animate), gui);

	gui->inhibit_cookie = gtk_application_inhibit(gui->app, NULL,
					 GTK_APPLICATION_INHIBIT_IDLE, "boids");
}

static void gui_simulation_stop(BoidsGui *gui)
{
	gui->running = FALSE;
	g_idle_remove_by_data(gui);
	gui_set_boids_draw_operator(gui);
	gui_update(gui);

	if (gui->inhibit_cookie) {
		gtk_application_uninhibit(gui->app, gui->inhibit_cookie);
		gui->inhibit_cookie = 0;
	}
}

static void on_draw(GtkDrawingArea *da, cairo_t *cr, BoidsGui *gui)
{
	cairo_set_source_surface(cr, gui->surface, 0, 0);
	cairo_paint(cr);

	if (swarm_show_debug_vectors(gui->swarm)) {
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
}

static void on_start_clicked(GtkButton *button, BoidsGui *gui)
{
	if (gui->running) {
		gtk_button_set_label(button, "Start");
		gui_simulation_stop(gui);
	} else {
		gtk_button_set_label(button, "Stop");
		gui_simulation_start(gui);
	}
}

static void on_step_clicked(GtkButton *button, BoidsGui *gui)
{
	if (gui->running)
		return;

	gui_animate(gui);
}

static void on_avoid_clicked(GtkToggleButton *button, BoidsGui *gui)
{
	gboolean active = gtk_toggle_button_get_active(button);

	swarm_set_rule_active(gui->swarm, RULE_AVOID, active);
}

static void on_align_clicked(GtkToggleButton *button, BoidsGui *gui)
{
	gboolean active = gtk_toggle_button_get_active(button);

	swarm_set_rule_active(gui->swarm, RULE_ALIGN, active);
}

static void on_cohesion_clicked(GtkToggleButton *button, BoidsGui *gui)
{
	gboolean active = gtk_toggle_button_get_active(button);

	swarm_set_rule_active(gui->swarm, RULE_COHESION, active);
}

static void on_walls_clicked(GtkToggleButton *button, BoidsGui *gui)
{
	swarm_set_walls_enable(gui->swarm, gtk_toggle_button_get_active(button));

	gui_update(gui);
}

static void on_bg_color_changed(GtkComboBox *combo, BoidsGui *gui)
{
	gui_set_bg_color(gui, gtk_combo_box_get_active(combo));

	gui_draw_background(gui);

	gui_update(gui);
}

static void on_num_boids_changed(GtkSpinButton *spin, BoidsGui *gui)
{
	swarm_set_num_boids(gui->swarm, gtk_spin_button_get_value_as_int(spin));

	gui_update(gui);
}

static void on_dead_angle_clicked(GtkToggleButton *button, BoidsGui *gui)
{
	swarm_set_rule_active(gui->swarm, RULE_DEAD_ANGLE,
			      gtk_toggle_button_get_active(button));
}

static void on_dead_angle_changed(GtkSpinButton *spin, BoidsGui *gui)
{
	swarm_set_dead_angle(gui->swarm, gtk_spin_button_get_value_as_int(spin));
}

static void on_speed_changed(GtkSpinButton *spin, BoidsGui *gui)
{
	swarm_set_speed(gui->swarm, gtk_spin_button_get_value(spin));
}

static void on_mouse_mode_clicked(GtkToggleButton *button, BoidsGui *gui,
				  MouseMode mode)
{
	swarm_set_mouse_mode(gui->swarm, mode);
}

static void on_mouse_mode_none_clicked(GtkToggleButton *button, BoidsGui *gui)
{
	if (gtk_toggle_button_get_active(button))
		on_mouse_mode_clicked(button, gui, MOUSE_MODE_NONE);
}

static void on_mouse_mode_scary_clicked(GtkToggleButton *button, BoidsGui *gui)
{
	if (gtk_toggle_button_get_active(button))
		on_mouse_mode_clicked(button, gui, MOUSE_MODE_SCARY);
}

static void on_mouse_mode_attractive_clicked(GtkToggleButton *button, BoidsGui *gui)
{
	if (gtk_toggle_button_get_active(button))
		on_mouse_mode_clicked(button, gui, MOUSE_MODE_ATTRACTIVE);
}

static gboolean on_mouse_event(GtkWidget *da, GdkEvent *event, BoidsGui *gui)
{
	gboolean button1 = FALSE;
	gboolean control = FALSE;
	gdouble x, y;

	switch (event->type) {
	case GDK_BUTTON_PRESS:
		x = event->button.x;
		y = event->button.y;
		button1 = (event->button.button == 1);
		control = ((event->button.state & GDK_CONTROL_MASK) != 0);
		break;
	case GDK_MOTION_NOTIFY:
		x = event->motion.x;
		y = event->motion.y;
		button1 = ((event->motion.state & GDK_BUTTON1_MASK) != 0);
		control = ((event->motion.state & GDK_CONTROL_MASK) != 0);
		break;
	case GDK_ENTER_NOTIFY:
		x = event->motion.x;
		y = event->motion.y;
		break;
	case GDK_LEAVE_NOTIFY:
		x = -1000;
		y = -1000;
		break;
	default:
		return FALSE;
	}

	swarm_set_mouse_pos(gui->swarm, x, y);

	if (!button1)
		return FALSE;

	if (control)
		swarm_remove_obstacle(gui->swarm, x, y);
	else
		swarm_add_obstacle(gui->swarm, x, y, OBSTACLE_TYPE_IN_FIELD);

	gui_update(gui);

	return TRUE;
}

static void on_predator_clicked(GtkToggleButton *button, BoidsGui *gui)
{
	gboolean enable = gtk_toggle_button_get_active(button);

	if (enable && gtk_toggle_button_get_active(gui->walls_check))
		gtk_toggle_button_set_active(gui->walls_check, FALSE);
	gtk_widget_set_sensitive(GTK_WIDGET(gui->walls_check), !enable);

	swarm_set_predator_enable(gui->swarm, enable);

	gui_update(gui);
}

static void on_debug_vectors_clicked(GtkToggleButton *button, BoidsGui *gui)
{
	swarm_set_debug_vectors(gui->swarm, gtk_toggle_button_get_active(button));
}

static void on_avoid_dist_changed(GtkSpinButton *spin, BoidsGui *gui)
{
	swarm_set_rule_dist(gui->swarm, RULE_AVOID, gtk_spin_button_get_value_as_int(spin));
}

static void on_align_dist_changed(GtkSpinButton *spin, BoidsGui *gui)
{
	swarm_set_rule_dist(gui->swarm, RULE_ALIGN, gtk_spin_button_get_value_as_int(spin));
}

static void on_cohesion_dist_changed(GtkSpinButton *spin, BoidsGui *gui)
{
	swarm_set_rule_dist(gui->swarm, RULE_COHESION, gtk_spin_button_get_value_as_int(spin));
}

static void on_destroy(GtkWindow *win, BoidsGui *gui)
{
	g_idle_remove_by_data(gui);
}

static gboolean on_configure_event(GtkWidget *widget, GdkEventConfigure *event,
				   BoidsGui *gui)
{
	swarm_set_sizes(gui->swarm, event->width, event->height);
	gui_init(gui);

	return FALSE;
}

static gboolean on_keypress(GtkWidget *widget, GdkEventKey *event,
			    BoidsGui *gui)
{
	GdkWindowState state;

	if (event->type != GDK_KEY_PRESS)
		return FALSE;

	state = gdk_window_get_state(event->window);

	switch (event->keyval) {
	case GDK_KEY_F11:
		break;
	case GDK_KEY_Escape:
		if (!(state & GDK_WINDOW_STATE_FULLSCREEN))
			return FALSE;
		break;
	default:
		return FALSE;
	}

	gui_set_fullscreen(gui, !(state & GDK_WINDOW_STATE_FULLSCREEN));

	return TRUE;
}

static void gui_show_debug_controls(BoidsGui *gui, GtkWidget *vbox)
{
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *check;
	GtkWidget *spin;

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

	label = gtk_label_new("Avoid dist:");
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	spin = gtk_spin_button_new_with_range(AVOID_DIST_MIN, AVOID_DIST_MAX, 1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), swarm_get_rule_dist(gui->swarm, RULE_AVOID));
	g_signal_connect(G_OBJECT(spin), "value-changed",
			 G_CALLBACK(on_avoid_dist_changed), gui);
	gtk_box_pack_start(GTK_BOX(hbox), spin, FALSE, FALSE, 0);

	label = gtk_label_new("Align dist:");
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	spin = gtk_spin_button_new_with_range(ALIGN_DIST_MIN, ALIGN_DIST_MAX, 1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), swarm_get_rule_dist(gui->swarm, RULE_ALIGN));
	g_signal_connect(G_OBJECT(spin), "value-changed",
			 G_CALLBACK(on_align_dist_changed), gui);
	gtk_box_pack_start(GTK_BOX(hbox), spin, FALSE, FALSE, 0);

	label = gtk_label_new("Cohesion dist:");
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	spin = gtk_spin_button_new_with_range(COHESION_DIST_MIN, COHESION_DIST_MAX, 1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), swarm_get_rule_dist(gui->swarm, RULE_COHESION));
	g_signal_connect(G_OBJECT(spin), "value-changed",
			 G_CALLBACK(on_cohesion_dist_changed), gui);
	gtk_box_pack_start(GTK_BOX(hbox), spin, FALSE, FALSE, 0);

	label = gtk_label_new("");
	gui->timing_label = g_object_ref(label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
}

static void gui_activate(GtkApplication* app, BoidsGui *gui)
{
	GtkWidget *window;
	GtkWidget *main_vbox;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *drawing_area;
	GtkWidget *button;
	GtkWidget *spin;
	GtkWidget *label;
	GtkWidget *separator;
	GtkWidget *check;
	GtkWidget *radio;
	GtkWidget *combo;
	gboolean active;
	gint width;
	gint height;

	window = gtk_application_window_new(app);
	gtk_window_set_title(GTK_WINDOW(window), "Boids");
	g_signal_connect(G_OBJECT(window), "destroy",
			 G_CALLBACK(on_destroy), gui);
	g_signal_connect(G_OBJECT(window), "key_press_event",
			 G_CALLBACK(on_keypress), gui);
	gui->window = window;

	main_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_set_border_width(GTK_CONTAINER(main_vbox), 5);
	gtk_box_set_spacing(GTK_BOX(main_vbox), 5);
	gtk_container_add(GTK_CONTAINER(window), main_vbox);

	drawing_area = gtk_drawing_area_new();
	gui->drawing_area = g_object_ref(drawing_area);
	swarm_get_sizes(gui->swarm, &width, &height);
	gtk_widget_set_size_request(drawing_area, width, height);
	gtk_box_pack_start(GTK_BOX(main_vbox), drawing_area, TRUE, TRUE, 0);
	g_signal_connect(G_OBJECT(drawing_area), "draw",
			 G_CALLBACK(on_draw), gui);
	gtk_widget_add_events(drawing_area, GDK_STRUCTURE_MASK |
					    GDK_BUTTON_PRESS_MASK |
					    GDK_POINTER_MOTION_MASK |
					    GDK_ENTER_NOTIFY_MASK |
					    GDK_LEAVE_NOTIFY_MASK);
	g_signal_connect(G_OBJECT(drawing_area), "configure-event",
			 G_CALLBACK(on_configure_event), gui);
	g_signal_connect(G_OBJECT(drawing_area), "button-press-event",
			 G_CALLBACK(on_mouse_event), gui);
	g_signal_connect(G_OBJECT(drawing_area), "motion-notify-event",
			 G_CALLBACK(on_mouse_event), gui);
	g_signal_connect(G_OBJECT(drawing_area), "enter-notify-event",
			 G_CALLBACK(on_mouse_event), gui);
	g_signal_connect(G_OBJECT(drawing_area), "leave-notify-event",
			 G_CALLBACK(on_mouse_event), gui);

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_set_border_width(GTK_CONTAINER(main_vbox), 5);
	gtk_container_add(GTK_CONTAINER(main_vbox), vbox);
	gui->controls_vbox = vbox;

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_set_spacing(GTK_BOX(hbox), 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	button = gtk_button_new_with_label(gui->running ? "Stop" : "Start");
	gtk_widget_set_size_request(GTK_WIDGET(button), 68, -1);
	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(on_start_clicked), gui);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);

	button = gtk_button_new_with_label("Step");
	gtk_widget_set_size_request(GTK_WIDGET(button), 68, -1);
	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(on_step_clicked), gui);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);

	separator = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
	gtk_box_pack_start(GTK_BOX(hbox), separator, FALSE, FALSE, 5);

	label = gtk_label_new("Boids Number:");
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
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check),
				     swarm_get_walls_enable(gui->swarm));
	if (swarm_get_predator_enable(gui->swarm))
		gtk_widget_set_sensitive(check, FALSE);
	g_signal_connect(G_OBJECT(check), "toggled",
			 G_CALLBACK(on_walls_clicked), gui);
	gtk_box_pack_start(GTK_BOX(hbox), check, FALSE, FALSE, 0);
	gui->walls_check = GTK_TOGGLE_BUTTON(check);

	separator = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
	gtk_box_pack_start(GTK_BOX(hbox), separator, FALSE, FALSE, 5);

	label = gtk_label_new("Background:");
	gtk_label_set_xalign(GTK_LABEL(label), 1.0);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	combo = gtk_combo_box_text_new();
	gtk_combo_box_text_insert(GTK_COMBO_BOX_TEXT(combo), BG_COLOR_WHITE, NULL, "White");
	gtk_combo_box_text_insert(GTK_COMBO_BOX_TEXT(combo), BG_COLOR_REDDISH, NULL, "Reddish");
	gtk_combo_box_text_insert(GTK_COMBO_BOX_TEXT(combo), BG_COLOR_GREENISH, NULL, "Greenish");
	gtk_combo_box_text_insert(GTK_COMBO_BOX_TEXT(combo), BG_COLOR_BLUISH, NULL, "Bluish");
	gtk_combo_box_set_active(GTK_COMBO_BOX(combo), gui->bg_color);
	g_signal_connect(G_OBJECT(combo), "changed",
			 G_CALLBACK(on_bg_color_changed), gui);
	gtk_box_pack_start(GTK_BOX(hbox), combo, FALSE, FALSE, 0);

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_set_spacing(GTK_BOX(hbox), 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	label = gtk_label_new("Boids Rules:");
	gtk_label_set_xalign(GTK_LABEL(label), 1.0);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	check = gtk_check_button_new_with_label("Avoid");
	active = swarm_get_rule_active(gui->swarm, RULE_AVOID);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), active);
	g_signal_connect(G_OBJECT(check), "toggled",
			 G_CALLBACK(on_avoid_clicked), gui);
	gtk_box_pack_start(GTK_BOX(hbox), check, FALSE, FALSE, 0);

	check = gtk_check_button_new_with_label("Align");
	active = swarm_get_rule_active(gui->swarm, RULE_ALIGN);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), active);
	g_signal_connect(G_OBJECT(check), "toggled",
			 G_CALLBACK(on_align_clicked), gui);
	gtk_box_pack_start(GTK_BOX(hbox), check, FALSE, FALSE, 0);

	check = gtk_check_button_new_with_label("Cohesion");
	active = swarm_get_rule_active(gui->swarm, RULE_COHESION);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), active);
	g_signal_connect(G_OBJECT(check), "toggled",
			 G_CALLBACK(on_cohesion_clicked), gui);
	gtk_box_pack_start(GTK_BOX(hbox), check, FALSE, FALSE, 0);

	check = gtk_check_button_new_with_label("FoV Dead Angle");
	active = swarm_get_rule_active(gui->swarm, RULE_DEAD_ANGLE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), active);
	g_signal_connect(G_OBJECT(check), "toggled",
			 G_CALLBACK(on_dead_angle_clicked), gui);
	gtk_box_pack_start(GTK_BOX(hbox), check, FALSE, FALSE, 0);

	spin = gtk_spin_button_new_with_range(0, 360, 10);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), swarm_get_dead_angle(gui->swarm));
	g_signal_connect(G_OBJECT(spin), "value-changed",
			 G_CALLBACK(on_dead_angle_changed), gui);
	gtk_box_pack_start(GTK_BOX(hbox), spin, FALSE, FALSE, 0);

	label = gtk_label_new("Speed:");
	gtk_label_set_xalign(GTK_LABEL(label), 1.0);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	spin = gtk_spin_button_new_with_range(MIN_SPEED, MAX_SPEED, 0.2);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), swarm_get_speed(gui->swarm));
	g_signal_connect(G_OBJECT(spin), "value-changed",
			 G_CALLBACK(on_speed_changed), gui);
	gtk_box_pack_start(GTK_BOX(hbox), spin, FALSE, FALSE, 0);

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_set_spacing(GTK_BOX(hbox), 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	label = gtk_label_new("Mouse Mode:");
	gtk_label_set_xalign(GTK_LABEL(label), 1.0);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	radio = gtk_radio_button_new_with_label(NULL, "None");
	g_signal_connect(G_OBJECT(radio), "toggled",
			 G_CALLBACK(on_mouse_mode_none_clicked), gui);
	gtk_box_pack_start(GTK_BOX(hbox), radio, FALSE, FALSE, 0);

	radio = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(radio), "Scary");
	g_signal_connect(G_OBJECT(radio), "toggled",
			 G_CALLBACK(on_mouse_mode_scary_clicked), gui);
	gtk_box_pack_start(GTK_BOX(hbox), radio, FALSE, FALSE, 0);

	radio = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(radio), "Attractive");
	g_signal_connect(G_OBJECT(radio), "toggled",
			 G_CALLBACK(on_mouse_mode_attractive_clicked), gui);
	gtk_box_pack_start(GTK_BOX(hbox), radio, FALSE, FALSE, 0);

	separator = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
	gtk_box_pack_start(GTK_BOX(hbox), separator, FALSE, FALSE, 5);

	check = gtk_check_button_new_with_label("Predator");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check),
				     swarm_get_predator_enable(gui->swarm));
	g_signal_connect(G_OBJECT(check), "toggled",
			 G_CALLBACK(on_predator_clicked), gui);
	gtk_box_pack_start(GTK_BOX(hbox), check, FALSE, FALSE, 0);

	if (swarm_show_debug_controls(gui->swarm))
		gui_show_debug_controls(gui, vbox);

	gtk_widget_show_all(window);

	if (gui->running)
		gui_simulation_start(gui);
}

int gui_run(Swarm *swarm, int bg_color, gboolean start)
{
	BoidsGui *gui;

	gui = g_malloc0(sizeof(*gui));
	gui->swarm = swarm;
	gui->running = start;
	gui_set_bg_color(gui, bg_color);

	gui->app = gtk_application_new("org.escande.boids", G_APPLICATION_NON_UNIQUE);
	g_signal_connect(gui->app, "activate", G_CALLBACK(gui_activate), gui);

	g_application_run(G_APPLICATION(gui->app), 0, NULL);

	if (gui->timing_label)
		g_object_unref(gui->timing_label);

	g_object_unref(gui->drawing_area);
	cairo_destroy(gui->boids_cr);
	cairo_surface_destroy(gui->boids_surface);
	cairo_destroy(gui->cr);
	cairo_surface_destroy(gui->surface);
	cairo_surface_destroy(gui->bg_surface);

	g_object_unref(gui->app);

	g_free(gui);

	return 0;
}
