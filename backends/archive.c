/*  $Id$
**
**  Read batchfiles on standard input and archive them.
*/

#include "config.h"
#include "clibrary.h"
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <time.h>

#ifdef TM_IN_SYS_TIME
# include <sys/time.h>
#endif

#include "inn/innconf.h"
#include "inn/messages.h"
#include "inn/wire.h"
#include "libinn.h"
#include "paths.h"
#include "storage.h"


static char	*Archive = NULL;
static char	*ERRLOG = NULL;

/*
**  Return a YYYYMM string that represents the current year/month
*/
static char *
DateString(void)
{
    static char		ds[10];
    time_t		now;
    struct tm		*x;

    time(&now);
    x = localtime(&now);
    snprintf(ds, sizeof(ds), "%d%d", x->tm_year + 1900, x->tm_mon + 1);

    return ds;
}


/*
**  Try to make one directory.  Return false on error.
*/
static bool
MakeDir(char *Name)
{
    struct stat		Sb;

    if (mkdir(Name, GROUPDIR_MODE) >= 0)
	return true;

    /* See if it failed because it already exists. */
    return stat(Name, &Sb) >= 0 && S_ISDIR(Sb.st_mode);
}


/*
**  Given an entry, comp/foo/bar/1123, create the directory and all
**  parent directories needed.  Return false on error.
*/
static bool
MakeArchiveDirectory(char *Name)
{
    char	*p;
    char	*save;
    bool		made;

    if ((save = strrchr(Name, '/')) != NULL)
	*save = '\0';

    /* Optimize common case -- parent almost always exists. */
    if (MakeDir(Name)) {
	if (save)
	    *save = '/';
	return true;
    }

    /* Try to make each of comp and comp/foo in turn. */
    for (p = Name; *p; p++)
	if (*p == '/' && p != Name) {
	    *p = '\0';
	    made = MakeDir(Name);
	    *p = '/';
	    if (!made) {
		if (save)
		    *save = '/';
		return false;
	    }
	}

    made = MakeDir(Name);
    if (save)
	*save = '/';
    return made;
}


/*
**  Copy a file.  Return false if error.
*/
static bool
Copy(char *src, char *dest)
{
    FILE	*in;
    FILE	*out;
    size_t	i;
    char	*p;
    char	buff[BUFSIZ];

    /* Open the output file. */
    if ((out = fopen(dest, "w")) == NULL) {
	/* Failed; make any missing directories and try again. */
	if ((p = strrchr(dest, '/')) != NULL) {
	    if (!MakeArchiveDirectory(dest)) {
                syswarn("cannot mkdir for %s", dest);
		return false;
	    }
	    out = fopen(dest, "w");
	}
	if (p == NULL || out == NULL) {
            syswarn("cannot open %s for writing", dest);
	    return false;
	}
    }

    /* Opening the input file is easier. */
    if ((in = fopen(src, "r")) == NULL) {
        syswarn("cannot open %s for reading", src);
	fclose(out);
	unlink(dest);
	return false;
    }

    /* Write the data. */
    while ((i = fread(buff, 1, sizeof buff, in)) != 0)
	if (fwrite(buff, 1, i, out) != i) {
            syswarn("cannot write to %s", dest);
	    fclose(in);
	    fclose(out);
	    unlink(dest);
	    return false;
	}
    fclose(in);

    /* Flush and close the output. */
    if (ferror(out) || fflush(out) == EOF) {
        syswarn("cannot flush %s", dest);
	unlink(dest);
	fclose(out);
	return false;
    }
    if (fclose(out) == EOF) {
        syswarn("cannot close %s", dest);
	unlink(dest);
	return false;
    }

    return true;
}


