﻿/*-----------------------------------------------------------------------------
 * File: main.cpp
 *-----------------------------------------------------------------------------
 * Copyright (c) 2005       Kazuo Ishii <k-ishii@wb4.so-net.ne.jp>
 *				- original version
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *---------------------------------------------------------------------------*/
#include "ckw.h"
#include "rsrc.h"
#include "encoding.h"
#include "winmng.h"
#include <string>
#include <imm.h>
#include <dwmapi.h>
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

/*****************************************************************************/

HANDLE	gStdIn = nullptr;	/* console */
HANDLE	gStdOut = nullptr;
HANDLE	gStdErr = nullptr;
HWND	gConWnd = nullptr;

winmng gWinMng;

HANDLE	gChild = nullptr;	/* child process */

LOGFONT	gFontLog;	/* font IME */
HFONT	gFont;		/* font */
DWORD	gFontW;		/* char width */
DWORD	gFontH;		/* char height */
int		gFontSize;
UINT	gCodePage;

DWORD	gWinW;		/* window columns */
DWORD	gWinH;		/* window rows */

RECT	gFrame;		/* window frame size */
HBITMAP gBgBmp = nullptr;	/* background image */
HBRUSH	gBgBrush = nullptr;/* background brush */
DWORD	gBorderSize = 0;/* internal border */
DWORD	gLineSpace = 0;	/* line space */
bool	gVScrollHide = false;

BOOL	gImeOn = FALSE; /* IME-status */

bool	gMinimizeToTray = false; /* minimize to task tray */
int		gBgBmpPosOpt = 0;
POINT	gBgBmpPoint = {};
POINT	gBgBmpSize = {};
BOOL	gCurBlink = FALSE;
DWORD	gCurBlinkNext = 0;
BOOL	gCurHide = FALSE;
bool	gFocus = false;
bool	gNoAutoClose = false;
bool	gMouseEvent = false;

/* screen buffer - copy */
CONSOLE_SCREEN_BUFFER_INFO* gCSI = nullptr;
CHAR_INFO*	gScreen = nullptr;
wchar_t*	gTitle = nullptr;

/* index color */
enum {
	kColor0 = 0,
	          kColor1,  kColor2,  kColor3,
	kColor4,  kColor5,  kColor6,  kColor7,
	kColor8,  kColor9,  kColor10, kColor11,
	kColor12, kColor13, kColor14, kColor15,
	kColorCursorFg,
	kColorCursorBg,
	kColorCursorImeFg,
	kColorCursorImeBg,
	/**/
	kColorMax,
};
COLORREF gColorTable[ kColorMax ];

static BOOL create_font(const wchar_t* name, int height);
static BOOL checkDarkMode();

/*****************************************************************************/

#if 0
void trace(const wchar_t *msg)
{
	OutputDebugString(msg);
}
#else
#define trace(msg)
#endif

/*****************************************************************************/

/*----------*/
inline void __draw_invert_char_rect(HDC hDC, RECT& rc)
{
	rc.right++;
	rc.bottom++;
	rc.left   *= gFontW;
	rc.right  *= gFontW;
	rc.top    *= gFontH;
	rc.bottom *= gFontH;
	BitBlt(hDC, rc.left, rc.top, rc.right-rc.left, rc.bottom-rc.top, nullptr,0,0, DSTINVERT);
}

/*----------*/
static void __draw_selection(HDC hDC)
{
	SMALL_RECT sel;
	if(!selectionGetArea(sel))
		return;

	if(gCSI->srWindow.Top <= sel.Top && sel.Top <= gCSI->srWindow.Bottom)
		;
	else if(gCSI->srWindow.Top <= sel.Bottom && sel.Bottom <= gCSI->srWindow.Bottom)
		;
	else if(sel.Top < gCSI->srWindow.Top && gCSI->srWindow.Bottom < sel.Bottom)
		;
	else
		return;

	RECT	rc;

	if(sel.Top == sel.Bottom) {
		/* single line */
		rc.left  = sel.Left - gCSI->srWindow.Left;
		rc.right = sel.Right-1 - gCSI->srWindow.Left;
		rc.top   = \
		rc.bottom = sel.Top - gCSI->srWindow.Top;
		__draw_invert_char_rect(hDC, rc);
		return;
	}

	/* multi line */
	if(gCSI->srWindow.Top <= sel.Top && sel.Top <= gCSI->srWindow.Bottom) {
		/* top */
		rc.left = sel.Left - gCSI->srWindow.Left;
		rc.right = gCSI->srWindow.Right - gCSI->srWindow.Left;
		rc.top = \
		rc.bottom = sel.Top - gCSI->srWindow.Top;
		__draw_invert_char_rect(hDC, rc);
	}
	if(sel.Top+1 <= sel.Bottom-1) {
		/* center */
		rc.left = 0;
		rc.right = gCSI->srWindow.Right - gCSI->srWindow.Left;

		if(gCSI->srWindow.Top <= sel.Top+1)
			rc.top = sel.Top+1 - gCSI->srWindow.Top;
		else
			rc.top = 0;

		if(gCSI->srWindow.Bottom >= sel.Bottom-1)
			rc.bottom = sel.Bottom-1 - gCSI->srWindow.Top;
		else
			rc.bottom = gCSI->srWindow.Bottom - gCSI->srWindow.Top;
		__draw_invert_char_rect(hDC, rc);
	}
	if(gCSI->srWindow.Top <= sel.Bottom && sel.Bottom <= gCSI->srWindow.Bottom) {
		/* bottom */
		rc.left = 0;
		rc.right = sel.Right-1 - gCSI->srWindow.Left;
		rc.top = \
		rc.bottom = sel.Bottom - gCSI->srWindow.Top;
		__draw_invert_char_rect(hDC, rc);
	}
}

static UINT get_ucs(CHAR_INFO *ptr)
{
	UINT ucs;
	if(IS_SURROGATE_PAIR(ptr->Char.UnicodeChar, (ptr+1)->Char.UnicodeChar) == true) {
		ucs = surrogate_to_ucs(ptr->Char.UnicodeChar, (ptr+1)->Char.UnicodeChar);
	} else {
		ucs = static_cast<UINT>(ptr->Char.UnicodeChar);
	}
	return ucs;
}

/*----------*/
/*
 CP932(TTF)   : 全角文字は2回出現。AttributesにCOMMON_LVB_*が立つ
 CP65001      : 全角文字は1回のみ。CSIの座標とCHAR_INFOの座標は一致しない
 */
