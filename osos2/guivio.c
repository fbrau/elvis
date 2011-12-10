/* guivio.c */

/*
 * This file contains a VIO (console) user interface for OS/2.  It is 
 * derived from the "guitcap.c" file of elvis 2.0.  Work by Lee Johnson
 * except a few fixes by herbert.
 *
 * herbert: 
 * - fixed bug with cursor dimensions in Visual Mode in fullscreens.
 */

#include "elvis.h"
#ifdef GUI_VIO
#include "guivio.h"
#include <signal.h>
#if 0
#include <ctype.h>
#endif
/* Forward declarations: */
static void movecurs (VWIN *vw);
static void chgsize (VWIN *vw, int newheight, BOOLEAN winch);
static void vio_CE (VCOLOR color);
static void vio_CM (int row, int col);

#define	MINHEIGHT	4

/* This table lists the keys which are mapped automatically */
struct
{
	char	*label;		/* keytop legend of the key */
	char	*rawin;		/* raw characters sent by key */
	char	*cooked;	/* what the key should map to (if anything) */
	MAPFLAGS flags;		/* when the map should be effective */
} keys[] =
{
	/* Grey keys. */
	{ "<Up>",		"\013e048",		"k",		MAP_ALL },
	{ "<Down>",		"\013e050",		"j",		MAP_ALL },
	{ "<Left>",		"\013e04b",		"h",		MAP_ALL },
	{ "<Right>",	"\013e04d",		"l",		MAP_ALL },
	{ "<PgUp>",		"\013e049",		"\002", 	MAP_ALL },
	{ "<PgDn>",		"\013e051",		"\006", 	MAP_ALL },
	{ "<Home>",		"\013e047",		"^",		MAP_ALL },
	{ "<End>",		"\013e04f",		"$",		MAP_ALL },
	{ "<Insert>",	"\013e052",		"i",		MAP_ALL },
	{ "<Delete>",	"\013e053",		"x",		MAP_ALL },

	/* Numeric keypad keys (with Num Lock off.) */
	{ "<kpUp>",		"\0130048",		"k",		MAP_ALL },
	{ "<kpDown>",	"\0130050",		"j",		MAP_ALL },
	{ "<kpLeft>",	"\013004b",		"h",		MAP_ALL },
	{ "<kpRight>",	"\013004d",		"l",		MAP_ALL },
	{ "<kpPgUp>",	"\0130049",		"\002", 	MAP_ALL },
	{ "<kpPgDn>",	"\0130051",		"\006", 	MAP_ALL },
	{ "<kpHome>",	"\0130047",		"^",		MAP_ALL },
	{ "<kpEnd>",	"\013004f",		"$",		MAP_ALL },
	{ "<kpInsert>",	"\0130052",		"i",		MAP_ALL },
	{ "<kpDelete>",	"\0130053",		"x",		MAP_ALL },
	{ "<kp/>",		"\0132fe0",		"/",		MAP_ALL },
	{ "<kpEnter>",	"\0130de0",		"\r",		MAP_ALL },

	/* Unshifted function keys. */
	{ "#1",			"\013003b",		":help\r",	MAP_ALL_VISUAL },
	{ "#2",			"\013003c" },
	{ "#3",			"\013003d" },
	{ "#4",			"\013003e" },
	{ "#5",			"\013003f" },
	{ "#6",			"\0130040" },
	{ "#7",			"\0130041" },
	{ "#8",			"\0130042" },
	{ "#9",			"\0130043" },
	{ "#10",		"\0130044" },
	{ "#11",		"\0130085" },
	{ "#12",		"\0130086" },

	/* Shift-function keys. */
	{ "#1s",		"\0130054" },
	{ "#2s",		"\0130055" },
	{ "#3s",		"\0130056" },
	{ "#4s",		"\0130057" },
	{ "#5s",		"\0130058" },
	{ "#6s",		"\0130059" },
	{ "#7s",		"\013005a" },
	{ "#8s",		"\013005b" },
	{ "#9s",		"\013005c" },
	{ "#10s",		"\013005d" },
	{ "#11s",		"\0130087" },
	{ "#12s",		"\0130088" },

	/* Ctrl-function keys. */
	{ "#1c",		"\013005e" },
	{ "#2c",		"\013005f" },
	{ "#3c",		"\0130060" },
	{ "#4c",		"\0130061" },
	{ "#5c",		"\0130062" },
	{ "#6c",		"\0130063" },
	{ "#7c",		"\0130064" },
	{ "#8c",		"\0130065" },
	{ "#9c",		"\0130066" },
	{ "#10c",		"\0130067" },
	{ "#11c",		"\0130089" },
	{ "#12c",		"\013008a" },

	/* Alt-function keys. */
	{ "#1a",		"\0130068" },
	{ "#2a",		"\0130069" },
	{ "#3a",		"\013006a" },
	{ "#4a",		"\013006b" },
	{ "#5a",		"\013006c" },
	{ "#6a",		"\013006d" },
	{ "#7a",		"\013006e" },
	{ "#8a",		"\013006f" },
	{ "#9a",		"\0130070" },
	{ "#10a",		"\0130071" },
	{ "#11a",		"\013008b" },
	{ "#12a",		"\013008c" }
};

static char	GC_V = 0xb3;	/* vertical bar character */
static char	GC_H = 0xc4;	/* horizontal bar character */
static char	GC_1 = 0xc0;	/* lower left corner character */
static char	GC_2 = 0xc1;	/* horizontal line with up-tick character */
static char	GC_3 = 0xd9;	/* lower right corner character */
static char	GC_4 = 0xc3;	/* vertical line with right-tick character */
static char	GC_5 = 0xc5;	/* center cross character */
static char	GC_6 = 0xb4;	/* vertical line with left-tick character */
static char	GC_7 = 0xda;	/* upper left corner character */
static char	GC_8 = 0xc2;	/* horizontal line with down-tick character */
static char	GC_9 = 0xbf;	/* upper right corner character */

/* These are GUI-dependent global options */

#if 0
/* use the definitions in gui.h instead! */
static struct
{
	OPTVAL	term;			/* string - terminal type */
	OPTVAL	ttyrows;		/* number - rows of screen */
	OPTVAL	ttycolumns;		/* number - columns of screen */
	OPTVAL	ttyunderline;	/* boolean - whether colors and underline mix */
} ttygoptvals;
#endif
struct ttygoptvals_s ttygoptvals;

