#ifndef CKW_OPT_H
#define CKW_OPT_H
#pragma once

#include "ckw.h"
#include <string>


class ckOpt {
public:
	ckOpt();
	~ckOpt();

	void		setFile(const wchar_t *path=nullptr);
	void		loadXdefaults();
	bool		set(int argc, wchar_t *argv[]);

	bool		isWinPos() const	{ return(m_isWinPos); }
	int		getWinPosX() const		{ return(m_winPosX); }
	int		getWinPosY() const		{ return(m_winPosY); }
	int		getWinCharW() const		{ return(m_winCharW); }
	int		getWinCharH() const		{ return(m_winCharH); }
	bool		isIconic() const	{ return(m_isIconic); }
	COLORREF	getColorFg() const	{ return(m_colorFg); }
	COLORREF	getColorBg() const	{ return(m_colorBg); }
	COLORREF	getColorCursor() const	{ return(m_colorCursor); }
	COLORREF	getColorCursorIme() const	{ return(m_colorCursorIme); }
	COLORREF	getColor(int i) const
	{
		return((0 <= i && i <= 15) ? m_colors[i] : m_colors[0]);
	}

	bool		isAlwaysTray() const		{ return(m_alwaysTray); }
	bool		isMinimizeToTray() const	{ return(m_minimizeToTray); }
	bool		isScrollHide() const		{ return(m_scrollHide); }
	bool		isScrollRight() const		{ return(m_scrollRight); }
	int		getSaveLines() const		{ return(m_saveLines); }
	int		getBorderSize() const		{ return(m_borderSize); }
	int		getLineSpace() const		{ return(m_lineSpace); }
	int		getTransp() const		{ return(m_transp); }
	bool		isTranspColor() const		{ return(m_isTranspColor); }
	COLORREF	getTranspColor() const	{ return(m_transpColor); }
	bool		isTopMost() const		{ return(m_isTopMost); }
	int		getBgBmpPos() const		{ return(m_bgBmpPos); }
	bool		isCurBlink() const		{ return(m_isCurBlink); }
	int		getCodePage() const		{ return(m_codePage); }
	bool		isNoAutoClose() const	{ return(m_isNoAutoClose); }
	bool		isMouseEvent() const	{ return(m_isMouseEvent); }

	const wchar_t*	getCmd() const
	{
		return((m_cmd.size()) ? m_cmd.c_str() : nullptr);
	}
	int		getFontSize() const		{ return(m_fontSize); }
	const wchar_t*	getFont() const
	{
		return((m_font.size()) ? m_font.c_str() : nullptr);
	}
	const wchar_t*	getBgBmp() const
	{
		return((m_bgBmp.size()) ? m_bgBmp.c_str() : nullptr);
	}
	const wchar_t*	getCurDir() const
	{
		return((m_curDir.size()) ? m_curDir.c_str() : nullptr);
	}
	const wchar_t*	getTitle() const
	{
		return((m_title.size()) ? m_title.c_str() : nullptr);
	}
	const wchar_t*	getIcon() const
	{
		return((m_icon.size()) ? m_icon.c_str() : nullptr);
	}


protected:
	void	cmdsMake(int argc, wchar_t *argv[]);
	void	geometry(const wchar_t *str);
	int	setOption(const wchar_t *name, const wchar_t *value, bool rsrc);
	void	_loadXdefaults(const wchar_t *path);

private:
	bool		m_isWinPos;
	int		m_winPosX;
	int		m_winPosY;
	int		m_winCharW;
	int		m_winCharH;
	bool		m_isIconic;
	std::wstring	m_cmd;
	std::wstring	m_font;
	int		m_fontSize;
	COLORREF	m_colorFg;
	COLORREF	m_colorBg;
	COLORREF	m_colorCursor;
	COLORREF	m_colorCursorIme;
	COLORREF	m_colors[16];
	std::wstring	m_bgBmp;
	std::wstring	m_icon;
	bool		m_alwaysTray;
	bool		m_minimizeToTray;
	bool		m_scrollHide;
	bool		m_scrollRight;
	int		m_saveLines;
	int		m_borderSize;
	int		m_lineSpace;
	int		m_transp;
	bool		m_isTranspColor;
	COLORREF	m_transpColor;
	bool		m_isTopMost;
	std::wstring	m_curDir;
	std::wstring	m_title;
	wchar_t	m_config_file[MAX_PATH+1];
	int		m_bgBmpPos;
	bool		m_isCurBlink;
	int		m_codePage;
	bool	m_isNoAutoClose;
	bool	m_isMouseEvent;
};

#endif /* CKW_OPT_H */