static void __draw_screen(HDC hDC)
{
	int	pntX, pntY;
	int	x, y;
	int	color_fg;
	int	color_bg;
	CHAR_INFO* ptr = gScreen;
	int	 work_color_fg = -1;
	int	 work_color_bg = -1;
	wchar_t* work_text = new wchar_t[ CSI_WndCols(gCSI) * 2 ];
	INT*	 work_width = new INT[ CSI_WndCols(gCSI) * 2 ];
	int	 work_pntX;

	pntY = 0;
	for(y = gCSI->srWindow.Top ; y <= gCSI->srWindow.Bottom ; y++) {
		pntX = 0;
		work_pntX = 0;
		wchar_t* work_text_ptr = work_text;
		INT* work_width_ptr = work_width;
		for(x = gCSI->srWindow.Left ; x <= gCSI->srWindow.Right ; x++) {

			if(ptr->Attributes & COMMON_LVB_TRAILING_BYTE) {
				ptr++;
				continue;
			}
			if(IS_LOW_SURROGATE(ptr->Char.UnicodeChar) == true) {
				if (x != gCSI->srWindow.Left) {
					*work_text_ptr++ = ptr->Char.UnicodeChar;
					*work_width_ptr++ = 0;
					ptr++;
					continue;
				}
			}

			color_fg = ptr->Attributes & 0xF;
			color_bg = (ptr->Attributes>>4) & 0xF;

			if(color_fg != work_color_fg ||
			   color_bg != work_color_bg) {
				if(work_text_ptr > work_text) {
					ExtTextOut(hDC, work_pntX, pntY, 0, nullptr,
						   (LPCWSTR)work_text,
						   (UINT)(work_text_ptr - work_text),
						   work_width);
				}
				work_text_ptr = work_text;
				work_width_ptr = work_width;
				work_pntX = pntX;
				work_color_fg = color_fg;
				work_color_bg = color_bg;
				SetTextColor(hDC, gColorTable[work_color_fg]);
				SetBkColor(  hDC, gColorTable[work_color_bg]);
				SetBkMode(hDC, (work_color_bg) ? OPAQUE : TRANSPARENT);
			}

			if (is_dblchar_cjk(get_ucs(ptr)) == true) {
				// Double width
				*work_text_ptr++ = ptr->Char.UnicodeChar;
				*work_width_ptr++ = gFontW * 2;
				pntX += gFontW;
			}
			else {
				*work_text_ptr++ = ptr->Char.UnicodeChar;
				*work_width_ptr++ = gFontW;
			}
			pntX += gFontW;
			ptr++;
		}

		if(work_text_ptr > work_text) {
			ExtTextOut(hDC, work_pntX, pntY, 0, nullptr,
				   (LPCWSTR)work_text,
				   (UINT)(work_text_ptr - work_text),
				   work_width);
		}

		pntY += gFontH;
	}

	/* draw selection */
	__draw_selection(hDC);

	/* draw cursor */
	if(gCSI->srWindow.Top    <= gCSI->dwCursorPosition.Y &&
	   gCSI->srWindow.Bottom >= gCSI->dwCursorPosition.Y &&
	   gCSI->srWindow.Left   <= gCSI->dwCursorPosition.X &&
	   gCSI->srWindow.Right  >= gCSI->dwCursorPosition.X) {
		color_fg = (gImeOn) ? kColorCursorImeFg : kColorCursorFg;
		color_bg = (gImeOn) ? kColorCursorImeBg : kColorCursorBg;
		pntX = gCSI->dwCursorPosition.X - gCSI->srWindow.Left;
		pntY = gCSI->dwCursorPosition.Y - gCSI->srWindow.Top;

		ptr = gScreen + CSI_WndCols(gCSI) * pntY;
		work_pntX = 0;
		for (int i=0; i<pntX; i++) {
			if((ptr->Attributes & COMMON_LVB_TRAILING_BYTE)) {
				ptr++;
				continue;
			}
			if(IS_LOW_SURROGATE(ptr->Char.UnicodeChar) == true) {
				work_pntX += gFontW;
				ptr++;
				continue;
			}
			if(IS_HIGH_SURROGATE(ptr->Char.UnicodeChar) == true) {
				work_pntX += gFontW;
				ptr++;
				continue;
			}
			if (is_dblchar_cjk(get_ucs(ptr)) == true) {
				work_pntX += gFontW * 2;
			} else {
				work_pntX += gFontW;
			}
			ptr++;
		}

		ptr = gScreen + CSI_WndCols(gCSI) * pntY + pntX;
		pntX *= gFontW;
		pntY *= gFontH;
		if (is_dblchar_cjk(get_ucs(ptr)) == true) {
			*work_width = gFontW*2;
		} else {
			*work_width = gFontW;
		}
		if(!gFocus){
			HPEN hPen = CreatePen(PS_SOLID, 1, gColorTable[ color_bg ]);
			HPEN hOldPen = (HPEN)SelectObject(hDC, hPen);
			HBRUSH hOldBrush = (HBRUSH)SelectObject(hDC, GetStockObject(NULL_BRUSH));
			Rectangle(hDC, work_pntX, pntY, work_pntX+(*work_width), pntY+gFontH);
			SelectObject(hDC, hOldPen); DeleteObject(hPen);
			SelectObject(hDC, hOldBrush);
		}else if(!gCurHide){
			SetTextColor(hDC, gColorTable[ color_fg ]);
			SetBkColor(  hDC, gColorTable[ color_bg ]);
			SetBkMode(hDC, OPAQUE);
			if(IS_SURROGATE_PAIR(ptr->Char.UnicodeChar, (ptr+1)->Char.UnicodeChar) == true) {
				wchar_t src[2];
				src[0] = ptr->Char.UnicodeChar;
				src[1] = (ptr+1)->Char.UnicodeChar;
				*work_width = *work_width/2;		// 苦肉の策
				ExtTextOut(hDC, work_pntX, pntY, ETO_CLIPPED, nullptr,
						src, 2, work_width);
			} else {
				ExtTextOut(hDC, work_pntX, pntY, 0, nullptr,
						&ptr->Char.UnicodeChar, 1, work_width);
			}
		}
	}

	delete [] work_width;
	delete [] work_text;
}

