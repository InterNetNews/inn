
/*  A Bison parser, made from configfile.y
 by  GNU Bison version 1.25
  */

#define YYBISON 1  /* Identify Bison output.  */

#define	PEER	258
#define	GROUP	259
#define	IVAL	260
#define	RVAL	261
#define	NAME	262
#define	XSTRING	263
#define	SCOPE	264
#define	COLON	265
#define	LBRACE	266
#define	RBRACE	267
#define	TRUEBVAL	268
#define	FALSEBVAL	269
#define	CHAR	270
#define	WORD	271
#define	IP_ADDRESS	272

#line 1 "configfile.y"

/* -*- text -*-
 *
 * Author:      James Brister <brister@vix.com> -- berkeley-unix --
 * Start Date:  Fri, 17 Jan 1997 16:09:10 +0100
 * Project:     INN (innfeed)
 * File:        config.y
 * RCSId:       $Id$
 * Description: 
 * 
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <syslog.h>
  
#include "config.h"
#include "configfile.h"
#include "msgs.h"
#include "misc.h"

#define UNKNOWN_SCOPE_TYPE "line %d: unknown scope type: %s"
#define SYNTAX_ERROR "line %d: syntax error"

extern int lineCount ;
scope *topScope = NULL ;
static scope *currScope = NULL ;
char *errbuff = NULL ;

static void appendName (scope *s, char *p) ;
static char *valueScopedName (value *v) ;
static void freeValue (value *v) ;
static char *checkName (scope *s, const char *name) ;
static void addValue (scope *s, value *v) ;
static char *addScope (scope *s, const char *name, scope *val) ;
static void printScope (FILE *fp, scope *s, int indent) ;
static void printValue (FILE *fp, value *v, int indent) ;
static scope *newScope (const char *type) ;
#if 0
static int strNCaseCmp (const char *a, const char *b, size_t len) ;
#endif 



#if 0
int isString (scope *s, const char *name, int inherit)
{
  value *v = findValue (s,name,inherit) ;

  return (v != NULL && v->type == stringval) ;
}
#endif 

int getBool (scope *s, const char *name, int *rval, int inherit)
{
  value *v = findValue (s,name,inherit) ;

  if (v == NULL)
    return 0 ;
  else if (v->type != boolval)
    return 0 ;

  *rval = v->v.bool_val ;
  return 1 ;
}


int getString (scope *s, const char *name, char **rval, int inherit)
{
  value *v = findValue (s,name,inherit) ;

  if (v == NULL)
    return 0 ;
  else if (v->type != stringval)
    return 0 ;

  *rval = strdup (v->v.charp_val) ;
  return 1 ;
}


int getReal (scope *s, const char *name, double *rval, int inherit)
{
  value *v = findValue (s,name,inherit) ;

  if (v == NULL)
    return 0 ;
  else if (v->type != realval)
    return 0 ;

  *rval = v->v.real_val ;
  return 1 ;
}

int getInteger (scope *s, const char *name, long *rval, int inherit)
{
  value *v = findValue (s,name,inherit) ;

  if (v == NULL)
    return 0 ;
  else if (v->type != intval)
    return 0 ;

  *rval = v->v.int_val ;
  return 1 ;
}

void freeScopeTree (scope *s)
{
  int i ;

  if (s == NULL)
    return ;

  if (s->parent == NULL && s->me != NULL) 
    {                           /* top level scope */
      free (s->me->name) ;
      free (s->me) ;
    }
  
  
  for (i = 0 ; i < s->value_idx ; i++)
    if (s->values[i] != NULL)
	freeValue (s->values [i]) ;

  free (s->values) ;
  free (s->scope_type) ;

  s->parent = NULL ;
  s->values = NULL ;

  free (s) ;
}


char *addInteger (scope *s, const char *name, long val) 
{
  value *v ;
  char *error ;
  
  if ((error = checkName (currScope,name)) != NULL)
    return error ;

  v = (value *) calloc (1,sizeof (value)) ;
  v->name = strdup (name) ;
  v->type = intval ;
  v->v.int_val = val ;
  
  addValue (s,v) ;

  return NULL ;
}

char *addChar (scope *s, const char *name, char val) 
{
  value *v ;
  char *error ;
  
  if ((error = checkName (currScope,name)) != NULL)
    return error ;

  v = (value *) calloc (1,sizeof (value)) ;
  v->name = strdup (name) ;
  v->type = charval ;
  v->v.char_val = val ;
  
  addValue (s,v) ;

  return NULL ;
}

char *addBoolean (scope *s, const char *name, int val) 
{
  value *v ;
  char *error ;
  
  if ((error = checkName (currScope,name)) != NULL)
    return error ;

  v = (value *) calloc (1,sizeof (value)) ;
  v->name = strdup (name) ;
  v->type = boolval ;
  v->v.bool_val = val ;
  
  addValue (s,v) ;

  return NULL ;
}

char *addReal (scope *s, const char *name, double val)
{
  value *v ;
  char *error ;

  if ((error = checkName (currScope,name)) != NULL)
    return error ;

  v = (value *) calloc (1,sizeof (value)) ;
  v->name = strdup (name) ;
  v->type = realval ;
  v->v.real_val = val ;
  
  addValue (s,v) ;

  return NULL ;
}

char *addString (scope *s, const char *name, const char *val)
{
  value *v ;
  char *error ;

  if ((error = checkName (currScope,name)) != NULL)
    return error ;

  v = (value *) calloc (1,sizeof (value)) ;
  v->name = strdup (name) ;
  v->type = stringval ;
  v->v.charp_val = strdup (val) ;
  
  addValue (s,v) ;

  return NULL ;
}

value *findValue (scope *s, const char *name, int inherit) 
{
  const char *p ;
  
  if (name == NULL || *name == '\0')
    return NULL ;

  if (*name == ':')
    return findValue (topScope,name + 1,0) ;
  else if (s == NULL)
    return findValue (topScope,name,0) ;
  else 
    {
      int i ;
      
      if ((p = strchr (name,':')) == NULL)
        p = name + strlen (name) ;

      for (i = 0 ; i < s->value_idx ; i++)
        {
          if (strncmp (s->values[i]->name,name,p - name) == 0)
            {
              if (*p == '\0')     /* last segment of name */
                return s->values[i] ;
              else if (s->values[i]->type != scopeval)
                errbuff = strdup ("Component not a scope") ;
              else
                return findValue (s->values[i]->v.scope_val,p + 1,0) ;
            }
        }

      /* not in this scope. Go up if inheriting values and only if no ':'
         in name */
      if (inherit && *p == '\0')
        return findValue (s->parent,name,inherit) ;
    }

  return NULL ;
}

