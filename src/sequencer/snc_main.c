/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/**************************************************************************
			GTA PROJECT   AT division
	Copyright, 1990, The Regents of the University of California.
		         Los Alamos National Laboratory

	snc_main.c,v 1.2 1995/06/27 15:26:11 wright Exp
		

	DESCRIPTION: Main program and miscellaneous routines for
	State Notation Compiler.

	ENVIRONMENT: UNIX
	HISTORY:
20nov91,ajk	Removed call to init_snc().
20nov91,ajk	Removed some debug stuff.
28apr92,ajk	Implemented new event flag mode.
29oct93,ajk	Added 'v' (vxWorks include) option.
17may94,ajk	Changed setlinebuf() to setvbuf().
17may94,ajk	Removed event flag option (-e).
17feb95,ajk	Changed yyparse() to Global_yyparse(), because FLEX makes
		yyparse() static.
02mar95,ajk	Changed bcopy () to strcpy () in 2 places.
26jun95,ajk	Due to popular demand, reinstated event flag (-e) option.
***************************************************************************/
extern	char *sncVersion;	/* snc version and date created */

#include	<stdio.h>

#ifndef	TRUE
#define	TRUE 1
#define	FALSE 0
#endif

/* SNC Globals: */
char		in_file[200];	/* input file name */
char		out_file[200];	/* output file name */
char		*src_file;	/* ptr to (effective) source file name */
int		line_num;	/* current src file line number */
int		c_line_num;	/* line number for beginning of C code */
/* Compile & run-time options: */
int		async_opt = FALSE;	/* do pvGet() asynchronously */
int		conn_opt = TRUE;	/* wait for all connections to complete */
int		debug_opt = FALSE;	/* run-time debug */
int		newef_opt = TRUE;	/* new event flag mode */
int		line_opt = TRUE;	/* line numbering */
int		reent_opt = FALSE;	/* reentrant at run-time */
int		warn_opt = TRUE;	/* compiler warnings */
int		vx_opt = TRUE;		/* include vxWorks def's */

/*+************************************************************************
*  NAME: main
*
*  CALLING SEQUENCE
*	type		argument	I/O	description
*	-------------------------------------------------------------
*	int		argc		I	arg count
*	char		*argv[]		I	array of ptrs to args
*
*  RETURNS: n/a
*
*  FUNCTION: Program entry.
*
*  NOTES: The streams stdin and stdout are redirected to files named in the
* command parameters.  This accomodates the use by lex of stdin for input
* and permits printf() to be used for output.  Stderr is not redirected.
*
* This routine calls yyparse(), which never returns.
*-*************************************************************************/
main(argc, argv)
int	argc;
char	*argv[];
{
	FILE	*infp, *outfp, *freopen();

	/* Get command arguments */
	get_args(argc, argv);

	/* Redirect input stream from specified file */
	infp = freopen(in_file, "r", stdin);
	if (infp == NULL)
	{
		perror(in_file);
		exit(1);
	}

	/* Redirect output stream to specified file */
	outfp = freopen(out_file, "w", stdout);
	if (outfp == NULL)
	{
		perror(out_file);
		exit(1);
	}

	/* src_file is used to mark the output file for snc & cc errors */
	src_file = in_file;

	/* Use line buffered output */
	setvbuf(stdout, NULL, _IOLBF, 0);
 	setvbuf(stderr, NULL, _IOLBF, 0);
	
	printf("/* %s: %s */\n\n", sncVersion, in_file);

	/* Call the SNC parser */
	Global_yyparse();
}
/*+************************************************************************
*  NAME: get_args
*
*  CALLING SEQUENCE
*	type		argument	I/O	description
*	-----------------------------------------------------------
*	int		argc		I	number of arguments
*	char		*argv[]		I	shell command arguments
*  RETURNS: n/a
*
*  FUNCTION: Get the shell command arguments.
*
*  NOTES: If "*.s" is input file then "*.c" is the output file.  Otherwise,
*  ".c" is appended to the input file to form the output file name.
*  Sets the gloabals in_file[] and out_file[].
*-*************************************************************************/
get_args(argc, argv)
int	argc;
char	*argv[];
{
	char	*s;

	if (argc < 2)
	{
		fprintf(stderr, "%s\n", sncVersion);
		fprintf(stderr, "usage: snc <options> <infile>\n");
		fprintf(stderr, "options:\n");
		fprintf(stderr, "  +a - do async. pvGet\n");
		fprintf(stderr, "  -c - don't wait for all connects\n");
		fprintf(stderr, "  +d - turn on debug run-time option\n");
		fprintf(stderr, "  -e - don't use new event flag mode\n");
		fprintf(stderr, "  -l - supress line numbering\n");
		fprintf(stderr, "  +r - make reentrant at run-time\n");
		fprintf(stderr, "  -w - supress compiler warnings\n");
		fprintf(stderr, "  -v - don't include VxWorks definitions\n");
		fprintf(stderr, "example:\n snc +a -c vacuum.st\n");
		exit(1);
	}

	for (argc--, argv++; argc > 0; argc--, argv++)
	{
		s = *argv;
		if (*s == '+' || *s == '-')
			get_options(s);
		else
			get_in_file(s);
	}
}

