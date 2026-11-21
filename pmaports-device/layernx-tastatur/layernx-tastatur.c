// SPDX-License-Identifier: MIT
/*
 * layernx-tastatur: Bildschirmtastatur fuer LayerNX-Tablet-Linux (Touch, Querformat 1280x800)
 *
 *   layernx-tastatur                 Dienst starten (versteckt, automatisch ein/aus)
 *   layernx-tastatur an|aus|umschalten
 *
 * - deutsches QWERTZ mit Ziffernreihe, Umlauten, Symbol-Ebene
 * - Umschalt: 1x tippen = ein Grossbuchstabe, 2x schnell = Feststelltaste, nochmal = aus.
 *   Die Tasten zeigen dabei wirklich Gross-/Kleinbuchstaben.
 * - ⌫ gedrueckt halten wiederholt
 * - Rueckmeldung: Taste leuchtet beim Druck lila auf (das Geraet hat keinen Vibrationsmotor)
 * - automatisch: geht auf, wenn ein Textfeld (GTK-Programme, Terminal) den Fokus bekommt, und
 *   zu, wenn der Fokus woanders hingeht. Ausgenommen: Web-Apps (layernx-webview/Chromium),
 *   dort ueber den Tastatur-Knopf in der Leiste.
 * - das Fenster nimmt nie den Fokus; Tastendruecke gehen per XTest an das aktive Fenster.
 *   Zeichen, die in der X-Tastaturbelegung fehlen (ä ö ü ß € § ° ...), werden beim Start EINMAL
 *   fest auf freie Tastencodes gelegt (kein Umbelegen waehrend des Tippens -> keine falschen Zeichen).
 * - Waehrend sie offen ist, steht ihr Bereich in /tmp/layernx-tastatur.bereich; die Touch-Bruecke
 *   (moritz-v5x-touch) meldet dort jeden Finger sofort als eigenen Druck (kein Scrollen/Langdruck).
 *   Waehrend die Tastatur offen ist, reserviert sie den Platz (_NET_WM_STRUT), maximierte
 *   Fenster werden darueber verkleinert.
 */
#include <atspi/atspi.h>
#include <errno.h>
#include <gdk/gdkx.h>
#include <glib-unix.h>
#include <gtk/gtk.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <X11/Xatom.h>
#include <X11/XKBlib.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>

#define HOEHE 320
#define LEISTE 64           /* tint2 unten */
#define DOPPEL_MS 380       /* zwei Tipps auf Umschalt = Feststelltaste */

enum { K_ZEICHEN, K_TASTE, K_SHIFT, K_EBENE, K_WEG, K_LEER };
typedef struct { const char *klein, *gross; int art; int breite; /* in halben Einheiten */ const char *keysym; } Taste;

/* Buchstaben-Ebene (13 Einheiten pro Reihe) */
static const Taste E0[] = {
	{"1","1",K_ZEICHEN,2},{"2","2",K_ZEICHEN,2},{"3","3",K_ZEICHEN,2},{"4","4",K_ZEICHEN,2},{"5","5",K_ZEICHEN,2},
	{"6","6",K_ZEICHEN,2},{"7","7",K_ZEICHEN,2},{"8","8",K_ZEICHEN,2},{"9","9",K_ZEICHEN,2},{"0","0",K_ZEICHEN,2},
	{"ß","ß",K_ZEICHEN,2},{"⌫","⌫",K_TASTE,4,"BackSpace"},{NULL}};
static const Taste E1[] = {
	{"⇥","⇥",K_TASTE,4,"Tab"},{"q","Q",K_ZEICHEN,2},{"w","W",K_ZEICHEN,2},{"e","E",K_ZEICHEN,2},{"r","R",K_ZEICHEN,2},
	{"t","T",K_ZEICHEN,2},{"z","Z",K_ZEICHEN,2},{"u","U",K_ZEICHEN,2},{"i","I",K_ZEICHEN,2},{"o","O",K_ZEICHEN,2},
	{"p","P",K_ZEICHEN,2},{"ü","Ü",K_ZEICHEN,2},{NULL}};