/* find the scope that name belongs to. If mustExist is true then the name
   must be a fully scoped name of a value. relative scopes start at s.  */
scope *findScope (scope *s, const char *name, int mustExist)
{
  scope *p = NULL ;
  char *q ;
  int i ;

  
  if ((q = strchr (name,':')) == NULL)
    {
      if (!mustExist)
        p = s ;
      else
        for (i = 0 ; p == NULL && i < s->value_idx ; i++)
          if (strcmp (s->values[i]->name,name) == 0)
            p = s ;
      
      return p ;
    }
  else if (*name == ':')
    {
      while (s->parent != NULL)
        s = s->parent ;

      return findScope (s,name + 1,mustExist) ;
    }
  else
    {
      for (i = 0 ; i < s->value_idx ; i++)
        if (strncmp (s->values[i]->name,name,q - name) == 0)
          if (s->values[i]->type == scopeval)
            return findScope (s->values[i]->v.scope_val,q + 1,mustExist) ;
    }

  return NULL ;
}

/****************************************************************************/
/*                                                                          */
/****************************************************************************/


static void appendName (scope *s, char *p) 
{
  if (s == NULL)
    return ;
  else
    {
      appendName (s->parent,p) ;
      strcat (p,s->me->name) ;
      strcat (p,":") ;
    }
}

static char *valueScopedName (value *v)
{
  scope *p = v->myscope ;
  int len = strlen (v->name) ;
  char *q ;
  
  while (p != NULL)
    {
      len += strlen (p->me->name) + 1 ;
      p = p->parent ;
    }

  q = malloc (len + 1) ;
  q [0] = '\0' ;
  appendName (v->myscope,q) ;
  strcat (q,v->name) ;

  return q ;
}

static void freeValue (value *v)
{
  free (v->name) ;
  switch (v->type)
    {
      case scopeval:
        freeScopeTree (v->v.scope_val) ;
        break ;

      case stringval:
        free (v->v.charp_val) ;
        break ;

      default:
        break ;
    }
  free (v) ;
}

static char *checkName (scope *s, const char *name)
{
  int i ;	
  char *error = NULL ;

  if (s == NULL)
    return NULL ;
  
  for (i = 0 ; i < s->value_idx ; i++)
    {
      char *n = NULL ;
      
      if (strcmp (name,s->values [i]->name) == 0) {

#define FMT "Two definitions of %s"

        n = valueScopedName (s->values[i]) ;
        error = malloc (strlen (FMT) + strlen (n) + 2) ;
        sprintf (error,FMT,n) ;
        free (n) ;
        return error ;
      }
    }
  
  return error ;
}


static void addValue (scope *s, value *v) 
{
  v->myscope = s ;
  
  if (s == NULL)
    return ;
      
  if (s->value_count == s->value_idx)
    {
      if (s->values == 0)
        {
          s->values = (value **) calloc (10,sizeof (value *)) ;
          s->value_count = 10 ;
        }
      else
        {
          s->value_count += 10 ;
          s->values = (value **) realloc (s->values,
                                          sizeof (value *) * s->value_count);
        }
    }
  
  s->values [s->value_idx++] = v ;
}



static char *addScope (scope *s, const char *name, scope *val)
{
  value *v ;
  char *error ;

  if ((error = checkName (s,name)) != NULL)
    return error ;

  v = (value *) calloc (1,sizeof (value)) ;
  v->name = strdup (name) ;
  v->type = scopeval ;
  v->v.scope_val = val ;
  val->me = v ;
  val->parent = s ;

  addValue (s,v) ;

  currScope = val ;

  return NULL ;
}


static void printScope (FILE *fp, scope *s, int indent)
{
  int i ;
  for (i = 0 ; i < s->value_idx ; i++)
    printValue (fp,s->values [i],indent + 5) ;
}

static void printValue (FILE *fp, value *v, int indent) 
{
  int i ;
  
  for (i = 0 ; i < indent ; i++)
    fputc (' ',fp) ;
  
  switch (v->type) 
    {
      case intval:
        fprintf (fp,"%s : %ld # INTEGER\n",v->name,v->v.int_val) ;
        break ;
        
      case stringval:
        fprintf (fp,"%s : \"",v->name) ;
        {
          char *p = v->v.charp_val ;
          while (*p) 
            {
              if (*p == '"' || *p == '\\')
                fputc ('\\',fp) ;
              fputc (*p,fp) ;
              p++ ;
            }
        }
        fprintf (fp,"\" # STRING\n") ;
        break ;

      case charval:
        fprintf (fp,"%s : %c",v->name,047) ;
        switch (v->v.char_val)
          {
            case '\\':
              fprintf (fp,"\\\\") ;
              break ;

            default:
              if (isprint (v->v.char_val))
                fprintf (fp,"%c",v->v.char_val) ;
              else
                fprintf (fp,"\\%03o",v->v.char_val) ;
          }
        fprintf (fp,"%c # CHARACTER\n",047) ;
        break ;
        
      case realval:
        fprintf (fp,"%s : %f # REAL\n",v->name,v->v.real_val) ;
        break ;

      case boolval:
        fprintf (fp,"%s : %s # BOOLEAN\n",
                 v->name,(v->v.bool_val ? "true" : "false")) ;
        break ;
        
      case scopeval:
        fprintf (fp,"%s %s { # SCOPE\n",v->v.scope_val->scope_type,v->name) ;
        printScope (fp,v->v.scope_val,indent + 5) ;
        for (i = 0 ; i < indent ; i++)
          fputc (' ',fp) ;
        fprintf (fp,"}\n") ;
        break ;

      default:
        fprintf (fp,"UNKNOWN value type: %d\n",v->type) ;
        exit (1) ;
    }
}

  

