// SPDX-License-Identifier: MIT
/*
 * layernx-einstellungen: Touch-Einstellungen fuer LayerNX-Tablet-Linux
 * Seiten: WLAN, Bluetooth, Display, Ton, Second Screen, Geraet, Akku, Dienste, System.
 * WLAN ueber wpa_cli (Gruppe wheel), Root-Aktionen ueber layernx-einstellungen-helfer.
 */
#include <gtk/gtk.h>
#include <glib/gstdio.h>
#include <stdlib.h>
#include <string.h>

static GtkWidget *pw_wer, *pw1, *pw2, *pw_msg;
static GtkWidget *wlan_status, *wlan_list, *host_entry, *host_msg, *akku_status, *akku_switch,
	*akku_max, *ts_status, *ts_link, *ssh_switch, *sys_info, *bright;

static const char *css =
	"window, .seite { background: #0b0b10; color: #eceff4; }"
	"stacksidebar { background: #121022; min-width: 230px; }"
	"stacksidebar row { padding: 18px 16px; font: bold 19px 'DejaVu Sans'; color: #c9c4ff; }"
	"stacksidebar row:selected { background: #4931fd; color: #ffffff; }"
	".titel { font: bold 28px 'DejaVu Sans'; color: #ffffff; margin-bottom: 6px; }"
	".text { font: 17px 'DejaVu Sans'; color: #d8dee9; }"
	".hinweis { font: 15px 'DejaVu Sans'; color: #9b8cff; }"
	"button { min-height: 54px; padding: 0 22px; border-radius: 14px; border: 2px solid #2a2350;"
	"  background: #1a1630; color: #ffffff; font: bold 17px 'DejaVu Sans'; box-shadow: none; text-shadow: none; }"
	"button:active, button.primaer { background: #4931fd; border-color: #7a6bff; }"
	"entry { min-height: 52px; font: 18px 'DejaVu Sans'; border-radius: 12px; background: #15122a;"
	"  color: #ffffff; border: 2px solid #2a2350; padding: 0 12px; }"
	"list, row { background: #0b0b10; }"
	"list row { padding: 14px 10px; border-bottom: 1px solid #1e1a3a; font: 18px 'DejaVu Sans'; }"
	"list row label { color: #eceff4; font: 19px 'DejaVu Sans'; }"
	"list row:selected { background: #1e1a3a; }"
	"switch { min-width: 70px; min-height: 36px; }"
	"switch:checked { background: #4931fd; }"
	"scale trough { min-height: 14px; background: #1e1a3a; border-radius: 7px; }"
	"scale highlight { background: #4931fd; border-radius: 7px; }"
	"scale slider { min-width: 38px; min-height: 38px; border-radius: 19px; background: #ffffff; }";

/* ---------- Hilfsfunktionen ---------- */
static char *run(const char *cmd)
{
	char *out = NULL;
	if (!g_spawn_command_line_sync(cmd, &out, NULL, NULL, NULL)) return g_strdup("");
	return out ? g_strstrip(out) : g_strdup("");
}
static void run_bg(const char *cmd) { g_spawn_command_line_async(cmd, NULL); }

static GtkWidget *lbl(const char *t, const char *cls)
{
	GtkWidget *l = gtk_label_new(t);
	gtk_label_set_xalign(GTK_LABEL(l), 0);
	gtk_label_set_line_wrap(GTK_LABEL(l), TRUE);
	gtk_label_set_selectable(GTK_LABEL(l), FALSE);
	gtk_style_context_add_class(gtk_widget_get_style_context(l), cls);
	return l;
}
static GtkWidget *btn(const char *t, GCallback cb, gpointer d)
{
	GtkWidget *b = gtk_button_new_with_label(t);
	g_signal_connect(b, "clicked", cb, d);
	return b;
}
static GtkWidget *seite(const char *titel, GtkWidget **box)
{
	GtkWidget *sc = gtk_scrolled_window_new(NULL, NULL);
	*box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
	gtk_container_set_border_width(GTK_CONTAINER(*box), 28);
	gtk_style_context_add_class(gtk_widget_get_style_context(*box), "seite");
	gtk_box_pack_start(GTK_BOX(*box), lbl(titel, "titel"), FALSE, FALSE, 0);
	gtk_container_add(GTK_CONTAINER(sc), *box);
	return sc;
}
static GtkWidget *reihe(void) { return gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12); }
static void tastatur(GtkButton *b, gpointer d) { (void)b; (void)d; run_bg("layernx-tastatur umschalten"); }

/* ---------- WLAN ---------- */
static void wlan_status_update(void)
{
	char *s = run("wpa_cli -i wlan0 status");
	char *ip = run("moritz-v5x-status ip");
	const char *ssid = strstr(s, "\nssid=");
	char *txt;
	if (ssid) {
		char name[128] = "";
		sscanf(ssid + 6, "%127[^\n]", name);
		txt = g_strdup_printf("Verbunden mit „%s“ – IP %s", name, ip);
	} else txt = g_strdup_printf("Nicht verbunden (IP: %s)", ip);
	gtk_label_set_text(GTK_LABEL(wlan_status), txt);
	g_free(txt); g_free(s); g_free(ip);
}