static OPTDESC goptdesc[] =
{
	{"term", "termtype",	optsstring,	optisstring},
	{"ttyrows", "ttylines",	optnstring,	optisnumber},
	{"ttycolumns", "ttycolumns",optnstring,	optisnumber},
	{"ttyunderline", "ttyu",NULL,		NULL	   },
};

#define o_term			ttygoptvals.term.value.string
#define o_ttyrows		ttygoptvals.ttyrows.value.number
#define o_ttycolumns	ttygoptvals.ttycolumns.value.number
#define o_ttyunderline	ttygoptvals.ttyunderline.value.boolean

/*----------------------------------------------------------------------------*/
/* These are mid-level terminal I/O functions.  They buffer the output, but
 * don't do much more than that.
 */
static char 	ttybuf[2048];	/* the output buffer */
static int  	ttycount;		/* number of characters in ttybuf */
long        	ttycaught;		/* bitmap of recently-received signals */
static BOOLEAN	raw_mode;		/* If FALSE, use VioWrtTTY() to flush output */
static VCOLOR	vc_current;		/* Current fg/bg color attribute */
static VCOLOR	vc_term;		/* fg/bg color for shell commands */
static USHORT	c_col, c_row;	/* physical cursor position (VIO) */

static int		afterprg;		/* expose windows (after running prg) */
static int		afterscrl;		/* number of status lines (after running prg) */
static BOOLEAN	fgcolored;		/* have foreground colors been set? */
static BOOLEAN	bgcolored;		/* have background colors been set? */
static int		physx, physy;	/* physical cursor position */
static VWIN		*vwins;			/* list of windows */
static VWIN		*current;		/* window with keyboard focus */
static VWIN		defcolors;		/* default color strings */
static int		nwindows;		/* number of windows allocated */

/* This function writes the contents of 'ttybuf' to the screen */
static void 
do_flush (void)
{
	if (ttycount > 0)
	{
		if (raw_mode)
		{
			VioWrtCellStr (ttybuf, ttycount, c_row, c_col, 0);
			if ((c_col += ttycount / 2) >= o_ttycolumns)
			{
				c_col -= o_ttycolumns;
				c_row++;
			}
		}
		else
		{
			VioWrtTTY (ttybuf, ttycount, 0);
		}
		ttycount = 0;
	}
}

/* This function is used internally.  It is passed to the tputs() function
 * which uses it to output individual characters.  This function saves the
 * characters in a buffer and outputs them in a bunch.
 */
static int
ttych(int ch)
{
	if (raw_mode)
	{
		switch (ch)
		{
		case '\n':
			do_flush ();
			c_col = 0;
			c_row++;
			break;

		case '\r':
			do_flush ();
			c_col = 0;
			break;

		default:
			ttybuf[ttycount++] = ch;
			ttybuf[ttycount++] = vc_current;
			if (ttycount >= QTY (ttybuf))
			{
				do_flush ();
			}
			break;
		}
	}
	else
	{
		ttybuf[ttycount++] = ch;
		if (ttycount >= QTY (ttybuf))
		{
			do_flush ();
		}
	}

	return ch;
}

static int
vio_read (char *buf, int len, int timeout)
{
	KBDKEYINFO ki;

	/* Fast 'n' sleazy keyboard processing. */
	KbdCharIn(&ki, IO_WAIT, 0);
	if ((ki.fbStatus & 0x02) != 0)
	{
		/*
		 * Convert an extended scan code to ^K, followed by four
		 * hex digits.  This scheme is borrowed from the X11 GUI.
		 */
		sprintf (buf, "%c%02x%02x", ELVCTRL ('K'), ki.chChar, ki.chScan);
		return strlen (buf);
	}
	else
	{
		buf[0] = ki.chChar;
		return 1;
	}
}

static VCOLOR
vio_get_default_color (void)
{
	USHORT row, col;
	CHAR cell[2];
	USHORT len = sizeof (cell);

	VioGetCurPos (&row, &col, 0);
	VioReadCellStr (cell, &len, row, col, 0);
	return cell[1];
}

static void
vio_CE (VCOLOR color)
{
	CHAR cell[2];

	if (c_col < o_ttycolumns)
	{
		cell[0] = ' ';
		cell[1] = color;
		VioWrtNCell (cell, o_ttycolumns - c_col, c_row, c_col, 0);
	}
}

static void 
vio_CM (int row, int col)
{
	USHORT o_row, o_col;

	c_row = row;
	c_col = col;

	/*
	 * Don't move the cursor if it's already in the right place.  We
	 * want to avoid any weird flickering that might result if moving 
	 * the cursor resets its blink timer.
	 */
	VioGetCurPos (&o_row, &o_col, 0);
	if (o_row != c_row || o_col != c_col)
	{
		VioSetCurPos (c_row, c_col, 0);
	}
}

/*
 * Revert to the normal font for a given window... or just end any special
 * fonts if no window is given.
 */
static void 
vio_revert (VWIN *vw)	/* window whose normal font is to be used, or NULL */
{
	/*
	 * If a window is specified, and its normal font string isn't empty,
	 * then output it.
	 */
	vc_current = vw != NULL ? vw->vc_normal : vc_term;
}

/* Send any required termination strings.  Turn off "raw" mode. */
static void 
vio_suspend (void)
{
	/* revert to the normal font */
	vio_revert (NULL);

	do_flush ();
	raw_mode = False;
}

/* Put the terminal in RAW mode.  Send any required strings */
static void
vio_resume (BOOLEAN sendstr)		/* send strings? */
{
	/* Determine the present whereabouts of the cursor. */
	VioGetCurPos (&c_row, &c_col, 0);
	raw_mode = TRUE;

#if 1
	/* reset, so we don't try any suspicious optimizations */
	vio_reset ();
#else
	/* send the initialization strings */
	if (sendstr)
	{
		ttych ('\r');
		vio_CE (defcolors.vc_normal);

		/* reset, so we don't try any suspicious optimizations */
		vio_reset ();
	}
#endif
}

/*
 * This function determines the screen size.
 */
static void 
vio_getsize (void)
{
	VIOMODEINFO info;
	USHORT rc;

	/* get the window size, one way or another. */
	info.cb = sizeof (info);
	rc = VioGetMode (&info, 0);

	/* did we get a realistic value? */
	if (rc == NO_ERROR && info.row >= 2 && info.col >= 30)
	{
		o_ttyrows = info.row;
		o_ttycolumns = info.col;
	}
}