static scope *newScope (const char *type)
{
  scope *t ;
  int i ;
  
  t = (scope *) calloc (1,sizeof (scope)) ;
  t->parent = NULL ;
  t->scope_type = strdup (type) ;

  for (i = 0 ; t->scope_type[i] != '\0' ; i++)
    t->scope_type[i] = tolower (t->scope_type[i]) ;

  return t ;
}



#if 0
static int strNCaseCmp (const char *a, const char *b, size_t len)
{
  while (a && b && *a && *b && (tolower (*a) == tolower (*b)) && len > 0)
    a++, b++, len-- ;

  if (a == NULL && b == NULL)
    return 0 ;
  else if (a == NULL)
    return 1 ;
  else if (b == NULL)
    return -1 ;
  else if (*a == '\0' && *b == '\0')
    return 0 ;
  else if (*a == '\0')
    return 1 ;
  else if (*b == '\0')
    return -1 ;
  else if (*a < *b)
    return 1 ;
  else if (*a > *b)
    return -1 ;
  else
    return 0 ;

  abort () ;
}
#endif

#define BAD_KEY "line %d: illegal key name: %s"
#define NON_ALPHA "line %d: keys must start with a letter: %s"

static char *keyOk (const char *key) 
{
  const char *p = key ;
  char *rval ;

  if (key == NULL)
    return strdup ("NULL key") ;
  else if (*key == '\0')
    return strdup ("EMPTY KEY") ;
  
  if (!isalpha(*p))
    {
      rval = malloc (strlen (NON_ALPHA) + strlen (key) + 15) ;
      sprintf (rval,NON_ALPHA,lineCount, key) ;
      return rval ;
    }

  p++ ;
  while (*p)
    {
      if (!(isalnum (*p) || *p == '_' || *p == '-'))
        {
          rval = malloc (strlen (BAD_KEY) + strlen (key) + 15) ;
          sprintf (rval,BAD_KEY,lineCount,key) ;
          return rval ;
        }
      p++ ;
    }

  return NULL ;
}

static PFIVP *funcs = NULL ;
static void **args = NULL ;
static int funcCount ;
static int funcIdx ;

void configAddLoadCallback (PFIVP func,void *arg)
{
  if (func == NULL)
    return ;

  if (funcIdx == funcCount)
    {
      funcCount += 10 ;
      if (funcs == NULL)
        {
          funcs = (PFIVP *) malloc (sizeof (PFIVP) * funcCount);
          args = (void **) malloc (sizeof (void *) * funcCount) ;
        }
      else
        {
          funcs = (PFIVP *) realloc (funcs,sizeof (PFIVP) * funcCount);
          args = (void **) realloc (args,sizeof (void *) * funcCount) ;
        }
    }

  args [funcIdx] = arg ;
  funcs [funcIdx++] = func ;
  
}


void configRemoveLoadCallback (PFIVP func)
{
  int i, j ;

  for (i = 0 ; i < funcIdx ; i++)
    if (funcs [i] == func)
      break ;

  for (j = i ; j < funcIdx - 1 ; j++)
    {
      funcs [j] = funcs [j + 1] ;
      args [j] = args [j + 1] ;
    }

  if (funcIdx > 1 && i < funcIdx)
    {
      funcs [i - 2] = funcs [i - 1] ;
      args [i - 2] = args [i - 1] ;
    }

  if (funcIdx > 0 && i < funcIdx)
    funcIdx-- ;
}


static int doCallbacks (void)
{
  int i ;
  int rval = 1 ;
  
  for (i = 0 ; i < funcIdx ; i++)
    if (funcs [i] != NULL)
      rval = (funcs[i](args [i]) && rval) ;

  return rval ;
}





static char *key ;

#line 673 "configfile.y"
typedef union{
    scope *scp ;
    value *val ;
    char *name ;
    int integer ;
    double real ;
    char *string ;
    char chr ;
} YYSTYPE;
#include <stdio.h>

#ifndef __cplusplus
#ifndef __STDC__
#define const
#endif
#endif



#define	YYFINAL		33
#define	YYFLAG		-32768
#define	YYNTBASE	18

#define YYTRANSLATE(x) ((unsigned)(x) <= 272 ? yytranslate[x] : 27)

static const char yytranslate[] = {     0,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     1,     2,     3,     4,     5,
     6,     7,     8,     9,    10,    11,    12,    13,    14,    15,
    16,    17
};

#if YYDEBUG != 0
static const short yyprhs[] = {     0,
     0,     1,     4,     6,     7,    10,    13,    14,    21,    22,
    29,    33,    34,    39,    41,    43,    45,    47,    49,    51
};

static const short yyrhs[] = {    -1,
    19,    21,     0,    21,     0,     0,    21,    22,     0,    21,
     1,     0,     0,     3,    16,    11,    23,    20,    12,     0,
     0,     4,    16,    11,    24,    20,    12,     0,    16,    16,
    11,     0,     0,    16,    25,    10,    26,     0,    16,     0,
     5,     0,    13,     0,    14,     0,     6,     0,     8,     0,
    15,     0
};

#endif

#if YYDEBUG != 0
static const short yyrline[] = { 0,
   706,   710,   712,   714,   715,   716,   723,   727,   730,   734,
   737,   745,   750,   752,   758,   763,   769,   775,   780,   785
};
#endif


#if YYDEBUG != 0 || defined (YYERROR_VERBOSE)

static const char * const yytname[] = {   "$","error","$undefined.","PEER","GROUP",
"IVAL","RVAL","NAME","XSTRING","SCOPE","COLON","LBRACE","RBRACE","TRUEBVAL",
"FALSEBVAL","CHAR","WORD","IP_ADDRESS","input","@1","scope","entries","entry",
"@2","@3","@4","value", NULL
};
#endif

static const short yyr1[] = {     0,
    19,    18,    20,    21,    21,    21,    23,    22,    24,    22,
    22,    25,    22,    26,    26,    26,    26,    26,    26,    26
};

static const short yyr2[] = {     0,
     0,     2,     1,     0,     2,     2,     0,     6,     0,     6,
     3,     0,     4,     1,     1,     1,     1,     1,     1,     1
};

static const short yydefact[] = {     1,
     4,     0,     6,     0,     0,    12,     5,     0,     0,     0,
     0,     7,     9,    11,     0,     4,     4,    15,    18,    19,
    16,    17,    20,    14,    13,     0,     0,     0,     8,    10,
     0,     0,     0
};

