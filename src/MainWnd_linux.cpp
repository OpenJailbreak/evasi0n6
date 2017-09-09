#include <gtk/gtk.h>
#include "MainWnd.h"
#include "../res/gui/linux/e_logo.xpm"
#include "../res/gui/linux/background.xpm"
#include "../res/gui/linux/icon.xpm"

static guint atntimer = 0;
static int laststate = 0;
static int buttonExit = 0;

extern "C" {

gboolean blink_timer(GtkWidget* statusLabel)
{
	gdk_threads_enter();
	gtk_widget_set_visible(statusLabel, (laststate != 0));
	gdk_threads_leave();
	laststate++;
	if (laststate > 3) {
		laststate = 0;
	}
	return TRUE;
}

void timer_destroyed(GtkWidget* statusLabel)
{
	gdk_threads_enter();
	gtk_widget_set_visible(statusLabel, TRUE);
	gdk_threads_leave();
}

}

int MainWnd::msgBox(const char* message, const char* caption, int style)
{
	// get message type
	GtkMessageType mtype = GTK_MESSAGE_OTHER;
	if (style & mb_ICON_INFO) {
		mtype = GTK_MESSAGE_INFO;
	} else if (style & mb_ICON_WARNING) {
		mtype = GTK_MESSAGE_WARNING;
	} else if (style & mb_ICON_QUESTION) {
		mtype = GTK_MESSAGE_QUESTION;
	} else if (style & mb_ICON_ERROR) {
		mtype = GTK_MESSAGE_ERROR;
	}

	// get button type(s)
	GtkButtonsType btype = GTK_BUTTONS_OK;
	if (style & mb_OK) {
		btype = GTK_BUTTONS_OK;
	} else if (style & mb_CANCEL) {
		btype = GTK_BUTTONS_CANCEL;
	} else if (style & mb_OK_CANCEL) {
		btype = GTK_BUTTONS_OK_CANCEL;
	} else if (style & mb_YES_NO) {
		btype = GTK_BUTTONS_YES_NO;
	}

	GtkWidget* msgbox = gtk_message_dialog_new(GTK_WINDOW(this->mainwnd), GTK_DIALOG_MODAL, mtype, btype, "%s", caption);
	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(msgbox), "%s", message);
	gdk_threads_enter();
	int answer = gtk_dialog_run(GTK_DIALOG(msgbox));
	gtk_widget_destroy(msgbox);
	gdk_threads_leave();
	switch (answer) {
	case GTK_RESPONSE_OK:
		return mb_OK;
	case GTK_RESPONSE_CANCEL:
		return mb_CANCEL;
	case GTK_RESPONSE_YES:
		return mb_YES;
	case GTK_RESPONSE_NO:
		return mb_NO;
	default:
		return -1;
	}
}

void MainWnd::configureButtonForExit()
{
	gdk_threads_enter();
	gtk_widget_set_sensitive(this->btnStart, 1);
        gtk_button_set_label(GTK_BUTTON(this->btnStart), localize("Exit"));
	gtk_widget_queue_draw(this->btnStart);
        buttonExit = 1;
	gdk_threads_leave();
}

void MainWnd::setButtonEnabled(int enabled)
{
	gdk_threads_enter();
	gtk_widget_set_sensitive(this->btnStart, enabled);
	gtk_widget_queue_draw (this->btnStart);
	gdk_threads_leave();
}

void MainWnd::setStatusText(const char* text)
{
	gdk_threads_enter();
	gtk_label_set_text(GTK_LABEL(this->lbStatus), (gchar*)text);
	gtk_widget_queue_draw (this->lbStatus);
	gdk_threads_leave();
}

void MainWnd::setProgress(int percentage)
{
	gdk_threads_enter();
	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(this->progressBar), (double)percentage / 100.0f);
	gtk_widget_queue_draw (this->progressBar);
	gdk_threads_leave();
}