/*
 * The following are used for replacing character attributes with colors.
 * Currently only the sixteen 'standard' IBM PC colors are supported.
 */
static struct
{
	char	*name;	/* name of the color */
	int		rgb;	/* RGB color index */
} colors[] =
{
	{"black",	0},
	{"blue",	1},
	{"green",	2},
	{"cyan",	3},
	{"red",		4},
	{"magenta",	5},
	{"brown",	6},
	{"white",	7},
	{"gray",	8},
	{"grey",	8},
	{"yellow",	14},
	{(char *)0}
};

static int
parse_color (char *name)
{
	int bright;		/* intensity bit mask */
	int i;

	/* see if we're supposed to set the brightness bit */
	bright = 0x00;
	if (!strncmp(name, "light", 5)) bright = 0x08, name += 5;
	if (!strncmp(name, "lt", 2)) bright = 0x08, name += 2;
	if (!strncmp(name, "bright", 6)) bright = 0x08, name += 6;

	/* skip leading garbage characters */
	while (*name && !isalpha (*name))
	{
		name++;
	}

	/* try to find the color */
	for (i = 0; colors[i].name && strcmp (colors[i].name, name); i++)
	{
	}
	if (!colors[i].name)
	{
		msg(MSG_ERROR, "[s]invalid color $1", name);
		return -1;
	}

	return colors[i].rgb | bright;
}

/* Extract foreground color index: */
#define	VC_FG(vc)		((vc) & 0x0f)

/* Extract background color index: */
#define	VC_BG(vc)		(((vc) & 0xf0) >> 4)

/*
 * Reduce foreground and background intensity, unless the result would
 * make the text invisible.
 */
#define	VC_DIM(vc) \
	(VC_FG((vc) & 0x77) != VC_BG((vc) & 0x77) ? (vc) & 0x77 : (vc))

static void
set_default_colors (VWIN *vw, VCOLOR vc_normal)
{
	vw->vc_normal = vc_normal;

	/* if normal is bright, then bold must be bright white */
	if ((vw->vc_normal & 0x08) != 0)
		vw->vc_bold = (vw->vc_normal & 0xf0) | 15;
	else
		vw->vc_bold = vw->vc_normal | 0x08;

	/* emphasized is same as bold */
	vw->vc_emph = vw->vc_bold;

	/* italic is a dim version of normal */
	vw->vc_italic = VC_DIM (vw->vc_normal);

	/* underline is dim underlined version of normal */
	vw->vc_underline = VC_DIM (vw->vc_normal);

	/* reverse video swaps foreground & background; always dim */
	vw->vc_hilite = VC_BG (vw->vc_normal) | (VC_FG (vw->vc_normal) << 4);

	/* other font is same as normal */
	vw->vc_fixed = vw->vc_normal;
}

static BOOLEAN 
set_font_color (
	VWIN	*vw,		/* window whose colors are to be set */
	_char_	font,		/* font code, one of n/b/i/u/e/o/N */
	char	*fgname,	/* foreground color */
	char	*bgname)	/* background color name, or NULL */
{
	int	fg, bg;
	VCOLOR	*build;
	static	int	normbg = -1;

	/* Parse the foreground color name. */
	if ((fg = parse_color (fgname)) < 0)
		return False;

	/* Parse the background color name, if given. */
	if (bgname && *bgname)
	{
		if ((bg = parse_color (bgname)) < 0)
			return False;
	}
	else /* no background specified */
	{
		/* use "normal" background color, if defined */
		bg = normbg;
	}

	/* build the string */
	switch (font)
	{
	  case 'n':	build = &vw->vc_normal;		break;
	  case 'f':	build = &vw->vc_fixed;		break;
	  case 'b':	build = &vw->vc_bold;		break;
	  case 'e':	build = &vw->vc_emph;		break;
	  case 'i':	build = &vw->vc_italic;		break;
	  case 'u':	build = &vw->vc_underline;	break;
	  default:	build = &vw->vc_hilite;		break;
	}
	if (bg >= 0)
	{
		*build = (bg << 4) | fg;
	}
	else /* no background specified; only affect foreground */
	{
		*build = (*build & 0xf0) | fg;
	}

	/* if 'n' font, copy startnormal into endXXX */
	if (font == 'n')
	{
		/* but first! if this the first color setting, then choose
		 * defaults for all other attributes.
		 */
		if (!fgcolored || (bg >= 0 && !bgcolored))
		{
			/*
			 * If no background color has been specified, use the
			 * color at the present location of the cursor.
			 */
			if (bg < 0)
			{
				bg = (defcolors.vc_normal >> 4) & 0x0f;
				vw->vc_normal = (bg << 4) | fg;
			}

			/* Choose default colors for the other fonts. */
			set_default_colors(vw, vw->vc_normal);
		}

		/* remember the new background color (if any) */
		normbg = bg;
	}

	/* success! */
	return True;
}

/* This is an internal function which moves the physical cursor to the logical
 * position of the cursor in a given window, if it isn't there already.
 */
static void movecurs (vw)
	VWIN	*vw;	/* window whose cursor is to be moved */
{
	int	y = vw->pos + vw->cursy;

	/* maybe we don't need to move at all? */
	if ((afterprg > 0 && y <= o_ttyrows - afterscrl)
		|| (y == physy && vw->cursx == physx))
	{
		/* already there */
		return;
	}

	/* revert to the normal font */
	vio_revert(vw);

	/* move the cursor to its current position */
	c_row = physy = y;
	c_col = physx = vw->cursx;
}

/* clear to end of line */
static BOOLEAN 
vio_clrtoeol (GUIWIN	*gw)		/* window whose row is to be cleared */
{
	VWIN	*vw = (VWIN *)gw;

	/* after running a program, disable the :ce: string for a while. */
	if (afterprg)
		return True;

	/* if we're on the bottom row of a window which doesn't end at the
	 * bottom of the screen, then fail.  This will cause elvis to output
	 * a bunch of spaces instead.  The vio_draw() function will convert 
	 * those spaces to underscore characters so the window has a border.
	 */
	if (vw->cursy == vw->height - 1 && vw->pos + vw->height != o_ttyrows)
	{
		return False;
	}

	/* revert to the normal font */
	vio_revert (vw);

	/* move the physical cursor to where the window thinks it should be */
	movecurs (vw);

	/* clear to the end of the current line */
	vio_CE (vw->vc_normal);

	return True;
}