/*----------*/
void	onPaint(HWND hWnd)
{
	PAINTSTRUCT ps;
	HDC	hDC = BeginPaint(hWnd, &ps);
	RECT	rc;
	GetClientRect(hWnd, &rc);

	HDC	hMemDC = CreateCompatibleDC(hDC);
	HBITMAP	hBmp = CreateCompatibleBitmap(hDC, rc.right-rc.left, rc.bottom-rc.top);
	HGDIOBJ	oldfont = SelectObject(hMemDC, gFont);
	HGDIOBJ	oldbmp  = SelectObject(hMemDC, hBmp);

	FillRect(hMemDC, &rc, gBgBrush);

	if((gBgBmp) && (gBgBmpPosOpt > 0)) {
		HDC	hBgBmpDC = CreateCompatibleDC(hDC);
		SelectObject(hBgBmpDC, gBgBmp);
		switch(gBgBmpPosOpt) {
		case 1:	case 2:	case 3:	case 4:
			BitBlt(hMemDC,
				   ((gBgBmpPosOpt == 1) || (gBgBmpPosOpt == 3)) ? 0 : rc.right  - gBgBmpSize.x,
				   ((gBgBmpPosOpt == 1) || (gBgBmpPosOpt == 2)) ? 0 : rc.bottom - gBgBmpSize.y,
				   gBgBmpSize.x, gBgBmpSize.y, hBgBmpDC, 0, 0, SRCCOPY);
			break;
		case 5:	case 6:	case 7:	default:
			SetStretchBltMode(hBgBmpDC, COLORONCOLOR);
			StretchBlt(hMemDC, 0, 0,
					   gBgBmpPosOpt == 6 ? (int)(((double)rc.bottom / gBgBmpSize.y) * gBgBmpSize.x) : rc.right,
					   gBgBmpPosOpt == 5 ? (int)(((double)rc.right  / gBgBmpSize.x) * gBgBmpSize.y) : rc.bottom,
					   hBgBmpDC, 0, 0, gBgBmpSize.x, gBgBmpSize.y, SRCCOPY);
			break;
		}
		DeleteDC(hBgBmpDC);
	}

	if(gScreen && gCSI) {
		SetWindowOrgEx(hMemDC, -(int)gBorderSize, -(int)gBorderSize, nullptr);
		__draw_screen(hMemDC);
		SetWindowOrgEx(hMemDC, 0, 0, nullptr);
	}

	BitBlt(hDC,rc.left,rc.top, rc.right-rc.left, rc.bottom-rc.top, hMemDC,0,0, SRCCOPY);

	SelectObject(hMemDC, oldfont);
	SelectObject(hMemDC, oldbmp);
	DeleteObject(hBmp);
	DeleteDC(hMemDC);

	EndPaint(hWnd, &ps);
}

static int get_dpi()
{
	HDC	hDC = GetDC(nullptr);
	int pixlsy = GetDeviceCaps(hDC, LOGPIXELSY);
	ReleaseDC(nullptr, hDC);
	return pixlsy;
}

/*----------*/
static void __set_console_window_size(LONG cols, LONG rows)
{
	CONSOLE_SCREEN_BUFFER_INFO csi;
	GetConsoleScreenBufferInfo(gStdOut, &csi);

	gWinW = cols;
	gWinH = rows;

	if(cols == CSI_WndCols(&csi) && rows == CSI_WndRows(&csi))
		return;

	//SMALL_RECT tmp = { 0,0,0,0 };
	//SetConsoleWindowInfo(gStdOut, TRUE, &tmp);

	csi.dwSize.X = (SHORT)cols;
	csi.srWindow.Left = 0;
	csi.srWindow.Right = (SHORT)(cols -1);

	if(csi.dwSize.Y < rows || csi.dwSize.Y == CSI_WndRows(&csi))
		csi.dwSize.Y = (SHORT)rows;

	csi.srWindow.Bottom += (SHORT)(rows - CSI_WndRows(&csi));
	if(csi.dwSize.Y <= csi.srWindow.Bottom) {
		csi.srWindow.Top -= csi.srWindow.Bottom - csi.dwSize.Y +1;
		csi.srWindow.Bottom = csi.dwSize.Y -1;
	}

	SetConsoleScreenBufferSize(gStdOut, csi.dwSize);
	SetConsoleWindowInfo(gStdOut, TRUE, &csi.srWindow);
}

/*----------*/
void	onSizing(HWND hWnd, DWORD side, LPRECT rc)
{
	trace(L"onSizing\n");
	LONG fw = (gFrame.right - gFrame.left) + (gBorderSize * 2);
	LONG fh = (gFrame.bottom - gFrame.top) + (gBorderSize * 2);
	LONG width  = rc->right - rc->left;
	LONG height = rc->bottom - rc->top;

	width  -= fw;
	width  -= width  % gFontW;
	width  += fw;

	height -= fh;
	height -= height % gFontH;
	height += fh;

	if(side==WMSZ_LEFT || side==WMSZ_TOPLEFT || side==WMSZ_BOTTOMLEFT)
		rc->left = rc->right - width;
	else
		rc->right = rc->left + width;

	if(side==WMSZ_TOP || side==WMSZ_TOPLEFT || side==WMSZ_TOPRIGHT)
		rc->top = rc->bottom - height;
	else
		rc->bottom = rc->top + height;
}

/*----------*/
void	onWindowPosChange(HWND hWnd, WINDOWPOS* wndpos)
{
	trace(L"onWindowPosChange\n");
	if(!(wndpos->flags & SWP_NOSIZE) && !IsIconic(hWnd)) {
		LONG fw = (gFrame.right - gFrame.left) + (gBorderSize * 2);
		LONG fh = (gFrame.bottom - gFrame.top) + (gBorderSize * 2);
		LONG width  = wndpos->cx;
		LONG height = wndpos->cy;
		width  = (width - fw) / gFontW;
		height = (height - fh) / gFontH;

		__set_console_window_size(width, height);

		wndpos->cx = width  * gFontW + fw;
		wndpos->cy = height * gFontH + fh;
	}
}

static void __set_ime_position(HWND hWnd)
{
	if(!gImeOn || !gCSI) return;
	HIMC imc = ImmGetContext(hWnd);
	LONG px = gCSI->dwCursorPosition.X - gCSI->srWindow.Left;
	LONG py = gCSI->dwCursorPosition.Y - gCSI->srWindow.Top;

	CHAR_INFO *ptr = gScreen + CSI_WndCols(gCSI) * py;
	LONG work_px = 0;
	for (int i=0; i<px; i++) {
		if (IS_LOW_SURROGATE(ptr->Char.UnicodeChar) == true) {
			ptr++;
			continue;
		}
		if (is_dblchar_cjk(get_ucs(ptr)) == true) {
			if((ptr->Attributes & COMMON_LVB_TRAILING_BYTE)) {
				// do nothing
			} else {
				work_px += 2;
			}
		} else {
			work_px += 1;
		}
		ptr++;
	}
	px = work_px;

	COMPOSITIONFORM cf;
	cf.dwStyle = CFS_POINT;
	cf.ptCurrentPos.x = px * gFontW + gBorderSize;
	cf.ptCurrentPos.y = py * gFontH + gBorderSize;
	ImmSetCompositionWindow(imc, &cf);
	ImmReleaseContext(hWnd, imc);
}

