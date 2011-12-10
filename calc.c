/* calc.c */
/* Copyright 1995 by Steve Kirkendall */

char id_calc[] = "$Id: calc.c,v 2.61 1998/12/04 23:12:25 steve Exp $";

#include "elvis.h"
#include <setjmp.h>
#ifdef TRY
# include <getopt.h>
# undef o_true
# define o_true "True"
# undef o_false
# define o_false "False"
# undef isdigit
# undef isupper
# undef islower
# undef isalnum
# undef toupper
# undef tolower
# if USE_PROTOTYPES
    extern int isdigit(int c);
    extern int isupper(int c);
    extern int islower(int c);
    extern int isalnum(int c);
    extern int toupper(int c);
    extern int tolower(int c);
# endif
#endif


#if USE_PROTOTYPES
static int copyname(CHAR *dest, CHAR *src, BOOLEAN num);
static BOOLEAN func(CHAR *name, CHAR *arg);
static BOOLEAN apply(void);
static CHAR *applyall(int prec);
#endif

#ifndef TRY
static CHAR *feature[] =
{
# ifdef PROTOCOL_HTTP
	toCHAR("http"),
# endif
# ifdef PROTOCOL_FTP
	toCHAR("ftp"),
# endif
# ifdef FEATURE_SHOWTAG
	toCHAR("showtag"),
# endif
# ifdef FEATURE_LPR
	toCHAR("lpr"),
# endif
# ifdef FEATURE_ALIAS
	toCHAR("alias"),
# endif
# ifdef FEATURE_MKEXRC
	toCHAR("mkexrc"),
# endif
# ifdef FEATURE_COMPLETE
	toCHAR("complete"),
#endif
# ifdef FEATURE_RAM
	toCHAR("ram"),
# endif
	NULL
};
#endif

/* This array describes the operators */
static struct
{
	char	*name;	/* name of the operator */
	short	prec;	/* natural precedence of the operator */
	char	code;	/* function code for applying operator */
	char	subcode;/* details, depend on function */
} opinfo[] =
{
	{"Func",1,	'f',	'('},
	{";",	2,	'c',	';'},
	{",",	2,	'c',	','},
	{"||",	4,	'b',	'|'},
	{"&&",	5,	'b',	'&'},
	{"!=",	9,	's',	'!'},
	{"==",	9,	's',	'='},
	{"<=",	10,	's',	'l'},
	{">=",	10,	's',	'g'},
	{"<<",	11,	'i',	'<'},
	{">>",	11,	'i',	'>'},
	{":",	3,	't',	':'},
	{"?",	2,	't',	'?'},
	{"|",	6,	'i',	'|'},
	{"^",	7,	'i',	'^'},
	{"&",	8,	'i',	'&'},
	{"=",	9,	's',	'='},
	{"<",	10,	's',	'<'},
	{">",	10,	's',	'>'},
	{"+",	12,	'i',	'+'},
	{"-",	12,	'i',	'-'},
	{"%",	13,	'i',	'%'},
	{"*",	13,	'i',	'*'},
	{"/",	13,	'i',	'/'},
	{"!",	14,	'u',	'!'},
	{"~",	14,	'u',	'~'},
};


/* The following variables are used during the evaluation of each expression */
static struct
{
	CHAR	*first;	/* first argument to operator */
	int	idx;	/* index into opinfo of the operator */
	int	prec;	/* precedence of operator */
} opstack[10];
static int	ops;	/* stack pointer for opstack[] */

/* The following variables are used for storing the parenthesis level */
static CHAR	*parstack[20];

/* ultimately, the result returned by calculate() */
static	CHAR	result[1024];
#define RESULT_END			(&result[QTY(result) - 3])
#define RESULT_AVAIL(from)		((int)(RESULT_END - (CHAR *)(from)))
#define RESULT_OVERFLOW(from, need)	(RESULT_AVAIL(from) < (int)(need))


/* This function returns True iff a string looks like an integer */
BOOLEAN calcnumber(str)
	CHAR	*str;	/* a nul-terminated string to check */
{
	BOOLEAN	dp = False;	/* has decimal point been seen yet? */

	/* allow a leading "-" */
	if (*str == '-')
		str++;

	/* The only decimal number that can start with '0' is zero */
	if (str[0] == '0' && str[1])
		return False;

	/* Require at least one digit, and don't allow any non-digits */
	do
	{
		if (*str == '.' && dp == False && str[1])
			dp = True;
		else if (!isdigit(*str))
			return False;
	} while (*++str);
	return True;
}


/* This function returns True if passed a string which looks like a number,
 * and False if it doesn't.  This function differs from calcnumber() in that
 * this one also converts octal numbers, hex numbers, and literal characters
 * to base ten.  Note that the length of the string may change.
 */
