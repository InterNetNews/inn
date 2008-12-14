/*  $Id$
**
**  Read a C news "sys" file and split it up into a set of INN
**  newsfeeds entries.  Also works with B news.
**
**  Once done, edit all files that have HELP or all in them.
**  Review all files, anyway.
*/

#include "config.h"
#include "clibrary.h"
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>

#include "inn/innconf.h"
#include "inn/libinn.h"
#include "nntp.h"

#define TEMPFILE	":tmp"
static char		**Groups;


/*
**  Fill in the Groups array with the names of all active newsgroups.
*/
static void
ReadActive(act)
    char	*act;
{
    FILE	*F;
    int		i;
    char	buff[BUFSIZ];
    char	*p;

    /* Open file, count lines. */
    if ((F = fopen(act, "r")) == NULL) {
	perror(act);
	exit(1);
    }
    for (i = 0; fgets(buff, sizeof buff, F) != NULL; i++)
	continue;
    Groups = xmalloc((i + 2) * sizeof(char *));

    /* Fill in each word. */
    rewind(F);
    for (i = 0; fgets(buff, sizeof buff, F) != NULL; i++) {
	if ((p = strchr(buff, ' ')) != NULL)
	    *p = '\0';
	Groups[i] = xstrdup(buff);
    }
    Groups[i] = NULL;
    fclose(F);
}


/*
**  Read in the sys file and turn it into an array of strings, one
**  per continued line.
*/
char **
ReadSys(sys)
    char		*sys;
{
    char	*p;
    char	*to;
    char	*site;
    int	i;
    char		*data;
    char		**strings;

    /* Read in the file, get rough count. */
    if ((data = ReadInFile(sys, (struct stat *)NULL)) == NULL) {
	perror(sys);
	exit(1);
    }
    for (p = data, i = 0; (p = strchr(p, '\n')) != NULL; p++, i++)
	continue;

    /* Scan the file, glue all multi-line entries. */
    for (strings = xmalloc((i + 1) * sizeof(char *)), i = 0, to = p = data; *p; ) {
	for (site = to; *p; ) {
	    if (*p == '\n') {
		p++;
		*to = '\0';
		break;
	    }
	    if (*p == '\\' && p[1] == '\n')
		while (*++p && CTYPE(isspace, *p))
		    continue;
	    else
		*to++ = *p++;
	}
	*to++ = '\0';
	if (*site == '\0')
	    continue;
	strings[i++] = xstrdup(site);
    }
    strings[i] = NULL;
    free(data);
    return strings;
}


/*
**  Is this the name of a top-level group?  We want a simple name, "foo",
**  and should find a "foo." in the group list.
*/
static bool
Toplevel(p)
    char	*p;
{
    char	**gp;
    char	*g;
    int		i;

    if (strchr(p, '.') != NULL)
	return false;
    for (i = strlen(p) - 1, gp = Groups; (g = *gp++) != NULL; )
	if (strncmp(p, g, i) == 0 && g[i + 1] == '.')
	    return true;
    return false;
}


/*
**  Do we have a name that's a prefix for more then one newsgroup?
**  For "foo.bar", we must find more then one "foo.bar" or "foo.bar."
*/
static bool
GroupPrefix(p)
    char	*p;
{
    char	**gp;
    char	*g;
    int		count;
    int		i;

    if (strchr(p, '.') == NULL)
	return false;
    for (i = strlen(p), count = 0, gp = Groups; (g = *gp++) != NULL; )
	if (strcmp(p, g) == 0 || (strncmp(p, g, i) == 0 && g[i] == '.'))
	    count++;
    return count > 1;
}