/* insert or delete columns */
static BOOLEAN 
vio_shift (GUIWIN *gw,	/* window to be shifted */
		  int qty,		/* columns to insert (may be negative to delete) */
		  int rows)		/* number of rows affected (always 1 for this GUI) */
{
	CHAR cell[2];

	/* revert to the normal font */
	vio_revert ((VWIN *)gw);

	/* move the physical cursor to where this window thinks it is */
	movecurs ((VWIN *)gw);

	/* This is what we'll fill the scrolled region with: */
	cell[0] = ' ';
	cell[1] = ((VWIN *)gw)->vc_normal;
cell[0] = '#';
cell[1] = 0xe0;

	if (qty > 0)
	{
		VioScrollRt (c_row, c_col, c_row, o_ttycolumns - 1, qty, cell, 0);
	}
	else
	{
		/* take the absolute value of qty */
		qty = -qty;

		VioScrollLf (c_row, c_col, c_row, o_ttycolumns - 1, qty, cell, 0);
	}
	return True;
}

/* insert or delete rows.  qty is positive to insert, negative to delete */
static BOOLEAN 
vio_scroll (GUIWIN *gw,		/* window to be scrolled */
		   int qty,			/* rows to insert (may be negative to delete) */
		   BOOLEAN notlast)	/* if True, then leave last row unchanged */
{
	VWIN	*vw = (VWIN *)gw;
	CHAR 	cell[2];
	USHORT	bottom_row;

	/* Mentally adjust the number of rows used for messages.  This is only
	 * significant immediately after running an external program, and is
	 * used for hiding any premature attempts to redraw the window's text
	 * but still show the window's messages.
	 */
	afterscrl -= qty;

	/* If this window isn't the only window, then fail.
	 * Later, this function may be smart enough to use scrolling regions,
	 * or do the idlok() kind of thing, but not yet.
	 */
	if (vwins->next)
	{
		return False;
	}

	/* revert to the normal font */
	vio_revert (vw);

	/* move the physical cursor to where the window thinks it should be */
	movecurs (vw);

	/* This is what we'll fill the scrolled region with: */
	cell[0] = ' ';
	cell[1] = vw->vc_normal;

	bottom_row = notlast ? o_ttyrows - 2 : o_ttyrows - 1;
	if (qty > 0)
	{
		VioScrollDn (c_row, 0, bottom_row, o_ttycolumns - 1, qty, cell, 0);
	}
	else
	{
		VioScrollUp (c_row, 0, bottom_row, o_ttycolumns - 1, -qty, cell, 0);
	}
	return True;
}

/* Forget where the cursor is, and which mode we're in */
static void 
vio_reset (void)
{
	physx = physy = 9999;
	vio_revert (&defcolors);
}

/* Flush any changes out to the display */
static void 
vio_flush (void)
{
	if (current)
	{
		movecurs (current);
	}
	do_flush ();
	vio_CM (c_row, c_col);
}

/* Set a window's cursor position. */
static void 
vio_moveto (
	GUIWIN	*gw,	/* window whose cursor is to be moved */
	int		column,	/* new column of cursor */
	int		row		/* new row of cursor */
	)
{
	VWIN *vw = (VWIN *)gw;

	do_flush ();

	vw->cursy = row;
	vw->cursx = column;
}

/* put graphic characters.  This function is called only from vio_draw() */
static void drawgraphic (
	GUIWIN	*gw,	/* window where text should be drawn */
	_char_	font,	/* font to use for drawing this text - 'g' or 'G' */
	CHAR	*text,	/* plain chars to be mapped to graphic chars */
	int		len		/* length of text */
	)
{
	VWIN	*vw = (VWIN *)gw;
	int	i;
	BOOLEAN	graf;
	char	gc;

	/* if this is supposed to be hilighted, and we aren't already in
	 * standout mode, then switch to standout mode now.
	 */
	vc_current = font == 'G' ? vw->vc_hilite : vw->vc_normal;

	/* draw each character */
	for (graf = False, i = 0; i < len; i++)
	{
		/* try to convert plain character to graphic character */
		switch (text[i])
		{
		  case '-':	gc = GC_H;	break;
		  case '|':	gc = GC_V;	break;
		  case '1':	gc = GC_1;	break;
		  case '2':	gc = GC_2;	break;
		  case '3':	gc = GC_3;	break;
		  case '4':	gc = GC_4;	break;
		  case '5':	gc = GC_5;	break;
		  case '6':	gc = GC_6;	break;
		  case '7':	gc = GC_7;	break;
		  case '8':	gc = GC_8;	break;
		  case '9':	gc = GC_9;	break;
		  default:	gc = 0;
		}
		
		/* did we get a graphic character? */
		if (gc)
		{
			/* output the graphic character in graphic mode */
			ttych (gc);
		}
		else
		{
			/* output elvis' plain character in text mode */
			if (isdigit (text[i]))
				ttych('+');
			else
				ttych (text[i]);
		}
	}

	/* drawing the characters has the side-effect of moving the cursor */
	vw->cursx += len;
	physx += len;
}

/* put characters: first move, then set attribute, then execute char.
 */