static const short yydefgoto[] = {    31,
     1,    26,    27,     7,    16,    17,    11,    25
};

static const short yypact[] = {-32768,
-32768,     0,-32768,   -14,   -11,    -9,-32768,    -1,     1,     2,
     4,-32768,-32768,-32768,    14,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,     3,     5,     6,-32768,-32768,
    11,    23,-32768
};

static const short yypgoto[] = {-32768,
-32768,     7,    24,-32768,-32768,-32768,-32768,-32768
};


#define	YYLAST		30


static const short yytable[] = {    -2,
     3,     8,     4,     5,     9,     3,    10,     4,     5,    12,
    32,    13,    14,    15,    29,     6,    -3,    30,    18,    19,
     6,    20,    33,    28,     2,     0,    21,    22,    23,    24
};

static const short yycheck[] = {     0,
     1,    16,     3,     4,    16,     1,    16,     3,     4,    11,
     0,    11,    11,    10,    12,    16,    12,    12,     5,     6,
    16,     8,     0,    17,     1,    -1,    13,    14,    15,    16
};
/* -*-C-*-  Note some compilers choke on comments on `#line' lines.  */
#line 3 "/usr/share/misc/bison.simple"

/* Skeleton output parser for bison,
   Copyright (C) 1984, 1989, 1990 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* As a special exception, when this file is copied by Bison into a
   Bison output file, you may use that output file without restriction.
   This special exception was added by the Free Software Foundation
   in version 1.24 of Bison.  */

#ifndef alloca
#ifdef __GNUC__
#define alloca __builtin_alloca
#else /* not GNU C.  */
#if (!defined (__STDC__) && defined (sparc)) || defined (__sparc__) || defined (__sparc) || defined (__sgi)
#include <alloca.h>
#else /* not sparc */
#if defined (MSDOS) && !defined (__TURBOC__)
#include <malloc.h>
#else /* not MSDOS, or __TURBOC__ */
#if defined(_AIX)
#include <malloc.h>
 #pragma alloca
#else /* not MSDOS, __TURBOC__, or _AIX */
#ifdef __hpux
#ifdef __cplusplus
extern "C" {
void *alloca (unsigned int);
};
#else /* not __cplusplus */
void *alloca ();
#endif /* not __cplusplus */
#endif /* __hpux */
#endif /* not _AIX */
#endif /* not MSDOS, or __TURBOC__ */
#endif /* not sparc.  */
#endif /* not GNU C.  */
#endif /* alloca not defined.  */

/* This is the parser code that is written into each bison parser
  when the %semantic_parser declaration is not specified in the grammar.
  It was written by Richard Stallman by simplifying the hairy parser
  used when %semantic_parser is specified.  */

/* Note: there must be only one dollar sign in this file.
   It is replaced by the list of actions, each action
   as one case of the switch.  */

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		-2
#define YYEOF		0
#define YYACCEPT	return(0)
#define YYABORT 	return(1)
#define YYERROR		goto yyerrlab1
/* Like YYERROR except do call yyerror.
   This remains here temporarily to ease the
   transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  */
#define YYFAIL		goto yyerrlab
#define YYRECOVERING()  (!!yyerrstatus)
#define YYBACKUP(token, value) \
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    { yychar = (token), yylval = (value);			\
      yychar1 = YYTRANSLATE (yychar);				\
      YYPOPSTACK;						\
      goto yybackup;						\
    }								\
  else								\
    { yyerror ("syntax error: cannot back up"); YYERROR; }	\
while (0)

#define YYTERROR	1
#define YYERRCODE	256

#ifndef YYPURE
#define YYLEX		yylex()
#endif

#ifdef YYPURE
#ifdef YYLSP_NEEDED
#ifdef YYLEX_PARAM
#define YYLEX		yylex(&yylval, &yylloc, YYLEX_PARAM)
#else
#define YYLEX		yylex(&yylval, &yylloc)
#endif
#else /* not YYLSP_NEEDED */
#ifdef YYLEX_PARAM
#define YYLEX		yylex(&yylval, YYLEX_PARAM)
#else
#define YYLEX		yylex(&yylval)
#endif
#endif /* not YYLSP_NEEDED */
#endif

/* If nonreentrant, generate the variables here */

#ifndef YYPURE

int	yychar;			/*  the lookahead symbol		*/
YYSTYPE	yylval;			/*  the semantic value of the		*/
				/*  lookahead symbol			*/

#ifdef YYLSP_NEEDED
YYLTYPE yylloc;			/*  location data for the lookahead	*/
				/*  symbol				*/
#endif

int yynerrs;			/*  number of parse errors so far       */
#endif  /* not YYPURE */

#if YYDEBUG != 0
int yydebug;			/*  nonzero means print parse trace	*/
/* Since this is uninitialized, it does not stop multiple parsers
   from coexisting.  */
#endif

/*  YYINITDEPTH indicates the initial size of the parser's stacks	*/

#ifndef	YYINITDEPTH
#define YYINITDEPTH 200
#endif

/*  YYMAXDEPTH is the maximum size the stacks can grow to
    (effective only if the built-in stack extension method is used).  */

#if YYMAXDEPTH == 0
#undef YYMAXDEPTH
#endif

#ifndef YYMAXDEPTH
#define YYMAXDEPTH 10000
#endif

/* Prevent warning if -Wstrict-prototypes.  */
#ifdef __GNUC__
int yyparse (void);
#endif

#if __GNUC__ > 1		/* GNU C and GNU C++ define this.  */
#define __yy_memcpy(TO,FROM,COUNT)	__builtin_memcpy(TO,FROM,COUNT)
#else				/* not GNU C or C++ */
#ifndef __cplusplus

/* This is the most reliable way to avoid incompatibilities
   in available built-in functions on various systems.  */
static void
__yy_memcpy (to, from, count)
     char *to;
     char *from;
     int count;
{
  register char *f = from;
  register char *t = to;
  register int i = count;

  while (i-- > 0)
    *t++ = *f++;
}

#else /* __cplusplus */

/* This is the most reliable way to avoid incompatibilities
   in available built-in functions on various systems.  */
static void
__yy_memcpy (char *to, char *from, int count)
{
  register char *f = from;
  register char *t = to;
  register int i = count;

  while (i-- > 0)
    *t++ = *f++;
}

