/*  $Id$
**
**  unified overview processing
*/

#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <ctype.h>
#include <macros.h>
#include <configdata.h>
#include <clibrary.h>
#include <libinn.h>
#include <logging.h>
#include <paths.h>
#include <methods.h>
#include <overview.h>
#include <errno.h>

static UNIOVER		*OVERconfig = (UNIOVER *)NULL;
static BOOL		Initialized = FALSE;
static char		*Overbuff = (char *)NULL;
static BOOL		Newfp = FALSE;
static char		*OVERctl = (char *)NULL;
static char		*OVERdir = (char *)NULL;
static char		*OVERnewdir = (char *)NULL;
static char		*OVERmode = (char *)NULL;
static char		*OVERnewmode = (char *)NULL;
static BOOL		OVERmmap = TRUE;
#define DEFAULTMODE	"a+"
#define MAXOVERLINE	4096

/*
** Read overview.ctl to determin which overview file is to be written.
*/
STATIC OVERCONFIG OVERreadconfig(BOOL New)
{
    FILE *f;
    char buf[1024];
    int i, line = 0;
    char *p, *q;
    char *overindex;
    char *patterns;
    UNIOVER *config, *newconfig = (UNIOVER *)NULL, *prev = (UNIOVER *)NULL;
    unsigned char index;
    FILE *fp;
    BOOL nextline;
    char *dirpath;
    char *newdirpath;
    char *path;
    char *newpath;
    int pathlen;
    int newpathlen;
    struct stat sb;
    char *addr;

    if ((f = fopen(OVERctl, "r")) == NULL) {
	syslog(L_ERROR, "OVER cannot open %s: %m", OVERctl);
	return OVER_NULL; 
    }

    pathlen = strlen(OVERdir);
    newpathlen = strlen(OVERnewdir);
    dirpath = NEW(char, pathlen + sizeof("/255") + 1);
    newdirpath = NEW(char, newpathlen + sizeof("/255") + 1);
    path = NEW(char, pathlen + sizeof("/255") + sizeof("/overview") + 1);
    newpath = NEW(char, newpathlen + sizeof("/255") + sizeof("/overview.new") + 1);
    while(fgets(buf, 1024, f) != NULL) {
	line++;
	if ((p = strchr(buf, '#')) != NULL)
	    *p = '\0';
	for (p = q = buf; *p != '\0'; p++)
	    if (!isspace(*p))
		*q++ = *p;
	*q = '\0';
	if (!buf[0])
	    continue;
	if ((p = strchr(buf, ':')) == NULL) {
	    syslog(L_ERROR, "OVER Could not find end of the first field, line %d: %m", line);
	    DISPOSE(dirpath);
	    DISPOSE(newdirpath);
	    DISPOSE(path);
	    DISPOSE(newpath);
	    return OVER_ERROR;
	}

	overindex = buf;
	*p = '\0';
	patterns = ++p;
	for (i = 1, p = patterns; *p && (p = strchr(p+1, ',')); i++);
	index = atoi(overindex);
	if (index >= OVER_NONE) {
	    syslog(L_ERROR, "OVER index is out of range, line %d: %m", line);
	    DISPOSE(dirpath);
	    DISPOSE(newdirpath);
	    DISPOSE(path);
	    DISPOSE(newpath);
	    return OVER_ERROR;
	}

	nextline = FALSE;
	for(config=OVERconfig;config!=(UNIOVER *)NULL;config=config->next) {
	    if (config->index == index) {
		if (New) {
		    newconfig = config;
		} else {
		    syslog(L_ERROR, "OVER duplicate index, line %d: %m", line);
		    DISPOSE(dirpath);
		    DISPOSE(newdirpath);
		    DISPOSE(path);
		    DISPOSE(newpath);
		    return OVER_ERROR;
		}
	    }
	}
	addr = (char *)-1;
	sb.st_size = 0;
	if (New) {
	    sprintf(newdirpath, "%s/%d", OVERnewdir, index);
	    sprintf(newpath, "%s/%d/overview.new", OVERnewdir, index);
	    if ((fp = fopen(newpath, OVERnewmode)) == (FILE *)NULL &&
		(!MakeDirectory(newdirpath, TRUE) ||
		(fp = fopen(newpath, OVERnewmode)) == (FILE *)NULL)) {
		syslog(L_ERROR, "OVER cant open new overview file, line %d: %m", line);
		DISPOSE(dirpath);
		DISPOSE(newdirpath);
		DISPOSE(path);
		DISPOSE(newpath);
		return OVER_ERROR;
	    }
	} else {
	    sprintf(dirpath, "%s/%d", OVERdir, index);
	    sprintf(path, "%s/%d/overview", OVERdir, index);
	    if ((fp = fopen(path, OVERmode)) == (FILE *)NULL &&
		(!MakeDirectory(dirpath, TRUE) ||
		(fp = fopen(path, OVERmode)) == (FILE *)NULL)) {
		syslog(L_ERROR, "OVER cant open overview file, line %d: %m", line);
		DISPOSE(dirpath);
		DISPOSE(newdirpath);
		DISPOSE(path);
		DISPOSE(newpath);
		return OVER_ERROR;
	    }
	    if (OVERmmap) {
		if (fstat(fileno(fp), &sb) != 0) {
		    syslog(L_ERROR, "OVER cant stat overview file, line %d: %m", line);
		    DISPOSE(dirpath);
		    DISPOSE(newdirpath);
		    DISPOSE(path);
		    DISPOSE(newpath);
		    return OVER_ERROR;
		}
		if (sb.st_size > 0 && (int)(addr = (char *)mmap(0, sb.st_size, PROT_READ, MAP_SHARED, fileno(fp), 0)) == -1) {
		    syslog(L_ERROR, "OVER cant mmap overview file, line %d: %m", line);
		    DISPOSE(dirpath);
		    DISPOSE(newdirpath);
		    DISPOSE(path);
		    DISPOSE(newpath);
		    return OVER_ERROR;
		}
	    }
	}
	CloseOnExec(fileno(fp), 1);
	if (newconfig == (UNIOVER *)NULL) {
	    newconfig = NEW(UNIOVER, 1);
	    if (New) {
		newconfig->fp = (FILE *)NULL;
		newconfig->newfp = fp;
	    } else {
		newconfig->fp = fp;
		newconfig->newfp = (FILE *)NULL;
	    }
	    newconfig->index = index;
	    newconfig->numpatterns = i;
	    newconfig->patterns = NEW(char *, i);
	    newconfig->addr = addr;
	    newconfig->size = sb.st_size;
	    newconfig->offset = -1;
	    /* Store the patterns in reverse order like storage.ctl */
	    for (i--, p = strtok(patterns, ","); p != NULL; i--, p = strtok(NULL, ","))
	        newconfig->patterns[i] = COPY(p);
	} else {
	    if (New)
		newconfig->newfp = fp;
	    else
		newconfig->fp = fp;
	}

	if (prev)
	    prev->next = newconfig;
	else
	    OVERconfig = newconfig;
	prev = newconfig;
	newconfig->next = (UNIOVER *)NULL;
    }
    (void)fclose(f);
    DISPOSE(dirpath);
    DISPOSE(newdirpath);
    DISPOSE(path);
    DISPOSE(newpath);
    return OVER_DONE;
}

