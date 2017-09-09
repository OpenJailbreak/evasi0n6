#include <windows.h>
#include <commctrl.h>
#include <version.h>
#include "MainWnd.h"

#include <stdio.h>

static MainWnd* self;

static HWND panel1 = NULL;
static HWND panel2 = NULL;
static HWND lbTop = NULL;
static HWND lbDisc = NULL;
static HWND lbCopyright = NULL;
static HWND lbTwitter = NULL;
static HWND lbCredits = NULL;
static HWND lbPaypal = NULL;
static HWND lbHP = NULL;
static HWND e_logo = NULL;
static HICON e_logo_icon = NULL;

static UINT_PTR atntimer = 0;
static int laststate = 0;

typedef struct {
  UINT  cbSize;
  HWND  hwnd;
  DWORD dwFlags;
  UINT  uCount;
  DWORD dwTimeout;
} FLASHWINFO, *PFLASHWINFO;

#define FLASHW_STOP 0
#define FLASHW_ALL 0x3
#define FLASHW_TIMERNOFG 0xC

#define BLINK_TIMER_ID 17845

extern "C" {
extern BOOL WINAPI FlashWindowEx(PFLASHWINFO pfwi);
}

static int buttonExit = 0;

int MainWnd::msgBox(const char* message, const char* caption, int style)
{
	// get message type
	int mtype = 0;
	if (style & mb_ICON_INFO) {
		mtype = MB_ICONINFORMATION;
	} else if (style & mb_ICON_WARNING) {
		mtype = MB_ICONWARNING;
	} else if (style & mb_ICON_QUESTION) {
		mtype = MB_ICONQUESTION;
	} else if (style & mb_ICON_ERROR) {
		mtype = MB_ICONERROR;
	}

	// get button type(s)
	int btype = MB_OK;
	if (style & mb_OK) {
		btype = MB_OK;
	} else if (style & mb_CANCEL) {
		btype = MB_OK;
	} else if (style & mb_OK_CANCEL) {
		btype = MB_OKCANCEL;
	} else if (style & mb_YES_NO) {
		btype = MB_YESNO;
	}

	HWND hnd;
	if (this->mainwnd == NULL) {
		hnd = GetActiveWindow();
	} else {
		hnd = this->mainwnd;
	}

	int answer = MessageBox(hnd, utf8_to_wchar(message), utf8_to_wchar(caption), btype | mtype);

	switch (answer) {
	case IDOK:
		return mb_OK;
	case IDCANCEL:
		return mb_CANCEL;
	case IDYES:
		return mb_YES;
	case IDNO:
		return mb_NO;
	default:
		return -1;
	}
}

void MainWnd::configureButtonForExit()
{
        SetWindowText(this->btnStart, utf8_to_wchar(localize("Exit")));
	this->setButtonEnabled(1);
        buttonExit = 1;
}

void MainWnd::setButtonEnabled(int enabled)
{
	EnableWindow(this->btnStart, enabled);
}

void MainWnd::setStatusText(const char* text)
{
	SetWindowTextW(this->lbStatus, utf8_to_wchar(text));
	ShowWindow(this->lbStatus, SW_HIDE);
	ShowWindow(this->lbStatus, SW_SHOW);
}

void MainWnd::setProgress(int percentage)
{
	SendMessage(this->progressBar, PBM_SETPOS, (WPARAM)percentage, 0);
}

void CALLBACK blink_timer(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
	ShowWindow(self->lbStatus, (laststate == 0) ? SW_HIDE : SW_SHOW);
	laststate++;
	if (laststate > 3) {
		laststate = 0;
	}
}

void MainWnd::requestUserAttention(int level)
{
	this->cancelUserAttention();

	FLASHWINFO flash;
	flash.cbSize = sizeof(FLASHWINFO);
	flash.hwnd = this->mainwnd;
	flash.dwFlags = FLASHW_ALL | FLASHW_TIMERNOFG;
	flash.uCount = 0;
	flash.dwTimeout = 0;

	FlashWindowEx(&flash);

	if (level > 1) {
		laststate = 0;
		atntimer = SetTimer(mainwnd, BLINK_TIMER_ID, 500, blink_timer);
	}
}

