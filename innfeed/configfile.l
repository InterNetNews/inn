%{
/*  $Id$
**
**  A flex input file for the innfeed config file.
**
**  Written by James Brister <brister@vix.com>
*/

#include "innfeed.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include "libinn.h"

#include "configfile.h"
#include "config_y.h"
#include "misc.h"

#if ! defined (FLEX_SCANNER)
#error "You must use FLEX to process the lex input file."
#endif

#if defined (FLEX_DEBUG)
#define YY_USER_INIT yy_flex_debug = (getenv ("YYDEBUG") == NULL ? 0 : 1)
#endif

/* We never use this function but flex always defines it, so silence the
   warnings about it. */
static void yyunput(int, char *) UNUSED;

char *strPtr = 0 ;
int strPtrLen = 0 ;
int strIdx = 0 ;
int sawBsl ;
int lineCount = 0 ;
int current ;

static void strAppend (int ch);
static void strAppend (int ch)
{
  if (strIdx == strPtrLen)
    {
      if (strPtr == 0)
        strPtr = xmalloc (strPtrLen = 50) ;
      else
        strPtr = xrealloc (strPtr,strPtrLen += 10) ;
    }
  strPtr [strIdx++] = ch ;
}

#define MAX_INCLUDE_DEPTH 11
struct includeFile {
    YY_BUFFER_STATE state;
    char *name ;
} include_stack[MAX_INCLUDE_DEPTH];
int include_stack_ptr = 0;

%}

%x incl

ID	[a-zA-Z][-a-zA-Z0-9._/]+

%%

\n			lineCount++ ;

":"			{ return (COLON) ; }

"{"			{ return (LBRACE) ; }

"}"			{ return (RBRACE) ; }

[pP][eE][eE][rR]	{ return (PEER) ; }

^"$INCLUDE"		BEGIN(incl);

<incl>[ \t]*		/* eat the whitespace before include filename */

<incl>[^ \t\n]+		{
  if (include_stack_ptr == MAX_INCLUDE_DEPTH - 1)
    {
      int i ;
      fprintf( stderr, "Includes nested too deeply:\n" );
      for (i = 1 ; i <= include_stack_ptr ; i++)
        fprintf (stderr,"\t%s\n",include_stack[i].name) ;
      
      syslog (LOG_ERR, "includes nested to deeply") ;
      exit( 1 );
    }

  if ((yyin = fopen(yytext,"r")) == NULL)
    {
      syslog (LOG_CRIT,"include file fopen failed: %s %s",
              yytext,strerror(errno));
      fprintf (stderr,"include file fopen failed: %s %s\n",
               yytext,strerror(errno));
      exit (1) ;
    }
  else
    {
      d_printf (1,"Including (%d) from %s\n",
               include_stack_ptr + 1,yytext) ;
      include_stack[include_stack_ptr].state = YY_CURRENT_BUFFER;
      include_stack[++include_stack_ptr].name = xstrdup (yytext) ;
      yy_switch_to_buffer(yy_create_buffer(yyin, YY_BUF_SIZE));
    }

  BEGIN(INITIAL);
}

<<EOF>> {
  if ( include_stack_ptr <= 0 )
    yyterminate();
  else
    {
      free (include_stack[include_stack_ptr].name) ;
      yy_delete_buffer(YY_CURRENT_BUFFER);
      yy_switch_to_buffer(include_stack[--include_stack_ptr].state);
    }
}

[gG][rR][oO][uU][pP]	{ return (GROUP) ; }

#[^\n]*			{ (void) 0 ; }

[ \t]+			{ (void) 1 ; }

'\\[\\abfnrtv]'		{
	switch (yytext[2]) {
		case '\\': yylval.chr = '\\' ; break ;
		case 'a': yylval.chr = 007 ; break ;
		case 'b': yylval.chr = 010 ; break ;
		case 'f': yylval.chr = 014 ; break ;
		case 'n': yylval.chr = 012 ; break ;
		case 'r': yylval.chr = 015 ; break ;
		case 't': yylval.chr = 011 ; break ;
		case 'v': yylval.chr = 013 ; break ;
	}
	return (CHAR) ; }

'.'			{ yylval.chr = yytext[1] ; return (CHAR) ; }

'\\[0-9][0-9][0-9]'	{ yylval.chr = (char)strtol(&yytext[2], (char **)NULL, 8);
			  return (CHAR) ;}

\"[^\"]*	{{
	int i ;

	for (i = 1, strIdx = 0, sawBsl = 0 ; ; i++)
          {
            if (i < yyleng)
              current = yytext [i] ;
            else
              current = input() ;
            
            if (current != EOF)
              {
                switch (current)
                  {
                    case '\\':	
                      if (sawBsl)
                        {
                          strAppend (current) ;
                          sawBsl = 0 ;
                        }
                      else
                        sawBsl = 1 ;
                      break ;

                    case '\n':	
                      if (!sawBsl)
                        strAppend(current) ;
                      sawBsl = 0 ;
                      lineCount++ ;
                      break ;

                    case '\"':	
                      if (sawBsl)
                        { 
                          strAppend (current) ;
                          sawBsl = 0 ;
                        }
                      else
                        {
                          strAppend ('\0') ;
                          yylval.string = strPtr ;
                          strPtr = 0 ;
                          strPtrLen = strIdx = 0 ;
                          return (XSTRING) ;
                        }
                      break ;

                    case 'a':
                    case 'b':
                    case 'f':
                    case 'n':
                    case 'r':
                    case 't':
                    case 'v':
                      if (sawBsl)
                        {
                          switch (current) 
                            {
                              case 'a': strAppend (007) ; break ;
                              case 'b': strAppend (010) ; break ;
                              case 'f': strAppend (014) ; break ;
                              case 'n': strAppend (012) ; break ;
                              case 'r': strAppend (015) ; break ;
                              case 't': strAppend (011) ; break ;
                              case 'v': strAppend (013) ; break ;
                            }
                          sawBsl = 0 ;
                        }
                      else
                        strAppend (current) ;
                      break ;

                    default:	
                      strAppend (current) ;
                      sawBsl = 0 ;
                      break ;
                  }
              }
            else
              {
                return (XSTRING) ;
              }
          }
}}

[-0-9][0-9]*		{ yylval.integer = atoi (yytext) ; return (IVAL) ; }

[-0-9][0-9]*\.[0-9]*	{ yylval.real = atof (yytext) ; return (RVAL) ; }

[^#:\'\" \t\n]+	{
  yylval.name = xstrdup (yytext) ;
  if (strcasecmp (yylval.name,"false") == 0)
    return (FALSEBVAL) ;
  else if (strcasecmp (yylval.name,"true") == 0)
    return (TRUEBVAL) ;
  else
  return (WORD) ;
}

%%