/*
** setup overview environment (directory etc.)
*/
BOOL OVERsetup(OVERSETUP type, void *value) {
    if (Initialized)
	return FALSE;
    switch (type) {
    case OVER_CTL:
	DISPOSE(OVERctl);
	if (value != (void *)NULL)
	    OVERctl = COPY((char *)value);
	else {
	    OVERctl = COPY(_PATH_OVERVIEWCTL);
	}
	break;
    case OVER_DIR:
	DISPOSE(OVERdir);
	OVERdir = COPY((char *)(value ? value : _PATH_OVERVIEWDIR));
	break;
    case OVER_NEWDIR:
	DISPOSE(OVERnewdir);
	OVERnewdir = COPY((char *)(value ? value : _PATH_OVERVIEWDIR));
	break;
    case OVER_MODE:
	DISPOSE(OVERmode);
	OVERmode = COPY((char *)(value ? value : DEFAULTMODE));
	break;
    case OVER_NEWMODE:
	DISPOSE(OVERnewmode);
	OVERnewmode = COPY((char *)(value ? value : DEFAULTMODE));
	break;
    case OVER_MMAP:
	OVERmmap = *(BOOL *)value;
	break;
    default:
	return FALSE;
    }
    return TRUE;
}

/*
** Calls the setup function for overview data and returns
** TRUE if they all initialize ok, FALSE if they don't
*/
BOOL OVERinit(void) {
    OVERCONFIG		status;

    if (Initialized)
	return TRUE;

    if (OVERctl == (char *)NULL) {
	OVERctl = COPY(_PATH_OVERVIEWCTL);
    }
    if (OVERdir == (char *)NULL) {
	OVERdir = COPY(_PATH_OVERVIEWDIR);
    }
    if (OVERnewdir == (char *)NULL) {
	OVERnewdir = COPY(_PATH_OVERVIEWDIR);
    }
    if (OVERmode == (char *)NULL) {
	OVERmode = COPY(DEFAULTMODE);
    }
    if (OVERnewmode == (char *)NULL) {
	OVERnewmode = COPY(DEFAULTMODE);
    }

    Initialized = TRUE;
    status = OVERreadconfig(FALSE);
    if (status == OVER_ERROR) {
	Initialized = FALSE;
	return FALSE;
    } else if (status == OVER_DONE && atexit(OVERshutdown) < 0) {
	OVERshutdown();
	Initialized = FALSE;
	return FALSE;
    }
    return TRUE;
}