BOOLEAN calcbase10(str)
	CHAR	*str;
{
	long	num;
	int	i;

	/* do the easy tests first */
	if (calcnumber(str)) return True;
	if (*str != '0' && *str != '\'') return False;

	if (str[0] == '\'')
	{
		if (str[1] == '\\')
		{
			if (!str[2] || str[3] != '\'')
				return False;
			switch (str[2])
			{
			  case '0':	num = 0;	break;
			  case 'a':	num = '\007';	break;
			  case 'b':	num = '\b';	break;
			  case 'E':	num = '\033';	break;
			  case 'f':	num = '\f';	break;
			  case 'n':	num = '\n';	break;
			  case 'r':	num = '\r';	break;
			  case 't':	num = '\t';	break;
			  default:	num = str[2];	break;
			}
			i = 4;
		}
		else
		{
			if (str[1] == '\'' || !str[1] || str[2] != '\'')
				return False;
			num = str[1];
			i = 3;
		}
	}
	else if (str[1] == 'x')
	{
		/* is it hex? */
		for (i = 2, num = 0; ; i++)
		{
			if (str[i] >= '0' && str[i] <= '9')
				num = num * 16 + str[i] - '0';
			else if (str[i] >= 'a' && str[i] <= 'f')
				num = num * 16 + str[i] - 'a' + 10;
			else
				break;
		}
	}
	else
	{
		/* is it octal? */
		for (i = 1, num = 0; ; i++)
		{
			if (str[i] >= '0' && str[i] <= '7')
				num = num * 8 + str[i] - '0';
			else
				break;
		}
	}

	/* If we hit a problem before the end of the string, it isn't a number */
	if (str[i]) return False;

	/* We have a number!  Convert to decimal */
	long2CHAR(str, num);
	return True;
}


/* This function returns False if the string looks like any kind of "false"
 * value, and True otherwise.  The false values are "", "0", "false", "no",
 * and the value of the `false' option.
 */
BOOLEAN calctrue(str)
	CHAR	*str;	/* the sting to be checked */
{
	if (!str || !*str || !CHARcmp(str, toCHAR("0"))
		|| !CHARcmp(str, toCHAR("false")) || !CHARcmp(str, toCHAR("no"))
		|| (o_false && !CHARcmp(str, o_false)))
	{
		return False;
	}
	return True;
}


/* This function copies characters up to the next non-alphanumeric character,
 * nul-terminates the copy, and returns the number of characters copied.
 */
static int copyname(dest, src, num)
	CHAR	*dest;	/* where to copy into */
	CHAR	*src;	/* start of alphanumeric string to copy from */
	BOOLEAN	num;	/* treat numbers specially? */
{
	int	i;

	/* copy until non-alphanumeric character is encountered */
	if (num && isdigit(*src))
	{
		for (i = 0; isdigit(*src); i++)
		{
			if (RESULT_OVERFLOW(dest, 2)) return 0;
			*dest++ = *src++;
		}
	}
	else
	{
		for (i = 0; isalnum(*src) || *src == '_'; i++)
		{
			if (RESULT_OVERFLOW(dest, 2)) return 0;
			*dest++ = *src++;
		}
	}
	*dest = '\0';
	return i;
}

/* This function implements the built-in "functions".  The name indicates
 * which function should be performed, and arg is its only argument.  The
 * result should be a string, and it should be copied over the name; func
 * should then return True to indicate success.  If the function fails for
 * any reason, func() should emit an error message and return False.
 *
 * The functions supported are:
 *   strlen(string)	return the number of characters in the string.
 *   tolower(string)	returns a lowercase version of string.
 *   toupper(string)	returns an uppercase version of string.
 *   isnumber(string)	return "true" iff string is a decimal number
 *   htmlsafe(string)	return an HTML-encoded version of string
 *   quote(chars,str)	return a copy of str with backslashes before chars
 *   unquote(chars,str)	return a copy of str with backslashes before chars
 *   hex(number)	return a string representing the hex value of number
 *   octal(number)	return a string representing the octal value of number
 *   char(number)	return a single-character string
 *   exists(file)	return "true" iff file exists
 *   dirperm(file)	return a string indicating file attributes.
 *   dirfile(file)	return the filename part of a pathname (including ext)
 *   dirname(file)	return the directory part of a pathname.
 *   dirdir(file)	return the directory, like dirname(file).
 *   dirext(file)	return the filename extension.
 *   basename(file)	return the filename without extension.
 *   elvispath(file)	return the pathname of a file in elvis' path, or ""
 *   buffer(buf)	return "true" iff buffer exist
 *   feature(name)	return "true" iff the named feature is supported
 *   knownsyntax(file)	return language if the file's extension is in elvis.syn
 *   current(what)	return line, column, word, mode, next, prev, or tag
 */
