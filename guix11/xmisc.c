/* xmisc.c */

/* Copyright 1997 by Steve Kirkendall */

char id_xmisc[] = "$Id: xmisc.c,v 2.9 1997/11/12 02:10:36 steve Exp $";

#include "elvis.h"
#ifdef GUI_X11
# include "guix11.h"


static X_LOADEDFONT	*fonts;		/* list of allocated fonts */
static X_LOADEDCOLOR	*colors;	/* list of allocated colors */


/* This function loads a font if it isn't already loaded. */
X_LOADEDFONT *x_loadfont(name)
	char	*name;	/* name of the font */
{
	X_LOADEDFONT *font;
	XFontStruct *info;

	/* see if it is already loaded */
	for (font = fonts; font && strcmp(name, font->name); font = font->next)
	{
	}

	/* if already loaded, then just increment its count */
	if (font)
	{
		font->links++;
		return font;
	}

	/* else load the font into a new stucture */
	info = XLoadQueryFont(x_display, name);
	if (!info)
	{
		msg(MSG_ERROR, "[s]can't load font $1", name);
		return NULL;
	}
	font = (X_LOADEDFONT *)safekept(1, sizeof(X_LOADEDFONT));
	font->fontinfo = info;
	font->name = safekdup(name);
	font->height = info->descent + info->ascent;
	font->links = 1;

	/* link the new structure into the list, and return it */
	font->next = fonts;
	fonts = font;
	return font;
}


/* This function unloads a font.  If no window is using the font after that,
 * then the storage space is freed.
 */
void x_unloadfont(font)
	X_LOADEDFONT	*font;	/* font to be freed */
{
	X_LOADEDFONT	*scan;

	/* If no font was given, do nothing */
	if (!font)
	{
		return;
	}

	assert(fonts != NULL);

	/* decrement the count.  If other windows are still using this
	 * font, then that's all we should for now.
	 */
	if (--font->links > 0)
	{
		return;
	}

	/* delete the font from the list of fonts */
	if (fonts == font)
	{
		fonts = font->next;
	}
	else
	{
		for (scan = fonts; scan->next != font; scan = scan->next)
		{
			assert(scan->next);
		}
		scan->next = font->next;
	}

	/* free its resources */
	safefree(font->name);
	XFreeFont(x_display, font->fontinfo);
	safefree(font);
}


/* This function allocates a color */
unsigned long x_loadcolor(name, def)
	CHAR		*name;	/* name of color to load */
	unsigned long	def;	/* default color, if can't load named color */
{
	XColor		exact, color;
	X_LOADEDCOLOR	*scan;

	/* if mono, or no name, then just use the default */
	if (x_mono || !name || !*name)
		goto UseDefault;

	/* was this color name used before? */
	for (scan = colors; scan; scan = scan->next)
	{
		if (!CHARcmp(name, scan->name))
		{
			scan->links++;
			return scan->pixel;
		}
	}

	/* try to load the color */
	if (!XAllocNamedColor(x_display, x_colormap, tochar8(name), &exact, &color))
	{
		msg(MSG_WARNING, "[S]could not allocate color $1", name);
		goto UseDefault;
	}

	/* was it rounded to a previous color? */
	for (scan = colors; scan; scan = scan->next)
	{
		if (scan->pixel == color.pixel)
		{
			scan->links++;
			return scan->pixel;
		}
	}

	/* new color -- remember it */
	scan = safekept(1, sizeof(*scan));
	scan->name = CHARkdup(name);
	scan->pixel = color.pixel;
	scan->links = 1;
	scan->next = colors;
	colors = scan;
	return color.pixel;

UseDefault:
	/* look for the default pixel code in the list */
	for (scan = colors; scan; scan = scan->next)
	{
		if (scan->pixel == def)
		{
			scan->links++;
			return def;
		}
	}

	/* not in list -- add it */
	scan = safekept(1, sizeof *scan);
	scan->name = toCHAR(def == x_black ? "black" : "white");
	scan->pixel = def;
	scan->links = 1;
	scan->next = colors;
	colors = scan;
	return def;
}