/*
** Calls the setup function for the new overview data and returns
** TRUE if they all initialize ok, FALSE if they don't
** this is for expiry process
*/
BOOL OVERnewinit(void) {
    OVERCONFIG		status;

    if (Newfp)
	return TRUE;
    if (!Initialized)
	return FALSE;
    Newfp = TRUE;
    status = OVERreadconfig(TRUE);
    if (status == OVER_ERROR) {
	Newfp = FALSE;
	return FALSE;
    }
    return TRUE;
}

/*
** reopen all overview data except for new overview data and returns
** TRUE if they all initialize ok, FALSE if they don't
** this is used for processing last part of history and overview while paused
*/
BOOL OVERreinit(void) {
    UNIOVER *config;
    char *path;
    struct stat sb;

    if (!Initialized)
	return FALSE;
    path = NEW(char, strlen(OVERdir) + sizeof("/255") + sizeof("/overview") + 1);
    for(config=OVERconfig;config!=(UNIOVER *)NULL;config=config->next) {
	if (config->fp != (FILE *)NULL) {
	    if (OVERmmap)
		munmap(config->addr, config->size);
	    (void)fclose(config->fp);
	    sprintf(path, "%s/%d/overview", OVERdir, config->index);
	    if ((config->fp = fopen(path, OVERmode)) == (FILE *)NULL) {
		syslog(L_ERROR, "OVER cant reopen overview file, index %d: %m", config->index);
		DISPOSE(path);
	        return FALSE;
	    }
	    if (OVERmmap) {
		if (fstat(fileno(config->fp), &sb) != 0) {
		    syslog(L_ERROR, "OVER cant stat reopend overview file, index %d: %m", config->index);
		    DISPOSE(path);
	            return FALSE;
		}
		if (sb.st_size > 0 && (int)(config->addr = (char *)mmap(0, sb.st_size, PROT_READ, MAP_SHARED, fileno(config->fp), 0)) == -1) {
		    syslog(L_ERROR, "OVER cant mmap reopend overview file, index %d: %m", config->index);
		    DISPOSE(path);
	            return FALSE;
		}
	    }
	}
    }
    DISPOSE(path);
    return TRUE;
}

