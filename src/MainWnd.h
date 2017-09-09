#ifndef __MAINWND_H
#define __MAINWND_H

#include "version.h"
#include "localize.h"

/* gui definitions */
#define APPNAME localize("evasi0n")
#define WND_TITLE localize("%s - Version %s")
#define WND_WIDTH 480
#define WND_HEIGHT 320

#define WELCOME_LABEL_TEXT localize("Welcome! evasi0n is an untethered jailbreak for iOS 6.0 through 6.1.2.")
#define DISCLAIMER_LABEL_TEXT localize("NOTE: Please make a backup of your device before applying the jailbreak.\nWe don't think there will be any problems, but we can't make any guarantees.\nUse evasi0n at your own risk.")
#define BTN_START_TEXT localize("Jailbreak")
#define COPYRIGHT_LABEL_TEXT localize("evasi0n © 2013")
#define TWITTER_LINK_TEXT localize("@evad3rs")
#define TWITTER_LINK_URL localize("https://twitter.com/evad3rs")
#define CREDITS_LABEL_TEXT localize("jailbreak exploits by @planetbeing and @pimskeks.\ngraphic design by @Surenix - interface by Hanéne Samara.")
#define PAYPAL_LINK_TEXT localize("Support Us (PayPal)")
#define PAYPAL_LINK_URL localize("https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=36S6KLTRK7FTQ")
#define HOMEPAGE_LINK_TEXT localize("http://evasi0n.com")
#define HOMEPAGE_LINK_URL localize("http://evasi0n.com/")

enum {
	mb_OK = 1 << 0,
	mb_CANCEL = 1 << 1,
	mb_YES_NO = 1 << 2,
	mb_OK_CANCEL = 1 << 3,
	mb_ICON_INFO = 1 << 8,
	mb_ICON_WARNING = 1 << 9,
	mb_ICON_QUESTION = 1 << 10,
	mb_ICON_ERROR = 1 << 11
};

#define mb_YES mb_OK
#define mb_NO mb_CANCEL

#ifdef __linux__
#include <gtk/gtk.h>
#define WIDGET_TYPE GtkWidget*
#endif

#ifdef __APPLE__
#include <objc/objc.h>
#define WIDGET_TYPE id
#endif

#ifdef WIN32
#include <windows.h>
#define WIDGET_TYPE HWND
#endif

#ifndef WIDGET_TYPE
#define WIDGET_TYPE void*
#endif

#include "DeviceHandler.h"

class DeviceHandler;

class MainWnd
{
public:
	WIDGET_TYPE mainwnd;
	WIDGET_TYPE progressBar;
	WIDGET_TYPE btnStart;
	WIDGET_TYPE lbStatus;
	DeviceHandler* devhandler;

	int closeBlocked;
	MainWnd(int* pargc, char*** pargv);
	~MainWnd();
	void run(void);

	int msgBox(const char* message, const char* caption, int style);

	void configureButtonForExit();
	void setButtonEnabled(int enabled);
	void setStatusText(const char* text);
	void setProgress(int percentage);

	void requestUserAttention(int level);
	void cancelUserAttention();

	void handleStartClicked(void* data);
	bool onClose(void* data);
};
#endif
