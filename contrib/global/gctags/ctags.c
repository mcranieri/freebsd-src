/*
 * Copyright (c) 1987, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1987, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#if defined(LIBC_SCCS) && !defined(lint)
static const char rcsid[] =
	"$Id: ctags.c,v 1.1.1.3.2.1 1998/02/05 04:41:39 cwt Exp $";
#endif /* LIBC_SCCS and not lint */

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "ctags.h"
#ifdef GLOBAL
#include "lookup.h"
#include "die.h"
#endif

/*
 * ctags: create a tags file
 */

NODE	*head;			/* head of the sorted binary tree */

				/* boolean "func" (see init()) */
bool	_wht[256], _etk[256], _itk[256], _btk[256], _gd[256];

FILE	*inf;			/* ioptr for current input file */
FILE	*outf;			/* ioptr for tags file */

long	lineftell;		/* ftell after getc( inf ) == '\n' */

int	lineno;			/* line number of current line */
#ifdef GLOBAL
int	cflag;			/* -c: compact index */
#endif
int	dflag;			/* -d: non-macro defines */
#ifdef GLOBAL
int	eflag;			/* -e: '{' at 0 column force function end */
#endif
int	tflag;			/* -t: create tags for typedefs */
int	vflag;			/* -v: vgrind style index output */
int	wflag;			/* -w: suppress warnings */
int	xflag;			/* -x: cxref style output */
#ifdef GLOBAL
int	Dflag;			/* -D: allow duplicate entrys */
int	rflag;			/* -r: function reference */
int	sflag;			/* -s: collect symbols */
#endif
#ifdef YACC
int	yaccfile;		/* yacc file */
#endif

char	*curfile;		/* current input file name */
char	searchar = '/';		/* use /.../ searches by default */
char	lbuf[LINE_MAX];
char	*progname = "gctags";	/* program name */

void	init __P((void));
void	find_entries __P((char *));
void	main __P((int, char **));
static void usage __P((void));

void
main(argc, argv)
	int	argc;
	char	**argv;
{
	static char	*outfile = "tags";	/* output file */
	int	aflag;				/* -a: append to tags */
	int	uflag;				/* -u: update tags */
	int	exit_val;			/* exit value */
	int	step;				/* step through args */
	int	ch;				/* getopts char */
	char	cmd[100];			/* too ugly to explain */
	extern char *optarg;
	extern int optind;

	aflag = uflag = NO;
#ifdef GLOBAL
	while ((ch = getopt(argc, argv, "BDFacdef:rstuwvx")) != -1)
#else
	while ((ch = getopt(argc, argv, "BFadf:tuwvx")) != -1)
#endif
		switch(ch) {
		case 'B':
			searchar = '?';
			break;
#ifdef GLOBAL
		case 'D':
			Dflag++;
			break;
#endif
		case 'F':
			searchar = '/';
			break;
#ifdef GLOBAL
		case 's':
			sflag++;
			break;
#endif
		case 'a':
			aflag++;
			break;
#ifdef GLOBAL
		case 'c':
			cflag++;
			break;
#endif
		case 'd':
			dflag++;
			break;
#ifdef GLOBAL
		case 'e':
			eflag++;
			break;
#endif
		case 'f':
			outfile = optarg;
			break;
#ifdef GLOBAL
		case 'r':
			rflag++;
			break;
#endif
		case 't':
			tflag++;
			break;
		case 'u':
			uflag++;
			break;
		case 'w':
			wflag++;
			break;
		case 'v':
			vflag++;
		case 'x':
			xflag++;
			break;
		case '?':
		default:
			usage();
		}
	argv += optind;
	argc -= optind;
	if (!argc)
		usage();
#ifdef GLOBAL
	if (sflag && rflag)
		die("-s and -r conflict.");
	if (rflag) {
		char	*dbpath;

		if (!(dbpath = getenv("GTAGSDBPATH")))
			dbpath = ".";
		lookupopen(dbpath);
	}
#endif
	init();

	for (exit_val = step = 0; step < argc; ++step)
		if (!(inf = fopen(argv[step], "r"))) {
			fprintf(stderr, "%s: %s cannot open\n", progname, argv[step]);
			exit_val = 1;
		}
		else {
			curfile = argv[step];
			find_entries(argv[step]);
			(void)fclose(inf);
		}

	if (head)
		if (xflag) {
			put_entries(head);
#ifdef GLOBAL
			if (cflag)
				compact_print("", 0, "");/* flush last record */
#endif
		} else {
			if (uflag) {
				for (step = 0; step < argc; step++) {
					(void)sprintf(cmd,
						"mv %s OTAGS; fgrep -v '\t%s\t' OTAGS >%s; rm OTAGS",
							outfile, argv[step],
							outfile);
					system(cmd);
				}
				++aflag;
			}
			if (!(outf = fopen(outfile, aflag ? "a" : "w"))) {
				fprintf(stderr, "%s: %s cannot open\n", progname, outfile);
				exit(exit_val);
			}
			put_entries(head);
			(void)fclose(outf);
			if (uflag) {
				(void)sprintf(cmd, "sort -o %s %s",
						outfile, outfile);
				system(cmd);
			}
		}
#ifdef GLOBAL
	if (rflag)
		lookupclose();
#endif
	exit(exit_val);
}