static void vio_draw (
	GUIWIN	*gw,	/* window where text should be drawn */
	_char_	font,	/* font to use for drawing this text */
	CHAR	*text,	/* text to draw */
	int		len		/* length of text */
	)
{
	VWIN	*vw = (GUIWIN *)gw;
	int	i;
#ifndef NDEBUG
	VWIN	*scan;

	for (scan = vwins; scan != vw && scan; scan = scan->next)
	{
	}
	assert (scan);
#endif

	/* After a program, don't output any text except messages for a while.
	 * This is mostly an optimization; the window is about to be redrawn
	 * from scratch anyway.  But it also prevents the screen from doing
	 * strange, unexpected things.
	 */
	if (afterprg > 0)
	{
#if 0
fprintf (stderr, "draw(\"%.*s\"), vw->cursy=%d, vw->height=%d, physy=%d, afterscrl=%d\n", len, tochar8 (text), vw->cursy, vw->height, physy, afterscrl);
#endif
		if (vw->cursy < vw->height - afterscrl - 1)
			return;

		/*
		 * movecurs() could put us on the wrong row if the current
		 * window isn't the one on the bottom; however, we still
		 * need to be able to adjust the column in order to be
		 * correct for all values of (guivio.movecost).
		 */
		c_col = vw->cursx;
	}
	else
	{
		/* move the cursor to where this window thinks it is */
		movecurs (vw);
	}

	/* if graphic characters, then handle specially */
	if (font == 'g' || font == 'G')
	{
		drawgraphic (gw, font, text, len);
		return;
	}

	/* If we're on the bottom row of a window (except the last window)
	 * then any normal characters should be underlined.  This will give
	 * us the effect of a window border.
	 */
	if (vw->cursy == vw->height - 1 && physy < o_ttyrows - 1 && !isupper (font))
	{
		font = 'u';
	}

	/* find the font strings */
	if (isupper (font))
	{
		vc_current = vw->vc_hilite;
	}
	else
	{
		switch (font)
		{
		  case 'b':	vc_current = vw->vc_bold; break;
		  case 'e':	vc_current = vw->vc_emph; break;
		  case 'i':	vc_current = vw->vc_italic; break;
		  case 'u':	vc_current = vw->vc_underline; break;
		  case 'f':	vc_current = vw->vc_fixed; break;
			/* 'g' is handled separately, above */
		  case 'p':	vc_current = vw->vc_hilite; break;
		  default:	vc_current = vw->vc_normal; break;
		}
	}

	/* draw each character.  If this is the bottom row of any window except
	 * the bottom window, then also replace any blanks with '_' characters.
	 * This will provide a window border in case the terminal can't do
	 * real underlining.
	 */
	if (vw->cursy == vw->height - 1 && physy < o_ttyrows - 1 && !isupper (font))
	{
		for (i = 0; i < len; i++)
		{
			ttych (text[i] == ' ' ? '_' : text[i]);
		}
	}
	else /* normal row */
	{
		for (i = 0; i < len; i++)
		{
			ttych (text[i]);
		}
	}

	/* drawing the characters has the side-effect of moving the cursor */
	vw->cursx += len;
	physx += len;
}

/* return True if VIO is available. */
static int 
vio_test (void)
{
	VIOMODEINFO info;
	USHORT rc;

#if 0
	{
		static char digits[] = "0123456789abcdef";
		char buffer[32];
		int i, j;

		for (i = 0; i < 16; i++)
		{
			for (j = 0; j < 16; j++)
			{
				buffer[j * 2] = digits[j];
				buffer[j * 2 + 1] = i * 16 + j;
				buffer[j * 2 + 1] = 0x07;
				buffer[j * 2 ] = i * 16 + j;
			}
			VioWrtCellStr (buffer, 32, i, 0, 0);
		}
		vio_read (buffer, 1, 0);
	}
#endif

	info.cb = sizeof (info);
	rc = VioGetMode (&info, 0);

	return rc == NO_ERROR;
}

/* initialize the PC BIOS interface. */
static int 
vio_init (int argc,		/* number of command-line arguments */
		 char **argv)	/* values of command-line arguments */
{
	int	i;

	/* Choose font colors in the absence of user settings. */
	vc_term = vio_get_default_color ();
	set_default_colors (&defcolors, vc_term);
	vc_current = defcolors.vc_normal;

	/* change the terminal mode to cbreak/noecho */
	vio_resume (True);

	/* try to get true screen size, from the operating system */
	vio_getsize ();

	/* pretend the cursor is in an impossible place, so we're guaranteed
	 * to move it on the first vio_moveto() or vio_draw() call.
	 */
	physx = physy = -100;

	/* map the arrow keys */
	for (i = 0; i < QTY (keys); i++)
	{
		if (keys[i].cooked != NULL && keys[i].rawin != NULL)
		{
			mapinsert (toCHAR (keys[i].rawin), (int)strlen (keys[i].rawin),
				toCHAR (keys[i].cooked), (int)strlen (keys[i].cooked),
				toCHAR (keys[i].label),
				keys[i].flags);
		}
	}

	/* add the global options to the list known to :set */
	o_ttyunderline = True;
	optinsert ("vio", QTY (goptdesc), goptdesc, &ttygoptvals.term);

	return argc;
}

/* change the shape of the cursor */
static void 
cursorshape (ELVCURSOR shape)
{
	static ELVCURSOR prev_shape;	/* current shape */
	VIOCURSORINFO ci;

	if (shape != prev_shape)
	{
		/* Default width (1 column): */
		ci.cx = 0;
		ci.attr = 0;

		switch (shape)
		{
		case CURSOR_INSERT:
			ci.yStart = -80;
			ci.cEnd = -100;
			break;

		case CURSOR_REPLACE:
			ci.yStart = -0;
			ci.cEnd = -100;
			break;

		case CURSOR_COMMAND:
			ci.yStart = -10;
			ci.cEnd = -90;
			break;

		case CURSOR_QUOTE:
			ci.yStart = -20;
			ci.cEnd = -80;
			break;

		default:
			/* Hide the cursor. */
			ci.attr = -1;
			break;
		}
		VioSetCurType (&ci, 0L);
		prev_shape = shape;
	}
}

/* Repeatedly get events (keystrokes), and call elvis' event functions */
static void 
vio_loop (void)
{
	char	buf[20];
	int		len;
	int		timeout = 0;
	MAPSTATE mst = MAP_CLEAR;
	VWIN	*scan;

	while (vwins)
	{
		/* reset the ttycaught bitmap */
		ttycaught = 0;

		/* if no window is current, then make the newest current */
		if (!current)
		{
			current = vwins;
		}

		/* redraw the window(s) */
		{
			/* redraw each window; the current one last */
			for (scan = vwins; scan; scan = scan->next)
			{ 
				if (scan != current)
				{
					scan->shape = eventdraw ((GUIWIN *)scan);
				}
			}
			current->shape = eventdraw ((GUIWIN *)current);
			movecurs (current);

			/* make the cursor be this window's shape */
			cursorshape (current->shape);
		}

		/* choose a timeout value */
		switch (mst)
		{
		  case MAP_CLEAR:	timeout = 0;		break;
		  case MAP_USER:	timeout = o_usertime;	break;
		  case MAP_KEY:		timeout = o_keytime;	break;
		}

		/* read events */
		do_flush ();
		vio_CM (c_row, c_col);
		len = vio_read (buf, sizeof buf, timeout);

		/* process keystroke data */
		if (len == -2)
		{
			/* vio_read() itself did something.  We don't need to
			 * do anything except the usual screen updates.
			 */
		}
		else if (len == -1)
		{
			/* Maybe the screen was resized?  Get new size */
			vio_getsize ();

			/* Resize the windows to match the new screen.  The
			 * easiest way to do this is to "change" the size of the
			 * current window to its original size and force the
			 * other windows to compensate.  If there is only one
			 * window, then should be resized to the screen size.
			 */
			chgsize (current, vwins->next ? current->height : (int)o_ttyrows, True);
		}
		else
		{
			mst = eventkeys ((GUIWIN *)current, toCHAR (buf), len);

			/* if first keystroke after running an external
			 * program, then we need to expose every window.
			 */
			if (afterprg == 1)
			{
				/* reset the flag BEFORE exposing windows,
				 * or else the eventexpose() won't work right.
				 */
				afterprg = 0;
				vio_resume (True);
				for (scan = vwins; scan; scan = scan->next)
				{ 
					eventexpose  ((GUIWIN *)scan, 0, 0,
					    scan->height - 1, (int)(o_ttycolumns - 1));
				}
			}
			else if (afterprg == 2)
			{
				/* it became 2 while processing the earlier
				 * keystrokes.  Set it to 1 now, so we'll
				 * read one more keystroke before exposing
				 * all the windows.
				 */
				afterprg = 1;
			}
		}
	}
}