/*----------*/
void	onTimer(HWND hWnd)
{
	if (gNoAutoClose == false) {
		if(WaitForSingleObject(gChild, 0) != WAIT_TIMEOUT) {
			PostMessage(hWnd, WM_CLOSE, 0,0);
			return;
		}
	}

	// check codepage
	UINT codepage = GetConsoleCP();
	if (gCodePage != codepage) {
		gCodePage = codepage;
		CONSOLE_FONT_INFOEX info = {};
		info.cbSize       = sizeof(info);
		info.FontWeight   = FW_NORMAL;
		info.dwFontSize.X = 3;
		info.dwFontSize.Y = 6;
		lstrcpyn(info.FaceName, L"MS GOTHIC", LF_FACESIZE);
		SetCurrentConsoleFontEx(gStdOut, FALSE, &info);
	}

	/* refresh handle */
	if(gStdOut) CloseHandle(gStdOut);
	gStdOut = CreateFile(L"CONOUT$", GENERIC_READ|GENERIC_WRITE,
			     FILE_SHARE_READ|FILE_SHARE_WRITE,
			     nullptr, OPEN_EXISTING, 0, nullptr);

	/* title update */
	static int timer_count = 0;
	if((++timer_count & 0xF) == 1) {
		wchar_t *str = new wchar_t[256];
		GetConsoleTitle(str, 256);
		if(gTitle && !wcscmp(gTitle, str)) {
			delete [] str;
		}
		else {
			delete [] gTitle;
			gTitle = str;
			SetWindowText(hWnd, gTitle);
			updateTrayTip(hWnd, gTitle);
		}
	}

	CONSOLE_SCREEN_BUFFER_INFO* csi = new CONSOLE_SCREEN_BUFFER_INFO;
	COORD	size;

	GetConsoleScreenBufferInfo(gStdOut, csi);
	size.X = CSI_WndCols(csi);
	size.Y = CSI_WndRows(csi);

	/* copy screen buffer */
	DWORD      nb = size.X * size.Y;
	CHAR_INFO* buffer = new CHAR_INFO[nb];
	CHAR_INFO* ptr = buffer;
	SMALL_RECT sr = {};
	COORD      pos = {};

	/* ReadConsoleOuput - maximum read size 64kByte?? */
	size.Y = 0x8000 / sizeof(CHAR_INFO) / size.X;
	sr.Left  = csi->srWindow.Left;
	sr.Right = csi->srWindow.Right;
	sr.Top   = csi->srWindow.Top;
	do {
		sr.Bottom = sr.Top + size.Y -1;
		if(sr.Bottom > csi->srWindow.Bottom) {
			sr.Bottom = csi->srWindow.Bottom;
			size.Y = sr.Bottom - sr.Top +1;
		}
		ReadConsoleOutput(gStdOut, ptr, size, pos, &sr);
		ptr += size.X * size.Y;
		sr.Top = sr.Bottom +1;
	} while(sr.Top <= csi->srWindow.Bottom);

	/* cursor blink */
	if(gCurBlink && gFocus) {
		static DWORD caret_blink_time = (DWORD)GetCaretBlinkTime();
		DWORD curr = GetTickCount();
		if(curr >= gCurBlinkNext) {
			gCurHide = !gCurHide;
			gCurBlinkNext = curr + caret_blink_time;
			InvalidateRect(hWnd, nullptr, TRUE);
		}
	}

	/* compare */
	if(gScreen && gCSI &&
	   !memcmp(csi, gCSI, sizeof(CONSOLE_SCREEN_BUFFER_INFO)) &&
	   !memcmp(buffer, gScreen, sizeof(CHAR_INFO) * nb)) {
		/* no modified */
		delete [] buffer;
		delete csi;
		return;
	}

	/* swap buffer */
	if(gScreen) delete [] gScreen;
	if(gCSI) delete gCSI;
	gScreen = buffer;
	gCSI = csi;

	/* redraw request */
	InvalidateRect(hWnd, nullptr, TRUE);

	/* set vertical scrollbar status */
	if(!gVScrollHide) {
		SCROLLINFO si;
		si.cbSize = sizeof(si);
		si.fMask = SIF_DISABLENOSCROLL | SIF_POS | SIF_PAGE | SIF_RANGE;
		si.nPos = gCSI->srWindow.Top;
		si.nPage = CSI_WndRows(gCSI);
		si.nMin = 0;
		si.nMax = gCSI->dwSize.Y-1;
		SetScrollInfo(hWnd, SB_VERT, &si, TRUE);
	}

	if(gImeOn) {
		__set_ime_position(hWnd);
	}

	DWORD w = CSI_WndCols(gCSI);
	DWORD h = CSI_WndRows(gCSI);
	if(gWinW != w || gWinH != h) {
		w = (w * gFontW) + (gBorderSize * 2) + (gFrame.right - gFrame.left);
		h = (h * gFontH) + (gBorderSize * 2) + (gFrame.bottom - gFrame.top);
		SetWindowPos(hWnd, nullptr, 0,0,w,h, SWP_NOMOVE|SWP_NOZORDER);
	}
}

// ckw to console
static LPARAM convert_position(LPARAM lp)
{
	int x = GET_X_LPARAM(lp);
	int y = GET_Y_LPARAM(lp);
	x -= gBorderSize;
	y -= gBorderSize;
	if (x<0) x=0;
	if (y<0) x=0;
	x = x * 3 / gFontW;
	y = y * 6 / gFontH;
	return MAKELPARAM(x,y);
}