/*
** replace current overview data with new one
** if replaced successfully, newfp is closed
*/
BOOL OVERreplace(void) {
    char *path;
    char *newpath;
    UNIOVER *config;
    BOOL retval = TRUE;

    if (!Initialized || !Newfp)
	return FALSE;
    path = NEW(char, strlen(OVERdir) + sizeof("/255") + sizeof("/overview") + 1);
    newpath = NEW(char, strlen(OVERnewdir) + sizeof("/255") + sizeof("/overview.new") + 1);
    for(config=OVERconfig;config!=(UNIOVER *)NULL;config=config->next) {
	if (config->newfp != (FILE *)NULL) {
	    (void)fclose(config->newfp);
	    config->newfp = (FILE *)NULL;
	    sprintf(path, "%s/%d/overview", OVERdir, config->index);
	    sprintf(newpath, "%s/%d/overview.new", OVERnewdir, config->index);
	    if (config->fp != (FILE *)NULL) {
		(void)fclose(config->fp);
		config->fp = (FILE *)NULL;
	    }
	    if (rename(newpath, path) < 0) {
		syslog(L_ERROR, "OVER cant rename overview file, index %d: %m", config->index);
		DISPOSE(path);
		DISPOSE(newpath);
		return FALSE;
	    }
	    if ((config->fp = fopen(path, OVERmode ? OVERmode : DEFAULTMODE)) == (FILE *)NULL) {
		syslog(L_ERROR, "OVER cant reopen overview file, index %d: %m", config->index);
		DISPOSE(path);
		DISPOSE(newpath);
		return FALSE;
	    }
	}
    }
    Newfp = FALSE;
    DISPOSE(path);
    DISPOSE(newpath);
    return retval;
}

/*
** get the number of overview data
** return -1, if not initialized or overview.ctl has some error
*/
int OVERgetnum(void) {
    int			i;
    UNIOVER		*config;

    if (!Initialized && !OVERinit())
	return -1;
    for(i=0,config=OVERconfig;config!=(UNIOVER *)NULL;config=config->next,i++);
    return i;
}

/*
** assumes *g is Xref formatted
*/
static BOOL MatchGroups(const char *g, int num, char **patterns) {
    char                *group;
    char                *groups;
    const char          *p;
    int                 i;
    BOOL                wanted = FALSE;

    /* Find the end of the line */
    for (p = g; *p != '\0'; p++);

    groups = NEW(char, p - g);
    memcpy(groups, g, p - g);
    groups[p - g] = '\0';

    for (group = strtok(groups, " "); group != NULL; group = strtok(NULL, " ")) {
	if ((p = strchr(group, ':')) != (char *)NULL)
	    p = '\0';
	for (i = 0; i < num; i++) {
	    switch (patterns[i][0]) {
	    case '!':
		if (!wanted && wildmat(group, &patterns[i][1]))
		    break;
	    case '@':
		if (wildmat(group, &patterns[i][1])) {
		    DISPOSE(groups);
		    return FALSE;
		}
	    default:
		if (wildmat(group, patterns[i]))
		    wanted = TRUE;
	    }
	}
    }

    DISPOSE(groups);
    return wanted;
}

