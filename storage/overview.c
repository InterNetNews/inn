/*  $Id$
**
**  unified overview processing
*/

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <ctype.h>
#include <fcntl.h>
#include <macros.h>
#include <configdata.h>
#include <clibrary.h>
#include <libinn.h>
#include <syslog.h> 
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
static int		Mode = 0;
static int		NewMode = 0;
static BOOL		OVERmmap = TRUE;
static BOOL		OVERbuffered = FALSE;
static BOOL		OVERpreopen = FALSE;
static OVERMMAP		OVERmmapconfig[MAXMMAPCONFIG];
#define DEFAULTMODE	"a"


/*
** get offset and overindex from token.
*/
void OVERsetoffset(TOKEN *token, OFFSET_T *offset, unsigned char *overindex, unsigned short *len)
{
    *overindex = token->index;
    *offset = token->offset;
    *len = token->overlen;
}

/*
** make token.
*/
void OVERmaketoken(TOKEN *token, OFFSET_T offset, unsigned char overindex, unsigned short len)
{
    token->index = overindex;
    token->offset = offset;
    token->cancelled = FALSE;
    token->overlen = len;
}

/*
** check mmap ovrerview file.
*/
STATIC BOOL OVERdommap(UNIOVER *config, OFFSET_T offset)
{
    int i, j, refcount = ~0;
    OFFSET_T pagefudge;

    pagefudge = offset % OVERPAGESIZE;
    config->mappedoffset = offset - pagefudge;
    if (config->mappedoffset > config->size - OVERMMAPLEN)
	config->len = config->size - config->mappedoffset;
    else
	config->len = OVERMMAPLEN;
    if ((config->base = (char *)mmap(0, config->len, PROT_READ, MAP_SHARED, config->fd, config->mappedoffset)) == (MMAP_PTR)-1) {
	config->mappedoffset = 0;
	config->len = 0;
	syslog(L_ERROR, "OVER cant mmap overview file index %d length %l, offset %l: %m", config->index, config->len, config->mappedoffset);
	return FALSE;
    }
    return TRUE;
}

/*
** check mmap ovrerview file.
*/
STATIC BOOL OVERcheckmmap(UNIOVER *config, OFFSET_T offset, unsigned short len)
{
    int i, j, refcount = ~0;
    OFFSET_T pagefudge;

    if ((config == (UNIOVER *)NULL) || (config->size == 0))
	return FALSE;
    for (i = 0 ; i < MAXMMAPCONFIG ; i++) {
	if (OVERmmapconfig[i].config == NULL)
	    break;
	if (OVERmmapconfig[i].config == config)
	    break;
	if (refcount < OVERmmapconfig[i].refcount) {
	    /* find least referenced config */
	    refcount = OVERmmapconfig[i].refcount;
	    j = i;
	}
    }
    if (i == MAXMMAPCONFIG) {
	/* all OVERMMAP is occupied, drop least used one */
	if (!(OVERmmapconfig[j].config->len == 0))
	    munmap(OVERmmapconfig[j].config->base, OVERmmapconfig[j].config->len);
	OVERmmapconfig[j].config->base = (char *)-1;
	OVERmmapconfig[j].config->len = 0;
	OVERmmapconfig[j].config->mappedoffset = 0;
	OVERmmapconfig[j].config = config;
	if (!OVERdommap(config, offset)) {
	    OVERmmapconfig[j].refcount = 0;
	    return FALSE;
	}
	OVERmmapconfig[j].refcount = 1;
	return TRUE;
    } else if (OVERmmapconfig[i].config == NULL) {
	/* not mmaped yet */
	OVERmmapconfig[i].config = config;
	if (!OVERdommap(config, offset)) {
	    OVERmmapconfig[i].refcount = 0;
	    return FALSE;
	}
	OVERmmapconfig[i].refcount = 1;
	return TRUE;
    } else {
	/* already mmaped */
	if (!(OVERmmapconfig[i].config->base == (char *)-1)) {
	    if (((OFFSET_T)OVERmmapconfig[i].config->mappedoffset < offset) &&
		(((OFFSET_T)OVERmmapconfig[i].config->mappedoffset - len) > (offset - OVERmmapconfig[i].config->len))) {
		/* within mmapped area */
		OVERmmapconfig[i].refcount++;
		return TRUE;
	    }
	    munmap(OVERmmapconfig[i].config->base, OVERmmapconfig[i].config->len);
	}
	if (!OVERdommap(config, offset)) {
	    OVERmmapconfig[i].refcount = 0;
	    return FALSE;
	}
	OVERmmapconfig[i].refcount++;
	return TRUE;
    }
}