static const Taste E2[] = {
	{"a","A",K_ZEICHEN,2},{"s","S",K_ZEICHEN,2},{"d","D",K_ZEICHEN,2},{"f","F",K_ZEICHEN,2},{"g","G",K_ZEICHEN,2},
	{"h","H",K_ZEICHEN,2},{"j","J",K_ZEICHEN,2},{"k","K",K_ZEICHEN,2},{"l","L",K_ZEICHEN,2},{"ö","Ö",K_ZEICHEN,2},
	{"ä","Ä",K_ZEICHEN,2},{"⏎","⏎",K_TASTE,4,"Return"},{NULL}};
static const Taste E3[] = {
	{"⇧","⇧",K_SHIFT,3},{"y","Y",K_ZEICHEN,2},{"x","X",K_ZEICHEN,2},{"c","C",K_ZEICHEN,2},{"v","V",K_ZEICHEN,2},
	{"b","B",K_ZEICHEN,2},{"n","N",K_ZEICHEN,2},{"m","M",K_ZEICHEN,2},{",",";",K_ZEICHEN,2},{".",":",K_ZEICHEN,2},
	{"-","_",K_ZEICHEN,2},{"⇧","⇧",K_SHIFT,3},{NULL}};
static const Taste E4[] = {
	{"?#","?#",K_EBENE,4},{"@","@",K_ZEICHEN,2},{" "," ",K_TASTE,12,"space"},{"←","←",K_TASTE,2,"Left"},
	{"→","→",K_TASTE,2,"Right"},{"⌨ ⌄","⌨ ⌄",K_WEG,4},{NULL}};
/* Symbol-Ebene */
static const Taste S1[] = {
	{"⇥","⇥",K_TASTE,4,"Tab"},{"!","!",K_ZEICHEN,2},{"\"","\"",K_ZEICHEN,2},{"§","§",K_ZEICHEN,2},{"$","$",K_ZEICHEN,2},
	{"%","%",K_ZEICHEN,2},{"&","&",K_ZEICHEN,2},{"/","/",K_ZEICHEN,2},{"(","(",K_ZEICHEN,2},{")",")",K_ZEICHEN,2},
	{"=","=",K_ZEICHEN,2},{"?","?",K_ZEICHEN,2},{NULL}};
static const Taste S2[] = {
	{"#","#",K_ZEICHEN,2},{"€","€",K_ZEICHEN,2},{"+","+",K_ZEICHEN,2},{"*","*",K_ZEICHEN,2},{"~","~",K_ZEICHEN,2},
	{"'","'",K_ZEICHEN,2},{":",":",K_ZEICHEN,2},{";",";",K_ZEICHEN,2},{"_","_",K_ZEICHEN,2},{"°","°",K_ZEICHEN,2},
	{"^","^",K_ZEICHEN,2},{"⏎","⏎",K_TASTE,4,"Return"},{NULL}};
static const Taste S3[] = {
	{"Esc","Esc",K_TASTE,3,"Escape"},{"<","<",K_ZEICHEN,2},{">",">",K_ZEICHEN,2},{"|","|",K_ZEICHEN,2},{"\\","\\",K_ZEICHEN,2},
	{"[","[",K_ZEICHEN,2},{"]","]",K_ZEICHEN,2},{"{","{",K_ZEICHEN,2},{"}","}",K_ZEICHEN,2},{"`","`",K_ZEICHEN,2},
	{"↑","↑",K_TASTE,2,"Up"},{"↓","↓",K_TASTE,3,"Down"},{NULL}};
static const Taste S4[] = {
	{"abc","abc",K_EBENE,4},{"@","@",K_ZEICHEN,2},{" "," ",K_TASTE,12,"space"},{"←","←",K_TASTE,2,"Left"},
	{"→","→",K_TASTE,2,"Right"},{"⌨ ⌄","⌨ ⌄",K_WEG,4},{NULL}};

static const Taste *BUCHSTABEN[] = {E0, E1, E2, E3, E4};
static const Taste *SYMBOLE[] = {E0, S1, S2, S3, S4};