/*
** stores overview data based on Xref header
** if Newfp, it stores into newfp
*/
BOOL OVERstore(TOKEN *token, char *Overdata, int Overlen) {
    TOKEN               result;
    char                *Xref;
    UNIOVER		*config;
    static int		allocated = 0;
    int			offset;

    if (!Initialized)
	return FALSE;
    if (Overbuff != (char *)NULL && allocated < Overlen) {
	RENEW(Overbuff, char, Overlen+1);
	allocated = Overlen;
    } else if (Overbuff == (char *)NULL) {
	Overbuff = NEW(char, Overlen+1);
	allocated = Overlen;
    }
    memcpy(Overbuff, Overdata, Overlen);
    Overbuff[Overlen] = '\0';
    if ((Xref = strstr(Overbuff, "\tXref:")) == NULL) {
	token->index = OVER_NONE;
	token->cancelled = FALSE;
	return TRUE;
    }
    if ((Xref = strchr(Xref, ' ')) == NULL) {
	token->index = OVER_NONE;
	token->cancelled = FALSE;
	return TRUE;
    }
    for (Xref++; *Xref == ' '; Xref++);
    if ((Xref = strchr(Xref, ' ')) == NULL) { 
	token->index = OVER_NONE;
	token->cancelled = FALSE;
	return TRUE;
    }
    for (Xref++; *Xref == ' '; Xref++);
    for (config=OVERconfig;config!=NULL;config=config->next) {
	if (MatchGroups(Xref, config->numpatterns, config->patterns)) {
	    offset = ftell(Newfp ? config->newfp : config->fp);
	    if (fprintf(Newfp ? config->newfp : config->fp, "%s\n", Overbuff) == EOF || fflush(config->fp) == EOF) {
		syslog(L_ERROR, "OVER cant flush overview file, index %d: %m", config->index);
		return FALSE;
	    }
	    token->index = config->index;
	    token->offset = offset;
	    token->cancelled = FALSE;
	    break;
	}
    }
    if (config == NULL) {
	token->index = OVER_NONE;
	token->cancelled = FALSE;
    }
    return TRUE;
}

char *OVERretrieve(TOKEN *token, int *Overlen) {
    char *addr;
    char *p;
    static char *line = (char *)NULL;

    if (!Initialized || token->index == OVER_NONE)
	return (char *)NULL;
    if (OVERmmap) {
	if (OVERconfig[token->index].size <= token->offset || token->offset < 0)
	    return (char *)NULL;
	addr = OVERconfig[token->index].addr + token->offset;
	for (p = addr; p < OVERconfig[token->index].addr+OVERconfig[token->index].size; p++)
	    if ((*p == '\r') || (*p == '\n'))
		break;
	*Overlen = p - addr;
	return addr;
    } else {
	if (OVERconfig[token->index].offset != token->offset) {
	    fseek(OVERconfig[token->index].fp, token->offset, SEEK_SET);
	    OVERconfig[token->index].offset = token->offset;
	}
	if (line == (char *)NULL)
	    line = NEW(char, MAXOVERLINE);
	if (fgets(line, MAXOVERLINE, OVERconfig[token->index].fp) == NULL)
	    return NULL;
	if ((*Overlen = strlen(line)) < 10)
	    return (char *)NULL;
	if (*Overlen  >= MAXOVERLINE) {
	    OVERconfig[token->index].offset = -1;
	    *Overlen = MAXOVERLINE;
	} else {
	    OVERconfig[token->index].offset += *Overlen;
	}
	line[*Overlen-1] = '\0';
	return line;
    }
}

BOOL OVERcancel(TOKEN *token) {
    token->cancelled = TRUE;
    return TRUE;
}

void OVERshutdown(void) {
    int                 i;
    UNIOVER             *config;

    if (!Initialized)
	return;

    while (OVERconfig != (UNIOVER *)NULL) {
	config = OVERconfig;
	OVERconfig = OVERconfig->next;
	if (OVERmmap)
	    munmap(config->addr, config->size);
	if (config->fp != (FILE *)NULL)
	    (void)fclose(config->fp);
	if (config->newfp != (FILE *)NULL)
	    (void)fclose(config->newfp);
	DISPOSE(config->patterns);
	DISPOSE(config);
    }
    DISPOSE(Overbuff);
    Overbuff = (char *)NULL;
    Initialized = FALSE;
    Newfp = FALSE;
}