/* shut down the VIO interface */
static void 
vio_term (void)
{
	cursorshape (CURSOR_NONE);

	/* change the terminal mode back the way it was */
	vio_suspend ();

	do_flush ();
}

/* This draws a bunch of underscores on the physical screen on the bottom
 * row of a window, if that window doesn't end at the bottom of the screen.
 * This should be called after a window is resized or moved.
 */
static void 
drawborder (VWIN *vw)	/* window whose border needs to be redrawn */
{
	int	col;

	/* if this window ends at the bottom of the screen, then do nothing */
	if (vw->pos + vw->height == o_ttyrows)
	{
		return;
	}

	/* move the physical cursor to the bottom of the window */
	vw->cursx = 0;
	vw->cursy = vw->height - 1;
	movecurs (vw);

	vc_current = vw->vc_underline;
	for (col = 0; col < o_ttycolumns; col++)
	{
		ttych ('_');
	}

	/* figure out where the physical cursor would be after that */
	physx = o_ttycolumns;
}

/* This function changes the height of a given window.  The total heights of
 * all windows must be o_ttyrows and the minimum height of each window is
 * MINHEIGHT.
 */
static void chgsize (vw, newheight, winch)
	VWIN	*vw;		/* window to be resized */
	int	newheight;	/* desired height of window */
	BOOLEAN	winch;		/* Did the whole screen change size? */
{
	VWIN	*scan;
	int	pos;
	int	otherheight;
	int	oldheight;
	int	toosmall;

	/* if the current window can't be as large as requested, then reduce
	 * the requested size.
	 */
	if ((nwindows - 1) * MINHEIGHT + newheight > o_ttyrows)
	{
		newheight = o_ttyrows - (nwindows - 1) * MINHEIGHT;
	}

	/* if window is already the requested height, we're done */
	if (vw->height == newheight && !winch)
	{
		return;
	}

	/* Set the size of the current window.  Also, adjust the sizes of other
	 * windows, and maybe their positions.  If any window other than the
	 * requested one is moved, expose it.  If any window other than the
	 * requested one is resized, then resize it.
	 */
	toosmall = 0;
	do
	{
		for (oldheight = vw->height, pos = 0, scan = vwins;
		     scan;
		     pos += scan->newheight, scan = scan->next)
		{
			/* the requested window? */
			if (scan == vw)
			{
				/* yes, set it */
				scan->newpos = pos;
				scan->newheight = newheight;
			}
			else
			{
				/* no, some other window */

				/* compute the size that this window should be */
				if (!scan->next)
				{
					scan->newheight = o_ttyrows - pos;
					toosmall = MINHEIGHT - scan->newheight;
				}
				else if (scan->next == vw && !scan->next->next)
				{
					scan->newheight = o_ttyrows - pos - newheight;
					toosmall = MINHEIGHT - scan->newheight;
				}
				else
				{
					if (winch)
						otherheight = MINHEIGHT;
					else
						otherheight = scan->height * (o_ttyrows - newheight) / (o_ttyrows - oldheight);
					if (otherheight < MINHEIGHT)
					{
						scan->newheight = MINHEIGHT;
					}
					else
					{
						scan->newheight = otherheight - toosmall;
						if (scan->newheight < MINHEIGHT)
						{
							scan->newheight = MINHEIGHT;
						}
						toosmall -= otherheight - scan->newheight;
					}
				}
				scan->newpos = pos;
			}
		}
	} while (toosmall > 0);

	/* resize/redraw the windows, as necessary */
	for (scan = vwins; scan; scan = scan->next)
	{
		/* set the size & position of this window.  If its
		 * size has changed then resize the window; else if
		 * its position has changed resize the window.
		 */
		if (scan == vw && !winch)
		{
			/* just remember new stats.  Calling function will
			 * call eventredraw() or eventdraw(), as necessary.
			 */
			scan->height = scan->newheight;
			scan->pos = scan->newpos;
		}
		else if (scan->newheight != scan->height || winch)
		{
			scan->height = scan->newheight;
			scan->pos = scan->newpos;
			if (scan->pos + scan->height < o_ttyrows)
			{
				drawborder (scan);
			}
			else
			{
				/* draw the border the hard way: erase last row */
				vc_current = vw->vc_normal;
				physy = o_ttyrows - 1;
				physx = 0;
				c_row = physy;
				c_col = physx;
				vio_CE (vw->vc_normal);
			}
			eventresize ((GUIWIN *)scan, scan->height, (int)o_ttycolumns);
		}
		else if (scan->newpos != scan->pos)
		{
			scan->pos = scan->newpos;
			drawborder (scan);
			eventexpose ((GUIWIN *)scan, 0, 0, scan->height - 1, (int)(o_ttycolumns - 1));
		}
	}
}

