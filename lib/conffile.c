/*  $Id$
**
**  Routines for reading in incoming.conf-style config files.
*/

#include "config.h"
#include "clibrary.h"
#include "conffile.h"
#include "libinn.h"

static int getconfline(CONFFILE *F, char *buffer, int length) {
  if (F->f) {
    fgets(buffer, length, F->f);
    if (ferror(F->f)) {
      return 1;
    }
  } else if (F->array) {
    strlcpy(buffer, F->array[F->lineno], F->sbuf);
  }
  F->lineno++;
  if (strlen (F->buf) >= F->sbuf - 1) {
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
  char *p;
  char *s;
  char *t;
  char *word;
  bool flag, comment;

  if (!F) return (NULL);	/* No conf file */
  if (!F->buf || !F->buf[0]) {
    if (cfeof (F)) return (NULL);
    if (!F->buf) {
      F->sbuf = BIG_BUFFER;
      F->buf = xmalloc(F->sbuf);
    }
    if (getconfline(F, F->buf, F->sbuf) != 0)
      return (NULL); /* Line too long */
  }
  do {
     /* Ignore blank and comment lines. */
     if ((p = strchr(F->buf, '\n')) != NULL)
       *p = '\0';
     for (p = F->buf; *p == ' ' || *p == '\t' ; p++);
     flag = true;
     if ((*p == '\0' || *p == '#') && !cfeof(F)) {
       flag = false;
       if (getconfline(F, F->buf, F->sbuf))
         return (NULL); /* Line too long */
       continue;
     }
     break;
  } while (!cfeof(F) || !flag);

  comment = false;
  if (*p == '"') { /* double quoted string ? */
    p++;
    do {
      for (t = p; (*t != '"' || (*t == '"' && *(t - 1) == '\\')) &&
             *t != '\0'; t++);
      if (*t == '\0') {
        if (strlen(F->buf) >= F->sbuf - 2)
          return (NULL); /* Line too long */
        *t++ = '\n';
        *t = '\0';
        if (getconfline(F, t, F->sbuf - strlen(F->buf)))
          return (NULL); /* Line too long */
        if ((s = strchr(t, '\n')) != NULL)
          *s = '\0';
      }
      else 
        break;
    } while (!cfeof(F));
    if (*t != '"')
      return (NULL);
    *t++ = '\0';
  }
  else {
    for (t = p; *t != ' ' && *t != '\t' && *t != '\0'; t++)
      if (*t == '#' && (t == p || *(t - 1) != '\\')) {
        comment = true;
        break;
      }
    if (*t != '\0')
      *t++ = '\0';
  }
  if (*p == '\0' && cfeof(F)) return (NULL);
  word = xstrdup (p);
  p = F->buf;
  if (!comment)
    for (; *t != '\0'; t++)
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
  ret = xmalloc(sizeof(CONFFILE));
  if (!ret) {
    fclose(f);
    return(0);
  }
  ret->filename = xstrdup(filename);
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
    free(f->buf);
  if (f->filename)
    free(f->filename);
  free(f);
}

CONFTOKEN *CONFgettoken(CONFTOKEN *toklist, CONFFILE *file)
{
  char *word;
  static CONFTOKEN ret = {CONFstring, 0};
  int i;

  if (ret.name) {
    free(ret.name);
    ret.name = 0;
  }
  word = CONFgetword(file);
  if (!word)
    return(0);
  if (toklist) {
    for (i = 0; toklist[i].type; i++) {
       if (strcmp(word, toklist[i].name) == 0) {
         free(word);
         return(&toklist[i]);
       }
    }
  }
  ret.name = word;
  return(&ret);
}