static void wlan_list_fill(void)
{
	GList *ch = gtk_container_get_children(GTK_CONTAINER(wlan_list)), *l;
	char *res, **lines;
	int i;
	GHashTable *seen = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	for (l = ch; l; l = l->next) gtk_widget_destroy(GTK_WIDGET(l->data));
	g_list_free(ch);
	res = run("wpa_cli -i wlan0 scan_results");
	lines = g_strsplit(res, "\n", -1);
	for (i = 0; lines[i]; i++) {
		if (i == 0) continue; /* Kopfzeile "bssid / frequency / ..." (leere Antwort: Schleife endet sofort) */
		char **f = g_strsplit(lines[i], "\t", 5);
		if (g_strv_length(f) == 5 && *f[4] && !g_hash_table_contains(seen, f[4])) {
			int sig = atoi(f[2]);
			const char *bars = sig > -55 ? "▂▄▆█" : sig > -67 ? "▂▄▆" : sig > -78 ? "▂▄" : "▂";
			gboolean lock = strstr(f[3], "WPA") || strstr(f[3], "WEP") || strstr(f[3], "SAE");
			char *t = g_strdup_printf("%s   %s%s", f[4], bars, lock ? "   🔒" : "");
			GtkWidget *row = gtk_label_new(t);
			gtk_label_set_xalign(GTK_LABEL(row), 0);
			g_object_set_data_full(G_OBJECT(row), "ssid", g_strdup(f[4]), g_free);
			g_object_set_data(G_OBJECT(row), "lock", GINT_TO_POINTER(lock));
			gtk_list_box_insert(GTK_LIST_BOX(wlan_list), row, -1);
			g_hash_table_add(seen, g_strdup(f[4]));
			g_free(t);
		}
		g_strfreev(f);
	}
	g_strfreev(lines); g_free(res); g_hash_table_destroy(seen);
	gtk_widget_show_all(wlan_list);
}
static gboolean scan_done(gpointer d) { (void)d; wlan_list_fill(); wlan_status_update(); return G_SOURCE_REMOVE; }
static void wlan_scan(GtkButton *b, gpointer d)
{
	(void)b; (void)d;
	g_free(run("wpa_cli -i wlan0 scan"));
	gtk_label_set_text(GTK_LABEL(wlan_status), "Suche Netzwerke …");
	g_timeout_add(4000, scan_done, NULL);
}

static char *wpa(const char *a1, const char *a2, const char *a3, const char *a4)
{
	/* wpa_cli direkt mit Argumentliste aufrufen (kein Shell-Quoting -> Sonderzeichen sicher) */
	const char *argv[] = { "wpa_cli", "-i", "wlan0", a1, a2, a3, a4, NULL };
	char *out = NULL;
	if (!g_spawn_sync(NULL, (char **)argv, NULL, G_SPAWN_SEARCH_PATH | G_SPAWN_STDERR_TO_DEV_NULL, NULL, NULL, &out, NULL, NULL, NULL))
		return g_strdup("");
	return out ? g_strstrip(out) : g_strdup("");
}

static void wlan_connect(const char *ssid, const char *pw)
{
	char *id = wpa("add_network", NULL, NULL, NULL);
	char *qs = g_strdup_printf("\"%s\"", ssid);
	g_free(wpa("set_network", id, "ssid", qs));
	if (pw && *pw) {
		char *qp = g_strdup_printf("\"%s\"", pw);
		g_free(wpa("set_network", id, "psk", qp)); g_free(qp);
	} else g_free(wpa("set_network", id, "key_mgmt", "NONE"));
	g_free(wpa("select_network", id, NULL, NULL));
	g_free(wpa("save_config", NULL, NULL, NULL));
	g_free(id); g_free(qs);
	gtk_label_set_text(GTK_LABEL(wlan_status), "Verbinde …");
	g_timeout_add(7000, scan_done, NULL);
}

static void wlan_row(GtkListBox *lb, GtkListBoxRow *r, gpointer d)
{
	GtkWidget *row = gtk_bin_get_child(GTK_BIN(r));
	const char *ssid = g_object_get_data(G_OBJECT(row), "ssid");
	gboolean lock = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(row), "lock"));
	(void)lb; (void)d;
	if (!lock) { wlan_connect(ssid, NULL); return; }
	GtkWidget *dlg = gtk_dialog_new_with_buttons("WLAN-Passwort", GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(lb))),
		GTK_DIALOG_MODAL, "Abbrechen", GTK_RESPONSE_CANCEL, "Verbinden", GTK_RESPONSE_OK, NULL);
	GtkWidget *area = gtk_dialog_get_content_area(GTK_DIALOG(dlg)), *e = gtk_entry_new(), *r2 = reihe();
	char *t = g_strdup_printf("Passwort für „%s“:", ssid);
	gtk_container_set_border_width(GTK_CONTAINER(area), 20);
	gtk_box_pack_start(GTK_BOX(area), lbl(t, "text"), FALSE, FALSE, 8);
	gtk_entry_set_visibility(GTK_ENTRY(e), FALSE);
	gtk_entry_set_activates_default(GTK_ENTRY(e), TRUE);
	gtk_box_pack_start(GTK_BOX(r2), e, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(r2), btn("⌨", G_CALLBACK(tastatur), NULL), FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(area), r2, FALSE, FALSE, 8);
	gtk_window_set_default_size(GTK_WINDOW(dlg), 560, -1);
	gtk_dialog_set_default_response(GTK_DIALOG(dlg), GTK_RESPONSE_OK);
	gtk_widget_show_all(dlg);
	run_bg("layernx-tastatur an");
	if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_OK)
		wlan_connect(ssid, gtk_entry_get_text(GTK_ENTRY(e)));
	run_bg("layernx-tastatur aus");
	gtk_widget_destroy(dlg); g_free(t);
}
static void wlan_trennen(GtkButton *b, gpointer d) { (void)b; (void)d; g_free(run("wpa_cli -i wlan0 disconnect")); wlan_status_update(); }
static void wlan_verbinden(GtkButton *b, gpointer d) { (void)b; (void)d; g_free(run("wpa_cli -i wlan0 reconnect")); g_timeout_add(5000, scan_done, NULL); }
static void wlan_vergessen(GtkButton *b, gpointer d)
{
	(void)b; (void)d;
	char *s = run("wpa_cli -i wlan0 status"), *id = strstr(s, "\nid=");
	if (id) {
		char *cmd = g_strdup_printf("wpa_cli -i wlan0 remove_network %d", atoi(id + 4));
		g_free(run(cmd)); g_free(cmd); g_free(run("wpa_cli -i wlan0 save_config"));
	}
	g_free(s); wlan_status_update();
}