void x_unloadcolor(pixel)
	unsigned long	pixel;	/* a color to free */
{
	X_LOADEDCOLOR	*scan, *lag;

	/* never load/unload the default white & black values */
	if (pixel == x_white || pixel == x_black)
	{
		return;
	}

	/* find the color */
	for (lag = NULL, scan = colors; scan->pixel != pixel; lag = scan, scan = scan->next)
	{
		assert(scan->next);
	}

	/* decrement the link counter.  If other windows are using it,
	 * then leave it.
	 */
	if (--scan->links > 0)
	{
		return;
	}

	/* free it */
	XFreeColors(x_display, x_colormap, &pixel, 1, 0);
	safefree(scan->name);
	if (lag)
		lag->next = scan->next;
	else
		colors = scan->next;
	safefree(scan);
}


/* draw a beveled button.  It can be an arrow head, or a rectangular button.
 * If height=0, the area will be cleared to the window's background color.
 * Positive heights make the button "stick out", and negative values make it
 * appear to be "pushed in"; either way, the button's foreground will be
 * drawn in the scrollbar foreground color (regardless of the window's normal
 * foreground color).
 */
void x_drawbevel(xw, win, x, y, w, h, dir, height)
	X11WIN		*xw;	/* top-level app window to draw in */
	Window		win;	/* widget where drawing should occur */
	int		x, y;	/* upper-left corner of the rectangle */
	unsigned	w, h;	/* width and height of the rectangle */
	_char_		dir;	/* one of "udrl" for arrow, or 'b' for button */
	int		height;	/* height of button, 0 to disable */
{
	XGCValues	gcv;
	int		b, r;	/* bottom & right edges */
	unsigned long	topleft;
	unsigned long	bottomright;
	XPoint		vertex[4];	/* points of arrowhead */
	int		dx[4], dy[4];	/* inward movement */
	int		ntop;		/* number of points in topleft color */
	int		i;

	/* if height is 0, then clear the button area */
	if (height == 0)
	{
		XClearArea(x_display, win, x, y, w, h, False);
		return;
	}

	/* choose the edge colors */
	if (height > 0)
	{
		topleft = x_white;
		bottomright = x_black;
	}
	else
	{
		topleft = x_black;
		bottomright = x_white;
		height = -height;
	}

	/* set the foreground color to the scrollbar's color */
	gcv.foreground = (win == xw->sb.win ? xw->sb.fgscroll : xw->tb.face);
	gcv.fill_style = FillSolid;
	XChangeGC(x_display, xw->gc, GCForeground|GCFillStyle, &gcv);
	xw->fg = gcv.foreground;

	/* For arrow heads, choose the vertex points.  For rectangular,
	 * draw the button and then return.
	 */
	switch (dir)
	{
	  case 'u':
		vertex[0].x = 0, vertex[0].y = h - 1, dx[0] = 1, dy[0] = -1;
		vertex[1].x = w / 2, vertex[1].y = 0, dx[1] = 0, dy[1] = 1;
		vertex[2].x = w - 1, vertex[2].y = h - 1, dx[2] = dy[2] = -1;
		ntop = 1;
		break;

	  case 'd':
		vertex[0].x = w / 2, vertex[0].y = h - 1, dx[0] = 0, dy[0] = -1;
		vertex[1].x = 0, vertex[1].y = 0, dx[1] = dy[1] = 1;
		vertex[2].x = w - 1, vertex[2].y = 0, dx[2] = -1, dy[2] = 1;
		ntop = 2;
		break;

	  case 'l':
		vertex[0].x = 0, vertex[0].y = h / 2, dx[0] = 1, dy[0] = 0;
		vertex[1].x = w - 1, vertex[1].y = 0, dx[1] = -1, dy[1] = 1;
		vertex[2].x = w - 1, vertex[2].y = h - 1, dx[2] = dy[2] = -1;
		ntop = 1;
		break;

	  case 'r':
	  	vertex[0].x = 0, vertex[0].y = h - 1, dx[0] = 1, dy[0] = -1;
	  	vertex[1].x = 0, vertex[1].y = 0, dx[1] = dy[1] = 1;
	  	vertex[2].x = w - 1, vertex[2].y = h / 2, dx[2] = -1, dy[2] = 0;
	  	ntop = 2;
	  	break;

	  default: /* rectangular button */
		/* draw the button's face */
		XFillRectangle(x_display, win, xw->gc, x, y, w, h);

		/* locate the bottom & right edges */
		b = y + h - 1;
		r = x + w - 1;

		/* draw the outermost bevel sides */
		while (--height >= 0)
		{
			/* draw the bottom bevel edges */
			XSetForeground(x_display, xw->gc, bottomright);
			XDrawLine(x_display, win, xw->gc, x, b, r, b);

			/* draw the top & left bevel edges */
			XSetForeground(x_display, xw->gc, topleft);
			XDrawLine(x_display, win, xw->gc, x, y, r, y);
			XDrawLine(x_display, win, xw->gc, x, y, x, b);

			/* draw the right bevel edge */
			XSetForeground(x_display, xw->gc, bottomright);
			xw->fg = bottomright;
			XDrawLine(x_display, win, xw->gc, r, y, r, b);

			/* move the edges in slightly, to continue the bevel */
			x++;
			y++;
			r--;
			b--;
		}

		/* for rectangular buttons, we're done */
		return;
	}

	/* If we get here, then we're drawing a triangular button */

	/* Translate the polygon to its intended position within the window */
	for (i = 0; i < 3; i++)
	{
		vertex[i].x += x;
		vertex[i].y += y;
	}

	/* Close the polygon.  This isn't needed for XFillPolygon(), but it
	 * helps when drawing the edges via XDrawLines().
	 */
	vertex[3] = vertex[0], dx[3] = dx[0], dy[3] = dy[0];

	/* draw the face of the button */
	XFillPolygon(x_display, win, xw->gc, vertex, 3, Convex, CoordModeOrigin);

	/* draw the beveled edges */
	while (--height >= 0)
	{
		XSetForeground(x_display, xw->gc, bottomright);
		XDrawLines(x_display, win, xw->gc, vertex + ntop, 4 - ntop, CoordModeOrigin);
		XSetForeground(x_display, xw->gc, topleft);
		XDrawLines(x_display, win, xw->gc, vertex, ntop + 1, CoordModeOrigin);

		/* move the vertices inward for next iteration */
		for (i = 0; i < QTY(vertex); i++)
		{
			vertex[i].x += dx[i];
			vertex[i].y += dy[i];
		}
	}
	xw->fg = bottomright;
}

