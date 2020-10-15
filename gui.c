/* SPDX-License-Identifier: MIT */
#include <gtk/gtk.h>
#include <glib/gprintf.h>

#include "boids.h"

typedef struct {
	GtkWidget *drawing_area;
	cairo_surface_t *surface;
	cairo_t *cr;

	Swarm *swarm;
} BoidsGui;

static void on_draw(GtkDrawingArea *da, cairo_t *cr, BoidsGui *gui)
{
	cairo_set_source_surface(cr, gui->surface, 0, 0);
	cairo_paint(cr);
}

static void cairo_init(BoidsGui *gui)
{
	cairo_destroy(gui->cr);
	cairo_surface_destroy(gui->surface);

	gui->surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
						  gui->swarm->width,
						  gui->swarm->height);
	gui->cr = cairo_create(gui->surface);
}

static void on_start_clicked(GtkButton *button, BoidsGui *gui)
{
}

static void on_destroy(GtkWindow *win, BoidsGui *gui)
{
	gtk_main_quit();
}

static gboolean on_configure_event(GtkWidget *widget, GdkEventConfigure *event,
				   BoidsGui *gui)
{
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
	gtk_widget_set_size_request(drawing_area, gui->swarm->width, gui->swarm->height);
	gtk_box_pack_start(GTK_BOX(vbox), drawing_area, TRUE, TRUE, 0);
	g_signal_connect(G_OBJECT(drawing_area), "draw",
			 G_CALLBACK(on_draw), gui);
	gtk_widget_add_events(drawing_area, GDK_STRUCTURE_MASK);
	g_signal_connect(G_OBJECT(drawing_area), "configure-event",
			 G_CALLBACK(on_configure_event), gui);

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_set_spacing(GTK_BOX(hbox), 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	button = gtk_button_new_with_label("Start");
	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(on_start_clicked), gui);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);

	gtk_widget_show_all(window);
}

int gtk_boids_run(Swarm *swarm)
{
	BoidsGui *gui;

	gui = g_malloc0(sizeof(*gui));
	gui->swarm = swarm;

	gtk_init(0, NULL);
	gui_show(gui);

	gtk_main();

	g_object_unref(gui->drawing_area);
	cairo_destroy(gui->cr);
	cairo_surface_destroy(gui->surface);

	g_free(gui);

	return 0;
}