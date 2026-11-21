// SPDX-License-Identifier: MIT
/*
 * layernx-pinpad: PIN-Eingabe als Vollbild-Overlay (LayerNX-Tablet-Linux)
 *   layernx-pinpad [--titel TEXT] [--hinweis TEXT]
 * Links: Titel, 4 PIN-Felder, Hinweis, Loeschen/Abbrechen. Rechts: Tasten 1-9 und 0.
 * Nach 4 Ziffern: PIN auf stdout, Exitcode 0. Abbrechen: Exitcode 1.
 */
#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>

#define LEN 4
static char pin[LEN + 1];
static int n;
static GtkWidget *dots[LEN];
static int result = 1;

static const char *css =
	"window { background-color: #0b0b10; }"
	".titel { color: #ffffff; font: bold 30px 'DejaVu Sans'; }"
	".hinweis { color: #ff5470; font: 18px 'DejaVu Sans'; }"
	".dot { min-width: 34px; min-height: 34px; border-radius: 50%; border: 3px solid #4931fd; background: transparent; margin: 0 10px; }"
	".dot.voll { background: #4931fd; box-shadow: 0 0 14px rgba(73,49,253,0.8); }"
	".taste { min-width: 150px; min-height: 104px; border-radius: 24px; border: 2px solid #2a2350;"
	"  background: #15122a; color: #ffffff; font: bold 46px 'DejaVu Sans'; box-shadow: none; text-shadow: none; }"
	".taste:active { background: #4931fd; border-color: #7a6bff; }"
	".link { background: transparent; border: none; box-shadow: none; color: #9b8cff; font: 18px 'DejaVu Sans'; }"
	".link:active { color: #ffffff; }";

static void update_dots(void)
{
	int i;
	for (i = 0; i < LEN; i++) {
		GtkStyleContext *c = gtk_widget_get_style_context(dots[i]);
		if (i < n) gtk_style_context_add_class(c, "voll");
		else gtk_style_context_remove_class(c, "voll");
	}
}

static gboolean finish(gpointer d)
{
	(void)d;
	printf("%s\n", pin);
	fflush(stdout);
	result = 0;
	gtk_main_quit();
	return G_SOURCE_REMOVE;
}

static void on_digit(GtkButton *b, gpointer d)
{
	(void)b;
	if (n >= LEN) return;
	pin[n++] = (char)GPOINTER_TO_INT(d);
	pin[n] = 0;
	update_dots();
	if (n == LEN) g_timeout_add(180, finish, NULL); /* letzter Punkt kurz sichtbar */
}

static void on_delete(GtkButton *b, gpointer d)
{
	(void)b; (void)d;
	if (n > 0) pin[--n] = 0;
	update_dots();
}

static void on_cancel(GtkButton *b, gpointer d)
{
	(void)b; (void)d;
	result = 1;
	gtk_main_quit();
}

static GtkWidget *label(const char *t, const char *cls)
{
	GtkWidget *l = gtk_label_new(t);
	gtk_style_context_add_class(gtk_widget_get_style_context(l), cls);
	gtk_label_set_line_wrap(GTK_LABEL(l), TRUE);
	gtk_label_set_justify(GTK_LABEL(l), GTK_JUSTIFY_CENTER);
	return l;
}