/*****************************************************************************/

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp)
{
	switch(msg) {
	case WM_CREATE:
		{
			HIMC imc = ImmGetContext(hWnd);
			ImmSetCompositionFontW(imc, &gFontLog);
			ImmReleaseContext(hWnd, imc);
		}
		SetTimer(hWnd, 0x3571, 10, nullptr);
		break;
	case WM_DESTROY:
		sysicon_destroy(hWnd);

		KillTimer(hWnd, 0x3571);
		PostQuitMessage(0);
		if(WaitForSingleObject(gChild, 0) == WAIT_TIMEOUT)
			PostMessage(gConWnd, WM_CLOSE, 0, 0);
		break;
	case WM_TIMER:
		onTimer(hWnd);
		break;

	case WM_ERASEBKGND:
		break;
	case WM_PAINT:
		onPaint(hWnd);
		break;

	case WM_SIZING:
		onSizing(hWnd, (DWORD)wp, (LPRECT)lp);
		break;
	case WM_WINDOWPOSCHANGING:
	case WM_WINDOWPOSCHANGED:
		onWindowPosChange(hWnd, (WINDOWPOS*)lp);
		selectionClear(hWnd);

		// WM_WINDOWPOSCHANGEDでDefWindowProcを呼ばないとWM_SIZEが送信されない
		// http://msdn.microsoft.com/en-us/library/ms632652.aspx
		return( DefWindowProc(hWnd, msg, wp, lp) );
	case WM_LBUTTONDOWN:
		onLBtnDown(hWnd, (short)LOWORD(lp), (short)HIWORD(lp));
		if (gMouseEvent == true) {
			PostMessage(gConWnd, msg, wp, convert_position(lp));
		}
		break;
	case WM_LBUTTONUP:
		onLBtnUp(hWnd, (short)LOWORD(lp), (short)HIWORD(lp));
		if (gMouseEvent == true) {
			PostMessage(gConWnd, msg, wp, convert_position(lp));
		}
		break;
	case WM_MOUSEMOVE:
		onMouseMove(hWnd, (short)LOWORD(lp),(short)HIWORD(lp));
		// scroll when mouse is outside (craftware)
		{
			int x = GET_X_LPARAM(lp);
			int y = GET_Y_LPARAM(lp);

			RECT rc;
			GetClientRect(hWnd, &rc);

			if( y<0 ) {
				PostMessage(gConWnd, WM_MOUSEWHEEL, WHEEL_DELTA<<16, MAKELPARAM(x,y));
			}
			else if(y>=rc.bottom) {
				PostMessage(gConWnd, WM_MOUSEWHEEL, -WHEEL_DELTA<<16, MAKELPARAM(x,y));
			}
		}
		if (gMouseEvent == true) {
			PostMessage(gConWnd, msg, wp, convert_position(lp));
		}
		break;
	case WM_MBUTTONUP:
	case WM_MBUTTONDOWN:
		if (gMouseEvent == true) {
			PostMessage(gConWnd, msg, wp, convert_position(lp));
		}
		break;
	case WM_RBUTTONUP:
	//case WM_RBUTTONDOWN:
		onPasteFromClipboard(hWnd);
		break;

	case WM_DROPFILES:
		onDropFile((HDROP)wp);
		break;

	case WM_IME_STARTCOMPOSITION:
		__set_ime_position(hWnd);
		return( DefWindowProc(hWnd, msg, wp, lp) );
	case WM_IME_NOTIFY:
		if(wp == IMN_SETOPENSTATUS) {
			HIMC imc = ImmGetContext(hWnd);
			gImeOn = ImmGetOpenStatus(imc);
			ImmReleaseContext(hWnd, imc);
			InvalidateRect(hWnd, nullptr, TRUE);
		}
		return( DefWindowProc(hWnd, msg, wp, lp) );

	case WM_SYSCOMMAND:
		if(!onSysCommand(hWnd, (DWORD)wp))
			return( DefWindowProc(hWnd, msg, wp, lp) );
		break;
	case WM_VSCROLL:
		/* throw console window */
		PostMessage(gConWnd, msg, wp, lp);
		break;

	case WM_MOUSEWHEEL:
		if (GET_KEYSTATE_WPARAM(wp) & MK_CONTROL) {
			short delta = GET_WHEEL_DELTA_WPARAM(wp);
			if (delta != 0) {
				DeleteObject(gFont);
				gFontSize += (delta > 0) ? 1 : -1;
				if (gFontSize <= 2) gFontSize = 2;
				std::wstring facename = gFontLog.lfFaceName;
				create_font(facename.c_str(), gFontSize);
				DWORD w = (gWinW * gFontW) + (gBorderSize * 2) + (gFrame.right - gFrame.left);
				DWORD h = (gWinH * gFontH) + (gBorderSize * 2) + (gFrame.bottom - gFrame.top);
				SetWindowPos(hWnd, nullptr, 0,0,w,h, SWP_NOMOVE|SWP_NOZORDER);
				InvalidateRect(hWnd, nullptr, TRUE);
			}
		} else {
			/* throw console window */
			PostMessage(gConWnd, msg, wp, convert_position(lp));
		}
		break;

	case WM_IME_CHAR:
		PostMessage(gConWnd, msg, wp, lp);
		/* break */
	case WM_CHAR:
		selectionClear(hWnd);
		break;

	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP:
		if(wp != VK_RETURN) /* alt+enter */
			PostMessage(gConWnd, msg, wp, lp);
		break;
	case WM_KEYDOWN:
		if(gCurBlink) gCurBlinkNext = GetTickCount() + (gCurHide? 0: GetCaretBlinkTime());
	case WM_KEYUP:
		if((wp == VK_NEXT || wp == VK_PRIOR ||
		    wp == VK_HOME || wp == VK_END) &&
		   (GetKeyState(VK_SHIFT) & 0x8000)) {
			if(msg == WM_KEYDOWN) {
				WPARAM  sb = SB_PAGEDOWN;
				if(wp == VK_PRIOR)     sb = SB_PAGEUP;
				else if(wp == VK_HOME) sb = SB_TOP;
				else if(wp == VK_END)  sb = SB_BOTTOM;
				PostMessage(gConWnd, WM_VSCROLL, sb, 0);
			}
		}
		else if(wp == VK_INSERT &&
			(GetKeyState(VK_SHIFT) & 0x8000)) {
			if(msg == WM_KEYDOWN)
				onPasteFromClipboard(hWnd);
		}
		else if(wp == VK_TAB  &&
			(GetKeyState(VK_CONTROL) & 0x8000)) {
			if(msg == WM_KEYDOWN) {
				if (GetKeyState(VK_SHIFT) & 0x8000) {
					gWinMng.select_next_window(-1);
				} else {
					gWinMng.select_next_window(1);
				}
			}
		}
		else {
			PostMessage(gConWnd, msg, wp, lp);
		}
		break;
	case WM_TRAYICON:
		switch(lp) {
		case WM_LBUTTONUP:
			if(IsWindowVisible(hWnd)) {
				desktopToTray(hWnd);
			}else{
				trayToDesktop(hWnd);
			}
			break;
		case WM_RBUTTONUP:
			POINT curpos;
			GetCursorPos(&curpos);
			SendMessage(hWnd, 0x313, 0, MAKELPARAM(curpos.x, curpos.y));
			break;
		}
		break;
	case WM_SETFOCUS:
		gFocus = true;
		if(gCurBlink) gCurBlinkNext = GetTickCount() + (gCurHide? 0: GetCaretBlinkTime());
		InvalidateRect(hWnd, nullptr, TRUE);
		if (gMouseEvent == true) {
			PostMessage(gConWnd, msg, wp, lp);
		}
		break;
	case WM_KILLFOCUS:
		gFocus = false;
		InvalidateRect(hWnd, nullptr, TRUE);
		if (gMouseEvent == true) {
			PostMessage(gConWnd, msg, wp, lp);
		}
		break;
	case WM_SIZE:
		if(gMinimizeToTray && wp == SIZE_MINIMIZED) {
			desktopToTray(hWnd);
		}
		{
			CONSOLE_SCREEN_BUFFER_INFO csi;
			GetConsoleScreenBufferInfo(gStdOut, &csi);
			COORD size = {};
			size.X = (SHORT)(((lp & 0xffff) - (gBorderSize * 2)) / gFontW);
			size.Y = csi.dwSize.Y;
			SetConsoleScreenBufferSize(gStdOut, size);
		}
		return(0);
	case WM_SETTINGCHANGE:
		{
			BOOL value = checkDarkMode();
			DwmSetWindowAttribute(hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof(value));
			if (value == TRUE) {
				SetWindowTheme(hWnd, L"DarkMode_Explorer", nullptr);
			}
			else {
				SetWindowTheme(hWnd, L"Explorer", nullptr);
			}
		}
		break;
	default:
		return( DefWindowProc(hWnd, msg, wp, lp) );
	}
	return(1);
}