/* Draw some text.  For color displays, this just calls XDrawString.  For
 * mono displays, it draws black characters surrounded by a white outline.
 */
void x_drawstring(display, win, gc, x, y, str, len)
	Display	*display;
	Window	win;
	GC	gc;
	int	x, y;
	char	*str;
	int	len;
{
	int	i;
	static struct {int dx, dy;} delta[] = {
		{-2, -1}, { 2, -1}, {-2,  0}, { 2,  0}, {-2,  1}, { 2,  1},
		{-1, -1}, { 1, -1}, {-1,  1}, { 1,  1},
		{ 0, -1}, {-1,  0}, { 1,  0}, { 0,  1}};
	static int start[] = {QTY(delta), QTY(delta) - 4, QTY(delta) - 8, 0};

	if (x_mono && o_outlinemono > 0)
	{
		XSetForeground(display, gc, x_white);
		for (i = start[o_outlinemono]; i < QTY(delta); i++)
		{
			XDrawString(display, win, gc,
				x + delta[i].dx, y + delta[i].dy, str, len);
		}
		XSetForeground(display, gc, x_black);
	}
	XDrawString(display, win, gc, x, y, str, len);
}


# ifdef NEED_XRMCOMBINEFILEDATABASE
/* X11R4 lacks this function */
void XrmCombineFileDatabase(filename, target_db, override)
	char		*filename;	/* name of resource file */
	XrmDatabase	*target_db;	/* address of resource database */
	Bool		override;	/* ignored -- always overrides */
{
	XrmDatabase	*source_db;

	source_db = XrmGetFileDatabase(filename);
	if (source_db)
		XrmMergeDatabases(source_db, target_db);
}
# endif /* NEED_XRMCOMBINEFILEDATABASE */

#endif /* GUI_X11 */
