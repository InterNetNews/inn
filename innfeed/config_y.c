#ifndef lint
static char yysccsid[] = "@(#)yaccpar	1.9 (Berkeley) 02/21/93";
#endif
#define YYBYACC 1
#define YYMAJOR 1
#define YYMINOR 9
#define yyclearin (yychar=(-1))
#define yyerrok (yyerrflag=0)
#define YYRECOVERING (yyerrflag!=0)
#define YYPREFIX "yy"
#line 2 "configfile.y"
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
#line 692 "y.tab.c"
#define PEER 257
#define GROUP 258
#define IVAL 259
#define RVAL 260
#define NAME 261
#define XSTRING 262
#define SCOPE 263
#define COLON 264
#define LBRACE 265
#define RBRACE 266
#define TRUEBVAL 267
#define FALSEBVAL 268
#define CHAR 269
#define WORD 270
#define IP_ADDRESS 271
#define YYERRCODE 256
short yylhs[] = {                                        -1,
    2,    0,    3,    1,    1,    1,    5,    4,    6,    4,
    4,    7,    4,    8,    8,    8,    8,    8,    8,    8,
};
short yylen[] = {                                         2,
    0,    2,    1,    0,    2,    2,    0,    6,    0,    6,
    3,    0,    4,    1,    1,    1,    1,    1,    1,    1,
};
short yydefred[] = {                                      1,
    0,    4,    0,    6,    0,    0,    0,    5,    0,    0,
    0,    0,    7,    9,   11,    0,    4,    4,   15,   18,
   19,   16,   17,   20,   14,   13,    0,    0,    0,    8,
   10,
};
short yydgoto[] = {                                       1,
   27,    2,   28,    8,   17,   18,   12,   26,
};
short yysindex[] = {                                      0,
    0,    0, -256,    0, -267, -266, -265,    0, -259, -254,
 -253, -255,    0,    0,    0, -252,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0, -256, -247, -246,    0,
    0,
};
short yyrindex[] = {                                      0,
    0,    0,   13,    0,    0,    0, -243,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0, -244,    0,    0,    0,
    0,
};
short yygindex[] = {                                      0,
   21,    0,    6,    0,    0,    0,    0,    0,
};
#define YYTABLESIZE 24
short yytable[] = {                                       4,
    5,    6,    9,   10,   11,   13,   19,   20,   16,   21,
   14,   15,    2,    7,   22,   23,   24,   25,   30,   31,
   12,    3,    3,   29,
};
short yycheck[] = {                                     256,
  257,  258,  270,  270,  270,  265,  259,  260,  264,  262,
  265,  265,    0,  270,  267,  268,  269,  270,  266,  266,
  264,  266,    2,   18,
};
#define YYFINAL 1
#ifndef YYDEBUG
#define YYDEBUG 0
#endif
#define YYMAXTOKEN 271
#if YYDEBUG
char *yyname[] = {
"end-of-file",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"PEER","GROUP","IVAL","RVAL",
"NAME","XSTRING","SCOPE","COLON","LBRACE","RBRACE","TRUEBVAL","FALSEBVAL",
"CHAR","WORD","IP_ADDRESS",
};
char *yyrule[] = {
"$accept : input",
"$$1 :",
"input : $$1 entries",
"scope : entries",
"entries :",
"entries : entries entry",
"entries : entries error",
"$$2 :",
"entry : PEER WORD LBRACE $$2 scope RBRACE",
"$$3 :",
"entry : GROUP WORD LBRACE $$3 scope RBRACE",
"entry : WORD WORD LBRACE",
"$$4 :",
"entry : WORD $$4 COLON value",
"value : WORD",
"value : IVAL",
"value : TRUEBVAL",
"value : FALSEBVAL",
"value : RVAL",
"value : XSTRING",
"value : CHAR",
};
#endif
#ifdef YYSTACKSIZE
#undef YYMAXDEPTH
#define YYMAXDEPTH YYSTACKSIZE
#else
#ifdef YYMAXDEPTH
#define YYSTACKSIZE YYMAXDEPTH
#else
#define YYSTACKSIZE 500
#define YYMAXDEPTH 500
#endif
#endif
int yydebug;
int yynerrs;
int yyerrflag;
int yychar;
short *yyssp;
YYSTYPE *yyvsp;
YYSTYPE yyval;
YYSTYPE yylval;
short yyss[YYSTACKSIZE];
YYSTYPE yyvs[YYSTACKSIZE];
#define yystacksize YYSTACKSIZE
#line 793 "configfile.y"

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