static GtkWidget *win, *stack;
static GList *zeichen_knoepfe;      /* Knoepfe mit Gross/Klein-Beschriftung */
static GList *shift_knoepfe;
static Display *dpy;
static guint wdh_timer;
#define BEREICH "/tmp/layernx-tastatur.bereich"
static int shift;                   /* 0 aus, 1 einmal, 2 fest */
static gint64 shift_zeit;
static gboolean sichtbar, manuell_zu;
static guint hide_timer;

/* ---------- Anzeige ---------- */
static void beschriftung_aktualisieren(void)
{
	GList *l;
	for (l = zeichen_knoepfe; l; l = l->next) {
		const Taste *t = g_object_get_data(G_OBJECT(l->data), "taste");
		gtk_button_set_label(GTK_BUTTON(l->data), shift ? t->gross : t->klein);
	}
	for (l = shift_knoepfe; l; l = l->next) {
		GtkStyleContext *c = gtk_widget_get_style_context(GTK_WIDGET(l->data));
		gtk_style_context_remove_class(c, "einmal");
		gtk_style_context_remove_class(c, "fest");
		if (shift == 1) gtk_style_context_add_class(c, "einmal");
		if (shift == 2) gtk_style_context_add_class(c, "fest");
		gtk_button_set_label(GTK_BUTTON(l->data), shift == 2 ? "⇪" : "⇧");
	}
}

static void strut_setzen(gboolean an)
{
	GdkWindow *gw = gtk_widget_get_window(win);
	if (!gw) return;
	Display *d = GDK_WINDOW_XDISPLAY(gw);
	Window w = GDK_WINDOW_XID(gw);
	long s[12] = {0};
	if (an) { s[3] = HOEHE + LEISTE; s[10] = 0; s[11] = 1279; }
	XChangeProperty(d, w, XInternAtom(d, "_NET_WM_STRUT_PARTIAL", False), XA_CARDINAL, 32, PropModeReplace, (unsigned char *)s, 12);
	XChangeProperty(d, w, XInternAtom(d, "_NET_WM_STRUT", False), XA_CARDINAL, 32, PropModeReplace, (unsigned char *)s, 4);
	XFlush(d);
}

static void zeigen(void)
{
	if (hide_timer) { g_source_remove(hide_timer); hide_timer = 0; }
	if (sichtbar) return;
	sichtbar = TRUE;
	gtk_widget_show_all(win);
	gtk_window_move(GTK_WINDOW(win), 0, 800 - LEISTE - HOEHE);
	strut_setzen(TRUE);
	{
		gchar *b = g_strdup_printf("%d %d\n", 800 - LEISTE - HOEHE, 800 - LEISTE);
		g_file_set_contents(BEREICH, b, -1, NULL);
		chmod(BEREICH, 0644);
		g_free(b);
	}
}

static void verstecken(void)
{
	if (hide_timer) { g_source_remove(hide_timer); hide_timer = 0; }
	if (!sichtbar) return;
	if (wdh_timer) { g_source_remove(wdh_timer); wdh_timer = 0; }
	sichtbar = FALSE;
	unlink(BEREICH);
	strut_setzen(FALSE);
	gtk_widget_hide(win);
	if (shift == 1) { shift = 0; beschriftung_aktualisieren(); }
}

static gboolean verstecken_cb(gpointer d) { (void)d; hide_timer = 0; verstecken(); return G_SOURCE_REMOVE; }

/* ---------- Tasten (XTest) ---------- */
/* fehlendes Keysym fest auf einen freien Tastencode legen */
static KeyCode zuordnen(KeySym ks)
{
	int min, max, per, kc, i;
	KeyCode k = XKeysymToKeycode(dpy, ks);
	if (k) return k;
	XDisplayKeycodes(dpy, &min, &max);
	KeySym *map = XGetKeyboardMapping(dpy, min, max - min + 1, &per);
	for (kc = max; kc >= min; kc--) {
		int leer = 1;
		for (i = 0; i < per; i++) if (map[(kc - min) * per + i] != NoSymbol) { leer = 0; break; }
		if (leer) {
			KeySym neu[2] = { ks, ks };
			XChangeKeyboardMapping(dpy, kc, 2, neu, 1);
			XSync(dpy, False);
			g_usleep(20000);   /* Programme bekommen MappingNotify vor dem ersten Druck */
			XFree(map);
			return kc;
		}
	}
	XFree(map);
	return 0;
}