/* ---------- Firmen-WLAN (WPA2/3-Enterprise) ---------- */
typedef struct { GtkWidget *ssid, *eap, *ident, *pw, *ca, *cert, *key, *keypw, *domain, *msg; } Ent;
static GtkWidget *datei_knopf(const char *titel)
{
	GtkWidget *b = gtk_file_chooser_button_new(titel, GTK_FILE_CHOOSER_ACTION_OPEN);
	GtkFileFilter *f = gtk_file_filter_new();
	gtk_file_filter_set_name(f, "Zertifikate / Schlüssel");
	const char *muster[] = { "*.pem", "*.crt", "*.cer", "*.der", "*.key", "*.p12", "*.pfx", NULL };
	for (int i = 0; muster[i]; i++) gtk_file_filter_add_pattern(f, muster[i]);
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(b), f);
	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(b), g_get_home_dir());
	return b;
}
/* Datei als root nach /etc/wpa_supplicant/certs kopieren; liefert Zielpfad oder NULL */
static char *zert_install(GtkWidget *knopf, Ent *e)
{
	char *q = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(knopf));
	if (!q) return NULL;
	char *out = NULL, *err = NULL; int st = 1;
	gchar *argv[] = { "layernx-einstellungen-helfer", "zertifikat", q, NULL };
	g_spawn_sync(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, &out, &err, &st, NULL);
	g_free(q);
	if (st != 0 || !out || !*out) { gtk_label_set_text(GTK_LABEL(e->msg), err && *err ? err : "Zertifikat konnte nicht installiert werden."); g_free(out); g_free(err); return NULL; }
	g_free(err);
	return g_strstrip(out);
}
static void ent_set(const char *id, const char *k, const char *v, gboolean quote)
{
	if (!v || !*v) return;
	char *q = quote ? g_strdup_printf("\"%s\"", v) : g_strdup(v);
	g_free(wpa("set_network", id, k, q)); g_free(q);
}
static void ent_ok(GtkButton *b, gpointer d)
{
	(void)b;
	Ent *e = d;
	const char *ssid = gtk_entry_get_text(GTK_ENTRY(e->ssid));
	char *eap = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(e->eap));
	if (!*ssid || !eap) { gtk_label_set_text(GTK_LABEL(e->msg), "Bitte Netzwerkname und Methode angeben."); g_free(eap); return; }
	gboolean tls = g_str_has_prefix(eap, "TLS");
	char *ca = zert_install(e->ca, e), *cert = tls ? zert_install(e->cert, e) : NULL, *key = tls ? zert_install(e->key, e) : NULL;
	if (tls && !cert && !key) { gtk_label_set_text(GTK_LABEL(e->msg), "EAP-TLS braucht ein Client-Zertifikat (.p12/.pfx oder .pem + Schlüssel)."); goto ende; }
	{
		char *id = wpa("add_network", NULL, NULL, NULL);
		ent_set(id, "ssid", ssid, TRUE);
		ent_set(id, "key_mgmt", "WPA-EAP WPA-EAP-SHA256", FALSE);
		ent_set(id, "ieee80211w", "1", FALSE);
		ent_set(id, "eap", tls ? "TLS" : (g_str_has_prefix(eap, "TTLS") ? "TTLS" : "PEAP"), FALSE);
		ent_set(id, "identity", gtk_entry_get_text(GTK_ENTRY(e->ident)), TRUE);
		if (!tls) {
			ent_set(id, "password", gtk_entry_get_text(GTK_ENTRY(e->pw)), TRUE);
			ent_set(id, "phase2", "auth=MSCHAPV2", TRUE);
		}
		ent_set(id, "ca_cert", ca, TRUE);
		ent_set(id, "domain_suffix_match", gtk_entry_get_text(GTK_ENTRY(e->domain)), TRUE);
		if (tls) {
			gboolean p12 = (cert && (g_str_has_suffix(cert, ".p12") || g_str_has_suffix(cert, ".pfx")));
			if (p12) ent_set(id, "private_key", cert, TRUE);
			else { ent_set(id, "client_cert", cert, TRUE); ent_set(id, "private_key", key ? key : cert, TRUE); }
			ent_set(id, "private_key_passwd", gtk_entry_get_text(GTK_ENTRY(e->keypw)), TRUE);
		}
		g_free(wpa("select_network", id, NULL, NULL));
		g_free(wpa("save_config", NULL, NULL, NULL));
		g_free(id);
		gtk_label_set_text(GTK_LABEL(wlan_status), "Verbinde (Firmen-WLAN) …");
		g_timeout_add(9000, scan_done, NULL);
		gtk_widget_destroy(gtk_widget_get_toplevel(GTK_WIDGET(e->msg)));
	}