void MainWnd::cancelUserAttention()
{
	FLASHWINFO flash;
	flash.cbSize = sizeof(FLASHWINFO);
	flash.hwnd = this->mainwnd;
	flash.dwFlags = FLASHW_STOP;
	flash.uCount = 0;
	flash.dwTimeout = 0;

	FlashWindowEx(&flash);

	if (atntimer != 0) {
		KillTimer(mainwnd, atntimer);
		atntimer = 0;
	}
	ShowWindow(self->lbStatus, SW_SHOW);
}

void MainWnd::handleStartClicked(void* data)
{
	this->cancelUserAttention();
	this->setButtonEnabled(0);
	this->setProgress(0);
	this->devhandler->processStart();
}

bool MainWnd::onClose(void* data)
{
	if (this->closeBlocked) {
		return 1;
	}
	return 0;
}

extern "C" {

LRESULT CALLBACK PaintWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
        switch(uMsg){
                case WM_PAINT:
                        if (hWnd == e_logo) {
                                DrawIconEx(GetDC(hWnd), 0, 0, e_logo_icon, 32, 32, 0, NULL, DI_NORMAL);
                        }
                        break;
                default:
                        break;
        }
        return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

static LRESULT CALLBACK PanelWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg){
		case WM_COMMAND:
			if ((HWND)lParam == lbPaypal) {
				ShellExecute(NULL, L"open", utf8_to_wchar(PAYPAL_LINK_URL), NULL, NULL, SW_SHOWNORMAL);
			} else if ((HWND)lParam == lbHP) {
				ShellExecute(NULL, L"open", utf8_to_wchar(HOMEPAGE_LINK_URL), NULL, NULL, SW_SHOWNORMAL);
			} else if ((HWND)lParam == lbTwitter) {
				ShellExecute(NULL, L"open", utf8_to_wchar(TWITTER_LINK_URL), NULL, NULL, SW_SHOWNORMAL);
			}
			break;
		case WM_DRAWITEM:
			{
			LPDRAWITEMSTRUCT lpDrawItem;
			//HBRUSH hBrush;
			//int state;

			lpDrawItem = (LPDRAWITEMSTRUCT) lParam;
			if (hWnd == panel1) {
				SetTextAlign(lpDrawItem->hDC, TA_LEFT | TA_TOP);
				SetBkMode(lpDrawItem->hDC,TRANSPARENT);
				wchar_t txt[256];
				GetWindowText(lpDrawItem->hwndItem, txt, 256);
				if (lpDrawItem->hwndItem == lbCopyright) {
					SetTextColor(lpDrawItem->hDC, RGB(0xf3, 0xf3, 0xf3));
					TextOut(lpDrawItem->hDC, 0, 3, txt, wcslen(txt));
					SetTextColor(lpDrawItem->hDC, RGB(0, 0, 0));
					TextOut(lpDrawItem->hDC, 0, 2, txt, wcslen(txt));
				} else {
					SetTextColor(lpDrawItem->hDC, RGB(0xd8, 0xd8, 0xd8));
					TextOut(lpDrawItem->hDC, 0, 3, txt, wcslen(txt));
					SetTextColor(lpDrawItem->hDC, RGB(0x11, 0x82, 0xe2));
					TextOut(lpDrawItem->hDC, 0, 2, txt, wcslen(txt));					
				}
			} else if (hWnd == panel2) {
				SetTextAlign(lpDrawItem->hDC, TA_CENTER | TA_TOP);
				SetBkMode(lpDrawItem->hDC,TRANSPARENT);
				wchar_t txt[256];
				GetWindowText(lpDrawItem->hwndItem, txt, 256);
				SetTextColor(lpDrawItem->hDC, RGB(0xd8, 0xd8, 0xd8));
				TextOut(lpDrawItem->hDC, 75, 3, txt, wcslen(txt));
				SetTextColor(lpDrawItem->hDC, RGB(0x11, 0x82, 0xe2));
				TextOut(lpDrawItem->hDC, 75, 2, txt, wcslen(txt));
			}

			return 1;
			}
			break;
		case WM_CTLCOLORSTATIC:
			if (((HWND)lParam == lbCopyright)
			)  {
				SetBkMode((HDC)wParam, TRANSPARENT);
				SetTextColor((HDC)wParam, RGB(0,0,0));
				return (LRESULT)GetStockObject(NULL_BRUSH);
			} else {
				return TRUE;
			}
		case WM_CTLCOLORBTN:
			if (((HWND)lParam == lbTwitter)
			 || ((HWND)lParam == lbPaypal)
			 || ((HWND)lParam == lbHP)
			) {
				return (LRESULT)GetStockObject(NULL_BRUSH);
			} else {
				return TRUE;
			}
		default:
			break;
	}
	return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

static LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg) {
		case WM_COMMAND:
			if ((HWND)lParam == self->btnStart) {
                                if(buttonExit)
                                {
				    DestroyWindow(self->mainwnd);
                                } else
                                {
                                    self->handleStartClicked(NULL);
                                }
			}
			break;
		case WM_CLOSE:
			if (hWnd == self->mainwnd) {
				if (self->onClose(NULL) == 0) {
					DestroyWindow(self->mainwnd);
				}
			}
			return 0;
		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
		case WM_DRAWITEM:
			{
			LPDRAWITEMSTRUCT lpDrawItem;
			lpDrawItem = (LPDRAWITEMSTRUCT) lParam;
			RECT rc = lpDrawItem->rcItem;
			SetBkMode(lpDrawItem->hDC, TRANSPARENT);
			wchar_t txt[256];
			GetWindowText(lpDrawItem->hwndItem, txt, 256);
			SetTextColor(lpDrawItem->hDC, RGB(0xf3, 0xf3, 0xf3));
			rc.top = rc.top+1;
			DrawText(lpDrawItem->hDC, txt, wcslen(txt), &rc, DT_LEFT | DT_TOP | DT_WORDBREAK);
			SetTextColor(lpDrawItem->hDC, RGB(0, 0, 0));
			rc.top = rc.top-1;
			DrawText(lpDrawItem->hDC, txt, wcslen(txt), &rc, DT_LEFT | DT_TOP | DT_WORDBREAK);
			return 1;
			}
			break;
		case WM_CTLCOLORSTATIC:
			if (((HWND)lParam == lbTop)
			 || ((HWND)lParam == self->lbStatus)
			 || ((HWND)lParam == lbDisc)
			 || ((HWND)lParam == lbCredits)
			)  {
				SetBkMode((HDC)wParam, TRANSPARENT);
				SetTextColor((HDC)wParam, RGB(0,0,0));
				return (LRESULT)GetStockObject(NULL_BRUSH);
			} else {
				return TRUE;
			}
		case WM_ERASEBKGND:
			if (hWnd == self->lbStatus)
				return FALSE;
			break;
		default:
			break;
	}
	return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

}