static void druck(KeyCode kc, gboolean shift_n, gboolean ctrl_n)
{
	KeyCode sh = XKeysymToKeycode(dpy, XK_Shift_L), ct = XKeysymToKeycode(dpy, XK_Control_L);
	if (ctrl_n && ct) XTestFakeKeyEvent(dpy, ct, True, 0);
	if (shift_n && sh) XTestFakeKeyEvent(dpy, sh, True, 0);
	XTestFakeKeyEvent(dpy, kc, True, 0);
	XTestFakeKeyEvent(dpy, kc, False, 0);
	if (shift_n && sh) XTestFakeKeyEvent(dpy, sh, False, 0);
	if (ctrl_n && ct) XTestFakeKeyEvent(dpy, ct, False, 0);
	XFlush(dpy);
}

static void sende_keysym(KeySym ks, gboolean ctrl_n)
{
	KeyCode kc = zuordnen(ks);
	if (!kc) return;
	/* Ebene 0 = ohne Umschalt, sonst mit Umschalt (Ebene 1) */
	gboolean sh = XkbKeycodeToKeysym(dpy, kc, 0, 0) != ks && XkbKeycodeToKeysym(dpy, kc, 0, 1) == ks;
	druck(kc, sh, ctrl_n);
}

static KeySym keysym_von(const char *s)
{
	gunichar u = g_utf8_get_char(s);
	if (u < 0x100 && u >= 0x20 && u != 0x7f) return (KeySym)u;   /* Latin-1: Keysym = Codepunkt */
	return 0x01000000 | u;                                        /* Unicode-Keysym */
}

static void sende_zeichen(const char *s)
{
	KeySym ks = keysym_von(s);
	if (g_utf8_get_char(s) == 0x20ac) ks = XK_EuroSign;
	sende_keysym(ks, FALSE);
}
static void sende_taste(const char *name)
{
	gboolean ctrl = g_str_has_prefix(name, "ctrl+");
	KeySym ks = XStringToKeysym(ctrl ? name + 5 : name);
	if (ks != NoSymbol) sende_keysym(ks, ctrl);
}

/* alle Zeichen der Tastatur vorab zuordnen */
static void belegung_vorbereiten(const Taste **reihen)
{
	int r, i;
	for (r = 0; r < 5; r++)
		for (i = 0; reihen[r][i].klein; i++)
			if (reihen[r][i].art == K_ZEICHEN) {
				KeySym a = keysym_von(reihen[r][i].klein), b = keysym_von(reihen[r][i].gross);
				if (g_utf8_get_char(reihen[r][i].klein) == 0x20ac) a = b = XK_EuroSign;
				zuordnen(a); zuordnen(b);
			}
}

static gboolean wiederholen(gpointer d) { sende_taste(d); return G_SOURCE_CONTINUE; }
static gboolean wiederholen_start(gpointer d)
{
	sende_taste(d);
	wdh_timer = g_timeout_add(70, wiederholen, d);
	return G_SOURCE_REMOVE;
}