ende:
	g_free(eap); g_free(ca); g_free(cert); g_free(key);
}
static void ent_eap_changed(GtkComboBox *c, gpointer d)
{
	Ent *e = d;
	gboolean tls = gtk_combo_box_get_active(c) == 2;
	gtk_widget_set_sensitive(e->pw, !tls);
	gtk_widget_set_sensitive(e->cert, tls); gtk_widget_set_sensitive(e->key, tls); gtk_widget_set_sensitive(e->keypw, tls);
}
static void wlan_firma(GtkButton *b, gpointer d)
{
	(void)b; (void)d;
	static Ent e;
	GtkWidget *w = gtk_window_new(GTK_WINDOW_TOPLEVEL), *g = gtk_grid_new(), *r = reihe();
	int z = 0;
	gtk_window_set_title(GTK_WINDOW(w), "Firmen-WLAN");
	gtk_window_set_default_size(GTK_WINDOW(w), 1000, 640);
	gtk_container_set_border_width(GTK_CONTAINER(w), 18);
	gtk_grid_set_row_spacing(GTK_GRID(g), 8); gtk_grid_set_column_spacing(GTK_GRID(g), 12);
#define ZEILE(text, widget) do { gtk_grid_attach(GTK_GRID(g), lbl(text, "text"), 0, z, 1, 1); gtk_widget_set_hexpand(widget, TRUE); gtk_grid_attach(GTK_GRID(g), widget, 1, z++, 1, 1); } while (0)
	e.ssid = gtk_entry_new(); ZEILE("Netzwerkname (SSID)", e.ssid);
	e.eap = gtk_combo_box_text_new();
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(e.eap), "PEAP (Benutzer + Passwort)");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(e.eap), "TTLS (Benutzer + Passwort)");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(e.eap), "TLS (Client-Zertifikat)");
	gtk_combo_box_set_active(GTK_COMBO_BOX(e.eap), 0);
	ZEILE("Methode", e.eap);
	e.ident = gtk_entry_new(); ZEILE("Identität / Benutzer", e.ident);
	e.pw = gtk_entry_new(); gtk_entry_set_visibility(GTK_ENTRY(e.pw), FALSE); ZEILE("Passwort", e.pw);
	e.ca = datei_knopf("CA-Zertifikat"); ZEILE("CA-Zertifikat (empfohlen)", e.ca);
	e.domain = gtk_entry_new(); gtk_entry_set_placeholder_text(GTK_ENTRY(e.domain), "z. B. radius.firma.de (optional)"); ZEILE("Server-Domain", e.domain);
	e.cert = datei_knopf("Client-Zertifikat"); ZEILE("Client-Zertifikat (.p12/.pem)", e.cert);
	e.key = datei_knopf("Privater Schlüssel"); ZEILE("Privater Schlüssel (bei .pem)", e.key);
	e.keypw = gtk_entry_new(); gtk_entry_set_visibility(GTK_ENTRY(e.keypw), FALSE); ZEILE("Schlüssel-Passwort", e.keypw);
#undef ZEILE
	e.msg = lbl("Zertifikate werden nach /etc/wpa_supplicant/certs kopiert (nur root lesbar). Dateien z. B. per USB-Stick oder scp ins Home-Verzeichnis legen.", "hinweis");
	gtk_label_set_line_wrap(GTK_LABEL(e.msg), TRUE);
	g_signal_connect(e.eap, "changed", G_CALLBACK(ent_eap_changed), &e);
	ent_eap_changed(GTK_COMBO_BOX(e.eap), &e);
	GtkWidget *v = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
	gtk_box_pack_start(GTK_BOX(v), g, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(v), e.msg, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(r), btn("⌨", G_CALLBACK(tastatur), NULL), FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(r), btn("Verbinden", G_CALLBACK(ent_ok), &e), FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(v), r, FALSE, FALSE, 0);
	/* scrollbar, damit das Fenster ueber die Bildschirmtastatur schrumpfen kann */
	GtkWidget *sc = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sc), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(sc), v);
	gtk_container_add(GTK_CONTAINER(w), sc);
	gtk_widget_show_all(w);
}
static GtkWidget *page_wlan(void)
{
	GtkWidget *box, *pg = seite("WLAN", &box), *r = reihe(), *sc;
	wlan_status = lbl("", "text");
	gtk_box_pack_start(GTK_BOX(box), wlan_status, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(r), btn("Netzwerke suchen", G_CALLBACK(wlan_scan), NULL), FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(r), btn("Verbinden", G_CALLBACK(wlan_verbinden), NULL), FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(r), btn("Trennen", G_CALLBACK(wlan_trennen), NULL), FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(r), btn("Aktuelles vergessen", G_CALLBACK(wlan_vergessen), NULL), FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), r, FALSE, FALSE, 0);
	{ GtkWidget *r2 = reihe(); gtk_box_pack_start(GTK_BOX(r2), btn("Firmen-WLAN mit Zertifikat (PEAP/TTLS/TLS) …", G_CALLBACK(wlan_firma), NULL), FALSE, FALSE, 0);
	  gtk_box_pack_start(GTK_BOX(box), r2, FALSE, FALSE, 0); }
	gtk_box_pack_start(GTK_BOX(box), lbl("Netzwerk antippen, um zu verbinden:", "hinweis"), FALSE, FALSE, 0);
	wlan_list = gtk_list_box_new();
	g_signal_connect(wlan_list, "row-activated", G_CALLBACK(wlan_row), NULL);
	sc = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_set_size_request(sc, -1, 380);
	gtk_container_add(GTK_CONTAINER(sc), wlan_list);
	gtk_box_pack_start(GTK_BOX(box), sc, TRUE, TRUE, 0);
	wlan_status_update(); wlan_list_fill();
	return pg;
}

/* ---------- Bluetooth ---------- */
static GtkWidget *page_bt(void)
{
	GtkWidget *box, *pg = seite("Bluetooth", &box);
	gtk_box_pack_start(GTK_BOX(box), lbl("Bluetooth ist auf diesem Gerät derzeit nicht verfügbar.", "text"), FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), lbl("Der originale Hersteller-Kernel wurde ohne Linux-Bluetooth-Unterstützung (CONFIG_BT) gebaut. "
		"Der Bluetooth-Chip (MT6631) ist vorhanden, kann aber nur über einen eigenen Userspace-Stack angesprochen werden. "
		"Diese Seite wird erweitert, sobald das umgesetzt ist (Maus, Tastatur, Lautsprecher).", "hinweis"), FALSE, FALSE, 0);
	return pg;
}