static BOOLEAN func(name, arg)
	CHAR	*name;	/* name of function to apply */
	CHAR	*arg;	/* the argument to the function */
{
	CHAR	*tmp;
#ifndef TRY
	char	*c;
	int	i;
	MARK	begin;
	MARKBUF	end;
#endif

	if (!CHARcmp(name, toCHAR("strlen")))
	{
		long2CHAR(name, (long)CHARlen(arg));
		return True;
	}
	else if (!CHARcmp(name, toCHAR("tolower")))
	{
		for (; *arg; arg++)
			*name++ = (isupper(*arg) ? tolower(*arg) : *arg);
		*name = '\0';
		return True;
	}
	else if (!CHARcmp(name, toCHAR("toupper")))
	{
		for (; *arg; arg++)
			*name++ = (islower(*arg) ? toupper(*arg) : *arg);
		*name = '\0';
		return True;
	}
	else if (!CHARcmp(name, toCHAR("isnumber")))
	{
		if (RESULT_OVERFLOW(name, 6)) goto Overflow;
		CHARcpy(name, calcnumber(arg) ? o_true : o_false);
		return True;
	}
#ifndef TRY
	else if (!CHARcmp(name, toCHAR("htmlsafe")))
	{
		for (i = 0, tmp = NULL; arg[i]; i++)
		{
			switch (arg[i])
			{
			  case '&':	buildstr(&tmp, "&amp;");	break;
			  case '<':	buildstr(&tmp, "&lt;");		break;
			  case '>':	buildstr(&tmp, "&gt;");		break;
			  case '"':	buildstr(&tmp, "&quot;");	break;
			  case '\t':	buildCHAR(&tmp, ' ');		break;
			  default:	buildCHAR(&tmp, arg[i]);
			}
		}
		if (tmp)
		{
			if (RESULT_OVERFLOW(name, CHARlen(tmp))) goto Overflow;
			CHARcpy(name, tmp);
			safefree(tmp);
		}
		else
		{
			*name = '\0';
		}
		return True;
	}
#endif
	else if (!CHARcmp(name, toCHAR("hex")))
	{
		if (!calcnumber(arg)) goto NeedNumber;
		if (RESULT_OVERFLOW(name, 10)) goto Overflow;
		sprintf((char *)name, "0x%lx", CHAR2long(arg));
		return True;
	}
	else if (!CHARcmp(name, toCHAR("octal")))
	{
		if (!calcnumber(arg)) goto NeedNumber;
		if (RESULT_OVERFLOW(name, 12)) goto Overflow;
		sprintf((char *)name, "0%lo", CHAR2long(arg));
		return True;
	}
	else if (!CHARcmp(name, toCHAR("char")))
	{
		if (!calcnumber(arg)) goto NeedNumber;
		*name++ = (CHAR)CHAR2long(arg);
		*name = '\0';
		return True;
	}
	else if (!CHARcmp(name, toCHAR("dirext")))
	{
		/* find the last '.' in the name */
		for (tmp = arg + CHARlen(arg); --tmp >= arg && isalnum(*tmp); )
		{
		}
		if (*tmp != '.')
		{
			tmp = toCHAR("");
		}
		CHARcpy(name, tmp);
		return True;
	}
#ifndef TRY
	else if (!CHARcmp(name, toCHAR("quote")))
	{
		/* divide the arg into "chars" and "str" fields */
		tmp = CHARchr(arg, (CHAR)',');
		if (!tmp)
		{
			goto Need2Args;
		}
		*tmp++ = '\0';

		/* build a copy with backslashes */
		tmp = addquotes(arg, tmp);

		/* if the resulting string fits in buffer, then store it */
		if (RESULT_OVERFLOW(name, CHARlen(tmp)))
			goto Overflow;
		CHARcpy(name, tmp);
		safefree(tmp);
		return True;
	}
	else if (!CHARcmp(name, toCHAR("unquote")))
	{
		/* divide the arg into "chars" and "str" fields */
		tmp = CHARchr(arg, (CHAR)',');
		if (!tmp)
		{
			goto Need2Args;
		}
		*tmp++ = '\0';

		/* build a copy with backslashes */
		tmp = removequotes(arg, tmp);

		/* store the resulting string (it *will* fit) */
		CHARcpy(name, tmp);
		safefree(tmp);
		return True;
	}
	else if (!CHARcmp(name, toCHAR("exists")))
	{
		switch (urlperm(tochar8(arg)))
		{
		  case DIR_INVALID:
		  case DIR_BADPATH:
		  case DIR_NOTFILE:
		  case DIR_NEW:
			CHARcpy(name, o_false);
			break;

		  default:
			CHARcpy(name, o_true);
			break;
		}
		return True;
	}
	else if (!CHARcmp(name, toCHAR("dirperm")))
	{
		switch (urlperm(tochar8(arg)))
		{
		  case DIR_INVALID:
			CHARcpy(name, toCHAR("invalid"));
			break;

		  case DIR_BADPATH:
			CHARcpy(name, toCHAR("badpath"));
			break;

		  case DIR_NOTFILE:
			CHARcpy(name, toCHAR("notfile"));
			break;

		  case DIR_NEW:
			CHARcpy(name, toCHAR("new"));
			break;

		  case DIR_UNREADABLE:
			CHARcpy(name, toCHAR("unreadable"));
			break;

		  case DIR_READONLY:
			CHARcpy(name, toCHAR("readonly"));
			break;

		  case DIR_READWRITE:
			CHARcpy(name, toCHAR("readwrite"));
			break;
		}
		return True;
	}
	else if (!CHARcmp(name, toCHAR("dirfile")))
	{
		CHARcpy(name, toCHAR(dirfile(tochar8(arg))));
		return True;
	}
	else if (!CHARcmp(name, toCHAR("dirname")) || !CHARcmp(name, toCHAR("dirdir")))
	{
		CHARcpy(name, toCHAR(dirdir(tochar8(arg))));
		return True;
	}
	else if (!CHARcmp(name, toCHAR("basename")))
	{
		CHARcpy(name, toCHAR(dirfile(tochar8(arg))));
		/* find the last '.' in the name */
		for (tmp = name + CHARlen(name); --tmp >= name && isalnum(*tmp); )
		{
		}
		if (*tmp == '.')
		{
			*tmp = '\0';
		}
		return True;
	}
	else if (!CHARcmp(name, toCHAR("fileeol")))
	{
		CHARcpy(name, toCHAR(ioeol(tochar8(arg))));
		return True;
	}
	else if (!CHARcmp(name, toCHAR("elvispath")))
	{
		tmp = toCHAR(iopath(tochar8(o_elvispath), tochar8(arg), False));
		if (!tmp)
			*name = '\0';
		else
		{
			if (RESULT_OVERFLOW(name, CHARlen(tmp))) goto Overflow;
			CHARcpy(name, tmp);
		}
		return True;
	}
	else if (!CHARcmp(name, toCHAR("getcwd")))
	{
		c = dircwd();
		if (RESULT_OVERFLOW(name, strlen(c))) goto Overflow;
		CHARcpy(name, toCHAR(c));
		return True;
	}
	else if (!CHARcmp(name, toCHAR("absolute")))
	{
		c = urllocal(tochar8(arg));
		if (c)
			c = dirpath(dircwd(), c);
		else
			c = tochar8(arg);
		if (RESULT_OVERFLOW(name, strlen(c))) goto Overflow;
		CHARcpy(name, toCHAR(c));
		return True;
	}
	else if (!CHARcmp(name, toCHAR("buffer")))
	{
		CHARcpy(name, buffind(arg) ? o_true : o_false);
		return True;
	}
	else if (!CHARcmp(name, toCHAR("alias")))
	{
# ifdef FEATURE_ALIAS
		c = exisalias(tochar8(arg), True);
		tmp = (c ? o_true : o_false);
# else
		tmp = o_false;
# endif
		if (RESULT_OVERFLOW(name, CHARlen(tmp))) goto Overflow;
		CHARcpy(name, tmp);
		return True;
	}
	else if (!CHARcmp(name, toCHAR("feature")))
	{
		/* for now */
		CHARcpy(name, o_false);

		/* is it the name of a supported display mode? */
		i = 0;
		do
		{
			if (!CHARcmp(toCHAR(allmodes[i]->name), arg))
			{
				CHARcpy(name, o_true);
				return True;
			}
		} while (allmodes[i++] != &dmnormal);

		/* is it the name of a supported protocol */
		for (i = 0; feature[i] && CHARcmp(feature[i], arg); i++)
		{
		}
		if (feature[i])
			CHARcpy(name, o_true);

		return True;
	}
	else if (!CHARcmp(name, toCHAR("knownsyntax")))
	{
#ifdef DISPLAY_SYNTAX
		tmp = dmsknown(tochar8(arg));
		if (!tmp)
			*name = '\0';
		else if (RESULT_OVERFLOW(name, CHARlen(tmp)))
			goto Overflow;
		else
			CHARcpy(name, tmp);
#else
		*name = '\0'; /* no syntax modes are supported */
#endif
		return True;
	}
	else if (!CHARcmp(name, toCHAR("current")))
	{
		/* The default return value is an empty string */
		*name = '\0';

		/* Other possible values depend on the arg */
		switch (*arg)
		{
		  case 'l':	/* line number */
			if (windefault)
				sprintf(tochar8(name), "%ld", markline(windefault->cursor));
			break;

		  case 'c':	/* column number */
			if (windefault)
				sprintf(tochar8(name), "%ld", (*windefault->md->mark2col)(windefault, windefault->cursor, viiscmd(windefault)) + 1);
			break;

		  case 'w':	/* word at cursor */
		  	if (windefault)
		  	{
				end = *windefault->cursor;
				begin = wordatcursor(&end);
				if (begin && RESULT_OVERFLOW(name, markoffset(&end) - markoffset(begin)))
					goto Overflow;
				if (begin)
				{
					scanalloc(&tmp, begin);
					for (i = (int)(markoffset(&end) - markoffset(begin)); i > 0; i--)
					{
						*name++ = *tmp;
						scannext(&tmp);
					}
					scanfree(&tmp);
					*name = '\0';
				}
			}
			break;

		  case 'm':	/* mode */
		  	if (windefault && !windefault->state->acton)
		  	{
				for (c = windefault->state->modename; *c; c++)
				{
					if (*c != ' ')
						*name++ = tolower(*c);
				}
			}
			break;

		  case 's':	/* visible selection */
		  	if (windefault && !windefault->state->acton && windefault->seltop)
		  	{
		  		switch (windefault->seltype)
		  		{
		  		  case 'c':
		  			CHARcpy(name, toCHAR("character"));
		  			break;

		  		  case 'r':
		  			CHARcpy(name, toCHAR("rectangle"));
		  			break;

		  		  default:
		  		  	CHARcpy(name, toCHAR("line"));
		  		}
		  	}
		  	break;

		  case 'n':	/* next arg */
			if (arglist[argnext])
			{
				if (RESULT_OVERFLOW(name, strlen(arglist[argnext])))
					goto Overflow;
				for (c = arglist[argnext]; *c; )
				{
					*name++ = *c++;
				}
				*name = '\0';
			}
			break;

		  case 'p':	/* previous arg */
			if (argnext >= 2)
			{
				if (RESULT_OVERFLOW(name, strlen(arglist[argnext - 2])))
					goto Overflow;
				for (c = arglist[argnext - 2]; *c; )
				{
					*name++ = *c++;
				}
				*name = '\0';
			}
			break;

		  case 't':	/* tag or tagstack */
			tmp = NULL;
#ifdef FEATURE_SHOWTAG
			if (!CHARcmp(arg, "tag"))
				tmp = telabel(windefault->cursor);
			else
#endif
			if (windefault && windefault->tagstack->origin)
				tmp = o_bufname(markbuffer(windefault->tagstack->origin));
			if (tmp)
			{
				if (RESULT_OVERFLOW(name, CHARlen(tmp)))
					goto Overflow;
				CHARcpy(name, tmp);
			}
			break;
		}
		return True;
	}
#endif

#ifdef TRY
	msg(MSG_ERROR, "unknown function %s", name);
#else
	msg(MSG_ERROR, "[S]unknown function $1", name);
#endif
	return False;

NeedNumber:
#ifdef TRY
	msg(MSG_ERROR, "%s requires a numeric argument", name);
#else
	msg(MSG_ERROR, "[S]$1 requires a numeric argument", name);
#endif
	return False;

#ifndef TRY
Need2Args:
	msg(MSG_ERROR, "[S]$1 requires two arguments", name);
	return False;
#endif

Overflow:
	msg(MSG_ERROR, "result too long");
	return False;
}