static gboolean gedrueckt(GtkWidget *w, GdkEventButton *e, gpointer d)
{
	const Taste *t = d;
	if (e->type != GDK_BUTTON_PRESS) return TRUE;     /* Doppel-/Dreifachklick-Events ignorieren */
	if (e->button == 3) return TRUE;                  /* Rechtsklick (Langdruck) ignorieren */
	if (e->button != 1) return TRUE;
	gtk_widget_set_state_flags(w, GTK_STATE_FLAG_ACTIVE, FALSE);
	switch (t->art) {
	case K_ZEICHEN:
		sende_zeichen(shift ? t->gross : t->klein);
		if (shift == 1) { shift = 0; beschriftung_aktualisieren(); }
		break;
	case K_TASTE:
		if (!strcmp(t->keysym, "space")) sende_zeichen(" ");
		else sende_taste(t->keysym);
		if (!strcmp(t->keysym, "BackSpace")) {
			if (wdh_timer) g_source_remove(wdh_timer);
			wdh_timer = g_timeout_add(450, wiederholen_start, (gpointer)t->keysym);
		}
		break;
	case K_SHIFT: {
		gint64 jetzt = g_get_monotonic_time() / 1000;
		if (shift == 2) shift = 0;
		else if (shift == 1 && jetzt - shift_zeit < DOPPEL_MS) shift = 2;
		else if (shift == 1) shift = 0;
		else shift = 1;
		shift_zeit = jetzt;
		beschriftung_aktualisieren();
		break; }
	case K_EBENE:
		gtk_stack_set_visible_child_name(GTK_STACK(stack),
			strcmp(gtk_stack_get_visible_child_name(GTK_STACK(stack)), "abc") ? "abc" : "sym");
		break;
	case K_WEG:
		manuell_zu = TRUE;
		verstecken();
		break;
	}
	return TRUE;
}

static gboolean losgelassen(GtkWidget *w, GdkEventButton *e, gpointer d)
{
	(void)e; (void)d;
	if (wdh_timer) { g_source_remove(wdh_timer); wdh_timer = 0; }
	gtk_widget_unset_state_flags(w, GTK_STATE_FLAG_ACTIVE);
	return TRUE;
}

static GtkWidget *ebene(const Taste **reihen)
{
	GtkWidget *g = gtk_grid_new();
	int r, i;
	gtk_grid_set_row_homogeneous(GTK_GRID(g), TRUE);
	gtk_grid_set_column_homogeneous(GTK_GRID(g), TRUE);
	gtk_grid_set_row_spacing(GTK_GRID(g), 6);
	gtk_grid_set_column_spacing(GTK_GRID(g), 6);
	for (r = 0; r < 5; r++) {
		int x = 0;
		for (i = 0; reihen[r][i].klein; i++) {
			const Taste *t = &reihen[r][i];
			GtkWidget *b = gtk_button_new_with_label(t->klein);
			GtkStyleContext *c = gtk_widget_get_style_context(b);
			gtk_widget_set_can_focus(b, FALSE);
			gtk_style_context_add_class(c, "taste");
			if (t->art != K_ZEICHEN && !(t->keysym && !strcmp(t->keysym, "space"))) gtk_style_context_add_class(c, "sonder");
			g_object_set_data(G_OBJECT(b), "taste", (gpointer)t);
			g_signal_connect(b, "button-press-event", G_CALLBACK(gedrueckt), (gpointer)t);
			g_signal_connect(b, "button-release-event", G_CALLBACK(losgelassen), NULL);
			if (t->art == K_ZEICHEN && strcmp(t->klein, t->gross)) zeichen_knoepfe = g_list_prepend(zeichen_knoepfe, b);
			if (t->art == K_SHIFT) shift_knoepfe = g_list_prepend(shift_knoepfe, b);
			gtk_grid_attach(GTK_GRID(g), b, x, r, t->breite, 1);
			x += t->breite;
		}
	}
	return g;
}

static const char *CSS =
	"window { background: #0e0c16; }"
	".tastatur { padding: 8px 10px 8px 10px; border-top: 2px solid #4931fd; }"
	"button.taste { background: #2a2540; color: #ffffff; border: none; border-radius: 12px;"
	"  box-shadow: 0 2px 0 #08060f; font: 600 24px 'DejaVu Sans'; padding: 0; min-height: 0; min-width: 0;"
	"  transition: none; }"
	"button.taste.sonder { background: #1c1830; color: #c9c2ff; font-size: 22px; }"
	"button.taste:active, button.taste:checked { background: #4931fd; color: #ffffff; box-shadow: none; }"
	"button.taste.einmal { background: #3a2f8f; color: #ffffff; }"
	"button.taste.fest { background: #4931fd; color: #ffffff; }";