/* ---------- Display ---------- */
static void bright_changed(GtkRange *r, gpointer d)
{
	(void)d;
	char *cmd = g_strdup_printf("layernx-display brightness %d", (int)gtk_range_get_value(r));
	run_bg(cmd); g_free(cmd);
}
static void display_aus(GtkButton *b, gpointer d) { (void)b; (void)d; run_bg("layernx-display off"); }
static GtkWidget *page_display(void)
{
	GtkWidget *box, *pg = seite("Display", &box);
	char *st = run("layernx-display status");
	int cur = 77;
	char *p = strstr(st, "Stufe ");
	if (p) cur = atoi(p + 6);
	gtk_box_pack_start(GTK_BOX(box), lbl("Helligkeit (0–99):", "text"), FALSE, FALSE, 0);
	bright = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 99, 1);
	gtk_range_set_value(GTK_RANGE(bright), cur);
	g_signal_connect(bright, "value-changed", G_CALLBACK(bright_changed), NULL);
	gtk_box_pack_start(GTK_BOX(box), bright, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), lbl("Das Display dimmt und schaltet sich nie von selbst ab. "
		"Ein-/Ausschalten: Power-Taste kurz drücken (nur auf dem Homescreen).", "hinweis"), FALSE, FALSE, 0);
	{ GtkWidget *r = reihe(); gtk_box_pack_start(GTK_BOX(r), btn("Display jetzt ausschalten", G_CALLBACK(display_aus), NULL), FALSE, FALSE, 0);
	  gtk_box_pack_start(GTK_BOX(box), r, FALSE, FALSE, 0); }
	g_free(st);
	return pg;
}

static gchar *mic_aus_datei(void) { return g_build_filename(g_get_home_dir(), ".config", "layernx", "mikrofon-aus", NULL); }
static void mic_set(GtkWidget *w, GParamSpec *ps, gpointer d)
{
	(void)ps; (void)d;
	gchar *f = mic_aus_datei();
	if (gtk_switch_get_active(GTK_SWITCH(w))) g_unlink(f);
	else {
		gchar *dir = g_path_get_dirname(f);
		g_mkdir_with_parents(dir, 0700); g_file_set_contents(f, "", 0, NULL); g_free(dir);
	}
	g_free(f);
}

/* ---------- Ton ---------- */
static GtkWidget *ton_scale, *ton_stumm;
static gboolean ton_lade = FALSE;
static int pactl_zahl(const char *cmd)
{
	char *o = run(cmd), *p = strchr(o, '%');
	int v = -1;
	if (p) { while (p > o && g_ascii_isdigit(p[-1])) p--; v = atoi(p); }
	g_free(o);
	return v;
}
static void ton_changed(GtkRange *r, gpointer d)
{
	(void)d;
	if (ton_lade) return;
	char *cmd = g_strdup_printf("pactl set-sink-volume @DEFAULT_SINK@ %d%%", (int)gtk_range_get_value(r));
	run_bg(cmd); g_free(cmd);
}
static void stumm_set(GtkWidget *w, GParamSpec *ps, gpointer d)
{
	(void)ps; (void)d;
	if (ton_lade) return;
	run_bg(gtk_switch_get_active(GTK_SWITCH(w)) ? "pactl set-sink-mute @DEFAULT_SINK@ 1" : "pactl set-sink-mute @DEFAULT_SINK@ 0");
}
static void ton_test(GtkButton *b, gpointer d) { (void)b; (void)d; run_bg("paplay /usr/share/layernx/ton-test.wav"); }
static GtkWidget *page_ton(void)
{
	GtkWidget *box, *pg = seite("Ton", &box), *rs = reihe(), *rt = reihe();
	int v = pactl_zahl("pactl get-sink-volume @DEFAULT_SINK@");
	char *m = run("pactl get-sink-mute @DEFAULT_SINK@");
	ton_lade = TRUE;
	gtk_box_pack_start(GTK_BOX(box), lbl("Lautstärke Lautsprecher (0–100 %):", "text"), FALSE, FALSE, 0);
	ton_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 100, 1);
	gtk_range_set_value(GTK_RANGE(ton_scale), v >= 0 ? v : 70);
	g_signal_connect(ton_scale, "value-changed", G_CALLBACK(ton_changed), NULL);
	gtk_box_pack_start(GTK_BOX(box), ton_scale, FALSE, FALSE, 0);
	ton_stumm = gtk_switch_new();
	gtk_switch_set_active(GTK_SWITCH(ton_stumm), strstr(m, "yes") != NULL);
	g_signal_connect(ton_stumm, "notify::active", G_CALLBACK(stumm_set), NULL);
	gtk_box_pack_start(GTK_BOX(rs), lbl("Stumm", "text"), FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(rs), ton_stumm, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), rs, FALSE, FALSE, 12);
	gtk_box_pack_start(GTK_BOX(rt), btn("Testton abspielen", G_CALLBACK(ton_test), NULL), FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), rt, FALSE, FALSE, 0);
	if (v < 0) gtk_box_pack_start(GTK_BOX(box), lbl("Tondienst nicht erreichbar (rc-service layernx-pulse status).", "hinweis"), FALSE, FALSE, 0);
	ton_lade = FALSE;
	{
		GtkWidget *rm = reihe(), *sw = gtk_switch_new();
		gchar *f = mic_aus_datei();
		gtk_switch_set_active(GTK_SWITCH(sw), !g_file_test(f, G_FILE_TEST_EXISTS));
		g_free(f);
		g_signal_connect(sw, "notify::active", G_CALLBACK(mic_set), NULL);
		gtk_box_pack_start(GTK_BOX(rm), lbl("Mikrofon für Web-Apps erlauben", "text"), FALSE, FALSE, 0);
		gtk_box_pack_end(GTK_BOX(rm), sw, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(box), rm, FALSE, FALSE, 12);
		gtk_box_pack_start(GTK_BOX(box), lbl("Standard: an. Web-Apps bekommen nur das Mikrofon, sonst keine Rechte. Gilt ab dem nächsten Start der Web-App.", "hinweis"), FALSE, FALSE, 0);
	}
	g_free(m);
	return pg;
}

