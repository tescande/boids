/* SPDX-License-Identifier: MIT */
#include <gtk/gtk.h>
#include <glib/gprintf.h>

#include "boids.h"

#define DEBUG_VECT_FACTOR 20

typedef struct {
	GtkWidget *drawing_area;
	cairo_surface_t *surface;
	cairo_surface_t *boids_surface;
	cairo_t *boids_cr;
	cairo_surface_t *bg_surface;
	cairo_t *cr;

	GMutex lock;

	Swarm *swarm;

	GtkWidget *timing_label;
	gulong compute_time;
	gint64 update_label_time;
} BoidsGui;

static void on_draw(GtkDrawingArea *da, cairo_t *cr, BoidsGui *gui)
{
	g_mutex_lock(&gui->lock);

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

	g_mutex_unlock(&gui->lock);
}

static void draw_obstacles(BoidsGui *gui)
{
	int i;

	cairo_set_source_rgba(gui->cr, 0.3, 0.3, 0.3, 1.0);
	for (i = 0; i < swarm_num_obstacles(gui->swarm); i++) {
		Obstacle *o = swarm_obstacle_get(gui->swarm, i);

		if (o->type == OBSTACLE_TYPE_WALL ||
		    o->type == OBSTACLE_TYPE_SCARY_MOUSE)
			continue;

		cairo_arc(gui->cr, o->pos.x, o->pos.y, OBSTACLE_RADIUS, 0, 2 * G_PI);
		cairo_fill(gui->cr);
	}
}

static void draw_boid(cairo_t *cr, Boid *b)
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

static void draw(BoidsGui *gui)
{
	int i;
	cairo_operator_t op;
	gdouble alpha;

	cairo_set_source_surface(gui->cr, gui->bg_surface, 0, 0);
	cairo_paint(gui->cr);

	draw_obstacles(gui);

	if (swarm_thread_running(gui->swarm)) {
		op = CAIRO_OPERATOR_DEST_IN;
		alpha = 0.5;
	} else {
		op = CAIRO_OPERATOR_CLEAR;
		alpha = 1.0;
	}

	cairo_save(gui->boids_cr);
	cairo_set_operator(gui->boids_cr, op);
	cairo_set_source_rgba(gui->boids_cr, 1.0, 1.0, 1.0, alpha);
	cairo_paint(gui->boids_cr);
	cairo_restore(gui->boids_cr);

	for (i = 0; i < swarm_get_num_boids(gui->swarm); i++) {
		Boid *b = swarm_get_boid(gui->swarm, i);
		draw_boid(gui->boids_cr, b);
	}

	cairo_set_source_surface(gui->cr, gui->boids_surface, 0, 0);
	cairo_paint(gui->cr);
}

#define DELAY 20000

static void animate_cb(BoidsGui *gui, gulong time)
{
	GTimer *timer = g_timer_new();
	gulong elapsed;
	gulong compute_time;
	gint64 curr_time;

	g_timer_start(timer);

	g_mutex_lock(&gui->lock);
	draw(gui);
	g_mutex_unlock(&gui->lock);

	g_timer_elapsed(timer, &elapsed);
	compute_time = time + elapsed;

	if (swarm_show_debug_controls(gui->swarm)) {
		curr_time = g_get_monotonic_time();

		if (curr_time - gui->update_label_time > G_USEC_PER_SEC ||
		    compute_time > gui->compute_time) {
			gui->update_label_time = curr_time;
			gui->compute_time = compute_time;
		}
	}

	if (compute_time < DELAY)
		g_usleep(DELAY - compute_time);
}

static gboolean queue_draw(BoidsGui *gui)
{
	if (swarm_show_debug_controls(gui->swarm)) {
		gchar label[32];
		g_snprintf(label, 32, "%2ldms, %ld fps", gui->compute_time / 1000,
			    gui->compute_time ? 1000000 / gui->compute_time : 0);
		gtk_label_set_text(GTK_LABEL(gui->timing_label), label);
	}

	gtk_widget_queue_draw(gui->drawing_area);

	return swarm_thread_running(gui->swarm);
}