/*****************************************************************************/
#include "option.h"

/*----------*/
static BOOL create_window(ckOpt& opt)
{
	trace(L"create_window\n");

	HINSTANCE hInstance = GetModuleHandle(nullptr);
	LPCWSTR	className = L"CkwWindowClass";
	const wchar_t*	conf_icon;
	DWORD	style = WS_OVERLAPPEDWINDOW;
	DWORD	exstyle = WS_EX_ACCEPTFILES;
	LONG	width, height;
	LONG	posx, posy;
	HICON	icon;
	HICON	iconsm;

	if(opt.isTranspColor() ||
	   (0 < opt.getTransp() && opt.getTransp() < 255))
		exstyle |= WS_EX_LAYERED;

	if(opt.isScrollRight())
		exstyle |= WS_EX_RIGHTSCROLLBAR;
	else
		exstyle |= WS_EX_LEFTSCROLLBAR;

	if(opt.isTopMost())
		exstyle |= WS_EX_TOPMOST;

	if(opt.isScrollHide() || opt.getSaveLines() < 1)
		gVScrollHide = true;
	else
		style |= WS_VSCROLL;

	conf_icon = opt.getIcon();
	if(!conf_icon || !conf_icon[0]){
		icon   = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(IDR_ICON), IMAGE_ICON, GetSystemMetrics(SM_CXICON),   GetSystemMetrics(SM_CYICON),   LR_DEFAULTCOLOR);
		iconsm = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(IDR_ICON), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
	}else{
		icon   = (HICON)LoadImage(nullptr, conf_icon, IMAGE_ICON, GetSystemMetrics(SM_CXICON),   GetSystemMetrics(SM_CYICON),   LR_LOADFROMFILE);
		iconsm = (HICON)LoadImage(nullptr, conf_icon, IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_LOADFROMFILE);
	}

	/* calc window size */
	CONSOLE_SCREEN_BUFFER_INFO csi;
	GetConsoleScreenBufferInfo(gStdOut, &csi);

	AdjustWindowRectEx(&gFrame, style, FALSE, exstyle);
	if(!gVScrollHide)
		gFrame.right += GetSystemMetrics(SM_CXVSCROLL);

	gWinW = width  = csi.srWindow.Right  - csi.srWindow.Left + 1;
	gWinH = height = csi.srWindow.Bottom - csi.srWindow.Top  + 1;
	width  *= gFontW;
	height *= gFontH;
	width  += gBorderSize * 2;
	height += gBorderSize * 2;
	width  += gFrame.right  - gFrame.left;
	height += gFrame.bottom - gFrame.top;

	if(opt.isWinPos()) {
		RECT	rc;
		SystemParametersInfo(SPI_GETWORKAREA,0,(LPVOID)&rc,0);
		posx = opt.getWinPosX();
		if(posx < 0) posx = rc.right - (width - posx -1);
		else         posx += rc.left;
		if(posx < rc.left) posx = rc.left;
		if(posx > rc.right-5) posx = rc.right -5;
		posy = opt.getWinPosY();
		if(posy < 0) posy = rc.bottom - (height - posy -1);
		else         posy += rc.top;
		if(posy < rc.top) posy = rc.top;
		if(posy > rc.bottom-5) posy = rc.bottom -5;
	}
	else {
		posx = CW_USEDEFAULT;
		posy = CW_USEDEFAULT;
	}

	/**/
	WNDCLASSEX wc = {};
	wc.cbSize = sizeof(wc);
	wc.style = 0;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = icon;
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.hbrBackground = CreateSolidBrush(gColorTable[0]);
	wc.lpszMenuName = nullptr;
	wc.lpszClassName = className;
	wc.hIconSm = iconsm;
	if(! RegisterClassEx(&wc))
		return(FALSE);

	HWND hWnd = CreateWindowEx(exstyle, className, gTitle, style,
				   posx, posy, width, height,
				   nullptr, nullptr, hInstance, nullptr);
	if(!hWnd){
		return(FALSE);
	}

	BOOL value = checkDarkMode();
	if (value == TRUE) {
		DwmSetWindowAttribute(hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof(value));
		SetWindowTheme(hWnd, L"DarkMode_Explorer", nullptr);
	}

	gWinMng.register_window(hWnd);
	sysmenu_init(hWnd);
	sysicon_init(hWnd, iconsm, gTitle, opt.isAlwaysTray());

	gMinimizeToTray = opt.isMinimizeToTray();

	// メッセージで最小化しないとショートカットの「実行時の大きさ」などと干渉してうまく動かない
	if(opt.isIconic())
		SendMessage(hWnd, WM_SYSCOMMAND, SC_MINIMIZE, 0);

	if(0 < opt.getTransp() && opt.getTransp() < 255)
		SetLayeredWindowAttributes(hWnd, 0, static_cast<BYTE>(opt.getTransp()), LWA_ALPHA);
	else if(opt.isTranspColor())
		SetLayeredWindowAttributes(hWnd, opt.getTranspColor(), 255u, LWA_COLORKEY);

	ShowWindow(hWnd, SW_SHOW);
	return(TRUE);
}

static BOOL set_current_directory(const wchar_t *dir)
{
	trace(L"set_current_directory\n");
	if (!dir) return(TRUE);

	std::wstring cwd = dir;
	size_t index = cwd.find(L"\"");
	if (index != std::wstring::npos) {
		cwd.resize(index+1);
		cwd[index] = L'\\';
	}
	BOOL b = SetCurrentDirectory(cwd.c_str());

	return b;
}

/*----------*/
static BOOL create_child_process(const wchar_t* cmd)
{
	trace(L"create_child_process\n");

	wchar_t* buf = nullptr;

	if(!cmd || !cmd[0]) {
		buf = new wchar_t[32768];
		buf[0] = 0;
		if(!GetEnvironmentVariable(L"COMSPEC", buf, 32768))
			lstrcpy(buf, L"cmd.exe");
	}
	else {
		buf = new wchar_t[ lstrlen(cmd)+1 ];
		lstrcpy(buf, cmd);
	}

	PROCESS_INFORMATION pi;
	STARTUPINFO si = {};
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESTDHANDLES;
	si.hStdInput  = gStdIn;
	si.hStdOutput = gStdOut;
	si.hStdError  = gStdErr;

	if(! CreateProcess(nullptr, buf, nullptr, nullptr, TRUE,
					   0, nullptr, nullptr, &si, &pi)) {
		delete [] buf;
		return(FALSE);
	}
	delete [] buf;
	CloseHandle(pi.hThread);
	gChild = pi.hProcess;
	return(TRUE);
}