/*
** Open and some other processing on overview files.
*/
STATIC BOOL OVERopen(UNIOVER *config, BOOL New, BOOL Newconfig)
{
    char *dirpath;
    char *newdirpath;
    char *path;
    char *newpath;
    int pathlen;
    int newpathlen;
    int fd;
    struct stat sb;
    FILE *fp;

    if (config == (UNIOVER *)NULL)
	return FALSE;
    pathlen = strlen(OVERdir);
    newpathlen = strlen(OVERnewdir);
    dirpath = NEW(char, pathlen + sizeof("/255") + 1);
    newdirpath = NEW(char, newpathlen + sizeof("/255") + 1);
    path = NEW(char, pathlen + sizeof("/255") + sizeof("/overview") + 1);
    newpath = NEW(char, newpathlen + sizeof("/255") + sizeof("/overview.new") + 1);
    sb.st_size = 0;
    if (New) {
	sprintf(newdirpath, "%s/%d", OVERnewdir, config->index);
	sprintf(newpath, "%s/%d/overview.new", OVERnewdir, config->index);
	if ((fd = open(newpath, NewMode, 0666)) < 0 &&
	    (!MakeDirectory(newdirpath, TRUE) ||
	    (fd = open(newpath, NewMode, 0666)) < 0)) {
	    syslog(L_ERROR, "OVER cant open new overview file: %m");
	    DISPOSE(dirpath);
	    DISPOSE(newdirpath);
	    DISPOSE(path);
	    DISPOSE(newpath);
	    return FALSE;
	}
	if (NewMode == (O_WRONLY | O_APPEND | O_CREAT))
	    lseek(fd, 0L, SEEK_END);
    } else {
	sprintf(dirpath, "%s/%d", OVERdir, config->index);
	sprintf(path, "%s/%d/overview", OVERdir, config->index);
	if ((fd = open(path, Mode, 0666)) < 0 &&
	    (!MakeDirectory(dirpath, TRUE) ||
	    (fd = open(path, Mode, 0666)) < 0)) {
	    syslog(L_ERROR, "OVER cant open overview file: %m");
	    DISPOSE(dirpath);
	    DISPOSE(newdirpath);
	    DISPOSE(path);
	    DISPOSE(newpath);
	    return FALSE;
	}
	if (Mode == (O_WRONLY | O_APPEND | O_CREAT))
	    lseek(fd, 0L, SEEK_END);
	if (fstat(fd, &sb) != 0) {
	    syslog(L_ERROR, "OVER cant stat overview file: %m");
	    DISPOSE(dirpath);
	    DISPOSE(newdirpath);
	    DISPOSE(path);
	    DISPOSE(newpath);
	    (void)close(fd);
	    return FALSE;
	}
    }
    fp = (FILE *)NULL;
    if (OVERbuffered) {
	if ((fp = fdopen(fd, New ? OVERnewmode : OVERmode)) == (FILE *)NULL) {
	    syslog(L_ERROR, "OVER cant fdopen overview file: %m");
	    DISPOSE(dirpath);
	    DISPOSE(newdirpath);
	    DISPOSE(path);
	    DISPOSE(newpath);
	    (void)close(fd);
	    return FALSE;
	}
    }
    CloseOnExec(fd, 1);
    if (Newconfig) {
	config->base = (char *)-1;
	config->len = 0;
	if (New) {
	    config->newfd = fd;
	    config->newfp = fp;
	    config->newsize = sb.st_size;
	    config->newoffset = lseek(fd, 0, SEEK_CUR);
	} else {
	    config->fd = fd;
	    config->fp = fp;
	    config->size = sb.st_size;
	    config->offset = lseek(fd, 0, SEEK_CUR);
	    if (OVERmmap) {
		if (!OVERcheckmmap(config, 0, 0)) {
		    DISPOSE(dirpath);
		    DISPOSE(newdirpath);
		    DISPOSE(path);
		    DISPOSE(newpath);
		    (void)close(fd);
		    return FALSE;
		}
	    }
	}
    } else {
	if (New) {
	    config->newfd = fd;
	    config->newfp = fp;
	    config->newsize = sb.st_size;
	} else {
	    config->fd = fd;
	    config->fp = fp;
	    config->size = sb.st_size;
	}
    }
    return TRUE;
}