#endif
#endif

#line 196 "/usr/share/misc/bison.simple"

/* The user can define YYPARSE_PARAM as the name of an argument to be passed
   into yyparse.  The argument should have type void *.
   It should actually point to an object.
   Grammar actions can access the variable by casting it
   to the proper pointer type.  */

#ifdef YYPARSE_PARAM
#ifdef __cplusplus
#define YYPARSE_PARAM_ARG void *YYPARSE_PARAM
#define YYPARSE_PARAM_DECL
#else /* not __cplusplus */
#define YYPARSE_PARAM_ARG YYPARSE_PARAM
#define YYPARSE_PARAM_DECL void *YYPARSE_PARAM;
#endif /* not __cplusplus */
#else /* not YYPARSE_PARAM */
#define YYPARSE_PARAM_ARG
#define YYPARSE_PARAM_DECL
#endif /* not YYPARSE_PARAM */

int
yyparse(YYPARSE_PARAM_ARG)
     YYPARSE_PARAM_DECL
{
  register int yystate;
  register int yyn;
  register short *yyssp;
  register YYSTYPE *yyvsp;
  int yyerrstatus;	/*  number of tokens to shift before error messages enabled */
  int yychar1 = 0;		/*  lookahead token as an internal (translated) token number */

  short	yyssa[YYINITDEPTH];	/*  the state stack			*/
  YYSTYPE yyvsa[YYINITDEPTH];	/*  the semantic value stack		*/

  short *yyss = yyssa;		/*  refer to the stacks thru separate pointers */
  YYSTYPE *yyvs = yyvsa;	/*  to allow yyoverflow to reallocate them elsewhere */

#ifdef YYLSP_NEEDED
  YYLTYPE yylsa[YYINITDEPTH];	/*  the location stack			*/
  YYLTYPE *yyls = yylsa;
  YYLTYPE *yylsp;

#define YYPOPSTACK   (yyvsp--, yyssp--, yylsp--)
#else
#define YYPOPSTACK   (yyvsp--, yyssp--)
#endif

  int yystacksize = YYINITDEPTH;

#ifdef YYPURE
  int yychar;
  YYSTYPE yylval;
  int yynerrs;
#ifdef YYLSP_NEEDED
  YYLTYPE yylloc;
#endif
#endif

  YYSTYPE yyval;		/*  the variable used to return		*/
				/*  semantic values from the action	*/
				/*  routines				*/

  int yylen;

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Starting parse\n");
#endif

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY;		/* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */

  yyssp = yyss - 1;
  yyvsp = yyvs;
#ifdef YYLSP_NEEDED
  yylsp = yyls;
#endif

/* Push a new state, which is found in  yystate  .  */
/* In all cases, when you get here, the value and location stacks
   have just been pushed. so pushing a state here evens the stacks.  */
yynewstate:

  *++yyssp = yystate;

  if (yyssp >= yyss + yystacksize - 1)
    {
      /* Give user a chance to reallocate the stack */
      /* Use copies of these so that the &'s don't force the real ones into memory. */
      YYSTYPE *yyvs1 = yyvs;
      short *yyss1 = yyss;
#ifdef YYLSP_NEEDED
      YYLTYPE *yyls1 = yyls;
#endif

      /* Get the current used size of the three stacks, in elements.  */
      int size = yyssp - yyss + 1;

#ifdef yyoverflow
      /* Each stack pointer address is followed by the size of
	 the data in use in that stack, in bytes.  */
#ifdef YYLSP_NEEDED
      /* This used to be a conditional around just the two extra args,
	 but that might be undefined if yyoverflow is a macro.  */
      yyoverflow("parser stack overflow",
		 &yyss1, size * sizeof (*yyssp),
		 &yyvs1, size * sizeof (*yyvsp),
		 &yyls1, size * sizeof (*yylsp),
		 &yystacksize);
#else
      yyoverflow("parser stack overflow",
		 &yyss1, size * sizeof (*yyssp),
		 &yyvs1, size * sizeof (*yyvsp),
		 &yystacksize);
#endif

      yyss = yyss1; yyvs = yyvs1;
#ifdef YYLSP_NEEDED
      yyls = yyls1;
#endif
#else /* no yyoverflow */
      /* Extend the stack our own way.  */
      if (yystacksize >= YYMAXDEPTH)
	{
	  yyerror("parser stack overflow");
	  return 2;
	}
      yystacksize *= 2;
      if (yystacksize > YYMAXDEPTH)
	yystacksize = YYMAXDEPTH;
      yyss = (short *) alloca (yystacksize * sizeof (*yyssp));
      __yy_memcpy ((char *)yyss, (char *)yyss1, size * sizeof (*yyssp));
      yyvs = (YYSTYPE *) alloca (yystacksize * sizeof (*yyvsp));
      __yy_memcpy ((char *)yyvs, (char *)yyvs1, size * sizeof (*yyvsp));
#ifdef YYLSP_NEEDED
      yyls = (YYLTYPE *) alloca (yystacksize * sizeof (*yylsp));
      __yy_memcpy ((char *)yyls, (char *)yyls1, size * sizeof (*yylsp));
#endif
#endif /* no yyoverflow */

      yyssp = yyss + size - 1;
      yyvsp = yyvs + size - 1;
#ifdef YYLSP_NEEDED
      yylsp = yyls + size - 1;
#endif

#if YYDEBUG != 0
      if (yydebug)
	fprintf(stderr, "Stack size increased to %d\n", yystacksize);
#endif

      if (yyssp >= yyss + yystacksize - 1)
	YYABORT;
    }

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Entering state %d\n", yystate);
#endif

  goto yybackup;
 yybackup:

/* Do appropriate processing given the current state.  */
/* Read a lookahead token if we need one and don't already have one.  */
/* yyresume: */

  /* First try to decide what to do without reference to lookahead token.  */

  yyn = yypact[yystate];
  if (yyn == YYFLAG)
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* yychar is either YYEMPTY or YYEOF
     or a valid token in external form.  */

  if (yychar == YYEMPTY)
    {
#if YYDEBUG != 0
      if (yydebug)
	fprintf(stderr, "Reading a token: ");
#endif
      yychar = YYLEX;
    }

  /* Convert token to internal form (in yychar1) for indexing tables with */

  if (yychar <= 0)		/* This means end of input. */
    {
      yychar1 = 0;
      yychar = YYEOF;		/* Don't call YYLEX any more */

#if YYDEBUG != 0
      if (yydebug)
	fprintf(stderr, "Now at end of input.\n");
#endif
    }
  else
    {
      yychar1 = YYTRANSLATE(yychar);

#if YYDEBUG != 0
      if (yydebug)
	{
	  fprintf (stderr, "Next token is %d (%s", yychar, yytname[yychar1]);
	  /* Give the individual parser a way to print the precise meaning
	     of a token, for further debugging info.  */
#ifdef YYPRINT
	  YYPRINT (stderr, yychar, yylval);
#endif
	  fprintf (stderr, ")\n");
	}
#endif
    }

  yyn += yychar1;
  if (yyn < 0 || yyn > YYLAST || yycheck[yyn] != yychar1)
    goto yydefault;

  yyn = yytable[yyn];

  /* yyn is what to do for this token type in this state.
     Negative => reduce, -yyn is rule number.
     Positive => shift, yyn is new state.
       New state is final state => don't bother to shift,
       just return success.
     0, or most negative number => error.  */

  if (yyn < 0)
    {
      if (yyn == YYFLAG)
	goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }
  else if (yyn == 0)
    goto yyerrlab;

  if (yyn == YYFINAL)
    YYACCEPT;

  /* Shift the lookahead token.  */

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Shifting token %d (%s), ", yychar, yytname[yychar1]);
#endif

  /* Discard the token being shifted unless it is eof.  */
  if (yychar != YYEOF)
    yychar = YYEMPTY;

  *++yyvsp = yylval;
#ifdef YYLSP_NEEDED
  *++yylsp = yylloc;
#endif

  /* count tokens shifted since error; after three, turn off error status.  */
  if (yyerrstatus) yyerrstatus--;

  yystate = yyn;
  goto yynewstate;

/* Do the default action for the current state.  */
yydefault:

  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;

/* Do a reduction.  yyn is the number of a rule to reduce with.  */
yyreduce:
  yylen = yyr2[yyn];
  if (yylen > 0)
    yyval = yyvsp[1-yylen]; /* implement default value of the action */

#if YYDEBUG != 0
  if (yydebug)
    {
      int i;

      fprintf (stderr, "Reducing via rule %d (line %d), ",
	       yyn, yyrline[yyn]);

      /* Print the symbols being reduced, and their result.  */
      for (i = yyprhs[yyn]; yyrhs[i] > 0; i++)
	fprintf (stderr, "%s ", yytname[yyrhs[i]]);
      fprintf (stderr, " -> %s\n", yytname[yyr1[yyn]]);
    }
#endif


  switch (yyn) {

case 1:
#line 706 "configfile.y"
{ 	
		lineCount = 1 ;
		addScope (NULL,"",newScope ("")) ;
		topScope = currScope ;
	;
    break;}
case 2:
#line 710 "configfile.y"
{ if (!doCallbacks()) YYABORT ; ;
    break;}
case 6:
#line 716 "configfile.y"
{
		errbuff = malloc (strlen(SYNTAX_ERROR) + 12) ;
		sprintf (errbuff,SYNTAX_ERROR,lineCount) ;
		YYABORT ;
	;
    break;}
case 7:
#line 723 "configfile.y"
{
		errbuff = addScope (currScope,yyvsp[-1].name,newScope ("peer")) ;
                free (yyvsp[-1].name) ;
		if (errbuff != NULL) YYABORT ;
	;
    break;}
case 8:
#line 727 "configfile.y"
{
		currScope = currScope->parent ;
	;
    break;}
case 9:
#line 730 "configfile.y"
{
		errbuff = addScope (currScope,yyvsp[-1].name,newScope ("group")) ;
                free (yyvsp[-1].name) ;
		if (errbuff != NULL) YYABORT ;
	;
    break;}
case 10:
#line 734 "configfile.y"
{
		currScope = currScope->parent ;
	;
    break;}
case 11:
#line 737 "configfile.y"
{
		errbuff = malloc (strlen(UNKNOWN_SCOPE_TYPE) + 15 +
					  strlen (yyvsp[-2].name)) ;
		sprintf (errbuff,UNKNOWN_SCOPE_TYPE,lineCount,yyvsp[-2].name) ;
                free (yyvsp[-2].name) ;
                free (yyvsp[-1].name) ;
		YYABORT ;
	;
    break;}
case 12:
#line 745 "configfile.y"
{ 
		if ((errbuff = keyOk(yyvsp[0].name)) != NULL) {
			YYABORT ;
		} else
			key = yyvsp[0].name ;
	;
    break;}
case 14:
#line 752 "configfile.y"
{
		if ((errbuff = addString (currScope, key, yyvsp[0].name)) != NULL)
			YYABORT ;
                free (key) ;
                free (yyvsp[0].name) ;
	;
    break;}
case 15:
#line 758 "configfile.y"
{
		if ((errbuff = addInteger(currScope, key, yyvsp[0].integer)) != NULL)
			YYABORT; 
                free (key) ;
	;
    break;}
case 16:
#line 763 "configfile.y"
{
		if ((errbuff = addBoolean (currScope, key, 1)) != NULL)
			YYABORT ; 
                free (key) ;
                free (yyvsp[0].name) ;
	;
    break;}
case 17:
#line 769 "configfile.y"
{
		if ((errbuff = addBoolean (currScope, key, 0)) != NULL)
			YYABORT ; 
                free (key) ;
                free (yyvsp[0].name) ;
	;
    break;}
case 18:
#line 775 "configfile.y"
{
		if ((errbuff = addReal (currScope, key, yyvsp[0].real)) != NULL)
			YYABORT ; 
                free (key) ;
	;
    break;}
case 19:
#line 780 "configfile.y"
{ 
		if ((errbuff = addString (currScope, key, yyvsp[0].string)) != NULL)
			YYABORT;
                free (key) ;
	;
    break;}
case 20:
#line 785 "configfile.y"
{
		if ((errbuff = addChar (currScope, key, yyvsp[0].chr)) != NULL)
			YYABORT ;
                free (key) ;
        ;
    break;}
}
   /* the action file gets copied in in place of this dollarsign */
#line 498 "/usr/share/misc/bison.simple"

  yyvsp -= yylen;
  yyssp -= yylen;
#ifdef YYLSP_NEEDED
  yylsp -= yylen;
#endif

#if YYDEBUG != 0
  if (yydebug)
    {
      short *ssp1 = yyss - 1;
      fprintf (stderr, "state stack now");
      while (ssp1 != yyssp)
	fprintf (stderr, " %d", *++ssp1);
      fprintf (stderr, "\n");
    }
#endif

  *++yyvsp = yyval;

#ifdef YYLSP_NEEDED
  yylsp++;
  if (yylen == 0)
    {
      yylsp->first_line = yylloc.first_line;
      yylsp->first_column = yylloc.first_column;
      yylsp->last_line = (yylsp-1)->last_line;
      yylsp->last_column = (yylsp-1)->last_column;
      yylsp->text = 0;
    }
  else
    {
      yylsp->last_line = (yylsp+yylen-1)->last_line;
      yylsp->last_column = (yylsp+yylen-1)->last_column;
    }
#endif

  /* Now "shift" the result of the reduction.
     Determine what state that goes to,
     based on the state we popped back to
     and the rule number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTBASE] + *yyssp;
  if (yystate >= 0 && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTBASE];

  goto yynewstate;

yyerrlab:   /* here on detecting error */

  if (! yyerrstatus)
    /* If not already recovering from an error, report this error.  */
    {
      ++yynerrs;

#ifdef YYERROR_VERBOSE
      yyn = yypact[yystate];

      if (yyn > YYFLAG && yyn < YYLAST)
	{
	  int size = 0;
	  char *msg;
	  int x, count;

	  count = 0;
	  /* Start X at -yyn if nec to avoid negative indexes in yycheck.  */
	  for (x = (yyn < 0 ? -yyn : 0);
	       x < (sizeof(yytname) / sizeof(char *)); x++)
	    if (yycheck[x + yyn] == x)
	      size += strlen(yytname[x]) + 15, count++;
	  msg = (char *) malloc(size + 15);
	  if (msg != 0)
	    {
	      strcpy(msg, "parse error");

	      if (count < 5)
		{
		  count = 0;
		  for (x = (yyn < 0 ? -yyn : 0);
		       x < (sizeof(yytname) / sizeof(char *)); x++)
		    if (yycheck[x + yyn] == x)
		      {
			strcat(msg, count == 0 ? ", expecting `" : " or `");
			strcat(msg, yytname[x]);
			strcat(msg, "'");
			count++;
		      }
		}
	      yyerror(msg);
	      free(msg);
	    }
	  else
	    yyerror ("parse error; also virtual memory exceeded");
	}
      else
#endif /* YYERROR_VERBOSE */
	yyerror("parse error");
    }

  goto yyerrlab1;
yyerrlab1:   /* here on error raised explicitly by an action */

  if (yyerrstatus == 3)
    {
      /* if just tried and failed to reuse lookahead token after an error, discard it.  */

      /* return failure if at end of input */
      if (yychar == YYEOF)
	YYABORT;

#if YYDEBUG != 0
      if (yydebug)
	fprintf(stderr, "Discarding token %d (%s).\n", yychar, yytname[yychar1]);
#endif

      yychar = YYEMPTY;
    }

  /* Else will try to reuse lookahead token
     after shifting the error token.  */

  yyerrstatus = 3;		/* Each real token shifted decrements this */

  goto yyerrhandle;

yyerrdefault:  /* current state does not do anything special for the error token. */

#if 0
  /* This is wrong; only states that explicitly want error tokens
     should shift them.  */
  yyn = yydefact[yystate];  /* If its default is to accept any token, ok.  Otherwise pop it.*/
  if (yyn) goto yydefault;
#endif

yyerrpop:   /* pop the current state because it cannot handle the error token */

  if (yyssp == yyss) YYABORT;
  yyvsp--;
  yystate = *--yyssp;
#ifdef YYLSP_NEEDED
  yylsp--;
#endif

#if YYDEBUG != 0
  if (yydebug)
    {
      short *ssp1 = yyss - 1;
      fprintf (stderr, "Error: state stack now");
      while (ssp1 != yyssp)
	fprintf (stderr, " %d", *++ssp1);
      fprintf (stderr, "\n");
    }
#endif

yyerrhandle:

  yyn = yypact[yystate];
  if (yyn == YYFLAG)
    goto yyerrdefault;

  yyn += YYTERROR;
  if (yyn < 0 || yyn > YYLAST || yycheck[yyn] != YYTERROR)
    goto yyerrdefault;

  yyn = yytable[yyn];
  if (yyn < 0)
    {
      if (yyn == YYFLAG)
	goto yyerrpop;
      yyn = -yyn;
      goto yyreduce;
    }
  else if (yyn == 0)
    goto yyerrpop;

  if (yyn == YYFINAL)
    YYACCEPT;

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Shifting error token, ");
#endif

  *++yyvsp = yylval;
#ifdef YYLSP_NEEDED
  *++yylsp = yylloc;
#endif

  yystate = yyn;
  goto yynewstate;
}
#line 792 "configfile.y"