/* ---------- Second Screen ---------- */
static GtkWidget *ss_switch, *ss_pc, *ss_msg;
static gboolean ss_lade;
static void ss_update(void)
{
	char *st = run("layernx-einstellungen-helfer second-screen status"), *pc = NULL;
	ss_lade = TRUE;
	gtk_switch_set_active(GTK_SWITCH(ss_switch), g_str_has_prefix(st, "an"));
	ss_lade = FALSE;
	if (g_file_get_contents("/etc/layernx/second-screen-pc", &pc, NULL, NULL) && *pc) {
		char *name = strchr(g_strstrip(pc), ' ');
		char *t = g_strdup_printf("Gekoppelter PC: %s (nur dieser darf sich verbinden)", name ? name + 1 : pc);
		gtk_label_set_text(GTK_LABEL(ss_pc), t); g_free(t);
	} else gtk_label_set_text(GTK_LABEL(ss_pc), "Noch kein PC gekoppelt – der erste, der sich verbindet, wird gemerkt.");
	g_free(st); g_free(pc);
}
static void ss_set(GtkWidget *w, GParamSpec *ps, gpointer d)
{
	(void)ps; (void)d;
	if (ss_lade) return;
	char *o = run(gtk_switch_get_active(GTK_SWITCH(w)) ? "layernx-einstellungen-helfer second-screen an" : "layernx-einstellungen-helfer second-screen aus");
	gtk_label_set_text(GTK_LABEL(ss_msg), o); g_free(o); ss_update();
}
static void ss_vergessen(GtkButton *b, gpointer d)
{
	(void)b; (void)d;
	char *o = run("layernx-einstellungen-helfer second-screen vergessen");
	gtk_label_set_text(GTK_LABEL(ss_msg), o); g_free(o); ss_update();
}
static GtkWidget *page_second_screen(void)
{
	GtkWidget *box, *pg = seite("Second Screen", &box), *r = reihe(), *r2 = reihe();
	char *h = run("hostname"), *t;
	ss_switch = gtk_switch_new();
	g_signal_connect(ss_switch, "notify::active", G_CALLBACK(ss_set), NULL);
	gtk_box_pack_start(GTK_BOX(r), lbl("Second Screen (drahtloser Bildschirm für Windows)", "text"), FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(r), ss_switch, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), r, FALSE, FALSE, 0);
	t = g_strdup_printf("Am PC: Win+K → „%s“ wählen. Mit Win+P „Erweitern“ oder „Duplizieren“ wählen. "
		"Kein gemeinsames WLAN nötig (Wi-Fi Direct). Nur Bild, kein Ton.", h);
	gtk_box_pack_start(GTK_BOX(box), lbl(t, "hinweis"), FALSE, FALSE, 0);
	ss_pc = lbl("", "text");
	gtk_box_pack_start(GTK_BOX(box), ss_pc, FALSE, FALSE, 12);
	gtk_box_pack_start(GTK_BOX(r2), btn("Rechner vergessen", G_CALLBACK(ss_vergessen), NULL), FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), r2, FALSE, FALSE, 0);
	ss_msg = lbl("Der Name in Windows ist der Gerätename (Einstellungen → Gerät).", "hinweis");
	gtk_box_pack_start(GTK_BOX(box), ss_msg, FALSE, FALSE, 0);
	ss_update();
	g_free(h); g_free(t);
	return pg;
}

/* ---------- Geraet ---------- */
static void host_set(GtkButton *b, gpointer d)
{
	(void)b; (void)d;
	char *cmd = g_strdup_printf("layernx-einstellungen-helfer hostname %s", gtk_entry_get_text(GTK_ENTRY(host_entry)));
	char *out = NULL, *err = NULL; int st = 0;
	g_spawn_command_line_sync(cmd, &out, &err, &st, NULL);
	gtk_label_set_text(GTK_LABEL(host_msg), st == 0 ? "Gespeichert. Wirkt sofort, Anzeige nach Neustart überall." : (err && *err ? err : "Fehler"));
	g_free(cmd); g_free(out); g_free(err);
}
static void pin_aendern(GtkButton *b, gpointer d) { (void)b; (void)d; run_bg("layernx-pin aendern"); }
static void pin_entfernen(GtkButton *b, gpointer d) { (void)b; (void)d; run_bg("layernx-pin entfernen"); }
static void pw_set(GtkButton *b, gpointer d)
{
	(void)b; (void)d;
	const char *a = gtk_entry_get_text(GTK_ENTRY(pw1)), *c = gtk_entry_get_text(GTK_ENTRY(pw2));
	if (!*a) { gtk_label_set_text(GTK_LABEL(pw_msg), "Bitte ein Passwort eingeben."); return; }
	if (strcmp(a, c)) { gtk_label_set_text(GTK_LABEL(pw_msg), "Die Passwörter stimmen nicht überein."); return; }
	const char *wer = gtk_combo_box_get_active(GTK_COMBO_BOX(pw_wer)) == 1 ? "root" : "benutzer";
	GError *e = NULL; char *out = NULL, *err = NULL, *in = g_strdup_printf("%s\n", a);
	GSubprocess *sp = g_subprocess_new(G_SUBPROCESS_FLAGS_STDIN_PIPE | G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_PIPE,
		&e, "layernx-einstellungen-helfer", "passwort", wer, NULL);
	if (sp && g_subprocess_communicate_utf8(sp, in, NULL, &out, &err, &e) && g_subprocess_get_successful(sp))
		gtk_label_set_text(GTK_LABEL(pw_msg), out && *out ? g_strstrip(out) : "Passwort geändert.");
	else gtk_label_set_text(GTK_LABEL(pw_msg), err && *err ? err : "Fehler beim Ändern.");
	gtk_entry_set_text(GTK_ENTRY(pw1), ""); gtk_entry_set_text(GTK_ENTRY(pw2), "");
	g_free(in); g_free(out); g_free(err); if (e) g_error_free(e); if (sp) g_object_unref(sp);
}