/*----------*/
static BOOL create_font(const wchar_t* name, int height)
{
	trace(L"create_font\n");

	int pixlsy = get_dpi();
	memset(&gFontLog, 0, sizeof(gFontLog));
	gFontLog.lfHeight = -MulDiv(height, pixlsy, 96);
	gFontLog.lfWidth = 0;
	gFontLog.lfEscapement = 0;
	gFontLog.lfOrientation = 0;
	gFontLog.lfWeight = FW_NORMAL;
	gFontLog.lfItalic = 0;
	gFontLog.lfUnderline = 0;
	gFontLog.lfStrikeOut = 0;
	gFontLog.lfCharSet = DEFAULT_CHARSET;
	gFontLog.lfOutPrecision = OUT_DEFAULT_PRECIS;
	gFontLog.lfClipPrecision = CLIP_DEFAULT_PRECIS;
	gFontLog.lfQuality = DEFAULT_QUALITY;
	gFontLog.lfPitchAndFamily = FIXED_PITCH | FF_DONTCARE;

	lstrcpy(gFontLog.lfFaceName, name);

	gFont = CreateFontIndirect(&gFontLog);

	/* calc font size */
	HDC	hDC = GetDC(nullptr);
	HGDIOBJ	oldfont = SelectObject(hDC, gFont);
	TEXTMETRIC met;
	INT	width1[26], width2[26], width = 0;

	GetTextMetrics(hDC, &met);
	GetCharWidth32(hDC, 0x41, 0x5A, width1);
	GetCharWidth32(hDC, 0x61, 0x7A, width2);
	SelectObject(hDC, oldfont);
	ReleaseDC(nullptr, hDC);

	for(int i = 0 ; i < 26 ; i++) {
		width += width1[i];
		width += width2[i];
	}
	width /= 26 * 2;
	gFontW = width; /* met.tmAveCharWidth; */
	gFontH = met.tmHeight + gLineSpace;

	return(TRUE);
}

#include <winternl.h>

#ifndef STARTF_TITLEISLINKNAME
#define STARTF_TITLEISLINKNAME 0x00000800
#endif

/*----------*/
static void __hide_alloc_console()
{
	bool bResult = false;

	/*
	 * Open Console Window
	 * hack StartupInfo.wShowWindow flag
	 */

#if defined(_WIN64)
	INT_PTR peb = *(INT_PTR*)((INT_PTR)NtCurrentTeb() + 0x60);
	INT_PTR param = *(INT_PTR*) (peb + 0x20);
	DWORD* pflags = (DWORD*) (param + 0xa4);
	WORD* pshow = (WORD*) (param + 0xa8);
#else
	PPEB peb = *(PPEB*)((INT_PTR)NtCurrentTeb() + 0x30);
	PRTL_USER_PROCESS_PARAMETERS param = peb->ProcessParameters;
	DWORD* pflags = (DWORD*)((INT_PTR)param + 0x68);
	WORD* pshow = (WORD*)((INT_PTR)param + 0x6C);
#endif

	DWORD	backup_flags = *pflags;
	WORD	backup_show  = *pshow;

	STARTUPINFO si;
	GetStartupInfo(&si);

	/* check */
	if(si.dwFlags == backup_flags && si.wShowWindow == backup_show) {
		// ショートカットからの起動する(STARTF_TITLEISLINKNAMEが立つ)と、
		// Console窓を隠すのに失敗するので除去
		if (*pflags & STARTF_TITLEISLINKNAME) {
			*pflags &= ~STARTF_TITLEISLINKNAME;
		}
		*pflags |= STARTF_USESHOWWINDOW;
		*pshow  = SW_HIDE;
		bResult = true;
	}

	AllocConsole();

	/* restore */
	*pflags = backup_flags;
	*pshow  = backup_show;

	while((gConWnd = GetConsoleWindow()) == nullptr) {
		Sleep(10);
	}
	if (!bResult){
		while (!IsWindowVisible(gConWnd)) {
			Sleep(10);
		}
		while(IsWindowVisible(gConWnd)) {
			ShowWindow(gConWnd, SW_HIDE);
			Sleep(10);
		}
	}
}

static void _terminate();

/*----------*/
BOOL WINAPI sig_handler(DWORD n)
{
	switch(n) {
	case CTRL_CLOSE_EVENT:
	case CTRL_LOGOFF_EVENT:
	case CTRL_SHUTDOWN_EVENT:
		_terminate();
		ExitProcess(0);
		break;
	default:
		break;
	}
	return(TRUE);
}

static BOOL create_console(ckOpt& opt)
{
	__hide_alloc_console();

	if(gTitle)
		SetConsoleTitle(gTitle);

	SetConsoleCtrlHandler(sig_handler, TRUE);

	SECURITY_ATTRIBUTES sa;
	sa.nLength = sizeof(sa);
	sa.lpSecurityDescriptor = nullptr;
	sa.bInheritHandle = TRUE;

	gStdIn  = CreateFile(L"CONIN$",  GENERIC_READ|GENERIC_WRITE,
			     FILE_SHARE_READ|FILE_SHARE_WRITE,
			     &sa, OPEN_EXISTING, 0, nullptr);
	gStdOut = CreateFile(L"CONOUT$",  GENERIC_READ|GENERIC_WRITE,
			     FILE_SHARE_READ|FILE_SHARE_WRITE,
			     &sa, OPEN_EXISTING, 0, nullptr);
	gStdErr = CreateFile(L"CONOUT$",  GENERIC_READ|GENERIC_WRITE,
			     FILE_SHARE_READ|FILE_SHARE_WRITE,
			     &sa, OPEN_EXISTING, 0, nullptr);

	if(!gConWnd || !gStdIn || !gStdOut || !gStdErr)
		return(FALSE);

	UINT codepage = opt.getCodePage();
	if (IsValidCodePage(codepage)) {
		gCodePage = codepage;
		if (GetConsoleCP() != codepage) {
			SetConsoleCP(codepage);
			SetConsoleOutputCP(codepage);
		}
	} else {
		gCodePage = GetConsoleCP();
	}

	gMouseEvent = opt.isMouseEvent();
	if (gMouseEvent == true) {
		// マウスイベント有効時には簡易編集モードをOFFにする
		DWORD flag = 0;
		if (GetConsoleMode(gStdIn, &flag) == TRUE) {
			flag &= ~ENABLE_QUICK_EDIT_MODE;
			SetConsoleMode(gStdIn, flag);
		}
	}

	// 最大化不具合の解消の為フォントを最小化
	// レイアウト崩れ/CP65001での日本語出力対策でMS GOTHICを指定
	CONSOLE_FONT_INFOEX info = {};
	info.cbSize       = sizeof(info);
	info.FontWeight   = FW_NORMAL;
	info.dwFontSize.X = 3;
	info.dwFontSize.Y = 6;
	lstrcpyn(info.FaceName, L"MS GOTHIC", LF_FACESIZE);
	if (SetCurrentConsoleFontEx(gStdOut, FALSE, &info) == FALSE) {
		return(FALSE);
	}

	/* set buffer & window size */
	COORD size;
	SMALL_RECT sr = {0,0,0,0};
	SetConsoleWindowInfo(gStdOut, TRUE, &sr);
	size.X = opt.getWinCharW();
	size.Y = opt.getWinCharH() + opt.getSaveLines();
	SetConsoleScreenBufferSize(gStdOut, size);
	sr.Left = 0;
	sr.Right = opt.getWinCharW()-1;
	sr.Top = size.Y - opt.getWinCharH();
	sr.Bottom = size.Y-1;
	SetConsoleWindowInfo(gStdOut, TRUE, &sr);
	size.X = sr.Left;
	size.Y = sr.Top;
	SetConsoleCursorPosition(gStdOut, size);

	int pixlsy = get_dpi();
	gBorderSize = MulDiv(gBorderSize, pixlsy, 96);
	gLineSpace = MulDiv(gLineSpace, pixlsy, 96);

	return(TRUE);
}