/*
**  Step through the old subscription list, try to update each one in
**  turn.
*/
static void
DoSub(F, p)
    FILE	*F;
    char		*p;
{
    char	*s;
    int	len, i;
    bool matched;
    bool	SawBang;
    bool	SawAll;

    /* Distributions, not newsgroups. */
    static const char * const distributions[] = {
        "world", "na", "usa", "inet", "mod", "net", "local"
    };

    /* Newsgroup hierarchies. */
    static const char * const hierarchies[] = {
        "comp", "misc", "news", "rec", "sci", "soc", "talk", "alt", "bionet",
        "bit", "biz", "clari", "ddn", "gnu", "ieee", "k12", "pubnet", "trial",
        "u3b", "vmsnet",

        "ba", "ca", "dc", "ne", "ny", "tx",

        "info", "mail", "opinions", "uunet"
    };

    if ((s = strtok(p, ",")) == NULL)
	return;

    fprintf(F, "!*");
    len = 8 + 1 + 2;
    do {
        for (matched = false, i = 0; i < ARRAY_SIZE(distributions); i++)
            if (strcmp(s, distributions[i]) == 0) {
                matched = true;
                break;
            }
        if (matched)
            continue;

	if (innconf->mergetogroups)
	    if (strcmp(s, "!to") == 0 || strncmp(s, "to.", 3) == 0)
		continue;

	putc(',', F);
	len++;

	if (len + strlen(s) + 3 > 72) {
	    fprintf(F,"\\\n\t    ");
	    len = 12;
	}

	SawBang = *s == '!';
	if (SawBang) {
	    putc('!', F);
	    len++;
	    s++;
	}

	SawAll = (strcmp(s, "all") == 0);
	if (SawAll)
	    s = SawBang ? "*" : "*,!control,!control.*";
	len += strlen(s);
	fprintf(F, "%s", s);

	if (SawAll)
	    ;
	else {
            for (matched = false, i = 0; i < ARRAY_SIZE(distributions); i++)
                if (strcmp(s, hierarchies[i]) == 0) {
                    matched = true;
                    break;
                }

            if (matched) {
                fprintf(F, ".*");
                len += 2;
            } else if (GroupPrefix(s)) {
                putc('*', F);
                len++;
            }
        }
    } while ((s = strtok((char *)NULL, ",")) != NULL);
}


int
main(ac, av)
    int		 ac;
    char	*av[];
{
    FILE	*F;
    FILE	*out;
    char	**sites;
    char	*f2;
    char	*f3;
    char	*f4;
    char	*p;
    char	*q;
    char	*site;
    char	buff[256];
    char	*act;
    char	*dir;
    char	*sys;
    int		i;

    if (!innconf_read(NULL))
        exit(1);
    /* Set defaults. */
    act = "/usr/local/lib/newslib/active";
    sys = "sys";
    dir = "feeds";
    while ((i = getopt(ac, av, "a:s:d:")) != EOF)
    switch (i) {
    default:
	exit(1);
	/* NOTREACHED */
    case 'a':	act = optarg;	break;
    case 'd':	dir = optarg;	break;
    case 's':	sys = optarg;	break;
    }

    sites = ReadSys(sys);
    ReadActive(act);
    if (mkdir(dir, 0777) < 0 && errno != EEXIST)
	perror(dir), exit(1);
    if (chdir(dir) < 0)
	perror("chdir"), exit(1);
    for ( ; ; ) {
	/* Get next non-comment ilne. */
	if ((p = *sites++) == NULL)
	    break;
	for (F = fopen(TEMPFILE, "w"); p && *p == '#'; p = *sites++)
	    fprintf(F, "%s\n", p);
	if (p == NULL) {
	    fclose(F);
	    break;
	}
	site = xstrdup(p);
	if ((f2 = strchr(site, ':')) == NULL)
	    f2 = "HELP";
	else
	    *f2++ = '\0';
	if ((f3 = strchr(f2, ':')) == NULL)
	    f3 = "HELP";
	else
	    *f3++ = '\0';
	if ((f4 = strchr(f3, ':')) == NULL)
	    f4 = "HELP";
	else
	    *f4++ = '\0';

	/* Write the fields. */
	fprintf(F, "%s\\\n", site);
	fprintf(F, "\t:");
	DoSub(F, f2);
	fprintf(F, "\\\n");
	if (strcmp(f3, "n") == 0)
	    fprintf(F, "\t:Tf,Wnm\\\n", f3);
	else
	    fprintf(F, "\t:HELP%s\\\n", f3);
	fprintf(F, "\t:%s\n", f4);
	if (ferror(F) || fclose(F) == EOF)
	    perror(TEMPFILE), exit(1);

	free(site);

	/* Find the sitename. */
	for (q = p; *q && *q != '/' && *q != ':'; q++)
	    continue;
	*q = '\0';

	/* Append temp file to site file. */
	if ((F = fopen(TEMPFILE, "r")) == NULL)
	    perror(TEMPFILE), exit(1);
	if ((out = xfopena(p)) == NULL)
	    perror(p), exit(1);
	while ((i = fread(buff, 1, sizeof buff, F)) > 0)
	    if (fwrite(buff, 1, i, out) != i)
		perror(p), exit(1);
	fclose(F);
	if (fclose(out) == EOF)
	    perror(p), exit(1);

	if (unlink(TEMPFILE) < 0)
	    perror("can't unlink temp file");
    }

    exit(0);
    /* NOTREACHED */
}