static GtkWidget *page_geraet(void)
{
	GtkWidget *box, *pg = seite("Gerät", &box), *r = reihe(), *r2 = reihe();
	char *h = run("hostname");
	gtk_box_pack_start(GTK_BOX(box), lbl("Gerätename (Hostname):", "text"), FALSE, FALSE, 0);
	host_entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(host_entry), h);
	gtk_box_pack_start(GTK_BOX(r), host_entry, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(r), btn("⌨", G_CALLBACK(tastatur), NULL), FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(r), btn("Übernehmen", G_CALLBACK(host_set), NULL), FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), r, FALSE, FALSE, 0);
	host_msg = lbl("Erlaubt: Buchstaben, Ziffern, Bindestrich.", "hinweis");
	gtk_box_pack_start(GTK_BOX(box), host_msg, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), lbl("PIN zum Verlassen von Vollbild-Apps (5× Power-Taste):", "text"), FALSE, FALSE, 12);
	gtk_box_pack_start(GTK_BOX(r2), btn("PIN ändern …", G_CALLBACK(pin_aendern), NULL), FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(r2), btn("PIN entfernen …", G_CALLBACK(pin_entfernen), NULL), FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), r2, FALSE, FALSE, 0);
	{
		GtkWidget *r3 = reihe(), *r4 = reihe(), *r5 = reihe();
		gtk_box_pack_start(GTK_BOX(box), lbl("Passwort ändern:", "text"), FALSE, FALSE, 12);
		pw_wer = gtk_combo_box_text_new();
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(pw_wer), "Benutzer (Anmeldung, SSH, doas)");
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(pw_wer), "root (Standard: lnx)");
		gtk_combo_box_set_active(GTK_COMBO_BOX(pw_wer), 0);
		gtk_box_pack_start(GTK_BOX(r3), pw_wer, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(r3), btn("⌨", G_CALLBACK(tastatur), NULL), FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(box), r3, FALSE, FALSE, 0);
		pw1 = gtk_entry_new(); pw2 = gtk_entry_new();
		gtk_entry_set_visibility(GTK_ENTRY(pw1), FALSE); gtk_entry_set_visibility(GTK_ENTRY(pw2), FALSE);
		gtk_entry_set_placeholder_text(GTK_ENTRY(pw1), "neues Passwort");
		gtk_entry_set_placeholder_text(GTK_ENTRY(pw2), "wiederholen");
		gtk_box_pack_start(GTK_BOX(r4), pw1, TRUE, TRUE, 0);
		gtk_box_pack_start(GTK_BOX(r4), pw2, TRUE, TRUE, 0);
		gtk_box_pack_start(GTK_BOX(box), r4, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(r5), btn("Passwort speichern", G_CALLBACK(pw_set), NULL), FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(box), r5, FALSE, FALSE, 0);
		pw_msg = lbl("Ab Werk: root / lnx (auch per SSH) – bitte ändern.", "hinweis");
		gtk_box_pack_start(GTK_BOX(box), pw_msg, FALSE, FALSE, 0);
	}
	g_free(h);
	return pg;
}

/* ---------- Akku ---------- */
static void akku_update(void)
{
	char *s = run("layernx-akku status");
	gtk_label_set_text(GTK_LABEL(akku_status), s);
	g_free(s);
}
static void akku_set(GtkWidget *w, gpointer d)
{
	(void)w; (void)d;
	char *cmd;
	if (gtk_switch_get_active(GTK_SWITCH(akku_switch)))
		cmd = g_strdup_printf("layernx-akku schutz an %d", (int)gtk_range_get_value(GTK_RANGE(akku_max)));
	else cmd = g_strdup("layernx-akku schutz aus");
	g_free(run(cmd)); g_free(cmd); akku_update();
}
static GtkWidget *page_akku(void)
{
	GtkWidget *box, *pg = seite("Akku & Strom", &box), *r = reihe();
	char *s = run("layernx-akku status");
	akku_status = lbl(s, "text");
	gtk_box_pack_start(GTK_BOX(box), akku_status, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), lbl("Akkuschutz für Dauerbetrieb am Netzteil (Server): Laden stoppt bei der Obergrenze "
		"und startet 5 % darunter wieder. Der Akkustand ist eine Schätzung.", "hinweis"), FALSE, FALSE, 0);
	akku_switch = gtk_switch_new();
	gtk_switch_set_active(GTK_SWITCH(akku_switch), strstr(s, "Akkuschutz: an") != NULL);
	gtk_box_pack_start(GTK_BOX(r), lbl("Akkuschutz", "text"), FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(r), akku_switch, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), r, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), lbl("Obergrenze in %:", "text"), FALSE, FALSE, 0);
	akku_max = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 30, 100, 5);
	gtk_range_set_value(GTK_RANGE(akku_max), 50);
	gtk_box_pack_start(GTK_BOX(box), akku_max, FALSE, FALSE, 0);
	{ GtkWidget *r2 = reihe(); gtk_box_pack_start(GTK_BOX(r2), btn("Übernehmen", G_CALLBACK(akku_set), NULL), FALSE, FALSE, 0);
	  gtk_box_pack_start(GTK_BOX(box), r2, FALSE, FALSE, 0); }
	g_free(s);
	return pg;
}