/*
** Open and some other processing on overview files.
*/
STATIC void OVERclose(UNIOVER *config, BOOL New)
{
    if (config == (UNIOVER *)NULL)
	return;
    if (New) {
	if (config->newfp != (FILE *)NULL) {
	    (void)fflush(config->newfp);
	    (void)fclose(config->newfp);
	    config->newfp = (FILE *)NULL;
	} else if (config->newfd >= 0) {
	    (void)close(config->newfd);
	    config->newfd = -1;
	}
	config->size = 0;
	config->offset = 0;
    } else {
	if (config->base != (char *)-1) {
	    munmap(config->base, config->len);
	    config->base = (char *)-1;
	}
	if (config->fp != (FILE *)NULL) {
	    (void)fflush(config->fp);
	    (void)fclose(config->fp);
	    config->fp = (FILE *)NULL;
	} else if (config->fd >= 0) {
	    (void)close(config->fd);
	    config->fd = -1;
	}
	config->newsize = 0;
	config->newoffset = 0;
    }
    return;
}

/*
** Read overview.ctl to determine which overview file is to be written.
*/
STATIC OVERCONFIG OVERreadconfig(BOOL New)
{
    FILE *f;
    char buf[1024];
    int i, line = 0;
    char *p, *q;
    char *overindex;
    char *patterns;
    UNIOVER *config, *newconfig, *prev = (UNIOVER *)NULL;
    unsigned char index;

    if ((f = Fopen(OVERctl, "r", TEMPORARYOPEN)) == NULL) {
	syslog(L_ERROR, "OVER cannot open %s: %m", OVERctl);
	return OVER_NULL; 
    }

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
	    (void)Fclose(f);
	    return OVER_ERROR;
	}

	overindex = buf;
	*p = '\0';
	patterns = ++p;
	for (i = 1, p = patterns; *p && (p = strchr(p+1, ',')); i++);
	index = atoi(overindex);
	if (index >= OVER_NONE) {
	    syslog(L_ERROR, "OVER index is out of range, line %d: %m", line);
	    (void)Fclose(f);
	    return OVER_ERROR;
	}

	newconfig = (UNIOVER *)NULL;
	for(config=OVERconfig;config!=(UNIOVER *)NULL;config=config->next) {
	    if (config->index == index) {
		if (New) {
		    newconfig = config;
		} else {
		    syslog(L_ERROR, "OVER duplicate index, line %d", line);
		    (void)Fclose(f);
		    return OVER_ERROR;
		}
	    }
	}
	if (newconfig == (UNIOVER *)NULL) {
	    newconfig = NEW(UNIOVER, 1);
	    newconfig->index = index;
	    newconfig->fd = -1;
	    newconfig->fp = (FILE *)NULL;
	    newconfig->size = 0;
	    newconfig->offset = 0;
	    newconfig->newfd = -1;
	    newconfig->newfp = (FILE *)NULL;
	    newconfig->newsize = 0;
	    newconfig->newoffset = 0;
	    newconfig->base = (char *)-1;
	    if (OVERpreopen && !OVERopen(newconfig, New, TRUE)) {
		syslog(L_ERROR, "OVERopen failed, line %d", line);
		DISPOSE(newconfig);
		(void)Fclose(f);
		return OVER_ERROR;
	    }
	    newconfig->numpatterns = i;
	    newconfig->patterns = NEW(char *, i);
	    /* Store the patterns */
	    for (i = 0, p = strtok(patterns, ","); p != NULL; i++, p = strtok(NULL, ","))
	        newconfig->patterns[i] = COPY(p);
	    if (prev)
		prev->next = newconfig;
	    else
		OVERconfig = newconfig;
	    prev = newconfig;
	    newconfig->next = (UNIOVER *)NULL;
	} else {
	    if (OVERpreopen && !OVERopen(newconfig, New, FALSE)) {
		(void)Fclose(f);
		return OVER_ERROR;
	    }
	}
    }
    (void)Fclose(f);
    return OVER_DONE;
}