static void draw_background(BoidsGui *gui)
{
	static gdouble rgb[4][3];
	static int full_color = -1;
	int bg_color;
	cairo_t *bg_cr;
	cairo_pattern_t *pattern;
	int width, height;
	int i;

	swarm_get_sizes(gui->swarm, &width, &height);

	bg_cr = cairo_create(gui->bg_surface);

	/*
	 * Get the dominant color
	 * The 2 others vary with x and y, from .3 to .6
	 */
	bg_color = swarm_get_bg_color(gui->swarm);
	if (full_color != bg_color) {
		int x_color, y_color;
		int corner_index;

		full_color = bg_color;
		x_color = (full_color == 0) ? 1 : 0;
		y_color = (x_color + full_color) ^ 0x3;

		#define SET_RGB(_rgb, _x_val, _y_val) \
				(_rgb)[full_color] = 1.0; \
				(_rgb)[x_color] = _x_val; \
				(_rgb)[y_color] = _y_val;
		#define MIN_VAL 0.2
		#define MAX_VAL 0.8

		corner_index = g_random_int_range(0, 4);
		SET_RGB(rgb[corner_index], MIN_VAL, MIN_VAL);

		corner_index = (corner_index + 1) & 3;
		SET_RGB(rgb[corner_index], MAX_VAL, MIN_VAL);

		corner_index = (corner_index + 1) & 3;
		SET_RGB(rgb[corner_index], MAX_VAL, MAX_VAL);

		corner_index = (corner_index + 1) & 3;
		SET_RGB(rgb[corner_index], MIN_VAL, MAX_VAL);

		#undef SET_RGB
		#undef MIN_VAL
		#undef MAX_VAL
	}

	pattern = cairo_pattern_create_mesh();
	cairo_mesh_pattern_begin_patch(pattern);

	/* Define pattern corners to fill the entire area */
	cairo_mesh_pattern_move_to(pattern, 0, 0);
	cairo_mesh_pattern_line_to(pattern, width, 0);
	cairo_mesh_pattern_line_to(pattern, width, height);
	cairo_mesh_pattern_line_to(pattern, 0, height);

	/* Set corner colors from the randomly filled rgb array */
	for (i = 0; i < 4; i++) {
		cairo_mesh_pattern_set_corner_color_rgb(pattern, i,
							rgb[i][0],
							rgb[i][1],
							rgb[i][2]);
	}

	cairo_mesh_pattern_end_patch(pattern);

	cairo_rectangle(bg_cr, 0, 0, width, height);
	cairo_set_source(bg_cr, pattern);
	cairo_fill(bg_cr);
	cairo_pattern_destroy(pattern);

	cairo_destroy(bg_cr);
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

	gui->boids_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
							width, height);

	gui->boids_cr = cairo_create(gui->boids_surface);

	gui->bg_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
						     width, height);

	draw_background(gui);

	draw(gui);

	g_mutex_unlock(&gui->lock);
}

static void on_start_clicked(GtkButton *button, BoidsGui *gui)
{
	if (swarm_thread_running(gui->swarm)) {
		swarm_thread_stop(gui->swarm);
		gtk_button_set_label(button, "Start");
		draw(gui);
		return;
	}

	gtk_button_set_label(button, "Stop");
	swarm_thread_start(gui->swarm, (SwarmAnimateFunc)animate_cb, gui);
	g_timeout_add(10, (GSourceFunc)queue_draw, gui);
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
	gboolean active = gtk_toggle_button_get_active(button);

	swarm_rule_set_active(gui->swarm, RULE_AVOID, active);
}

static void on_align_clicked(GtkToggleButton *button, BoidsGui *gui)
{
	gboolean active = gtk_toggle_button_get_active(button);

	swarm_rule_set_active(gui->swarm, RULE_ALIGN, active);
}

static void on_cohesion_clicked(GtkToggleButton *button, BoidsGui *gui)
{
	gboolean active = gtk_toggle_button_get_active(button);

	swarm_rule_set_active(gui->swarm, RULE_COHESION, active);
}