/* This function creates a window */
static BOOLEAN 
vio_creategw (char *name,		/* name of new window's buffer */
			 char *attributes)	/* other window parameters, if any */
{
	VWIN *newp;

	/* if we don't have room for any more windows, then fail */
	if (o_ttyrows / (nwindows + 1) < MINHEIGHT)
	{
		return False;
	}

	/* create a window */
	newp = safealloc (1, sizeof (VWIN));

	/* initialize the window */
	if (vwins)
	{
		newp->height = 0;
		newp->pos = o_ttyrows;
	}
	else
	{
		newp->height = o_ttyrows;
		newp->pos = 0;
	}
	newp->cursx = newp->cursy = 0;
	newp->shape = CURSOR_NONE;

	/* Set default colors. */
	newp->vc_bold = defcolors.vc_bold;
	newp->vc_italic = defcolors.vc_italic;
	newp->vc_underline = defcolors.vc_underline;
	newp->vc_hilite = defcolors.vc_hilite;
	newp->vc_normal = defcolors.vc_normal;
	newp->vc_fixed = defcolors.vc_fixed;
	newp->vc_emph = defcolors.vc_emph;

	/* insert the new window into the list of windows */
	newp->next = vwins;
	vwins = newp;
	nwindows++;

	/* adjust the heights of the other windows to make room for this one */
	chgsize (newp, (int)(o_ttyrows / nwindows), False);
	drawborder (newp);

	/* make elvis do its own initialization */
	if (!eventcreate ((GUIWIN *)newp, NULL, name, newp->height, (int)o_ttycolumns))
	{
		/* elvis can't make it -- fail */
		safefree (newp);
		return False;
	}

	/* make the new window be the current window */
	current = newp;

	return True;
}

/* This function deletes a window */
static void 
vio_destroygw (GUIWIN *gw,		/* window to be destroyed */
			  BOOLEAN force)	/* if True, try harder */
{
	VWIN *scan, *lag;

	/* delete the window from the list of windows */
	for (lag = NULL, scan = vwins; scan != (VWIN *)gw; lag = scan, scan = scan->next)
	{
	}
	if (lag)
	{
		lag->next = scan->next;
	}
	else
	{
		vwins = scan->next;
	}

	/* if it was the current window, it isn't now */
	if (scan == current)
	{
		current = vwins;
	}

	/* adjust the sizes of other windows (if any) */
	nwindows--;
	if (nwindows > 0)
	{
		chgsize ((VWIN *)gw, 0, False);
	}

	/* If this is the last window, move the cursor to the last line, and
	 * erase it.  If the buffer is going to be written, this is where the
	 * "wrote..." message will appear.
	 */
	if (nwindows == 0)
	{
		vio_revert (NULL);
		vio_CM(o_ttyrows - 1, 0);
		vio_CE(vc_term);
	}

	/* simulate a "destroy" event */
	eventdestroy (gw);

	/* free the storage */
	safefree (gw);
}

/* This function changes window focus */
static BOOLEAN
vio_focusgw (GUIWIN *gw)		/* window to be the new "current" window */
{
	current = (VWIN *)gw;
	return True;
}

/* This function handles the visual <Tab> command */
static BOOLEAN vio_tabcmd (gw, key2, count)
	GUIWIN	*gw;	/* window that the command should affect */
	_CHAR_	key2;	/* second key of <Tab> command */
	long	count;	/* argument of the <Tab> command */
{
	VWIN	*vw = (GUIWIN *)gw;
	int	newheight;
	int	oldheight;
	int	oldpos;

	/* if only one window, then we can't change its size */
	if (nwindows == 1)
		return False;

	/* remember the old position */
	newheight = oldheight = vw->height;
	oldpos = vw->pos;

	switch (key2)
	{
	  case '=':
		if (count >= MINHEIGHT)
		{
			newheight = count;
			break;
		}
		/* else fall through... */

	  case '+':
		newheight += (count ? count : 1);
		break;

	  case '-':
		newheight -= (count ? count : 1);
		if (newheight < MINHEIGHT)
		{
			newheight = MINHEIGHT;
		}
		break;

	  case '\\':
		newheight = o_ttyrows; /* will be reduced later */
		break;

	  default:	return False;
	}

	/* try to change the heights of other windows to make this one fit */
	chgsize (vw, newheight, False);
	newheight = vw->height;

	/* resize/expose this window */
	if (newheight != oldheight)
	{
		drawborder (vw);
		eventresize (vw, vw->height, (int)o_ttycolumns);
	}
	else if (vw->pos != oldpos)
	{
		drawborder (vw);
		eventexpose (vw, 0, 0, newheight - 1, (int)(o_ttycolumns - 1));
	}
	return True;
}

/* This function rings the bell */
static void
vio_beep (GUIWIN *gw)	/* window that generated the beep */
{
#if 0
	if (VB)
		tputs (VB, 0, ttych);
	else
		ttych ('\007');
#endif
}

/* This function converts key labels to raw codes */
static int keylabel (given, givenlen, label, rawptr)
	CHAR	*given;		/* what the user typed in as the key name */
	int	givenlen;	/* length of the "given" string */
	CHAR	**label;	/* standard name for that key */
	CHAR	**rawptr;	/* control code sent by that key */
{
	int	i;

	/* compare the given text to each key's strings */
	for (i = 0; i < QTY(keys); i++)
	{
		/* ignore unsupported keys */
		if (!keys[i].rawin)
			continue;

		/* does given string match key label or raw characters? */
		if ((!strncmp(keys[i].label, tochar8(given), (size_t)givenlen) && !keys[i].label[givenlen])
		 || (!strncmp(keys[i].rawin, tochar8(given), (size_t)givenlen) && !keys[i].rawin[givenlen]))
		{

			/* Set the label and rawptr pointers, return rawlen */
			*label = toCHAR(keys[i].label);
			*rawptr = toCHAR(keys[i].rawin);
			return CHARlen(*rawptr);
		}
	}

	/* We reached the end of the keys[] array without finding a match,
	 * so this given string is not a key.
	 */
	return 0;
}