static void
usage()
{
	(void)fprintf(stderr,
#ifdef GLOBAL
			"usage: gctags [-BDFacderstuvwx] [-f tagsfile] file ...\n");
#else
			"usage: ctags [-BFadtuwvx] [-f tagsfile] file ...\n");
#endif
		exit(1);
}

/*
 * init --
 *	this routine sets up the boolean psuedo-functions which work by
 *	setting boolean flags dependent upon the corresponding character.
 *	Every char which is NOT in that string is false with respect to
 *	the pseudo-function.  Therefore, all of the array "_wht" is NO
 *	by default and then the elements subscripted by the chars in
 *	CWHITE are set to YES.  Thus, "_wht" of a char is YES if it is in
 *	the string CWHITE, else NO.
 */
void
init()
{
	int		i;
	unsigned char	*sp;

	for (i = 0; i < 256; i++) {
		_wht[i] = _etk[i] = _itk[i] = _btk[i] = NO;
		_gd[i] = YES;
	}
#define	CWHITE	" \f\t\n"
	for (sp = (unsigned char *)CWHITE; *sp; sp++)	/* white space chars */
		_wht[*sp] = YES;
#define	CTOKEN	" \t\n\"'#()[]{}=-+%*/&|^~!<>;,.:?"
	for (sp = (unsigned char *)CTOKEN; *sp; sp++)	/* token ending chars */
		_etk[*sp] = YES;
#define	CINTOK	"ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz0123456789"
	for (sp = (unsigned char *)CINTOK; *sp; sp++)	/* valid in-token chars */
		_itk[*sp] = YES;
#define	CBEGIN	"ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz"
	for (sp = (unsigned char *)CBEGIN; *sp; sp++)	/* token starting chars */
		_btk[*sp] = YES;
#define	CNOTGD	",;"
	for (sp = (unsigned char *)CNOTGD; *sp; sp++)	/* invalid after-function chars */
		_gd[*sp] = NO;
}

/*
 * find_entries --
 *	this routine opens the specified file and calls the function
 *	which searches the file.
 */
void
find_entries(file)
	char	*file;
{
	char	*cp;

	lineno = 0;				/* should be 1 ?? KB */
	if ((cp = strrchr(file, '.')) != NULL) {
		if (cp[1] == 'l' && !cp[2]) {
			int	c;

#ifdef GLOBAL
			if (rflag)
				fprintf(stderr, "-r option is ignored in lisp file (Warning only)\n");
#endif
			for (;;) {
				if (GETC(==, EOF))
					return;
				if (!iswhite(c)) {
					rewind(inf);
					break;
				}
			}
#define	LISPCHR	";(["
/* lisp */		if (strchr(LISPCHR, c)) {
				l_entries();
				return;
			}
/* lex */		else {
				/*
				 * we search all 3 parts of a lex file
				 * for C references.  This may be wrong.
				 */
				toss_yysec();
				(void)strcpy(lbuf, "%%$");
				pfnote("yylex", lineno);
				rewind(inf);
			}
		}
/* yacc */	else if (cp[1] == 'y' && !cp[2]) {
#ifdef YACC
			/*
			 * we search all part of a yacc file for C references.
			 * but ignore yacc rule tags.
			 */
			yaccfile = YES;
			c_entries();
			return;
#endif
			/*
			 * we search only the 3rd part of a yacc file
			 * for C references.  This may be wrong.
			 */
			toss_yysec();
			(void)strcpy(lbuf, "%%$");
			pfnote("yyparse", lineno);
			y_entries();
		}
#ifdef GLOBAL
/* assembler */	else if ((cp[1] == 's' || cp[1] == 'S') && !cp[2]) {
			asm_entries();
			return;
		}
#endif
/* fortran */	else if ((cp[1] != 'c' && cp[1] != 'h') && !cp[2]) {
#ifdef GLOBAL
			if (rflag)
				fprintf(stderr, "-r option is ignored in fortran file (Warning only)\n");
#endif
			if (PF_funcs())
				return;
			rewind(inf);
		}
	}
#ifdef YACC
	yaccfile = NO;
#endif
/* C */	c_entries();
}