/*
** setup overview environment (directory etc.)
*/
BOOL OVERsetup(OVERSETUP type, void *value) {
    /* if innconf isn't already read in, do so. */
    if (innconf == NULL) {
	if (ReadInnConf() < 0) {
	    return FALSE;
	}
    }
    if (Initialized)
	return FALSE;
    switch (type) {
    case OVER_CTL:
	DISPOSE(OVERctl);
	if (value != (void *)NULL)
	    OVERctl = COPY((char *)value);
	else {
	    OVERctl = COPY(cpcatpath(innconf->pathetc, _PATH_OVERVIEWCTL));
	}
	break;
    case OVER_DIR:
	DISPOSE(OVERdir);
	OVERdir = COPY((char *)(value ? value : innconf->pathoverview));
	break;
    case OVER_NEWDIR:
	DISPOSE(OVERnewdir);
	OVERnewdir = COPY((char *)(value ? value : innconf->pathoverview));
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
    case OVER_BUFFERED:
	OVERbuffered = *(BOOL *)value;
	break;
    case OVER_PREOPEN:
	OVERpreopen = *(BOOL *)value;
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
    static BOOL		once = FALSE;

    /* if innconf isn't already read in, do so. */
    if (innconf == NULL) {
	if (ReadInnConf() < 0) {
	    return FALSE;
	}
    }
    if (Initialized)
	return TRUE;

    if (OVERctl == (char *)NULL) {
	OVERctl = COPY(cpcatpath(innconf->pathetc, _PATH_OVERVIEWCTL));
    }
    if (OVERdir == (char *)NULL) {
	OVERdir = COPY(innconf->pathoverview);
    }
    if (OVERnewdir == (char *)NULL) {
	OVERnewdir = COPY(innconf->pathoverview);
    }
    if (OVERmode == (char *)NULL) {
	OVERmode = COPY(DEFAULTMODE);
    }
    switch (OVERmode[0]) {
    default:
	return FALSE;
    case 'r':
	Mode = O_RDONLY;
	break;
    case 'w':
	Mode = O_WRONLY | O_TRUNC | O_CREAT;
	break;
    case 'a':
	Mode = O_WRONLY | O_APPEND | O_CREAT;
	break;
    }
    if (OVERmode[1] == '+' || (OVERmode[1] == 'b' && OVERmode[2] == '+')) {
	Mode &= ~(O_RDONLY | O_WRONLY);
	Mode |= O_RDWR;
    }
    if (OVERnewmode == 0) {
	OVERnewmode = DEFAULTMODE;
    }
    switch (OVERnewmode[0]) {
    default:
	return FALSE;
    case 'r':
	NewMode = O_RDONLY;
	break;
    case 'w':
	NewMode = O_WRONLY | O_TRUNC | O_CREAT;
	break;
    case 'a':
	NewMode = O_WRONLY | O_APPEND | O_CREAT;
	break;
    }
    if (OVERnewmode[1] == '+' || (OVERnewmode[1] == 'b' && OVERnewmode[2] == '+')) {
	NewMode &= ~(O_RDONLY | O_WRONLY);
	NewMode |= O_RDWR;
    }

    Initialized = TRUE;
    memset(OVERmmapconfig, '\0', sizeof(OVERMMAP) * MAXMMAPCONFIG);
    status = OVERreadconfig(FALSE);
    if (status == OVER_ERROR || (status == OVER_DONE && !once && atexit(OVERshutdown) < 0)) {
	OVERshutdown();
	Initialized = FALSE;
	return FALSE;
    }
    once = TRUE;
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
    for(config=OVERconfig;config!=(UNIOVER *)NULL;config=config->next) {
	if (config->fp != (FILE *)NULL || config->fd >= 0) {
	    OVERclose(config, FALSE);
	    if (OVERpreopen && !OVERopen(config, FALSE, TRUE)) {
		syslog(L_ERROR, "OVER cant reopen overview file, index %d", config->index);
                return FALSE;
            }
	}
    }
    memset(OVERmmapconfig, '\0', sizeof(OVERMMAP) * MAXMMAPCONFIG);
    return TRUE;
}

/*
** replace current overview data with new one
** if replaced successfully, newfp is closed
** OVERreplace assumes no subsequent operation for OVER*
*/
BOOL OVERreplace(void) {
    char *path;
    char *newpath;
    UNIOVER *config;

    if (!Initialized || !Newfp)
	return FALSE;
    path = NEW(char, strlen(OVERdir) + sizeof("/255") + sizeof("/overview") + 1);
    newpath = NEW(char, strlen(OVERnewdir) + sizeof("/255") + sizeof("/overview.new") + 1);
    for(config=OVERconfig;config!=(UNIOVER *)NULL;config=config->next) {
	if (config->newfp != (FILE *)NULL) {
	    OVERclose(config, TRUE);
	    OVERclose(config, FALSE);
	    sprintf(path, "%s/%d/overview", OVERdir, config->index);
	    sprintf(newpath, "%s/%d/overview.new", OVERnewdir, config->index);
	    if (rename(newpath, path) < 0) {
		syslog(L_ERROR, "OVER cant rename overview file, index %d: %m", config->index);
		DISPOSE(path);
		DISPOSE(newpath);
		return FALSE;
	    }
	    if (OVERpreopen && !OVERopen(config, FALSE, TRUE)) {
		syslog(L_ERROR, "OVER cant reopen overview file, index %d",config->index);
		DISPOSE(path);
		DISPOSE(newpath);
		return FALSE;
	    }
	}
    }
    Newfp = FALSE;
    memset(OVERmmapconfig, '\0', sizeof(OVERMMAP) * MAXMMAPCONFIG);
    DISPOSE(path);
    DISPOSE(newpath);
    return TRUE;
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
    char		*q;
    int                 i;
    BOOL                wanted = FALSE;

    /* Find the end of the line */
    for (p = g; (*p != '\0') && (*p != '\t'); p++);

    groups = NEW(char, p - g + 1);
    memcpy(groups, g, p - g);
    groups[p - g] = '\0';

    for (group = strtok(groups, " "); group != NULL; group = strtok(NULL, " ")) {
	if ((q = strchr(group, ':')) != (char *)NULL)
	    *q = '\0';
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
    OFFSET_T		offset;

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
	    if ((config->fp == (FILE *)NULL) && (config->fd < 0)) {
		if (!OVERopen(config, FALSE, TRUE)) {
		    token->index = OVER_NONE;
		    token->cancelled = FALSE;
		    return TRUE;
		}
	    }
	    if (Newfp) {
		offset = config->newsize;
		if (config->newfp != (FILE *)NULL) {
		    if (fprintf(config->newfp, "%s\n", Overbuff) == EOF) {
			syslog(L_ERROR, "OVER cant write overview file, index %d: %m", config->index);
			return FALSE;
		    }
		} else {
		    Overbuff[Overlen] = '\n';
		    if (write(config->newfd, Overbuff, Overlen + 1) != Overlen + 1) {
			syslog(L_ERROR, "OVER cant write overview file, index %d: %m", config->index);
			return FALSE;
		    }
		}
		if (Overlen > 0xffff)
		    OVERmaketoken(token, offset, config->index, 0);
		else
		    OVERmaketoken(token, offset, config->index, (unsigned short)Overlen);
		config->newsize += Overlen + 1;
	    } else {
		offset = config->size;
		if (config->fp != (FILE *)NULL) {
		    if (fprintf(config->fp, "%s\n", Overbuff) == EOF) {
			syslog(L_ERROR, "OVER cant flush overview file, index %d: %m", config->index);
			return FALSE;
		    }
		} else {
		    Overbuff[Overlen] = '\n';
		    if (write(config->fd, Overbuff, Overlen + 1) != Overlen + 1) {
			syslog(L_ERROR, "OVER cant write overview file, index %d: %m", config->index);
			return FALSE;
		    }
		}
		if (Overlen > 0xffff)
		    OVERmaketoken(token, offset, config->index, 0);
		else
		    OVERmaketoken(token, offset, config->index, (unsigned short)Overlen);
		config->size += Overlen + 1;
	    }
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
    UNIOVER		*config;
    int  size, i;

    if (!Initialized || token->index == OVER_NONE)
	return (char *)NULL;
    for (config=OVERconfig;config!=NULL;config=config->next) {
	if (config->index == token->index)
	    break;
    }
    if (config == NULL)
	return (char *)NULL;
    if ((config->fp == (FILE *)NULL) && (config->fd < 0)) {
	if (!OVERopen(config, FALSE, TRUE)) {
	    return (char *)NULL;
	}
    }
    if (OVERmmap) {
	if (config->size <= token->offset && !OVERreinit())
	    return (char *)NULL;
	if (config->size <= token->offset)
	    return (char *)NULL;
	if (!OVERcheckmmap(config, (OFFSET_T)token->offset, token->overlen))
	    return (char *)NULL;
	addr = config->base + token->offset - config->mappedoffset;
	if (token->overlen > 0) {
	    *Overlen = token->overlen;
	} else {
	    for (p = addr; p < config->base+config->size; p++)
	        if ((*p == '\r') || (*p == '\n'))
		    break;
	    *Overlen = p - addr;
	}
	return addr;
    } else {
	if (config->offset != token->offset) {
	    if (config->fp != (FILE *)NULL) {
		if (fseek(config->fp, token->offset, SEEK_SET) == -1)
		    return (char *)NULL;
	    } else {
		if (lseek(config->fd, token->offset, SEEK_SET) == -1)
		    return (char *)NULL;
	    }
	    config->offset = token->offset;
	}
	if (line == (char *)NULL)
	    line = NEW(char, MAXOVERLINE);
	line[MAXOVERLINE - 1] = '\0';
	if (config->fp != (FILE *)NULL) {
	    if (token->overlen > 0) {
		if (fgets(line, token->overlen, config->fp) == NULL)
		    return NULL;
		line[token->overlen] = '\0';
		config->offset += token->overlen;
		*Overlen = token->overlen;
	    } else {
		if (fgets(line, MAXOVERLINE, config->fp) == NULL)
		    return NULL;
		if ((*Overlen = strlen(line)) < 10)
		    return (char *)NULL;
		if (*Overlen  >= MAXOVERLINE - 1) {
		    config->offset = ftell(config->fp);
		    *Overlen = MAXOVERLINE - 1;
		    line[*Overlen-1] = '\0';
		} else {
		    config->offset += *Overlen;
		    if (line[*Overlen-1] == '\n') {
			line[*Overlen-1] = '\0';
			*Overlen--;
		    }
		}
	    }
	} else {
	    if (token->overlen > 0) {
		if ((size = read(config->fd, line, token->overlen)) < 0)
		    return NULL;
		line[token->overlen] = '\0';
		config->offset += token->overlen;
		*Overlen = token->overlen;
	    } else {
		if ((size = read(config->fd, line, MAXOVERLINE)) < 0)
		    return NULL;
		for (i=0;i<size;i++) {
		    if (line[i] == '\n') {
			line[i] = '\0';
			break;
		    }
		    if (line[i] == '\0') {
			break;
		    }
		}
		if ((*Overlen = i) < 9 )
		    return (char *)NULL;
		if (*Overlen  >= MAXOVERLINE - 1) {
		    config->offset = lseek(config->fd, 0, SEEK_CUR);
		    *Overlen = MAXOVERLINE - 1;
		    line[*Overlen-1] = '\0';
		} else {
		    config->offset += MAXOVERLINE;
		}
	    }
	}
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
	OVERclose(config, TRUE);
	OVERclose(config, FALSE);
	DISPOSE(config->patterns);
	DISPOSE(config);
    }
    DISPOSE(Overbuff);
    memset(OVERmmapconfig, '\0', sizeof(OVERMMAP) * MAXMMAPCONFIG);
    Overbuff = (char *)NULL;
    Initialized = FALSE;
    Newfp = FALSE;
}