/* This function defines colors for fonts */
static BOOLEAN 
vio_color (GUIWIN *gw,	/* window whose colors are being set */
		  _char_ font,	/* font being changed: n/b/i/u else highlighted */
		  CHAR *fg,		/* name of desired foreground color */
		  CHAR *bg)		/* name of desired background color */
{
	VWIN	*vw = (VWIN *)gw;
	VWIN	*other; /* some other window */
	BOOLEAN	ret;	/* return code -- True if successful */

	/* we must set normal colors first */
	if ((!fgcolored || (bg && !bgcolored)) && font != 'n')
	{
		msg(MSG_ERROR, "must set normal colors first");
		return False;
	}

	/* if no window specified, or this is the first :color command,
	 * then we're setting the default colors.
	 */
	if (!vw || !fgcolored)
	{
		vw = &defcolors;
	}

	/* revert to normal font now; if we wait until after setting colors,
	 * we might not know how to do it anymore!
	 */
	vio_revert (vw);

	/* set the colors */
	ret = set_font_color (vw, font, tochar8(fg), tochar8(bg));

	/* if colors weren't set before, then copy colors to all windows */
	if (!fgcolored || (!bgcolored && bg && *bg))
	{
		for (other = vwins; other; other = other->next)
		{
			/* skip the window that we just set */
			if (other == vw)
			{
				continue;
			}

			/* copy the colors */
			other->vc_normal = vw->vc_normal;
			other->vc_fixed = vw->vc_fixed;
			other->vc_bold = vw->vc_bold;
			other->vc_emph = vw->vc_emph;
			other->vc_italic = vw->vc_italic;
			other->vc_underline = vw->vc_underline;
			other->vc_hilite = vw->vc_hilite;
		}

		/* we've set colors now! */
		fgcolored = True;
		if (bg && *bg)
		{
			bgcolored = True;
		}
	}

	/* remember the current window's colors, to use them as the default
	 * for any window that gets created after this.
	 */
	if (vw != &defcolors)
	{
		defcolors = *vw;
	}

	/* We probably need to reset the screen's current attribute */
	vio_revert(vw);

	return ret;
}

static BOOLEAN isfilter;

/* Suspend curses while running an external program */
static BOOLEAN
vio_prgopen (char *command,		/* the shell command to run */
			BOOLEAN	willwrite,	/* redirect stdin from elvis */
			BOOLEAN	willread)	/* redirect stdiout back to elvis */
{
	/* unless both stdin and stdout/stderr are going to be redirected,
	 * move the cursor to the bottom of the screen before running program.
	 */
	isfilter = (BOOLEAN)(willwrite && willread);
	if (!isfilter)
	{
		vio_CM (o_ttyrows - 1, 0);
		vio_CE (vc_term);
		vio_reset ();
		do_flush ();
	
		/* suspend curses */
		vio_suspend ();
	}

	/* try to call the regular prgopen(); if it fails, then clean up */
	if (!prgopen (command, willwrite, willread))
	{
		if (!isfilter)
			vio_resume (True);
		return False;
	}

	return True;
}

/* After running a program, resume curses and redraw all screens */
static int
vio_prgclose (void)
{
	int	status;

	/* wait for the program to terminate */
	status = prgclose ();

	/* resume curses */
	if (!isfilter)
	{
		vio_resume (True);

		/* If the program left the cursor anywhere but the first
		 * column of the last line of the screen, move the cursor
		 * there and scroll to the next line, for good measure.
		 */
		if (c_row != o_ttyrows - 1 || c_col != 0)
		{
			vio_CM (o_ttyrows - 1, 0);
			VioWrtTTY ("\n", 1, 0);
		}
		vio_CE (defcolors.vc_normal);

		/* Okay, now we're in a weird sort of situation.  The screen is
		 * about to be forced to display "Hit <Enter> to continue" on
		 * the bottom of the window in open mode, and then wait for a
		 * keystroke.  That's a Good Thing.  But there are two problems
		 * we need to address:
		 *    * We want that prompt to appear at the bottom of the
		 *	screen, not the bottom of the window.
		 *    * After the user hits a key, we want to redraw all
		 *	windows.
		 *
		 * We'll set a flag indicating this situation.  The movecurs()
		 * function will test for that flag, and merely pretend to move
		 * the cursor when it is set.  The loop() function will test
		 * that flag after each keystroke, and expose all windows if
		 * it is set.
		 */
		afterprg = 2;
		afterscrl = 0;
	}

	return status;
}

#if 0
/* This function converts screen coordinates into a window, and coordinates
 * within that window.
 */
/* not needed, clashes with TCap ttywindow */
GUIWIN *ttywindow (ttyrow, ttycol, winrow, wincol)
	int	ttyrow, ttycol;		/* screen coordinates in */
	int	*winrow, *wincol;	/* window coordinates out */
{
	VWIN	*vw;

	if (ttycol < 0 || ttycol >= o_ttycolumns)
		return NULL;
	for (vw = vwins;
	     vw && (vw->pos > ttyrow || ttyrow >= vw->pos + vw->height);
	     vw = vw->next)
	{
	}
	if (vw)
	{
		*winrow = ttyrow - vw->pos;
		*wincol = ttycol;
	}
	return (GUIWIN *)vw;
}

/* This function catches signals, especially SIGINT */
static void catchsig (signo)
	int	signo;
{
	ttycaught |= (1 << signo);
}
#endif 


/* Check for signs of boredom from user, so we can abort a time-consuming
 * operation.  Here we check to see if SIGINT has been caught recently.
 * Returns True to abort an operation, or False to continue it.
 */
static BOOLEAN 
vio_poll (BOOLEAN reset)
{
	return (BOOLEAN)((ttycaught & (1 << SIGINT)) != 0);
}

/* structs of this type are used to describe each available GUI */
GUI guivio =
{
	"vio",          /* name */
	"OS/2 console interface with windows & color",
	False,          /* exonly */
	True,           /* newblank */
	False,          /* minimizeclr */
	False,          /* scrolllast */
	False,          /* shiftrows */
	8,              /* movecost */
	0,              /* opts */
	NULL,           /* optdescs */
	vio_test,
	vio_init,
	NULL,           /* usage */
	vio_loop,
	vio_poll,
	vio_term,
	vio_creategw,
	vio_destroygw,
	vio_focusgw,
	NULL,           /* retitle */
	vio_reset,
	vio_flush,
	vio_moveto,
	vio_draw,
	vio_shift,
	vio_scroll,
	vio_clrtoeol,
	NULL,           /* textline */
	vio_beep,       /* beep */
	NULL,           /* msg */
	NULL,           /* scrollbar */
	NULL,           /* status */
	keylabel,
	NULL,           /* clipopen */
	NULL,           /* clipwrite */
	NULL,           /* clipread */
	NULL,           /* clipclose */
	vio_color,      /* color */
	NULL,           /* guicmd */
	vio_tabcmd,
	NULL,           /* save */
	NULL,           /* wildcard */
	vio_prgopen,
	vio_prgclose,
	NULL            /* stop */
};
#endif