#line 1108 "y.tab.c"
#define YYABORT goto yyabort
#define YYREJECT goto yyabort
#define YYACCEPT goto yyaccept
#define YYERROR goto yyerrlab
int
yyparse()
{
    register int yym, yyn, yystate;
#if YYDEBUG
    register char *yys;
    extern char *getenv();

    if (yys = getenv("YYDEBUG"))
    {
        yyn = *yys;
        if (yyn >= '0' && yyn <= '9')
            yydebug = yyn - '0';
    }
#endif

    yynerrs = 0;
    yyerrflag = 0;
    yychar = (-1);

    yyssp = yyss;
    yyvsp = yyvs;
    *yyssp = yystate = 0;

yyloop:
    if (yyn = yydefred[yystate]) goto yyreduce;
    if (yychar < 0)
    {
        if ((yychar = yylex()) < 0) yychar = 0;
#if YYDEBUG
        if (yydebug)
        {
            yys = 0;
            if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
            if (!yys) yys = "illegal-symbol";
            printf("%sdebug: state %d, reading %d (%s)\n",
                    YYPREFIX, yystate, yychar, yys);
        }
#endif
    }
    if ((yyn = yysindex[yystate]) && (yyn += yychar) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yychar)
    {
#if YYDEBUG
        if (yydebug)
            printf("%sdebug: state %d, shifting to state %d\n",
                    YYPREFIX, yystate, yytable[yyn]);
#endif
        if (yyssp >= yyss + yystacksize - 1)
        {
            goto yyoverflow;
        }
        *++yyssp = yystate = yytable[yyn];
        *++yyvsp = yylval;
        yychar = (-1);
        if (yyerrflag > 0)  --yyerrflag;
        goto yyloop;
    }
    if ((yyn = yyrindex[yystate]) && (yyn += yychar) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yychar)
    {
        yyn = yytable[yyn];
        goto yyreduce;
    }
    if (yyerrflag) goto yyinrecovery;
#ifdef lint
    goto yynewerror;
#endif
yynewerror:
    yyerror("syntax error");
#ifdef lint
    goto yyerrlab;
#endif
yyerrlab:
    ++yynerrs;
yyinrecovery:
    if (yyerrflag < 3)
    {
        yyerrflag = 3;
        for (;;)
        {
            if ((yyn = yysindex[*yyssp]) && (yyn += YYERRCODE) >= 0 &&
                    yyn <= YYTABLESIZE && yycheck[yyn] == YYERRCODE)
            {
#if YYDEBUG
                if (yydebug)
                    printf("%sdebug: state %d, error recovery shifting\
 to state %d\n", YYPREFIX, *yyssp, yytable[yyn]);
#endif
                if (yyssp >= yyss + yystacksize - 1)
                {
                    goto yyoverflow;
                }
                *++yyssp = yystate = yytable[yyn];
                *++yyvsp = yylval;
                goto yyloop;
            }
            else
            {
#if YYDEBUG
                if (yydebug)
                    printf("%sdebug: error recovery discarding state %d\n",
                            YYPREFIX, *yyssp);
#endif
                if (yyssp <= yyss) goto yyabort;
                --yyssp;
                --yyvsp;
            }
        }
    }
    else
    {
        if (yychar == 0) goto yyabort;
#if YYDEBUG
        if (yydebug)
        {
            yys = 0;
            if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
            if (!yys) yys = "illegal-symbol";
            printf("%sdebug: state %d, error recovery discards token %d (%s)\n",
                    YYPREFIX, yystate, yychar, yys);
        }
#endif
        yychar = (-1);
        goto yyloop;
    }
yyreduce:
#if YYDEBUG
    if (yydebug)
        printf("%sdebug: state %d, reducing by rule %d (%s)\n",
                YYPREFIX, yystate, yyn, yyrule[yyn]);
#endif
    yym = yylen[yyn];
    yyval = yyvsp[1-yym];
    switch (yyn)
    {
case 1:
#line 706 "configfile.y"
{ 	
		lineCount = 1 ;
		addScope (NULL,"",newScope ("")) ;
		topScope = currScope ;
	}
break;
case 2:
#line 710 "configfile.y"
{ if (!doCallbacks()) YYABORT ; }
break;
case 6:
#line 716 "configfile.y"
{
		errbuff = malloc (strlen(SYNTAX_ERROR) + 12) ;
		sprintf (errbuff,SYNTAX_ERROR,lineCount) ;
		YYABORT ;
	}
break;
case 7:
#line 723 "configfile.y"
{
		errbuff = addScope (currScope,yyvsp[-1].name,newScope ("peer")) ;
                free (yyvsp[-1].name) ;
		if (errbuff != NULL) YYABORT ;
	}
break;
case 8:
#line 727 "configfile.y"
{
		currScope = currScope->parent ;
	}
break;
case 9:
#line 730 "configfile.y"
{
		errbuff = addScope (currScope,yyvsp[-1].name,newScope ("group")) ;
                free (yyvsp[-1].name) ;
		if (errbuff != NULL) YYABORT ;
	}
break;
case 10:
#line 734 "configfile.y"
{
		currScope = currScope->parent ;
	}
break;
case 11:
#line 737 "configfile.y"
{
		errbuff = malloc (strlen(UNKNOWN_SCOPE_TYPE) + 15 +
					  strlen (yyvsp[-2].name)) ;
		sprintf (errbuff,UNKNOWN_SCOPE_TYPE,lineCount,yyvsp[-2].name) ;
                free (yyvsp[-2].name) ;
                free (yyvsp[-1].name) ;
		YYABORT ;
	}
break;
case 12:
#line 745 "configfile.y"
{ 
		if ((errbuff = keyOk(yyvsp[0].name)) != NULL) {
			YYABORT ;
		} else
			key = yyvsp[0].name ;
	}
break;
case 14:
#line 752 "configfile.y"
{
		if ((errbuff = addString (currScope, key, yyvsp[0].name)) != NULL)
			YYABORT ;
                free (key) ;
                free (yyvsp[0].name) ;
	}
break;
case 15:
#line 758 "configfile.y"
{
		if ((errbuff = addInteger(currScope, key, yyvsp[0].integer)) != NULL)
			YYABORT; 
                free (key) ;
	}
break;
case 16:
#line 763 "configfile.y"
{
		if ((errbuff = addBoolean (currScope, key, 1)) != NULL)
			YYABORT ; 
                free (key) ;
                free (yyvsp[0].name) ;
	}
break;
case 17:
#line 769 "configfile.y"
{
		if ((errbuff = addBoolean (currScope, key, 0)) != NULL)
			YYABORT ; 
                free (key) ;
                free (yyvsp[0].name) ;
	}
break;
case 18:
#line 775 "configfile.y"
{
		if ((errbuff = addReal (currScope, key, yyvsp[0].real)) != NULL)
			YYABORT ; 
                free (key) ;
	}
break;
case 19:
#line 780 "configfile.y"
{ 
		if ((errbuff = addString (currScope, key, yyvsp[0].string)) != NULL)
			YYABORT;
                free (key) ;
	}
break;
case 20:
#line 785 "configfile.y"
{
		if ((errbuff = addChar (currScope, key, yyvsp[0].chr)) != NULL)
			YYABORT ;
                free (key) ;
        }
break;
#line 1376 "y.tab.c"
    }
    yyssp -= yym;
    yystate = *yyssp;
    yyvsp -= yym;
    yym = yylhs[yyn];
    if (yystate == 0 && yym == 0)
    {
#if YYDEBUG
        if (yydebug)
            printf("%sdebug: after reduction, shifting from state 0 to\
 state %d\n", YYPREFIX, YYFINAL);
#endif
        yystate = YYFINAL;
        *++yyssp = YYFINAL;
        *++yyvsp = yyval;
        if (yychar < 0)
        {
            if ((yychar = yylex()) < 0) yychar = 0;
#if YYDEBUG
            if (yydebug)
            {
                yys = 0;
                if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
                if (!yys) yys = "illegal-symbol";
                printf("%sdebug: state %d, reading %d (%s)\n",
                        YYPREFIX, YYFINAL, yychar, yys);
            }
#endif
        }
        if (yychar == 0) goto yyaccept;
        goto yyloop;
    }
    if ((yyn = yygindex[yym]) && (yyn += yystate) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yystate)
        yystate = yytable[yyn];
    else
        yystate = yydgoto[yym];
#if YYDEBUG
    if (yydebug)
        printf("%sdebug: after reduction, shifting from state %d \
to state %d\n", YYPREFIX, *yyssp, yystate);
#endif
    if (yyssp >= yyss + yystacksize - 1)
    {
        goto yyoverflow;
    }
    *++yyssp = yystate;
    *++yyvsp = yyval;
    goto yyloop;
yyoverflow:
    yyerror("yacc stack overflow");
yyabort:
    return (1);
yyaccept:
    return (0);
}