static void on_walls_clicked(GtkToggleButton *button, BoidsGui *gui)
{
	swarm_walls_set_enable(gui->swarm, gtk_toggle_button_get_active(button));

	if (!swarm_thread_running(gui->swarm)) {
		draw(gui);
		gtk_widget_queue_draw(gui->drawing_area);
	}
}

static void on_bg_color_changed(GtkComboBox *combo, BoidsGui *gui)
{
	swarm_set_bg_color(gui->swarm, gtk_combo_box_get_active(combo));

	cairo_init(gui);

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
	swarm_rule_set_active(gui->swarm, RULE_DEAD_ANGLE,
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
	gboolean redraw = TRUE;
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
		redraw = swarm_remove_obstacle(gui->swarm, x, y);
	else
		swarm_add_obstacle(gui->swarm, x, y, OBSTACLE_TYPE_IN_FIELD);

	if (!swarm_thread_running(gui->swarm) && redraw) {
		draw(gui);
		gtk_widget_queue_draw(gui->drawing_area);
	}

	return TRUE;
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
	swarm_thread_stop(gui->swarm);

	gtk_main_quit();
}

static gboolean on_configure_event(GtkWidget *widget, GdkEventConfigure *event,
				   BoidsGui *gui)
{
	swarm_set_sizes(gui->swarm, event->width, event->height);
	cairo_init(gui);

	return FALSE;
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
	GtkWidget *radio;
	GtkWidget *combo;
	gboolean active;
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

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_set_spacing(GTK_BOX(hbox), 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	button = gtk_button_new_with_label("Start");
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
				     swarm_walls_get_enable(gui->swarm));
	g_signal_connect(G_OBJECT(check), "toggled",
			 G_CALLBACK(on_walls_clicked), gui);
	gtk_box_pack_start(GTK_BOX(hbox), check, FALSE, FALSE, 0);

	separator = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
	gtk_box_pack_start(GTK_BOX(hbox), separator, FALSE, FALSE, 5);

	label = gtk_label_new("Background:");
	gtk_label_set_xalign(GTK_LABEL(label), 1.0);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	combo = gtk_combo_box_text_new();
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo), NULL, "Red");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo), NULL, "Green");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo), NULL, "Blue");
	gtk_combo_box_set_active(GTK_COMBO_BOX(combo), swarm_get_bg_color(gui->swarm));
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
	active = swarm_rule_get_active(gui->swarm, RULE_AVOID);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), active);
	g_signal_connect(G_OBJECT(check), "toggled",
			 G_CALLBACK(on_avoid_clicked), gui);
	gtk_box_pack_start(GTK_BOX(hbox), check, FALSE, FALSE, 0);

	check = gtk_check_button_new_with_label("Align");
	active = swarm_rule_get_active(gui->swarm, RULE_ALIGN);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), active);
	g_signal_connect(G_OBJECT(check), "toggled",
			 G_CALLBACK(on_align_clicked), gui);
	gtk_box_pack_start(GTK_BOX(hbox), check, FALSE, FALSE, 0);

	check = gtk_check_button_new_with_label("Cohesion");
	active = swarm_rule_get_active(gui->swarm, RULE_COHESION);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), active);
	g_signal_connect(G_OBJECT(check), "toggled",
			 G_CALLBACK(on_cohesion_clicked), gui);
	gtk_box_pack_start(GTK_BOX(hbox), check, FALSE, FALSE, 0);

	check = gtk_check_button_new_with_label("FoV Dead Angle");
	active = swarm_rule_get_active(gui->swarm, RULE_DEAD_ANGLE);
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

	if (swarm_show_debug_controls(gui->swarm))
		gui_show_debug_controls(gui, vbox);

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

	if (gui->timing_label)
		g_object_unref(gui->timing_label);

	g_object_unref(gui->drawing_area);
	cairo_destroy(gui->boids_cr);
	cairo_surface_destroy(gui->boids_surface);
	cairo_destroy(gui->cr);
	cairo_surface_destroy(gui->surface);

	g_free(gui);

	return 0;
}
