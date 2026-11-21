// SPDX-License-Identifier: MIT
/*
 * layernx-webview: minimaler WebKitGTK-Viewer fuer Web-Apps (LayerNX-Tablet-Linux)
 *   layernx-webview [--vollbild] [--tls-ignorieren] [--titel NAME] --profil DIR URL
 * - keine Bedienleisten, nur die Seite
 * - eigenes, dauerhaftes Profil: Cookies (SQLite), Local Storage, IndexedDB, Cache
 * - Sandbox aus: der Stock-Kernel hat keine Namespaces (CONFIG_NAMESPACES=n)
 */
#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>
#include <webkit2/webkit2.h>

static gboolean on_key(GtkWidget *w, GdkEventKey *e, gpointer d)
{
	(void)d;
	if (e->keyval == GDK_KEY_F5) { webkit_web_view_reload(WEBKIT_WEB_VIEW(gtk_bin_get_child(GTK_BIN(w)))); return TRUE; }
	return FALSE;
}

int main(int argc, char **argv)
{
	const char *url = NULL, *profil = NULL, *titel = "LayerNX Web-App";
	gboolean vollbild = FALSE, tls_ignorieren = FALSE;
	int i;

	setenv("WEBKIT_DISABLE_SANDBOX_THIS_IS_DANGEROUS", "1", 1);
	gtk_init(&argc, &argv);
	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--vollbild")) vollbild = TRUE;
		else if (!strcmp(argv[i], "--tls-ignorieren")) tls_ignorieren = TRUE;
		else if (!strcmp(argv[i], "--profil") && i + 1 < argc) profil = argv[++i];
		else if (!strcmp(argv[i], "--titel") && i + 1 < argc) titel = argv[++i];
		else url = argv[i];
	}
	if (!url || !profil) {
		g_printerr("usage: layernx-webview [--vollbild] [--titel NAME] --profil DIR URL\n");
		return 2;
	}

	gchar *data = g_build_filename(profil, "daten", NULL);
	gchar *cache = g_build_filename(profil, "cache", NULL);
	gchar *cookies = g_build_filename(profil, "cookies.sqlite", NULL);
	g_mkdir_with_parents(data, 0700);
	g_mkdir_with_parents(cache, 0700);

	WebKitWebsiteDataManager *dm = webkit_website_data_manager_new(
		"base-data-directory", data, "base-cache-directory", cache, NULL);
	if (tls_ignorieren) /* z. B. selbst signierte Zertifikate einer LAN-WebUI */
		webkit_website_data_manager_set_tls_errors_policy(dm, WEBKIT_TLS_ERRORS_POLICY_IGNORE);
	WebKitWebContext *ctx = webkit_web_context_new_with_website_data_manager(dm);
	webkit_cookie_manager_set_persistent_storage(webkit_website_data_manager_get_cookie_manager(dm),
		cookies, WEBKIT_COOKIE_PERSISTENT_STORAGE_SQLITE);
	webkit_cookie_manager_set_accept_policy(webkit_website_data_manager_get_cookie_manager(dm),
		WEBKIT_COOKIE_POLICY_ACCEPT_ALWAYS);

	GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(win), titel);
	gtk_window_set_default_size(GTK_WINDOW(win), 1280, 736);
	GtkWidget *view = webkit_web_view_new_with_context(ctx);
	WebKitSettings *s = webkit_web_view_get_settings(WEBKIT_WEB_VIEW(view));
	webkit_settings_set_enable_javascript(s, TRUE);
	webkit_settings_set_enable_html5_local_storage(s, TRUE);
	webkit_settings_set_enable_html5_database(s, TRUE);
	webkit_settings_set_enable_smooth_scrolling(s, FALSE);
	webkit_settings_set_hardware_acceleration_policy(s, WEBKIT_HARDWARE_ACCELERATION_POLICY_NEVER);
	gtk_container_add(GTK_CONTAINER(win), view);
	g_signal_connect(win, "destroy", G_CALLBACK(gtk_main_quit), NULL);
	g_signal_connect(win, "key-press-event", G_CALLBACK(on_key), NULL);
	if (vollbild) {
		gtk_window_set_decorated(GTK_WINDOW(win), FALSE);
		gtk_window_fullscreen(GTK_WINDOW(win));
	} else {
		gtk_window_maximize(GTK_WINDOW(win));
	}
	webkit_web_view_load_uri(WEBKIT_WEB_VIEW(view), url);
	gtk_widget_show_all(win);
	gtk_main();
	return 0;
}