void MainWnd::requestUserAttention(int level)
{
	if (atntimer > 0) {
		GSource* src = g_main_context_find_source_by_id(NULL, atntimer);
		if (src) {
			g_source_destroy(src);
		}
		atntimer = 0;
	}
	gdk_threads_enter();
	gtk_widget_set_visible(this->lbStatus, TRUE);
	gtk_window_set_urgency_hint(GTK_WINDOW(this->mainwnd), FALSE);	
	if (!gtk_window_has_toplevel_focus(GTK_WINDOW(this->mainwnd))) {
		gtk_window_set_urgency_hint(GTK_WINDOW(this->mainwnd), TRUE);
	}
	gdk_threads_leave();
	if (level > 1) {
		laststate = 0;
		atntimer = g_timeout_add_full(G_PRIORITY_DEFAULT, 500, (GSourceFunc)blink_timer, (gpointer)this->lbStatus, (GDestroyNotify)timer_destroyed);
	}
}

void MainWnd::cancelUserAttention()
{
	if (atntimer > 0) {
		GSource* src = g_main_context_find_source_by_id(NULL, atntimer);
		if (src) {
			g_source_destroy(src);
		}
		atntimer = 0;
	}
	gdk_threads_enter();
	gtk_window_set_urgency_hint(GTK_WINDOW(this->mainwnd), FALSE);
	gtk_widget_set_visible(this->lbStatus, TRUE);
	gdk_threads_leave();
}

void MainWnd::handleStartClicked(void* data)
{
	gtk_window_set_urgency_hint(GTK_WINDOW(this->mainwnd), FALSE);

	gtk_widget_set_sensitive(this->btnStart, 0);
	gtk_widget_queue_draw (this->btnStart);

	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(this->progressBar), 0.0);
	gtk_widget_queue_draw (this->progressBar);

	this->devhandler->processStart();
}

bool MainWnd::onClose(void* data)
{
	if (this->closeBlocked) {
		return TRUE;
	}
	return FALSE;
}

static gchar* g_strreplace (const gchar *string, const gchar *search, const gchar *replacement)
{
	gchar *str, **arr;

	g_return_val_if_fail (string != NULL, NULL);
	g_return_val_if_fail (search != NULL, NULL);

	if (replacement == NULL)
		replacement = "";

	arr = g_strsplit (string, search, -1);
	if (arr != NULL && arr[0] != NULL)
		str = g_strjoinv (replacement, arr);
	else
		str = g_strdup (string);

	g_strfreev (arr);

	return str;
}

extern "C" {

static void start_clicked(GtkWidget* widget, gpointer data)
{
	MainWnd* _this = (MainWnd*)data;
        if(buttonExit)
        {
	    gtk_main_quit();
        } else
        {
            _this->handleStartClicked(widget);
        }
}

static gboolean delete_event(GtkWidget* widget, gpointer data)
{
	MainWnd* _this = (MainWnd*)data;
	return _this->onClose(widget);
}

static void destroy_event(GtkWidget* widget, gpointer data)
{
	gtk_main_quit();
}

}