/* This function applies a single operator.  Returns False on error.  Its
 * side effects are that it decrements the "ops" counter, and alters the
 * contents of the result buffer.
 */
static BOOLEAN apply()
{
	long	i, j;
	CHAR	*first, *second, *third;
	char	subcode;

	assert(ops >= 1);

	second = opstack[ops--].first;
	first = opstack[ops].first;
	subcode = opinfo[opstack[ops].idx].subcode;
	switch (opinfo[opstack[ops].idx].code)
	{
	  case 'u':	/* unary operators */
		/* Unary operators depend only on their second argument.
		 * The result is concatenated to the first argument, which
		 * is normally an empty string.
		 */
		if (subcode == '!')
		{
			(void)CHARcat(first, calctrue(second) ? o_false : o_true);
		}
		else /* '~' */
		{
			if (calcnumber(second))
			{
				/* bitwise negation */
				long2CHAR(first + CHARlen(first), ~CHAR2long(second));
			}
			else
			{
				/* stuff a ~ between the strings */
				second[-1] = '~';
			}
		}
		break;

	  case 'i': /* integer operators */
		/* If either argument is a non-number, then concatenate them
		 * with the operator between them.  This is tricky because
		 * the << and >> operators are too large to simply replace
		 * the '\0' between the strings.
		 */
		if (!calcnumber(first) || !calcnumber(second))
		{
			if (subcode == '<' || subcode == '>')
			{
				/* As a special case, "string << number" and
				 * "string >> number" truncate the string to
				 * the length given by the number, keeping
				 * characters from the left or right.
				 */
				if (calcnumber(second))
				{
					/* convert arguments */
					i = CHARlen(first);
					j = CHAR2long(second);

					/* make sure this width wouldn't
					 * overflow the result buffer.
					 */
					if (RESULT_OVERFLOW(first, j))
					{
						msg(MSG_ERROR, "result too long");
						return False;
					}

					/* Pad or truncate */
					if (subcode == '<')
					{
						/* Pad or truncate, keeping
						 * chars on left.  The first
						 * arg's characters are already
						 * in the right place, so we
						 * don't need to copy them.
						 * Just pad if first is short.
						 */
						while (i < j)
							first[i++] = ' ';
						first[j] = '\0';
					}
					else
					{
						if (i < j)
						{
							/* String needs to be
							 * padded.  Shift it
							 * to right, and then
							 * pad on the left.
							 */
							first[j] = '\0';
							while (i > 0)
								first[--j] = first[--i];
							while (j > 0)
								first[--j] = ' ';
						}
						else if (i > j)
						{
							/* String needs to be
							 * truncated.  Shift it
							 * to the left.
							 */
							CHARcpy(first, first + i - j);
						}
					}
					break;
				}
				msg(MSG_WARNING, "<< and >> only partially implemented");
			}
#ifndef TRY
			else if (subcode == '/')
			{
				/* When the / operator is passed strings as
				 * arguments, it contatenates them as a
				 * directory name and a file name.
				 */
				CHARcpy(first, toCHAR(dirpath(tochar8(first), tochar8(second))));
				break;
			}
#endif
			second[-1] = subcode;
		}
		else
		{
			i = CHAR2long(first);
			j = CHAR2long(second);
			switch (subcode)
			{
			  case '*':	i *= j;		break;
			  case '+':	i += j;		break;
			  case '-':	i -= j;		break;
			  case '<':	i <<= j;	break;
			  case '>':	i >>= j;	break;
			  case '&':	i &= j;		break;
			  case '^':	i ^= j;		break;
			  case '|':	i |= j;		break;
			  case '/': 	if (j == 0) goto DivZero;
					i /= j;		break;
			  case '%':	if (j == 0) goto DivZero;
					i %= j;		break;
			}
			long2CHAR(first, i);
		}
		break;

	  case 's': /* string or integer comparison operators */
		/* if both arguments look like numbers, then compare
		 * numerically; else compare as strings.
		 */
		if (calcnumber(first) && calcnumber(second))
		{
			i = CHAR2long(first) - CHAR2long(second);
		}
		else
		{
			i = CHARcmp(first, second);
		}
		switch (subcode)
		{
		  case '<':	i = (i < 0);	break;
		  case 'l':	i = (i <= 0);	break;
		  case '>':	i = (i > 0);	break;
		  case 'g':	i = (i >= 0);	break;
		  case '=':	i = (i == 0);	break;
		  case '!':	i = (i != 0);	break;
		}
		(void)CHARcpy(first, toCHAR(i ? o_true : o_false));
		break;

	  case 'c': /* concatenation operators */
		if (subcode == ';')
			memmove(second - 1, second, sizeof(CHAR) * (CHARlen(second) + 1));
		else
			second[-1] = subcode;
		break;

	  case 'b': /* boolean operators */
		if (subcode == '&')
		{
			i = (calctrue(first) && calctrue(second));
		}
		else /* subcode == '|' */
		{
			i = (calctrue(first) || calctrue(second));
		}
		(void)CHARcpy(first, toCHAR(i ? o_true : o_false));
		break;

	  case 't': /* ternary operator */
		/* This should be either (bool ? string : string) if we're
		 * evaluating a ':', or just (bool ? string) if we're
		 * evaluating a '?'.  The '?' and ':' operators are parsed as
		 * if they were separate binary operators.  The '?' has a
		 * slightly lower precedence than ':', so if we're evaluating
		 * a ':' we can expect the '?' to be the preceding operator on
		 * the stack.  That's very important!
		 */
		if (subcode == ':')
		{
			/* complain if not after a '?' */
			if (ops < 1 || opstack[ops - 1].prec != opstack[ops].prec - 1)
			{
				msg(MSG_ERROR, "bad operator");
				return False;
			}

			/* shift the arguments */
			third = second;
			second = first;
			first = opstack[--ops].first;
		}
		else
		{
			/* (bool ? string) is legal -- assume third arg is "" */
			third = toCHAR("");
		}

		/* replace the first boolean with either second or third arg */
		CHARcpy(first, calctrue(first) ? second : third);
		break;

	  case 'f': /* functions */
		/* use func() to apply the function. */
		return func(first, second);
	}
	return True;

DivZero:
	msg(MSG_ERROR, "division by 0");
	return False;
}