/*----------*/
BOOL init_options(ckOpt& opt)
{
	/* create argv */
	int	i, argc;
	LPWSTR*	argv = CommandLineToArgvW(GetCommandLineW(), &argc);

	opt.loadXdefaults();
	bool result = opt.set(argc, argv);

	if(!result) return(FALSE);

	/* set */
	for(i = kColor0 ; i <= kColor15 ; i++)
		gColorTable[i] = opt.getColor(i);
	gColorTable[kColor7] = opt.getColorFg();
	gColorTable[kColor0] = opt.getColorBg();

	gColorTable[kColorCursorBg] = opt.getColorCursor();
	gColorTable[kColorCursorFg] = ~gColorTable[kColorCursorBg] & 0xFFFFFF;
	gColorTable[kColorCursorImeBg] = opt.getColorCursorIme();
	gColorTable[kColorCursorImeFg] = ~gColorTable[kColorCursorImeBg] & 0xFFFFFF;

	gBorderSize = opt.getBorderSize();
	gLineSpace = opt.getLineSpace();

	if(opt.getBgBmp()) {
		gBgBmp = (HBITMAP)LoadImage(nullptr, opt.getBgBmp(),
				IMAGE_BITMAP, 0,0, LR_LOADFROMFILE);
	}
	if(gBgBmp) {
		gBgBmpPosOpt = opt.getBgBmpPos();
		if(gBgBmpPosOpt == 0) {
			gBgBrush = CreatePatternBrush(gBgBmp);
		} else {
			BITMAP bm;
			GetObject(gBgBmp, sizeof(BITMAP), &bm);
			gBgBmpSize.x = bm.bmWidth;
			gBgBmpSize.y = bm.bmHeight;
		}
	}
	if(!gBgBrush) gBgBrush = CreateSolidBrush(gColorTable[0]);

	gCurBlink = opt.isCurBlink();
	gNoAutoClose = opt.isNoAutoClose();

	if(gTitle) delete [] gTitle;
	const wchar_t *conf_title = opt.getTitle();
	if(!conf_title || !conf_title[0]){
		gTitle = new wchar_t[ lstrlen(L"ckw")+1 ];
		wcscpy_s(gTitle, lstrlen(L"ckw")+1, L"ckw");
	}else{
		gTitle = new wchar_t[ lstrlen(conf_title)+1 ];
		wcscpy_s(gTitle, lstrlen(conf_title)+1, conf_title);
	}

	return(TRUE);
}

/*----------*/
static BOOL initialize()
{
	ckOpt opt;

	if(! init_options(opt)) {
		return(FALSE);
	}
	if(! create_console(opt)) {
		trace(L"create_console failed\n");
		return(FALSE);
	}
	if(! create_font(opt.getFont(), opt.getFontSize())) {
		trace(L"create_font failed\n");
		return(FALSE);
	}
	gFontSize = opt.getFontSize();
	if (! set_current_directory(opt.getCurDir())) {
		trace(L"set_current_directory failed\n");
	}
	if(! create_child_process(opt.getCmd())) {
		trace(L"create_child_process failed\n");
		return(FALSE);
	}
	if(! create_window(opt)) {
		trace(L"create_window failed\n");
		return(FALSE);
	}

	/*
	wchar_t path[MAX_PATH+1];
	GetSystemDirectory(path, MAX_PATH);
	SetCurrentDirectory(path);
	*/
	return(TRUE);
}

#define SAFE_CloseHandle(handle) \
	if(handle) { CloseHandle(handle); handle = nullptr; }

#define SAFE_DeleteObject(handle) \
	if(handle) { DeleteObject(handle); handle = nullptr; }

/*----------*/
static void _terminate()
{
	if(gTitle) {
		delete [] gTitle;
		gTitle = nullptr;
	}
	if(gScreen) {
		delete [] gScreen;
		gScreen = nullptr;
	}
	if(gCSI) {
		delete gCSI;
		gCSI = nullptr;
	}
	gConWnd = nullptr;
	SAFE_CloseHandle(gStdIn);
	SAFE_CloseHandle(gStdOut);
	SAFE_CloseHandle(gStdErr);
	SAFE_CloseHandle(gChild);
	SAFE_DeleteObject(gFont);
	SAFE_DeleteObject(gBgBrush);
	SAFE_DeleteObject(gBgBmp);
}

#ifdef _DEBUG
#include <crtdbg.h>
#endif

/*----------*/
int APIENTRY WinMain(_In_ HINSTANCE hInst, _In_opt_ HINSTANCE hPrev, _In_ LPSTR lpCmdLine, _In_ int nCmdShow)
{
	if(initialize()) {
		MSG msg;
		while(GetMessage(&msg, nullptr, 0,0)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	_terminate();
	return(0);
}

/* 新規ウインドウの作成 */
void makeNewWindow()
{
	STARTUPINFO si = {};
	si.cb = sizeof(si);

	PROCESS_INFORMATION pi = {};
	if(CreateProcess(nullptr, GetCommandLine(), nullptr, nullptr, FALSE, 0,
					   nullptr, nullptr, &si, &pi)){
		// 使用しないので，すぐにクローズしてよい
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
	}
}

static BOOL checkDarkMode()
{
	HKEY hKey = nullptr;
	RegOpenKeyEx(HKEY_CURRENT_USER
		, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize"
		, 0
		, KEY_QUERY_VALUE
		, &hKey);
	DWORD dwType = 0;
	DWORD lightMode = 1;
	DWORD dwReadSize = sizeof(DWORD);
	RegQueryValueEx(hKey, L"AppsUseLightTheme", 0, &dwType, reinterpret_cast<LPBYTE>(& lightMode), &dwReadSize);
	RegCloseKey(hKey);
	return lightMode == 0;
}

/* EOF */