int yyerror (const char *s)
{
#undef FMT
#define FMT "line %d: %s"
  
  errbuff = malloc (strlen (s) + strlen (FMT) + 20) ;
  sprintf (errbuff,FMT,lineCount,s) ;

  return 0 ;
}

int yywrap (void)
{
  return 1 ;
}

extern FILE *yyin ;
extern int yydebug ;

#define NO_INHERIT 0


#if ! defined (WANT_MAIN)

struct peer_table_s
{
    char *peerName ;
    value *peerValue ;
} ;

static struct peer_table_s *peerTable ;
static int peerTableCount ;
static int peerTableIdx ;

void configCleanup (void)
{
  int i ;

  for (i = 0 ; i < peerTableIdx ; i++)
    free (peerTable[i].peerName) ;
  free (peerTable) ;
  
  freeScopeTree (topScope);
  free (funcs) ;
  free (args) ;
}
  

int buildPeerTable (FILE *fp, scope *s)
{
  int rval = 1 ;
  int i, j ;

  for (i = 0 ; i < s->value_idx ; i++)
    {
      if (ISSCOPE (s->values[i]) && ISPEER (s->values[i]))
        {
          for (j = 0 ; j < peerTableIdx ; j++)
            {
              if (strcmp (peerTable[j].peerName,s->values[i]->name) == 0)
                {
                  logOrPrint (LOG_ERR,fp,DUP_PEER_NAME,peerTable[j].peerName) ;
                  rval = 0 ;
                  break ;
                }
            }

          if (j == peerTableIdx)
            {
              if (peerTableCount == peerTableIdx) 
                {
                  peerTableCount += 10 ;
                  if (peerTable == NULL)
                    peerTable = ALLOC(struct peer_table_s,peerTableCount) ;
                  else
                    peerTable = REALLOC (peerTable,struct peer_table_s,
                                         peerTableCount) ;
                }
  
              peerTable[peerTableIdx].peerName = strdup (s->values[i]->name);
              peerTable[peerTableIdx].peerValue = s->values[i] ;
              peerTableIdx++ ;
            }
        }
      else if (ISSCOPE (s->values[i]))
        rval = (buildPeerTable (fp,s->values[i]->v.scope_val) && rval) ;
    }

  return rval ;  
}