/* This function iteratively applies all preceding operators with a precedence
 * no lower than some given level.  Leaves "result" and "ops" altered.  Returns
 * a pointer to the end of the result, or NULL if error.
 */
static CHAR *applyall(prec)
	int	prec;	/* lowest precedence to apply */
{
	while (ops > 0 && opstack[ops - 1].prec >= prec)
	{
		if (!apply())
		{
			return (CHAR *)0;
		}
	}
	return opstack[ops].first + CHARlen(opstack[ops].first);
}


/* This function evaluates an expression, as for a :if or :let command.
 * Returns the result of the evaluation, or NULL if error.
 */
CHAR *calculate(expr, arg, asmsg)
	CHAR	*expr;	/* an expression to evaluate */
	CHAR	**arg;	/* arguments, to replace $1 through $9 */
	BOOLEAN	asmsg;	/* if True, only evaluate parts that are in parens */
{
	CHAR	*build;		/* the result so far */
	int	base = 0;	/* precedence base, keeps track or () pairs */
	int	nargs;		/* number of arguments in arg[] */
	CHAR	*tmp;
	int	i, prec;

	/* count the args */
	for (nargs = 0; arg && arg[nargs]; nargs++)
	{
	}

	/* reset stack & result */
	ops = 0;
	opstack[ops].first = build = result;
	*build = '\0';
	

	/* process the expression from left to right... */
	while (*expr)
	{
		if (RESULT_OVERFLOW(build, 1)) goto Overflow;
		switch (*expr)
		{
		  case ' ':
		  case '\t':
		  case '\n':
			/* whitespace is ignored unless asmsg */
			if (base == 0 && asmsg)
			{
				*build++ = *expr;
				*build = '\0';
			}
			expr++;
			break;

		  case '"':
			if (base == 0 && asmsg)
			{
				/* For messages, " is just another character */
				*build++ = *expr++;
				*build = '\0';
			}
			else
			{
				/* quoted text is copied verbatim */
				while (*++expr && *expr != '"')
				{
					if (RESULT_OVERFLOW(build, 1))
						goto Overflow;
					if (*expr != '\\')
					{
						*build++ = *expr;
						continue;
					}
					switch (*++expr)
					{
					  case 0:
						*build++ = '\\';
						expr--;
						break;

					  case '\n': break;
					  case 'a': *build++ = '\007'; break;
					  case 'b': *build++ = '\b'; break;
					  case 'E': *build++ = '\033'; break;
					  case 'f': *build++ = '\f'; break;
					  case 'n': *build++ = '\n'; break;
					  case 'r': *build++ = '\r'; break;
					  case 't': *build++ = '\t'; break;
					  default: *build++ = *expr;
					}
				}
				if (*expr == '"')
				{
					expr++;
				}
				*build = '\0';
			}
			break;

		  case '\\':
			/* In most contexts, a backslash is treated as a
			 * literal character.  However, it can also be used to
			 * quote the special characters of a message string:
			 * dollar sign, parentheses, and the backslash itself.
			 */
			expr++;
			if (build == result || !*expr || !strchr("$()\\", *expr))
			{
				/* at front of expression, or if followed by
				 * normal character - literal */
				*build++ = '\\';
			}
			else if (*expr)
			{
				/* followed by special character - quote */
				*build++ = *expr++;
			}
			*build = '\0';
			break;

		  case '$':
			/* if it isn't followed by an alphanumeric character,
			 * then treat the '$' as a literal character.
			 */
			expr++;
			if (!isalnum(*expr) && *expr != '_')
			{
				*build++ = '$';
				break;
			}

			/* copy the name into the result buffer temporarily,
			 * just so we have a nul-terminated copy of it.
			 */
			i = copyname(build, expr, True);
			if (i == 0) goto Overflow;
			expr += i;

			/* if number instead of a name, then use arg[i] */
			if (calcnumber(build))
			{
				i = CHAR2long(build);
				if (i <= 0 || i > nargs)
				{
#ifdef TRY
					msg(MSG_ERROR, "args must be $1 through $%d", nargs);
#else
					msg(MSG_ERROR, "[d]args must be \\$1 through \\$$1", nargs);
#endif
					return (CHAR *)0;
				}
				if (RESULT_OVERFLOW(build, CHARlen(arg[i - 1])))
					goto Overflow;
				(void)CHARcpy(build, arg[i - 1]);
				build += CHARlen(build);
			}
			else
			{
				/* try to fetch its value; stuff the value (or an
				 * empty string) into the result buffer.
				 */
				tmp = toCHAR(getenv(tochar8(build)));
				if (!tmp)
				{
					/* convert to uppercase & try again */
					for (tmp = build; *tmp; tmp++)
						*tmp = toupper(*tmp);
					tmp = toCHAR(getenv(tochar8(build)));
				}
				if (tmp)
				{
					if (RESULT_OVERFLOW(build, CHARlen(tmp)))
						goto Overflow;
					(void)CHARcpy(build, tmp);
					build += CHARlen(build);
				}
				else
				{
					*build = '\0';
				}
			}
			break;

		  case '(':
			/* increment the precedence base */
			parstack[base / 20] = opstack[ops].first;
			base += 20;

			/* adjust the start of arguments */
			opstack[ops].first = build;

			expr++;
			break;

		  case ')':
			/* detect mismatched ')' */
			if (base == 0)
			{
				goto Mismatch;
			}

			/* apply any preceding higher-precedence operators */
			build = applyall(base);
			if (!build)
			{
				return (CHAR *)0;
			}

			/* decrement the precedence base */
			base -= 20;
			opstack[ops].first = parstack[base / 20];

			expr++;
			break;

		  default:
			/* It may be an option name, a number, or an operator.
			 * If it appears to be alphanumeric, then assume it
			 * is either a number or a name.
			 */
			if (isalnum(*expr) || *expr == '_')
			{
				/* Copy the string into the result buffer. */
				i = copyname(build, expr, False);
				if (i == 0) goto Overflow;
				expr += i;

				/* if asmsg, then do no further processing of it */
				if (base == 0 && asmsg)
				{
					build += i;
					*build = '\0';
					break;
				}

				/* If the string looks like a number, leave it.
				 * If not a number, then look it up as the name
				 * of an option, and replace it with the value
				 * of that option.
				 */
				if (calcbase10(build))
				{
					/* number -- keep it */
					build += CHARlen(build);
				}
				else if (*expr == '(')
				{
					/* function name -- push a '('
					 * operator with a precedence set
					 * so that next ')' will cause it
					 * to be evaluated.
					 */
					parstack[base / 20] = opstack[ops].first;
					opstack[ops].first = build;

					/* keep the function name */
					build += i;

					/* increment the precedence base */
					base += 20;

					/* compute the precedence of the operator */
					prec = 1 + base;

					/* store this operator, and start a new
					 * one right after it.
					 */
					opstack[ops].idx = 0;
					opstack[ops].prec = prec;
					opstack[++ops].first = ++build;
					*build = '\0';
					expr++;
				}
				else
				{
					/* option name -- look it up */
					tmp = optgetstr(build, NULL);
					if (!tmp)
					{
#ifdef TRY
						msg(MSG_ERROR, "bad option name %s", build);
#else
						msg(MSG_ERROR, "[s]bad option name $1", build);
#endif
						return (CHAR *)0;
					}
					if (RESULT_OVERFLOW(build, CHARlen(tmp)))
						goto Overflow;
					(void)CHARcpy(build, tmp);
					build += CHARlen(build);
				}
			}
			else /* not alphanumeric */
			{
				/* if asmsg, then use it as plain text */
				if (base == 0 && asmsg)
				{
					*build++ = *expr++;
					*build = '\0';
					break;
				}

				/* may be a character constant, as in '\t' */
				if (expr[0] == '\'')
				{
					build[0] = expr[0];
					build[1] = expr[1];
					build[2] = expr[2];
					if (expr[1] == '\\')
					{
						build[3] = expr[3];
						build[4] = '\0';
					}
					else
					{
						build[3] = '\0';
					}
					if (calcbase10(build))
					{
						build += CHARlen(build);
						expr += (expr[1] == '\\' ? 4 : 3);
						break;
					}
				}

				/* try to identify an operator.  This is slightly
				 * trickier than it looks -- the order in which
				 * the comparisons was made had to be fine-tuned
				 * so it wouldn't think a "!=" was a "!".
				 */
				for (i = 0;
				     i < QTY(opinfo) && CHARncmp(opinfo[i].name, expr, strlen(opinfo[i].name));
				     i++)
				{
				}
				if (i >= QTY(opinfo))
				{
#ifdef TRY
					msg(MSG_ERROR, "bad operator %c", *expr);
#else
					msg(MSG_ERROR, "[C]bad operator $1", *expr);
#endif
					return (CHAR *)0;
				}

				/* compute the precedence of the operator */
				prec = opinfo[i].prec + base;

				/* apply any preceding operators with equal or
				 * higher priority.
				 */
				build = applyall(prec);
				if (!build)
				{
					return (CHAR *)0;
				}

				/* store this operator, and start a new one
				 * right after it.
				 */
				opstack[ops].idx = i;
				opstack[ops].prec = prec;
				opstack[++ops].first = ++build;
				*build = '\0';
				expr += strlen(opinfo[i].name);
			}
		}
	}

	/* detect situation where more '(' than ')' were given */
	if (base > 0)
	{
Mismatch:
		msg(MSG_ERROR, "\\(\\) mismatch");
		return (CHAR *)0;
	}

	/* evaluate any remaining operators */
	build = applyall(0);
	if (!build)
	{
		return (CHAR *)0;
	}

	/* return the result */
	return result;

Overflow:
	msg(MSG_ERROR, "result too long");
	return (CHAR *)0;
}