/* ---------- Automatik (AT-SPI) ---------- */
static gboolean ist_webapp(AtspiAccessible *a)
{
	gboolean ja = FALSE;
	AtspiAccessible *app = atspi_accessible_get_application(a, NULL);
	if (app) {
		gchar *n = atspi_accessible_get_name(app, NULL);
		guint pid = atspi_accessible_get_process_id(app, NULL);
		if (n && (g_strrstr(n, "WebKit") || g_strrstr(n, "webview") || g_strrstr(n, "hromium"))) ja = TRUE;
		if (pid) {
			gchar *p = g_strdup_printf("/proc/%u/comm", pid), *c = NULL;
			if (g_file_get_contents(p, &c, NULL, NULL) &&
			    (g_str_has_prefix(c, "layernx-webview") || g_str_has_prefix(c, "WebKit") || g_str_has_prefix(c, "chromium")))
				ja = TRUE;
			g_free(p); g_free(c);
		}
		g_free(n);
		g_object_unref(app);
	}
	return ja;
}

static gboolean ist_textfeld(AtspiAccessible *a)
{
	gboolean ja = FALSE;
	AtspiRole r = atspi_accessible_get_role(a, NULL);
	AtspiStateSet *s = atspi_accessible_get_state_set(a);
	if (r == ATSPI_ROLE_TERMINAL) ja = TRUE;
	else if (s && atspi_state_set_contains(s, ATSPI_STATE_EDITABLE) &&
	         (r == ATSPI_ROLE_ENTRY || r == ATSPI_ROLE_TEXT || r == ATSPI_ROLE_PASSWORD_TEXT ||
	          r == ATSPI_ROLE_SPIN_BUTTON || r == ATSPI_ROLE_COMBO_BOX || r == ATSPI_ROLE_PARAGRAPH ||
	          r == ATSPI_ROLE_DOCUMENT_TEXT || r == ATSPI_ROLE_EDITBAR)) ja = TRUE;
	if (s) g_object_unref(s);
	return ja;
}

static void fokus_event(AtspiEvent *ev, void *d)
{
	(void)d;
	if (!ev->source || ev->detail1 != 1) return;     /* nur "hat Fokus bekommen" */
	if (ist_webapp(ev->source)) return;
	if (ist_textfeld(ev->source)) {
		manuell_zu = FALSE;
		zeigen();
	} else if (sichtbar && !hide_timer) {
		hide_timer = g_timeout_add(350, verstecken_cb, NULL);
	}
}

/* ---------- Steuerung per Signal ---------- */
static gboolean sig_umschalten(gpointer d) { (void)d; if (sichtbar) { manuell_zu = TRUE; verstecken(); } else zeigen(); return G_SOURCE_CONTINUE; }
static gboolean sig_an(gpointer d) { (void)d; zeigen(); return G_SOURCE_CONTINUE; }
static gboolean sig_aus(gpointer d) { (void)d; verstecken(); return G_SOURCE_CONTINUE; }

static gchar *pid_datei(void)
{
	return g_strdup_printf("/tmp/layernx-tastatur-%u.pid", (unsigned)getuid());
}

static int steuern(const char *cmd)
{
	gchar *f = pid_datei(), *inhalt = NULL;
	int sig = !strcmp(cmd, "an") ? SIGUSR2 : !strcmp(cmd, "aus") ? SIGHUP : SIGUSR1;
	pid_t pid = 0;
	if (g_file_get_contents(f, &inhalt, NULL, NULL)) pid = atoi(inhalt);
	g_free(inhalt); g_free(f);
	if (pid > 0 && kill(pid, sig) == 0) return 0;
	if (!strcmp(cmd, "aus")) return 0;
	/* laeuft nicht: starten und sichtbar machen */
	if (fork() == 0) {
		setsid();
		execlp("layernx-tastatur", "layernx-tastatur", "--sichtbar", (char *)NULL);
		_exit(127);
	}
	return 0;
}