/* read the config file. Any errors go to errorDest if it is non-NULL,
   otherwise they are syslogged. If justCheck is true then return after
   parsing */
static int inited = 0 ;
int readConfig (const char *file, FILE *errorDest, int justCheck, int dump)
{
  scope *oldTop = topScope ;
  FILE *fp ;
  int rval ;

  if (!inited)
    {
      inited = 1 ;
      yydebug = (getenv ("YYDEBUG") == NULL ? 0 : 1) ;
      if (yydebug)
        atexit (configCleanup) ;
    }

  if (file == NULL || strlen (file) == 0 || !fileExistsP (file))
    {
      logOrPrint (LOG_ERR,errorDest,NOSUCH_CONFIG, file ? file : "(null)") ;
      dprintf (1,"No such config file: %s\n", file ? file : "(null)") ;
      exit (1) ;
    }

  if ((fp = fopen (file,"r")) == NULL)
    {
      logOrPrint (LOG_ERR,errorDest,CFG_FOPEN_FAILURE, file) ;
      exit (1) ;
    }

  logOrPrint (LOG_NOTICE,errorDest,"loading %s", file) ;

  yyin = fp ;

  topScope = NULL ;

  rval = yyparse () ;

  fclose (fp) ;
  
  if (rval != 0)                /* failure */
    {
      freeScopeTree (topScope) ;
      if (justCheck)
        freeScopeTree (oldTop) ;
      else
        topScope = oldTop ;
      topScope = NULL ;

      if (errbuff != NULL)
        {
          if (errorDest != NULL)
            fprintf (errorDest,CONFIG_PARSE_FAILED,"",errbuff) ;
          else
            syslog (LOG_ERR,CONFIG_PARSE_FAILED,"ME ",errbuff) ;
          
          free (errbuff) ;
        }
      
      return 0 ;
    }
  
  if (dump)
    {
      fprintf (errorDest ? errorDest : stderr,"Parsed config file:\n") ;
      printScope (errorDest ? errorDest : stderr,topScope,-5) ;
      fprintf (errorDest ? errorDest : stderr,"\n") ;
    }
  
  if (justCheck)
    {
      freeScopeTree (topScope) ;
      freeScopeTree (oldTop) ;

      topScope = NULL ;
    }
  else
    {
      for (peerTableIdx-- ; peerTableIdx >= 0 ; peerTableIdx--)
        {
          free (peerTable [peerTableIdx].peerName) ;
          peerTable [peerTableIdx].peerName = NULL ;
          peerTable [peerTableIdx].peerValue = NULL ;
        }
      peerTableIdx = 0 ;
      
      if (!buildPeerTable (errorDest,topScope))
        logAndExit (1,"Failed to build list of peers") ;
    }
  
  return 1 ;
}