/* ---------- Dienste ---------- */
static void ts_update(void) { char *s = run("layernx-einstellungen-helfer tailscale status"); gtk_label_set_text(GTK_LABEL(ts_status), s); g_free(s); }
static void ts_start(GtkButton *b, gpointer d) { (void)b; (void)d; g_free(run("layernx-einstellungen-helfer tailscale start")); ts_update(); }
static void ts_stop(GtkButton *b, gpointer d) { (void)b; (void)d; g_free(run("layernx-einstellungen-helfer tailscale stop")); ts_update(); }
static void ts_up(GtkButton *b, gpointer d)
{
	(void)b; (void)d;
	gtk_label_set_text(GTK_LABEL(ts_link), "Hole Anmelde-Link …");
	while (gtk_events_pending()) gtk_main_iteration();
	char *l = run("layernx-einstellungen-helfer tailscale up");
	char *t = *l ? g_strdup_printf("Diesen Link auf einem anderen Gerät öffnen und anmelden:\n%s", l) : g_strdup("Bereits angemeldet oder kein Link erhalten (Status prüfen).");
	gtk_label_set_text(GTK_LABEL(ts_link), t); g_free(t); g_free(l); ts_update();
}
static void ssh_set(GObject *o, GParamSpec *p, gpointer d)
{
	(void)p; (void)d;
	g_free(run(gtk_switch_get_active(GTK_SWITCH(o)) ? "layernx-einstellungen-helfer ssh an" : "layernx-einstellungen-helfer ssh aus"));
}
static GtkWidget *page_dienste(void)
{
	GtkWidget *box, *pg = seite("Dienste", &box), *r = reihe(), *r2 = reihe();
	char *ssh = run("sh -c 'rc-service sshd status >/dev/null 2>&1 && echo an || echo aus'");
	gtk_box_pack_start(GTK_BOX(r), lbl("SSH-Zugang", "text"), FALSE, FALSE, 0);
	ssh_switch = gtk_switch_new();
	gtk_switch_set_active(GTK_SWITCH(ssh_switch), !strcmp(ssh, "an"));
	g_signal_connect(ssh_switch, "notify::active", G_CALLBACK(ssh_set), NULL);
	gtk_box_pack_start(GTK_BOX(r), ssh_switch, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), r, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), lbl("Tailscale:", "text"), FALSE, FALSE, 12);
	ts_status = lbl("", "hinweis");
	gtk_box_pack_start(GTK_BOX(box), ts_status, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(r2), btn("Starten", G_CALLBACK(ts_start), NULL), FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(r2), btn("Anmelden", G_CALLBACK(ts_up), NULL), FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(r2), btn("Stoppen", G_CALLBACK(ts_stop), NULL), FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), r2, FALSE, FALSE, 0);
	ts_link = lbl("", "text");
	gtk_label_set_selectable(GTK_LABEL(ts_link), TRUE);
	gtk_box_pack_start(GTK_BOX(box), ts_link, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), lbl("cloudflared ist installiert. Einrichtung per Terminal: „cloudflared tunnel login“.", "hinweis"), FALSE, FALSE, 12);
	ts_update(); g_free(ssh);
	return pg;
}

/* ---------- System ---------- */
static void sys_update(GtkButton *b, gpointer d) { (void)b; (void)d; run_bg("lxterminal -t 'LayerNX Update' -e sh -c 'layernx-update; echo; echo Fertig – Fenster schliessen.; sleep 3600'"); }
static void sys_reboot(GtkButton *b, gpointer d) { (void)b; (void)d; run_bg("moritz-v5x-power reboot"); }
static void sys_off(GtkButton *b, gpointer d) { (void)b; (void)d; run_bg("moritz-v5x-power poweroff"); }
static GtkWidget *page_system(void)
{
	GtkWidget *box, *pg = seite("System", &box), *r = reihe();
	char *os = run("sh -c '. /etc/os-release; echo $PRETTY_NAME'"), *hn = run("hostname"), *ip = run("moritz-v5x-status ip"),
		*kr = run("uname -r"), *df = run("sh -c \"df -h / | tail -1\"");
	char *i = g_strdup_printf("%s\nHostname: %s\nIP: %s\nKernel: %s\nSpeicher (/): %s", os, hn, ip, kr, df);
	g_free(os); g_free(hn); g_free(ip); g_free(kr); g_free(df);
	sys_info = lbl(i, "text");
	gtk_label_set_selectable(GTK_LABEL(sys_info), TRUE);
	gtk_box_pack_start(GTK_BOX(box), sys_info, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(r), btn("Alles aktualisieren", G_CALLBACK(sys_update), NULL), FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(r), btn("Neu starten …", G_CALLBACK(sys_reboot), NULL), FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(r), btn("Ausschalten …", G_CALLBACK(sys_off), NULL), FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), r, FALSE, FALSE, 0);
	g_free(i);
	return pg;
}

int main(int argc, char **argv)
{
	GtkWidget *win, *hb, *side, *stack;
	GtkCssProvider *p;

	/* Grafische Sitzung hat kein /sbin im PATH (wpa_cli, ip, rc-service liegen dort) */
	setenv("PATH", "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin", 1);
	gtk_init(&argc, &argv);
	p = gtk_css_provider_new();
	gtk_css_provider_load_from_data(p, css, -1, NULL);
	gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(p), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

	win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(win), "Einstellungen");
	gtk_window_set_default_size(GTK_WINDOW(win), 1280, 736);
	gtk_window_maximize(GTK_WINDOW(win));
	g_signal_connect(win, "destroy", G_CALLBACK(gtk_main_quit), NULL);

	hb = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	stack = gtk_stack_new();
	gtk_stack_set_transition_type(GTK_STACK(stack), GTK_STACK_TRANSITION_TYPE_NONE);
	side = gtk_stack_sidebar_new();
	gtk_stack_sidebar_set_stack(GTK_STACK_SIDEBAR(side), GTK_STACK(stack));
	gtk_stack_add_titled(GTK_STACK(stack), page_wlan(), "wlan", "WLAN");
	gtk_stack_add_titled(GTK_STACK(stack), page_bt(), "bt", "Bluetooth");
	gtk_stack_add_titled(GTK_STACK(stack), page_display(), "display", "Display");
	gtk_stack_add_titled(GTK_STACK(stack), page_ton(), "ton", "Ton");
	gtk_stack_add_titled(GTK_STACK(stack), page_second_screen(), "second", "Second Screen");
	gtk_stack_add_titled(GTK_STACK(stack), page_geraet(), "geraet", "Gerät");
	gtk_stack_add_titled(GTK_STACK(stack), page_akku(), "akku", "Akku & Strom");
	gtk_stack_add_titled(GTK_STACK(stack), page_dienste(), "dienste", "Dienste");
	gtk_stack_add_titled(GTK_STACK(stack), page_system(), "system", "System");
	gtk_box_pack_start(GTK_BOX(hb), side, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hb), stack, TRUE, TRUE, 0);
	gtk_container_add(GTK_CONTAINER(win), hb);
	gtk_widget_show_all(win);
	if (argc > 1) gtk_stack_set_visible_child_name(GTK_STACK(stack), argv[1]);
	gtk_main();
	return 0;
}