MainWnd::MainWnd(int* pargc, char*** pargv)
{
        char buf[512];

	gdk_threads_init();

	gtk_init(pargc, pargv);

	mainwnd = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_resizable(GTK_WINDOW(mainwnd), 0);
	gtk_widget_set_size_request(mainwnd, WND_WIDTH, WND_HEIGHT);
	gtk_window_set_position(GTK_WINDOW(mainwnd), GTK_WIN_POS_CENTER);

        char title[100];
        snprintf(title, sizeof(title), WND_TITLE, APPNAME, EVASI0N_VERSION_STRING);
	gtk_window_set_title(GTK_WINDOW(mainwnd), title);

	gtk_window_set_icon(GTK_WINDOW(mainwnd), gdk_pixbuf_new_from_xpm_data((const char**)icon_xpm));

	gtk_container_set_border_width(GTK_CONTAINER(mainwnd), 10);

	g_signal_connect(mainwnd, "delete-event", G_CALLBACK(delete_event), this);
	g_signal_connect(mainwnd, "destroy", G_CALLBACK(destroy_event), NULL);

	GdkPixmap *background;
	GdkPixbuf *pixbuf = gdk_pixbuf_new_from_xpm_data((const char**)&background_xpm);
	gdk_pixbuf_render_pixmap_and_mask (pixbuf, &background, NULL, 0);
	GtkStyle* style = gtk_style_new();
	style->bg_pixmap[0] = background;
	gtk_widget_set_style(mainwnd, style);

	GtkWidget* vbox = gtk_vbox_new(0, 0);

	PangoFontDescription* small_font = pango_font_description_new();
	pango_font_description_set_size(small_font, 11000);

	PangoFontDescription* smaller_font = pango_font_description_new();
	pango_font_description_set_size(smaller_font, 10500);

	PangoFontDescription* even_smaller_font = pango_font_description_new();
	pango_font_description_set_size(even_smaller_font, 9500);

	GtkWidget* lbTop = gtk_label_new(WELCOME_LABEL_TEXT);
	gtk_label_set_line_wrap(GTK_LABEL(lbTop), 1);
	gtk_widget_set_size_request(lbTop, WND_WIDTH-20, 30);
	gtk_misc_set_alignment(GTK_MISC(lbTop), 0.0, 0.0);
	gtk_widget_modify_font(lbTop, small_font);

	lbStatus = gtk_label_new(NULL);
	gtk_label_set_line_wrap(GTK_LABEL(lbStatus), 1);
	gtk_label_set_selectable(GTK_LABEL(lbStatus), TRUE);
	gtk_widget_set_size_request(lbStatus, WND_WIDTH-20, 60);
	gtk_misc_set_alignment(GTK_MISC(lbStatus), 0.0, 0.0);
	gtk_widget_modify_font(lbStatus, small_font);

	GtkWidget* statusBox = gtk_vbox_new(0, 0);
	gtk_box_pack_start(GTK_BOX(statusBox), lbStatus, 0, 0, 0);
	
	GtkWidget* elogo = gtk_image_new_from_pixbuf(gdk_pixbuf_new_from_xpm_data((const char**)&e_logo_xpm));
;
	gtk_widget_set_size_request(elogo, 24, 24);
	gtk_misc_set_alignment(GTK_MISC(elogo), 0.0, 0.8);

	progressBar = gtk_progress_bar_new();
	GtkWidget* pbalign = gtk_alignment_new(0, 0.5, 0, 0);
	gtk_widget_set_size_request(progressBar, 318, 17);
	gtk_container_add(GTK_CONTAINER(pbalign), progressBar);

	btnStart = gtk_button_new_with_label(BTN_START_TEXT);
	gtk_widget_modify_font(btnStart, smaller_font);
	gtk_widget_set_sensitive(btnStart, FALSE);
	g_signal_connect(btnStart, "clicked", G_CALLBACK(start_clicked), this);
	gtk_widget_set_size_request(btnStart, 92, 24);

	GtkWidget* hbox = gtk_hbox_new(0, 12);
	gtk_box_pack_start(GTK_BOX(hbox), elogo, 0, 0, 0);
	gtk_box_pack_start(GTK_BOX(hbox), pbalign, 0, 0, 0);
	gtk_box_pack_start(GTK_BOX(hbox), btnStart, 0, 0, 0);

	GtkWidget* pbbtnalign = gtk_alignment_new(0.5, 0, 0, 0);
	gtk_container_add(GTK_CONTAINER(pbbtnalign), hbox);

	GtkWidget* sep = gtk_hseparator_new();

	GtkWidget* lbDisc = gtk_label_new(NULL);

        snprintf(buf, sizeof(buf), "<span style=\"italic\">%s</span>", DISCLAIMER_LABEL_TEXT);
        gtk_label_set_markup(GTK_LABEL(lbDisc), g_strreplace(buf, "&", "&amp;"));
	gtk_label_set_line_wrap(GTK_LABEL(lbDisc), 1);
	gtk_widget_modify_font(lbDisc, even_smaller_font);	
	gtk_widget_set_size_request(lbDisc, WND_WIDTH-20, 53);

	GtkWidget* lbCopyright = gtk_label_new(NULL);
        snprintf(buf, sizeof(buf), "%s  <a href=\"%s\"><span foreground=\"#1182e2\" underline=\"none\">%s</span></a>", COPYRIGHT_LABEL_TEXT, TWITTER_LINK_URL, TWITTER_LINK_TEXT);
	gchar* url = g_strreplace(buf, "&", "&amp;");
	gtk_label_set_markup(GTK_LABEL(lbCopyright), url);
	g_free(url);
	gtk_label_set_line_wrap(GTK_LABEL(lbCopyright), 1);	
	gtk_label_set_track_visited_links(GTK_LABEL(lbCopyright), FALSE);
	gtk_widget_modify_font(lbCopyright, smaller_font);
	gtk_widget_set_size_request(lbCopyright, WND_WIDTH-20, 18);
	gtk_misc_set_alignment(GTK_MISC(lbCopyright), 0.0, 0.0);

	GtkWidget* lbCredits = gtk_label_new(CREDITS_LABEL_TEXT);
	gtk_label_set_line_wrap(GTK_LABEL(lbCredits), 1);
	gtk_widget_modify_font(lbCredits, smaller_font);
	gtk_widget_set_size_request(lbCredits, WND_WIDTH-20, 36);
	gtk_misc_set_alignment(GTK_MISC(lbCredits), 0.0, 0.0);
	
	GtkWidget* lbPaypal = gtk_label_new(NULL);
        snprintf(buf, sizeof(buf), "<a href=\"%s\"><span foreground=\"#1182e2\" underline=\"none\">%s</span></a>", PAYPAL_LINK_URL, PAYPAL_LINK_TEXT);
	url = g_strreplace(buf, "&", "&amp;");
	gtk_label_set_markup(GTK_LABEL(lbPaypal), url);
	g_free(url);
	gtk_misc_set_alignment(GTK_MISC(lbPaypal), 0.5, 0.0);
	gtk_label_set_track_visited_links(GTK_LABEL(lbPaypal), FALSE);
	gtk_widget_modify_font(lbPaypal, small_font);

	GtkWidget* lbHP = gtk_label_new(NULL);
        snprintf(buf, sizeof(buf), "<a href=\"%s\"><span foreground=\"#1182e2\" underline=\"none\">%s</span></a>", HOMEPAGE_LINK_URL, HOMEPAGE_LINK_TEXT);
	url = g_strreplace(buf, "&", "&amp;");
	gtk_label_set_markup(GTK_LABEL(lbHP), url);
	g_free(url);
	gtk_misc_set_alignment(GTK_MISC(lbHP), 0.5, 0.0);
	gtk_label_set_track_visited_links(GTK_LABEL(lbHP), FALSE);
	gtk_widget_modify_font(lbHP, small_font);

	GtkWidget* hbox2 = gtk_hbox_new(0, 0);
	gtk_box_pack_start(GTK_BOX(hbox2), lbPaypal, 1, 0, 0);
	gtk_box_pack_start(GTK_BOX(hbox2), lbHP, 1, 0, 0);

	gtk_box_pack_start(GTK_BOX(vbox), lbTop, 0, 0, 0);
	gtk_box_pack_start(GTK_BOX(vbox), statusBox, TRUE, TRUE, 10);
	gtk_box_pack_start(GTK_BOX(vbox), pbbtnalign, 0, 0, 4);
	gtk_box_pack_start(GTK_BOX(vbox), sep, 0, 0, 4);
	gtk_box_pack_start(GTK_BOX(vbox), lbDisc, 0, 0, 2);	
	gtk_box_pack_start(GTK_BOX(vbox), lbCopyright, 0, 0, 2);
	gtk_box_pack_start(GTK_BOX(vbox), lbCredits, 0, 0, 2);
	gtk_box_pack_start(GTK_BOX(vbox), hbox2, 0, 0, 10);

	gtk_container_add(GTK_CONTAINER(mainwnd), vbox);
	gtk_widget_show_all(mainwnd);

	this->closeBlocked = 0;
	this->devhandler = new DeviceHandler(this);
}

void MainWnd::run(void)
{
	gdk_threads_enter();
	gtk_main();
	gdk_threads_leave();
}