int main(int argc, char **argv)
{
	gboolean start_sichtbar = FALSE;
	if (argc > 1 && strcmp(argv[1], "--sichtbar")) {
		if (!strcmp(argv[1], "an") || !strcmp(argv[1], "aus") || !strcmp(argv[1], "umschalten")) return steuern(argv[1]);
		fprintf(stderr, "usage: layernx-tastatur [an|aus|umschalten]\n");
		return 2;
	}
	if (argc > 1) start_sichtbar = TRUE;

	setenv("NO_AT_BRIDGE", "1", 1);   /* sich selbst nicht bei AT-SPI melden */
	gtk_init(&argc, &argv);

	gchar *pf = pid_datei(), *ps = g_strdup_printf("%d\n", getpid());
	g_file_set_contents(pf, ps, -1, NULL);
	g_free(ps);

	dpy = GDK_DISPLAY_XDISPLAY(gdk_display_get_default());
	{
		int ev, er, ma, mi;
		if (!XTestQueryExtension(dpy, &ev, &er, &ma, &mi)) { fprintf(stderr, "layernx-tastatur: keine XTest-Erweiterung\n"); return 1; }
	}
	belegung_vorbereiten(BUCHSTABEN);
	belegung_vorbereiten(SYMBOLE);
	unlink(BEREICH);

	GtkCssProvider *css = gtk_css_provider_new();
	gtk_css_provider_load_from_data(css, CSS, -1, NULL);
	gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

	win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(win), "LayerNX Tastatur");
	gtk_window_set_wmclass(GTK_WINDOW(win), "layernx-tastatur", "LayerNX-Tastatur");
	gtk_window_set_type_hint(GTK_WINDOW(win), GDK_WINDOW_TYPE_HINT_DOCK);
	gtk_window_set_accept_focus(GTK_WINDOW(win), FALSE);
	gtk_window_set_focus_on_map(GTK_WINDOW(win), FALSE);
	gtk_window_set_keep_above(GTK_WINDOW(win), TRUE);
	gtk_window_set_decorated(GTK_WINDOW(win), FALSE);
	gtk_window_set_skip_taskbar_hint(GTK_WINDOW(win), TRUE);
	gtk_window_set_skip_pager_hint(GTK_WINDOW(win), TRUE);
	gtk_window_stick(GTK_WINDOW(win));
	gtk_window_set_default_size(GTK_WINDOW(win), 1280, HOEHE);
	gtk_window_set_resizable(GTK_WINDOW(win), FALSE);
	gtk_widget_set_size_request(win, 1280, HOEHE);

	stack = gtk_stack_new();
	gtk_stack_set_transition_type(GTK_STACK(stack), GTK_STACK_TRANSITION_TYPE_NONE);
	gtk_style_context_add_class(gtk_widget_get_style_context(stack), "tastatur");
	gtk_stack_add_named(GTK_STACK(stack), ebene(BUCHSTABEN), "abc");
	gtk_stack_add_named(GTK_STACK(stack), ebene(SYMBOLE), "sym");
	gtk_container_add(GTK_CONTAINER(win), stack);
	gtk_widget_realize(win);
	gtk_widget_show_all(stack);
	gtk_stack_set_visible_child_name(GTK_STACK(stack), "abc");
	g_signal_connect(win, "delete-event", G_CALLBACK(gtk_widget_hide_on_delete), NULL);

	g_unix_signal_add(SIGUSR1, sig_umschalten, NULL);
	g_unix_signal_add(SIGUSR2, sig_an, NULL);
	g_unix_signal_add(SIGHUP, sig_aus, NULL);

	if (atspi_init() <= 1) {   /* 0 = ok, 1 = schon initialisiert */
		AtspiEventListener *l = atspi_event_listener_new(fokus_event, NULL, NULL);
		GError *e = NULL;
		if (!atspi_event_listener_register(l, "object:state-changed:focused", &e)) {
			fprintf(stderr, "layernx-tastatur: AT-SPI nicht verfuegbar (%s), nur manuell\n", e ? e->message : "?");
			g_clear_error(&e);
		}
	}
	if (start_sichtbar) zeigen();
	gtk_main();
	unlink(pf); g_free(pf); unlink(BEREICH);
	return 0;
}