/*
**  Copy an article from memory into a file.
*/
static bool
CopyArt(ARTHANDLE *art, char *dest, bool Concat)
{
    FILE	*out;
    const char		*p;
    char		*q, *article;
    size_t		i;
    const char		*mode = "w";

    if (Concat) mode = "a";

    /* Open the output file. */
    if ((out = fopen(dest, mode)) == NULL) {
	/* Failed; make any missing directories and try again. */
	if ((p = strrchr(dest, '/')) != NULL) {
	    if (!MakeArchiveDirectory(dest)) {
                syswarn("cannot mkdir for %s", dest);
		return false;
	    }
	    out = fopen(dest, mode);
	}
	if (p == NULL || out == NULL) {
            syswarn("cannot open %s for writing", dest);
	    return false;
	}
    }

    /* Copy the data. */
    article = xmalloc(art->len);
    for (i=0, q=article, p=art->data; p<art->data+art->len;) {
	if (&p[1] < art->data + art->len && p[0] == '\r' && p[1] == '\n') {
	    p += 2;
	    *q++ = '\n';
	    i++;
	    if (&p[1] < art->data + art->len && p[0] == '.' && p[1] == '.') {
		p += 2;
		*q++ = '.';
		i++;
	    }
	    if (&p[2] < art->data + art->len && p[0] == '.' && p[1] == '\r' && p[2] == '\n') {
		break;
	    }
	} else {
	    *q++ = *p++;
	    i++;
	}
    }
    *q++ = '\0';

    /* Write the data. */
    if (Concat) {
	/* Write a separator... */
	fprintf(out, "-----------\n");
    }
    if (fwrite(article, i, 1, out) != 1) {
        syswarn("cannot write to %s", dest);
	fclose(out);
	if (!Concat) unlink(dest);
	free(article);
	return false;
    }
    free(article);

    /* Flush and close the output. */
    if (ferror(out) || fflush(out) == EOF) {
        syswarn("cannot flush %s", dest);
	if (!Concat) unlink(dest);
	fclose(out);
	return false;
    }
    if (fclose(out) == EOF) {
        syswarn("cannot close %s", dest);
	if (!Concat) unlink(dest);
	return false;
    }

    return true;
}


/*
**  Write an index entry.  Ignore I/O errors; our caller checks for them.
*/
static void
WriteArtIndex(ARTHANDLE *art, char *ShortName)
{
    const char	*p;
    int	i;
    char		Subject[BUFSIZ];
    char		MessageID[BUFSIZ];

    Subject[0] = '\0';		/* default to null string */
    p = wire_findheader(art->data, art->len, "Subject");
    if (p != NULL) {
	for (i=0; *p != '\r' && *p != '\n' && *p != '\0'; i++) {
	    Subject[i] = *p++;
	}
	Subject[i] = '\0';
    }

    MessageID[0] = '\0';	/* default to null string */
    p = wire_findheader(art->data, art->len, "Message-ID");
    if (p != NULL) {
	for (i=0; *p != '\r' && *p != '\n' && *p != '\0'; i++) {
	    MessageID[i] = *p++;
	}
	MessageID[i] = '\0';
    }

    printf("%s %s %s\n",
	    ShortName,
	    MessageID[0] ? MessageID : "<none>",
	    Subject[0] ? Subject : "<none>");
}


/*
** Crack an Xref line apart into separate strings, each of the form "ng:artnum".
** Return in "lenp" the number of newsgroups found.
** 
** This routine blatantly stolen from tradspool.c
*/
static char **
CrackXref(const char *xref, unsigned int *lenp) {
    char *p;
    char **xrefs;
    char *q;
    unsigned int len, xrefsize;

    len = 0;
    xrefsize = 5;
    xrefs = xmalloc(xrefsize * sizeof(char *));

    /* skip pathhost */
    if ((p = strchr(xref, ' ')) == NULL) {
        warn("cannot find pathhost in Xref header");
	return NULL;
    }
    /* skip next spaces */
    for (p++; *p == ' ' ; p++) ;
    while (true) {
	/* check for EOL */
	/* shouldn't ever hit null w/o hitting a \r\n first, but best to be paranoid */
	if (*p == '\n' || *p == '\r' || *p == 0) {
	    /* hit EOL, return. */
	    *lenp = len;
	    return xrefs;
	}
	/* skip to next space or EOL */
	for (q=p; *q && *q != ' ' && *q != '\n' && *q != '\r' ; ++q) ;

        xrefs[len] = xstrndup(p, q - p);

	if (++len == xrefsize) {
	    /* grow xrefs if needed. */
	    xrefsize *= 2;
            xrefs = xrealloc(xrefs, xrefsize * sizeof(char *));
	}

 	p = q;
	/* skip spaces */
	for ( ; *p == ' ' ; p++) ;
    }
}