#ifndef TRY
/* This function evaluates a section of a buffer, and replaces that section
 * with the result.  Returns True if successful, or False if there's an error.
 */
BOOLEAN calcsel(from, to)
	MARK	from;
	MARK	to;
{
	CHAR	*expr = bufmemory(from, to);
	CHAR	*result = calculate(expr, (CHAR **)0, False);

	safefree(expr);
	if (!result)
	{
		return False;
	}
	bufreplace(from, to, result, (long)CHARlen(result));
	return True;
}
#endif

#ifdef TRY
# include <stdarg.h>
BUFFER bufdefault;

CHAR *optgetstr(name, desc)
	CHAR	*name;
	OPTDESC	**desc;
{
	return name;
}

void msg(MSGIMP imp, char *format, ...)
{
	va_list	argptr;

	va_start(argptr, format);
	vprintf(format, argptr);
	putchar('\n');
	va_end(argptr);
}

void main(int argc, char **argv)
{
	CHAR	expr[200];
	CHAR	*result;
	char	flag;
	int	i;
	BOOLEAN	msg = False;

	/* Parse options */
	expr[0] = '\0';
	while ((flag = getopt(argc, argv, "e:m")) >= 0)
	{
		switch (flag)
		{
		  case '?':
			fprintf(stderr, "usage: %s [-m] [-e expr] [arg]...\n", argv[0]);
			exit(0);
			break;

		  case 'm':
			msg = True;
			break;

		  case 'e':
			for (i = 0; (expr[i] = optarg[i]) != '\0'; i++)
			{
			}
			break;
		}
	}

	/* were we given an expression on the command line? */
	if (*expr)
	{
		result = calculate(expr, (CHAR **)&argv[optind], msg);
		if (result)
			puts(tochar8(result));
	}
	else
	{
		while (gets(tochar8(expr)))
		{
			result = calculate(expr, (CHAR **)&argv[optind], msg);
			if (result)
				puts(tochar8(result));
		}
	}
	exit(result ? 0 : 1);
}
#endif