value *getNextPeer (int *cookie)
{
  value *rval ;

  if (*cookie < 0 || *cookie >= peerTableIdx)
    return NULL ;

  rval = peerTable[*cookie].peerValue ;

  (*cookie)++ ;

  return rval ;
}


value *findPeer (const char *name)
{
  value *v = NULL ;
  int i ;

  for (i = 0 ; i < peerTableIdx ; i++)
    if (strcmp (peerTable[i].peerName,name) == 0)
      {
        v = peerTable[i].peerValue ;
        break ;
      }
  
  return v ;
}

#endif

#if defined (WANT_MAIN)
int main (int argc, char **argv) {
  if ( yyparse() )
    printf ("parsing failed: %s\n",errbuff ? errbuff : "NONE") ;
  else
    {
      printScope (stdout,topScope,-5) ;

      if (argc == 3)
        {
#if 0
          printf ("Looking for %s of type %s: ",argv[2],argv[1]) ;
          if (strncmp (argv[1],"int",3) == 0)
            {
              int i = 0 ;
          
              if (!getInteger (topScope,argv[2],&i))
                printf ("wasn't found.\n") ;
              else
                printf (" %d\n",i) ;
            }
          else if (strncmp (argv[1],"real",4) == 0)
            {
              double d = 0.0 ;

              if (!getReal (topScope,argv[2],&d))
                printf ("wasn't found.\n") ;
              else
                printf (" %0.5f\n",d) ;
            }
#else
          value *v = findValue (topScope,argv[1],1) ;

          if (v == NULL)
            printf ("Can't find %s\n",argv[1]) ;
          else
            {
              long ival = 987654 ;
              
              if (getInteger (v->v.scope_val,argv[2],&ival,1))
                printf ("Getting %s : %ld",argv[2],ival) ;
              else
                printf ("Name is not legal: %s\n",argv[2]) ;
            }
#endif
        }
      else if (argc == 2)
        {
#if 1
          value *v = findValue (topScope,argv[1],1) ;

          if (v == NULL)
            printf ("Can't find %s\n",argv[1]) ;
          else
            {
              printf ("Getting %s : ",argv[1]) ;
              printValue (stdout,v,0) ;
            }
#else
          if (findScope (topScope,argv[1],1) == NULL)
            printf ("Can't find the scope of %s\n",argv[1]) ;
#endif
        }
    }
  
  freeScopeTree (topScope) ;

  return 0 ;
}
#endif /* defined (WANT_MAIN) */