get_options(s)
char		*s;
{
	int		opt_val;

	if (*s == '+')
		opt_val = TRUE;
	else
		opt_val = FALSE;

	switch (s[1])
	{
	case 'a':
		async_opt = opt_val;
		break;

	case 'c':
		conn_opt = opt_val;
		break;

	case 'd':
		debug_opt = opt_val;
		break;

	case 'e':
		newef_opt = opt_val;
		break;

	case 'l':
		line_opt = opt_val;
		break;

	case 'r':
		reent_opt = opt_val;
		break;

	case 'w':
		warn_opt = opt_val;
		break;

	case 'v':
		vx_opt = opt_val;
		break;

	default:
		fprintf(stderr, "Unknown option: \"%s\"\n", s);
		break;
	}
}

get_in_file (s)
char		*s;
{				
	int		ls;

	ls = strlen (s);
	strcpy (in_file, s);
	strcpy (out_file, s);
	if ( strcmp (&in_file[ls-3], ".st") == 0 )
	{
		out_file[ls-2] = 'c';
		out_file[ls-1] = 0;
	}
	else if (in_file[ls-2] == '.')
	{	/* change suffix to 'c' */
		out_file[ls -1] = 'c';
	}
	else
	{	/* append ".c" */
		out_file[ls] = '.';
		out_file[ls+1] = 'c';
		out_file[ls+2] = 0;
	}
	return;
}
/*+************************************************************************
*  NAME: snc_err
*
*  CALLING SEQUENCE
*	type		argument	I/O	description
*	-------------------------------------------------------------------
*	char		*err_txt	I	Text of error msg.
*	int		line		I	Line no. where error ocurred
*	int		code		I	Error code (see snc.y)
*
*  RETURNS: no return
*
*  FUNCTION: Print the SNC error message and then exit.
*
*  NOTES:
*-*************************************************************************/
snc_err(err_txt)
char	*err_txt;
{
	fprintf(stderr, "     %s\n", err_txt);
	exit(1);
}
/*+************************************************************************
*  NAME: yyerror
*
*  CALLING SEQUENCE
*	type		argument	I/O	description
*	---------------------------------------------------
*	char		*err		I	yacc error
*
*  RETURNS: n/a
*
*  FUNCTION: Print yacc error msg
*
*  NOTES:
*-*************************************************************************/
yyerror(err)
char	*err;
{
	fprintf(stderr, "%s: line no. %d (%s)\n", err, line_num, src_file);
	return;
}

/*+************************************************************************
*  NAME: print_line_num
*
*  CALLING SEQUENCE
*	type		argument	I/O	description
*	---------------------------------------------------
*	int		line_num	I	current line number
 *	char		src_file	I	effective source file
*
*  RETURNS: n/a
*
*  FUNCTION: Prints the line number and input file name for use by the
*	C preprocessor.  e.g.: # line 24 "something.st"
*
*  NOTES:
*-*************************************************************************/
print_line_num(line_num, src_file)
int		line_num;
char		*src_file;
{
	if (line_opt)
		printf("# line %d \"%s\"\n", line_num, src_file);
	return;
}