/*
** Crack an groups pattern parameter apart into separate strings
** Return in "lenp" the number of patterns found.
*/
static char **
CrackGroups(char *group, unsigned int *lenp) {
    char *p;
    char **groups;
    char *q;
    unsigned int len, grpsize;

    len = 0;
    grpsize = 5;
    groups = xmalloc(grpsize * sizeof(char *));

    /* skip leading spaces */
    for (p=group; *p == ' ' ; p++) ;
    while (true) {
	/* check for EOL */
	/* shouldn't ever hit null w/o hitting a \r\n first, but best to be paranoid */
	if (*p == '\n' || *p == '\r' || *p == 0) {
	    /* hit EOL, return. */
	    *lenp = len;
	    return groups;
	}
	/* skip to next comma, space, or EOL */
	for (q=p; *q && *q != ',' && *q != ' ' && *q != '\n' && *q != '\r' ; ++q) ;

        groups[len] = xstrndup(p, q - p);

	if (++len == grpsize) {
	    /* grow groups if needed. */
	    grpsize *= 2;
            groups = xrealloc(groups, grpsize * sizeof(char *));
	}

 	p = q;
	/* skip commas and spaces */
	for ( ; *p == ' ' || *p == ',' ; p++) ;
    }
}


int
main(int ac, char *av[])
{
    char	*Name;
    char	*p;
    FILE	*F;
    int	i;
    bool		Flat;
    bool		Redirect;
    bool		Concat;
    char		*Index;
    char		buff[BUFSIZ];
    char		*spool;
    char		dest[BUFSIZ];
    char		**groups, *q, *ng;
    char		**xrefs;
    const char		*xrefhdr;
    ARTHANDLE		*art;
    TOKEN		token;
    unsigned int	numgroups, numxrefs;
    int			j;
    char		*base = NULL;
    bool		doit;

    /* First thing, set up our identity. */
    message_program_name = "archive";

    /* Set defaults. */
    if (!innconf_read(NULL))
        exit(1);
    Concat = false;
    Flat = false;
    Index = NULL;
    Redirect = true;
    umask(NEWSUMASK);
    ERRLOG = concatpath(innconf->pathlog, _PATH_ERRLOG);
    Archive = innconf->patharchive;
    groups = NULL;
    numgroups = 0;

    /* Parse JCL. */
    while ((i = getopt(ac, av, "a:cfi:p:r")) != EOF)
	switch (i) {
	default:
            die("usage error");
            break;
	case 'a':
	    Archive = optarg;
	    break;
	case 'c':
	    Flat = true;
	    Concat = true;
	    break;
	case 'f':
	    Flat = true;
	    break;
	case 'i':
	    Index = optarg;
	    break;
	case 'p':
	    groups = CrackGroups(optarg, &numgroups);
	    break;
	case 'r':
	    Redirect = false;
	    break;
	}

    /* Parse arguments -- at most one, the batchfile. */
    ac -= optind;
    av += optind;
    if (ac > 2)
        die("usage error");

    /* Do file redirections. */
    if (Redirect)
	freopen(ERRLOG, "a", stderr);
    if (ac == 1 && freopen(av[0], "r", stdin) == NULL)
        sysdie("cannot open %s for input", av[0]);
    if (Index && freopen(Index, "a", stdout) == NULL)
        sysdie("cannot open %s for output", Index);

    /* Go to where the action is. */
    if (chdir(innconf->patharticles) < 0)
        sysdie("cannot chdir to %s", innconf->patharticles);

    /* Set up the destination. */
    strcpy(dest, Archive);
    Name = dest + strlen(dest);
    *Name++ = '/';

    if (!SMinit())
        die("cannot initialize storage manager: %s", SMerrorstr);

    /* Read input. */
    while (fgets(buff, sizeof buff, stdin) != NULL) {
	if ((p = strchr(buff, '\n')) == NULL) {
            warn("skipping %.40s: too long", buff);
	    continue;
	}
	*p = '\0';
	if (buff[0] == '\0' || buff[0] == '#')
	    continue;

	/* Check to see if this is a token... */
	if (IsToken(buff)) {
	    /* Get a copy of the article. */
	    token = TextToToken(buff);
	    if ((art = SMretrieve(token, RETR_ALL)) == NULL) {
                warn("cannot retrieve %s", buff);
		continue;
	    }

	    /* Determine groups from the Xref header */
    	    xrefhdr = wire_findheader(art->data, art->len, "Xref");
	    if (xrefhdr == NULL) {
                warn("cannot find Xref header");
		SMfreearticle(art);
		continue;
	    }

	    if ((xrefs = CrackXref(xrefhdr, &numxrefs)) == NULL || numxrefs == 0) {
                warn("bogus Xref header");
		SMfreearticle(art);
		continue;
	    }

	    /* Process each newsgroup... */
	    if (base) {
		free(base);
		base = NULL;
	    }
	    for (i=0; (unsigned)i<numxrefs; i++) {
		/* Check for group limits... -p flag */
		if ((p=strchr(xrefs[i], ':')) == NULL) {
                    warn("bogus Xref entry %s", xrefs[i]);
		    continue;	/* Skip to next xref */
		}
		if (numgroups > 0) {
		    *p = '\0';
		    ng = xrefs[i];
		    doit = false;
		    for (j=0; (unsigned)j<numgroups && !doit; j++) {
			if (uwildmat(ng, groups[j]) != 0) doit=true;
		    }
		}
		else {
		    doit = true;
		}
		*p = '/';
		if (doit) {
		    p = Name;
		    q = xrefs[i];
		    while(*q) {
		        *p++ = *q++;
		    }
		    *p='\0';

		    if (!Flat) {
		        for (p=Name; *p; p++) {
			    if (*p == '.') {
			        *p = '/';
			    }
		        }
		    }

		    if (Concat) {
			p = strrchr(Name, '/');
			q = DateString();
			p++;
			while (*q) {
			    *p++ = *q++;
			}
			*p = '\0';
		    }
			
		    if (base && !Concat) {
			/* Try to link the file into the archive. */
			if (link(base, dest) < 0) {

			    /* Make the archive directory. */
			    if (!MakeArchiveDirectory(dest)) {
                                syswarn("cannot mkdir for %s", dest);
				continue;
			    }

			    /* Try to link again; if that fails, make a copy. */
			    if (link(base, dest) < 0) {
#if	defined(HAVE_SYMLINK)
				if (symlink(base, dest) < 0)
                                    syswarn("cannot symlink %s to %s",
                                            dest, base);
				else
#endif	/* defined(HAVE_SYMLINK) */
				if (!Copy(base, dest))
				    continue;
				continue;
			    }
			}
		    } else {
			if (!CopyArt(art, dest, Concat))
                            syswarn("copying %s to %s failed", buff, dest);
			base = xstrdup(dest);
		    }

	            /* Write index. */
	            if (Index) {
	                WriteArtIndex(art, Name);
	                if (ferror(stdout) || fflush(stdout) == EOF)
                            syswarn("cannot write index for %s", Name);
	            }
		}
	    }

	    /* Free up the article storage space */
	    SMfreearticle(art);
	    art = NULL;
	    /* Free up the xrefs storage space */
	    for ( i=0; (unsigned)i<numxrefs; i++) free(xrefs[i]);
	    free(xrefs);
	    numxrefs = 0;
	    xrefs = NULL;
	} else {
            warn("%s is not a token", buff);
	    continue;
	}
    }

    /* close down the storage manager api */
    SMshutdown();

    /* If we read all our input, try to remove the file, and we're done. */
    if (feof(stdin)) {
	fclose(stdin);
	if (av[0])
	    unlink(av[0]);
	exit(0);
    }

    /* Make an appropriate spool file. */
    p = av[0];
    if (p == NULL)
        spool = concatpath(innconf->pathoutgoing, "archive");
    else if (*p == '/')
        spool = concat(p, ".bch", (char *) 0);
    else
        spool = concat(innconf->pathoutgoing, "/", p, ".bch", (char *) 0);
    if ((F = xfopena(spool)) == NULL)
        sysdie("cannot spool to %s", spool);

    /* Write the rest of stdin to the spool file. */
    i = 0;
    if (fprintf(F, "%s\n", buff) == EOF) {
        syswarn("cannot start spool");
	i = 1;
    }
    while (fgets(buff, sizeof buff, stdin) != NULL) 
	if (fputs(buff, F) == EOF) {
            syswarn("cannot write to spool");
	    i = 1;
	    break;
	}
    if (fclose(F) == EOF) {
        syswarn("cannot close spool");
	i = 1;
    }

    /* If we had a named input file, try to rename the spool. */
    if (p != NULL && rename(spool, av[0]) < 0) {
        syswarn("cannot rename spool");
	i = 1;
    }

    exit(i);
    /* NOTREACHED */
}