MainWnd::MainWnd(int* pargc, char*** pargv)
{
	HINSTANCE hInst = (HINSTANCE)*pargc;
	WNDCLASS WinClass;
	WNDCLASS PanelClass;
	WNDCLASS PictureClass;
	HFONT hFont1;
	HFONT hFont2;
	int xOffset = 0;
	int yOffset = 0;
	
	int outerwidth = WND_WIDTH + (GetSystemMetrics(SM_CXFIXEDFRAME)*2);
	int outerheight = WND_HEIGHT + (GetSystemMetrics(SM_CYFIXEDFRAME)*2) + GetSystemMetrics(SM_CYSIZE) + GetSystemMetrics(SM_CYBORDER);

	self = this;

	memset(&WinClass, 0, sizeof(WNDCLASS));

        char title[100];
        snprintf(title, sizeof(title), WND_TITLE, APPNAME, EVASI0N_VERSION_STRING);

	WinClass.style = CS_CLASSDC | CS_PARENTDC | CS_HREDRAW | CS_VREDRAW;
	WinClass.lpfnWndProc = (WNDPROC)WindowProc;
	WinClass.cbClsExtra = 0;
	WinClass.cbWndExtra = 0;
	WinClass.hInstance = hInst;
	WinClass.hIcon = LoadIcon(hInst, L"AppIcon");
	WinClass.hCursor = LoadCursor(0 , IDC_ARROW);
	WinClass.hbrBackground = (HBRUSH)CreatePatternBrush((HBITMAP)LoadBitmap(hInst, L"BACKGROUND"));
	WinClass.lpszMenuName = NULL;
	WinClass.lpszClassName = L"Evasi0n_CLASS";

	PanelClass.style = CS_CLASSDC | CS_PARENTDC;
	PanelClass.lpfnWndProc = (WNDPROC)PanelWindowProc;
	PanelClass.cbClsExtra = 0;
	PanelClass.cbWndExtra = 0;
	PanelClass.hInstance = hInst;
	PanelClass.hIcon = 0;
	PanelClass.hCursor = LoadCursor(0 , IDC_ARROW);
	PanelClass.hbrBackground = 0;
	PanelClass.lpszMenuName = NULL;
	PanelClass.lpszClassName = L"Panel";

	PictureClass.style = CS_OWNDC; // |CS_PARENTDC;
        PictureClass.lpfnWndProc = (WNDPROC)PaintWindowProc;
        PictureClass.cbClsExtra = 0;
        PictureClass.cbWndExtra = 0;
        PictureClass.hInstance = hInst;
        PictureClass.hIcon = 0;
        PictureClass.hCursor = 0;
        PictureClass.hbrBackground = 0;
        PictureClass.lpszMenuName = NULL;
        PictureClass.lpszClassName = L"Picture";

	if (!RegisterClass(&WinClass)) {
		msgBox("Error registering window class! This should not happen!", localize("Error"), MB_OK | MB_ICONSTOP);
		return;
	}

	if (!RegisterClass(&PanelClass)) {
		msgBox("Error registering panel class! This should not happen!", localize("Error"), MB_OK | MB_ICONSTOP);
		return;
	}

	if (!RegisterClass(&PictureClass)) {
		msgBox("Error registering picture class! This should not happen!", localize("Error"), MB_OK | MB_ICONSTOP);
		return;
	}

	xOffset = GetSystemMetrics(SM_CXSCREEN);
	yOffset = GetSystemMetrics(SM_CYSCREEN);

	xOffset = (xOffset - outerwidth) / 2;
	yOffset = (yOffset - outerheight) / 2;

	InitCommonControls();

	mainwnd = CreateWindowEx(0, WinClass.lpszClassName, utf8_to_wchar(title), WS_VISIBLE | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, xOffset, yOffset, outerwidth, outerheight, 0, 0, hInst, NULL);
	if (mainwnd == NULL) {
		return;
	}

	lbTop = CreateWindowEx(0, L"Static", utf8_to_wchar(WELCOME_LABEL_TEXT), WS_VISIBLE | WS_CHILD | SS_NOPREFIX | SS_OWNERDRAW, 10, 10, WND_WIDTH-20, 20, mainwnd, (HMENU)1, hInst, NULL);

	lbStatus = CreateWindowEx(0, L"Static", L"", WS_VISIBLE | WS_CHILD | SS_NOPREFIX | SS_OWNERDRAW, 10, 50, WND_WIDTH-20, 50, mainwnd, (HMENU)2, hInst, NULL);

	e_logo = CreateWindow(L"Picture", NULL, WS_VISIBLE | WS_CHILD | SS_SUNKEN, 10, 104, 32, 32, mainwnd, 0, hInst, NULL);
	e_logo_icon = LoadIcon(hInst, L"LOGO");

	progressBar = CreateWindowEx(0, PROGRESS_CLASS, NULL, WS_CHILD | WS_VISIBLE | PBS_SMOOTH, 48, 110, 318, 17, mainwnd, (HMENU)3, hInst, NULL);
	SendMessage(progressBar, PBM_SETRANGE, 0, MAKELPARAM(0,100));
	SendMessage(progressBar, PBM_SETPOS, (WPARAM)0, 0);

	btnStart = CreateWindowEx(0, L"Button", utf8_to_wchar(BTN_START_TEXT), WS_VISIBLE | WS_CHILD | WS_TABSTOP | WS_DISABLED, 383, 107, 80, 21, mainwnd, (HMENU)4, hInst, NULL);

	CreateWindowEx(0, L"Static", NULL, WS_CHILD | WS_VISIBLE | SS_NOPREFIX | WS_BORDER, 10, 143, WND_WIDTH-26, 1, mainwnd, (HMENU)4, hInst, NULL);

	lbDisc = CreateWindowEx(0, L"Static", utf8_to_wchar(DISCLAIMER_LABEL_TEXT), WS_CHILD | WS_VISIBLE | SS_NOPREFIX | SS_OWNERDRAW, 10, 159, WND_WIDTH-20, 50, mainwnd, (HMENU)9, hInst, NULL);

	panel1 = CreateWindowEx(WS_EX_CONTROLPARENT, PanelClass.lpszClassName, L"", WS_VISIBLE | WS_CHILD, 0, 218, WND_WIDTH, 19, mainwnd, (HMENU)6, hInst, NULL);

	lbCopyright = CreateWindowEx(0, L"Static", utf8_to_wchar(COPYRIGHT_LABEL_TEXT), WS_CHILD | WS_VISIBLE | SS_NOPREFIX | SS_OWNERDRAW, 10, 2, 100, 17, panel1, (HMENU)10, hInst, NULL);

	lbTwitter = CreateWindowEx(0, L"Button", utf8_to_wchar(TWITTER_LINK_TEXT), WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 107, 2, 60, 19, panel1, (HMENU)11, hInst, NULL);

	lbCredits = CreateWindowEx(0, L"Static", utf8_to_wchar(CREDITS_LABEL_TEXT), WS_CHILD | WS_VISIBLE | SS_NOPREFIX | SS_OWNERDRAW, 10, 238, WND_WIDTH-20, 36, mainwnd, (HMENU)5, hInst, NULL);

	panel2 = CreateWindowEx(WS_EX_CONTROLPARENT, PanelClass.lpszClassName, L"", WS_VISIBLE | WS_CHILD, 0, 274, WND_WIDTH, 30, mainwnd, (HMENU)6, hInst, NULL);

	lbPaypal = CreateWindowEx(0, L"Button", utf8_to_wchar(PAYPAL_LINK_TEXT), WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 40, 4, 150, 20, panel2, (HMENU)7, hInst, NULL);

	lbHP = CreateWindowEx(0, L"Button", utf8_to_wchar(HOMEPAGE_LINK_TEXT), WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 280, 4, 150, 20, panel2, (HMENU)8, hInst, NULL);

	//+++ create font +++
	hFont1 = CreateFont(-13, 0, 0, 0, 400, 0, 0, 0, DEFAULT_CHARSET,
				OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
				DEFAULT_PITCH | FF_DONTCARE, L"Tahoma");

	if (hFont1 == NULL) {
		hFont1 = CreateFont(-12, 0, 0, 0, 400, 0, 0, 0, DEFAULT_CHARSET,
					OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
					DEFAULT_PITCH | FF_DONTCARE, L"MS Sans Serif");
	}

	hFont2 = CreateFont(-12, 0, 0, 0, 400, 0, 0, 0, DEFAULT_CHARSET,
				OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
				DEFAULT_PITCH | FF_DONTCARE, L"Tahoma");

	if (hFont2 == NULL) {
		hFont2 = CreateFont(-12, 0, 0, 0, 400, 0, 0, 0, DEFAULT_CHARSET,
					OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
					DEFAULT_PITCH | FF_DONTCARE, L"MS Sans Serif");
	}

	if (hFont1) {
		SendMessage(mainwnd, WM_SETFONT, (DWORD)hFont1, 0);
		SendMessage(lbTop, WM_SETFONT, (DWORD)hFont1, 0);
		SendMessage(lbStatus, WM_SETFONT, (DWORD)hFont1, 0);
		SendMessage(btnStart, WM_SETFONT, (DWORD)hFont1, 0);
		SendMessage(lbCopyright, WM_SETFONT, (DWORD)hFont1, 0);
		SendMessage(lbTwitter, WM_SETFONT, (DWORD)hFont1, 0);
		SendMessage(lbDisc, WM_SETFONT, (DWORD)hFont2, 0);
		SendMessage(lbCredits, WM_SETFONT, (DWORD)hFont1, 0);
		SendMessage(lbPaypal, WM_SETFONT, (DWORD)hFont1, 0);
		SendMessage(lbHP, WM_SETFONT, (DWORD)hFont1, 0);
	}

	UpdateWindow(mainwnd);
	this->closeBlocked = 0;
	this->devhandler = new DeviceHandler(this);
}

void MainWnd::run(void)
{
	MSG lpMsg;

	while (GetMessageW(&lpMsg, NULL, 0, 0)) {
		if (!IsDialogMessage(this->mainwnd, &lpMsg)) {
			TranslateMessage(&lpMsg);
			DispatchMessageW(&lpMsg);
		} else {
			if (lpMsg.hwnd != this->mainwnd) {
				WindowProc(lpMsg.hwnd, lpMsg.message, lpMsg.wParam, lpMsg.lParam);
			}
		}
	}
}