int main(int argc, char **argv)
{
	const char *titel = "PIN eingeben", *hinweis = "";
	GtkWidget *win, *root, *left, *dotbox, *grid, *links;
	GtkCssProvider *prov;
	int i;

	gtk_init(&argc, &argv);
	for (i = 1; i + 1 < argc; i++) {
		if (!strcmp(argv[i], "--titel")) titel = argv[++i];
		else if (!strcmp(argv[i], "--hinweis")) hinweis = argv[++i];
	}

	prov = gtk_css_provider_new();
	gtk_css_provider_load_from_data(prov, css, -1, NULL);
	gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
		GTK_STYLE_PROVIDER(prov), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

	win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(win), "LayerNX PIN");
	gtk_window_set_decorated(GTK_WINDOW(win), FALSE);
	gtk_window_set_keep_above(GTK_WINDOW(win), TRUE);
	gtk_window_set_skip_taskbar_hint(GTK_WINDOW(win), TRUE);
	gtk_window_fullscreen(GTK_WINDOW(win));
	g_signal_connect(win, "destroy", G_CALLBACK(gtk_main_quit), NULL);

	root = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_set_homogeneous(GTK_BOX(root), TRUE);
	gtk_container_add(GTK_CONTAINER(win), root);

	/* links: Titel, Punkte, Hinweis, Links */
	left = gtk_box_new(GTK_ORIENTATION_VERTICAL, 28);
	gtk_widget_set_valign(left, GTK_ALIGN_CENTER);
	gtk_widget_set_halign(left, GTK_ALIGN_CENTER);
	gtk_box_pack_start(GTK_BOX(left), label(titel, "titel"), FALSE, FALSE, 0);
	dotbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_set_halign(dotbox, GTK_ALIGN_CENTER);
	for (i = 0; i < LEN; i++) {
		dots[i] = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
		gtk_style_context_add_class(gtk_widget_get_style_context(dots[i]), "dot");
		gtk_box_pack_start(GTK_BOX(dotbox), dots[i], FALSE, FALSE, 0);
	}
	gtk_box_pack_start(GTK_BOX(left), dotbox, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(left), label(hinweis, "hinweis"), FALSE, FALSE, 0);
	links = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 30);
	gtk_widget_set_halign(links, GTK_ALIGN_CENTER);
	{
		GtkWidget *del = gtk_button_new_with_label("Löschen");
		GtkWidget *can = gtk_button_new_with_label("Abbrechen");
		gtk_style_context_add_class(gtk_widget_get_style_context(del), "link");
		gtk_style_context_add_class(gtk_widget_get_style_context(can), "link");
		g_signal_connect(del, "clicked", G_CALLBACK(on_delete), NULL);
		g_signal_connect(can, "clicked", G_CALLBACK(on_cancel), NULL);
		gtk_box_pack_start(GTK_BOX(links), del, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(links), can, FALSE, FALSE, 0);
	}
	gtk_box_pack_start(GTK_BOX(left), links, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(root), left, TRUE, TRUE, 0);

	/* rechts: 3x3 Tasten 1-9 */
	grid = gtk_grid_new();
	gtk_grid_set_row_spacing(GTK_GRID(grid), 18);
	gtk_grid_set_column_spacing(GTK_GRID(grid), 18);
	gtk_widget_set_valign(grid, GTK_ALIGN_CENTER);
	gtk_widget_set_halign(grid, GTK_ALIGN_CENTER);
	{
		/* PIN-Scrambling: Ziffern bei jedem Oeffnen zufaellig auf die Tasten verteilen,
		 * damit die PIN nicht an der Fingerposition abgelesen werden kann. */
		char ziffer[10];
		for (i = 0; i < 10; i++) ziffer[i] = (char)('0' + i);
		if (!getenv("LAYERNX_PINPAD_GEORDNET"))
			for (i = 9; i > 0; i--) {
				int j = g_random_int_range(0, i + 1);
				char t = ziffer[i]; ziffer[i] = ziffer[j]; ziffer[j] = t;
			}
		else { for (i = 0; i < 9; i++) ziffer[i] = (char)('1' + i); ziffer[9] = '0'; }
		for (i = 0; i < 10; i++) {
			char c = ziffer[i];
			char t[2] = { c, 0 };
			GtkWidget *b = gtk_button_new_with_label(t);
			gtk_style_context_add_class(gtk_widget_get_style_context(b), "taste");
			g_signal_connect(b, "clicked", G_CALLBACK(on_digit), GINT_TO_POINTER(c));
			if (i < 9) gtk_grid_attach(GTK_GRID(grid), b, i % 3, i / 3, 1, 1);
			else gtk_grid_attach(GTK_GRID(grid), b, 1, 3, 1, 1);
		}
	}
	gtk_box_pack_start(GTK_BOX(root), grid, TRUE, TRUE, 0);

	gtk_widget_show_all(win);
	gtk_window_present(GTK_WINDOW(win));
	gtk_main();
	return result;
}
