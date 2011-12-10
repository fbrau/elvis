/* config.h */

/* Originally, this file was automatically generated by the "configure"
 * shell script.
 *
 * This file contains C macro definitions which indicate which features
 * are to be supported, and which library functions are to be emulated.
 * In general, #define enables the feature or emulating function, and
 * #undef disables the feature or causes the library function to be used.
 */


/* The following determine which user interfaces are to be supported */
#define GUI_VIO     /* OS/2 console interface */
#undef 	GUI_X11		/* simple X-windows interface */
#undef	GUI_CURSES	/* curses interface */
#ifndef __WITH_TERMCAP
#undef 	GUI_TERMCAP	/* termcap interface */
#else
#define	GUI_TERMCAP	/* termcap interface */
#endif
#undef 	GUI_OPEN	/* open-mode only, does nothing fancy */
#undef	GUI_MFC  	/* MS-Windows gui interface */


/* These allow you to selectively disable the display modes, network protocols,
 * and other optional features.  If you disable the markup display modes then
 * the :help command is disabled because it depends on the "html" markup display
 * mode.  #define to enable the mode, #undef to exclude it.
 */
#define	DISPLAY_HEX	/* hex		interactive hex dump */
#define	DISPLAY_MARKUP	/* html/man/tex	formatted text */
#define	DISPLAY_SYNTAX	/* syntax	generic syntax coloring */
#undef 	PROTOCOL_HTTP	/* define to enable HTTP; undef to disable */
#undef 	PROTOCOL_FTP	/* define to enable FTP; undef to disable */
#define	FEATURE_SHOWTAG	/* the showtag option */
#define	FEATURE_LPR	/* the :lpr command */
#define	FEATURE_ALIAS	/* the :alias command */
#define	FEATURE_MKEXRC	/* the :mkexrc command */
#define FEATURE_COMPLETE /* filename completion */
#define FEATURE_RAM     /* using ram instead of disk for session files */
#define FEATURE_LITRE	/* faster searches for literal strings */


/* The following provide custom implementation of some common functions which
 * are either missing or poorly implemented on some systems.
 */
#undef	NEED_ABORT	/* replaces abort() with a simpler macro */
#undef	NEED_ASSERT	/* defines an custom assert() macro */
#undef	NEED_TGETENT	/* causes tinytcap.c to be used instead of library */
#define NEED_CTYPE	/* custom ctype macros -- digraph aware */
#undef	NEED_WINSIZE	/* includes <ptem.h> -- required by SCO */
#undef 	NEED_SPEED_T	/* includes <termcap.h> -- common on POSIX systems */
#undef	NEED_STRDUP	/* uses a custom version of strdup() */
#undef	NEED_OSPEED	/* causes guitcap.c to supply an ospeed variable */
#undef 	NEED_BC		/* causes guitcap.c to supply a BC variable */
#undef	NEED_SETPGID	/* use setpgrp() instead of setpgid() */
#undef 	NEED_WAIT_H	/* must include <sys/wait.h> */
#define	NEED_SELECT_H	/* must include <sys/select.h> */
#undef 	NEED_IOCTL_H	/* must include <sys/ioctl.h> */
#undef 	NEED_XOS_H	/* must include <X11/Xos.h> */
#undef 	NEED_IN_H	/* must include <netinet/in.h> */
#undef 	NEED_SOCKET_H	/* must include <sys/socket.h> */
#undef	NEED_XRMCOMBINEFILEDATABASE	/* X11R4 needs this */
#undef	NEED_INET_ATON	/* SunOS & Solaris need this */

/* The following control debugging features.  NDEBUG slows elvis down a lot,
 * and the others tend to make it output some confusing messages, so these
 * are all disabled by default.  (Note that NDEBUG is #define'd to disable it)
 */
#define	NDEBUG		/* undef to enable assert() calls; define to disable */
#undef	DEBUG_ALLOC	/* define to debug memory allocations; undef to disable */
#undef	DEBUG_SCAN	/* define to debug character scans; undef to disable */
#undef	DEBUG_SESSION	/* define to debug the block cache; undef to disable */
#undef	DEBUG_EVENT	/* define to trace events; undef to disable */
#undef	DEBUG_MARKUP	/* define to debug markup display modes */

