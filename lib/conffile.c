/*  $Id$
**
**  Routines for reading in incoming.conf-style config files.
*/

#include "config.h"
#include "clibrary.h"
#include "conffile.h"
#include "libinn.h"
#include "macros.h"

static int getconfline(CONFFILE *F, char *buffer, int length) {
  if (F->f) {
    fgets(buffer, length, F->f);
  } else if (F->array) {
    strncpy(buffer, F->array[F->lineno], length);
  }
  if (ferror(F->f)) {
    return 1;
  }
  F->lineno++;
  if (strlen (F->buf) == F->sbuf) {
    return 1; /* Line too long */
  } else {
    return 0;
  }
}

static int cfeof(CONFFILE *F) {
  if (F->f) {
    return feof(F->f);
  } else if (F->array) {
    return (F->lineno == F->array_len);
  } else {
    return 1;
  }
}

static char *CONFgetword(CONFFILE *F)
{
  register char *p;
  register char *s;
  register char *t;
  char          *word;
  register bool flag;

  if (!F) return (NULL);	/* No conf file */
  if (!F->buf || !F->buf[0]) {
    if (cfeof (F)) return (NULL);
    if (!F->buf) {
      F->sbuf = BIG_BUFFER;
      F->buf = NEW(char, F->sbuf);
    }
    if (getconfline(F, F->buf, F->sbuf) != 0)
      return (NULL); /* Line too long */
  }
  do {
     /* Ignore blank and comment lines. */
     if ((p = strchr(F->buf, '\n')) != NULL)
       *p = '\0';
     if ((p = strchr(F->buf, COMMENT_CHAR)) != NULL) {
       if (p == F->buf || (p > F->buf && *(p - 1) != '\\'))
           *p = '\0';
     }
     for (p = F->buf; *p == ' ' || *p == '\t' ; p++);
     flag = TRUE;
     if (*p == '\0' && !cfeof(F)) {
       flag = FALSE;
       if (getconfline(F, F->buf, F->sbuf))
         return (NULL); /* Line too long */
       continue;
     }
     break;
  } while (!cfeof(F) || !flag);

  if (*p == '"') { /* double quoted string ? */
    p++;
    do {
      for (t = p; (*t != '"' || (*t == '"' && *(t - 1) == '\\')) &&
             *t != '\0'; t++);
      if (*t == '\0') {
        *t++ = '\n';
        if (getconfline(F, t, F->sbuf - strlen(F->buf)))
          return (NULL); /* Line too long */
        if ((s = strchr(t, '\n')) != NULL)
          *s = '\0';
      }
      else 
        break;
    } while (!cfeof(F));
    *t++ = '\0';
  }
  else {
    for (t = p; *t != ' ' && *t != '\t' && *t != '\0'; t++);
    if (*t != '\0')
      *t++ = '\0';
  }
  if (*p == '\0' && cfeof(F)) return (NULL);
  word = COPY (p);
  for (p = F->buf; *t != '\0'; t++)
    *p++ = *t;
  *p = '\0';

  return (word);
}

CONFFILE *CONFfopen(char *filename)
{
  FILE *f;
  CONFFILE *ret;

  f = fopen(filename, "r");
  if (!f)
    return(0);
  ret = NEW(CONFFILE, 1);
  if (!ret) {
    fclose(f);
    return(0);
  }
  ret->filename = COPY(filename);
  ret->buf = 0;
  ret->sbuf = 0;
  ret->lineno = 0;
  ret->f = f;
  ret->array = NULL;
  return(ret);
}

void CONFfclose(CONFFILE *f)
{
  if (!f) return;		/* No conf file */
  fclose(f->f);
  if (f->buf)
    DISPOSE(f->buf);
  if (f->filename)
    DISPOSE(f->filename);
  DISPOSE(f);
}

CONFTOKEN *CONFgettoken(CONFTOKEN *toklist, CONFFILE *file)
{
  char *word;
  static CONFTOKEN ret = {CONFstring, 0};
  int i;

  if (ret.name) {
    DISPOSE(ret.name);
    ret.name = 0;
  }
  word = CONFgetword(file);
  if (!word)
    return(0);
  if (toklist) {
    for (i = 0; toklist[i].type; i++) {
       if (EQ(word, toklist[i].name)) {
         DISPOSE(word);
         return(&toklist[i]);
       }
    }
  }
  ret.name = word;
  return(&ret);
}
