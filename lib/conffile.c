/* $Revision$
**
** Routines for reading in incoming.conf-style config files.
*/

#include <stdio.h>
#include <sys/types.h>
#include <configdata.h>
#include <macros.h>
#include <clibrary.h>
#include <conffile.h>
#include <libinn.h>

static char *CONFgetword(CONFFILE *F)
{
  register char *p;
  register char *s;
  register char *t;
  char          *word;
  register BOOL flag;

  if (!F) return (NULL);	/* No conf file */
  if (!F->buf || !F->buf[0]) {
    if (feof (F->f)) return (NULL);
    if (!F->buf) {
      F->sbuf = SMBUF;
      F->buf = NEW(char, F->sbuf);
    }
    fgets(F->buf, F->sbuf, F->f);
    F->lineno++;
    if (strlen (F->buf) == F->sbuf)
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
     if (*p == '\0' && !feof (F->f)) {
       flag = FALSE;
       fgets(F->buf, F->sbuf, F->f);
       F->lineno++;
       if (strlen (F->buf) == F->sbuf)
         return (NULL); /* Line too long */
       continue;
     }
     break;
  } while (!feof (F->f) || !flag);

  if (*p == '"') { /* double quoted string ? */
    p++;
    do {
      for (t = p; (*t != '"' || (*t == '"' && *(t - 1) == '\\')) &&
             *t != '\0'; t++);
      if (*t == '\0') {
        *t++ = '\n';
        fgets(t, F->sbuf - strlen (F->buf), F->f);
	F->lineno++;
        if (strlen (F->buf) == F->sbuf)
          return (NULL); /* Line too long */
        if ((s = strchr(t, '\n')) != NULL)
          *s = '\0';
      }
      else 
        break;
    } while (!feof (F->f));
    *t++ = '\0';
  }
  else {
    for (t = p; *t != ' ' && *t != '\t' && *t != '\0'; t++);
    if (*t != '\0')
      *t++ = '\0';
  }
  if (*p == '\0' && feof (F->f)) return (NULL);
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
  ret->buf = 0;
  ret->sbuf = 0;
  ret->lineno = 0;
  ret->f = f;
  return(ret);
}

void CONFfclose(CONFFILE *f)
{
  if (!f) return;		/* No conf file */
  fclose(f->f);
  if (f->buf)
    DISPOSE(f->buf);
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
