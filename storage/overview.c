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
static int		Mode = 0;
static int		NewMode = 0;
static BOOL		OVERmmap = TRUE;
static BOOL		OVERbuffered = FALSE;
#define DEFAULTMODE	"a"
#define MAXOVERLINE	4096


/*
** get offset and overindex from token.
*/
void OVERsetoffset(TOKEN *token, OFFSET_T *offset, unsigned char *overindex)
{
    *overindex = token->index;
    *offset = token->offset;
}

/*
** make token.
*/
void OVERmaketoken(TOKEN *token, OFFSET_T offset, unsigned char overindex)
{
    token->index = overindex;
    token->offset = offset;
    token->cancelled = FALSE;
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
    int  fd;
    FILE *fp;
    char *dirpath;
    char *newdirpath;
    char *path;
    char *newpath;
    int pathlen;
    int newpathlen;
    struct stat sb;
    char *addr;

    if ((f = Fopen(OVERctl, "r", TEMPORARYOPEN)) == NULL) {
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
	    DISPOSE(dirpath);
	    DISPOSE(newdirpath);
	    DISPOSE(path);
	    DISPOSE(newpath);
	    (void)Fclose(f);
	    return OVER_ERROR;
	}

	newconfig = (UNIOVER *)NULL;
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
		    (void)Fclose(f);
		    return OVER_ERROR;
		}
	    }
	}
	addr = (char *)-1;
	sb.st_size = 0;
	if (New) {
	    sprintf(newdirpath, "%s/%d", OVERnewdir, index);
	    sprintf(newpath, "%s/%d/overview.new", OVERnewdir, index);
	    if ((fd = open(newpath, NewMode, 0666)) < 0 &&
		(!MakeDirectory(newdirpath, TRUE) ||
		(fd = open(newpath, NewMode, 0666)) < 0)) {
		syslog(L_ERROR, "OVER cant open new overview file, line %d: %m", line);
		DISPOSE(dirpath);
		DISPOSE(newdirpath);
		DISPOSE(path);
		DISPOSE(newpath);
		(void)Fclose(f);
		return OVER_ERROR;
	    }
	    if (NewMode == (O_WRONLY | O_APPEND | O_CREAT))
		lseek(fd, 0L, SEEK_END);
	} else {
	    sprintf(dirpath, "%s/%d", OVERdir, index);
	    sprintf(path, "%s/%d/overview", OVERdir, index);
	    if ((fd = open(path, Mode, 0666)) < 0 &&
		(!MakeDirectory(dirpath, TRUE) ||
		(fd = open(path, Mode, 0666)) < 0)) {
		syslog(L_ERROR, "OVER cant open overview file, line %d: %m", line);
		DISPOSE(dirpath);
		DISPOSE(newdirpath);
		DISPOSE(path);
		DISPOSE(newpath);
		(void)Fclose(f);
		return OVER_ERROR;
	    }
	    if (Mode == (O_WRONLY | O_APPEND | O_CREAT))
		lseek(fd, 0L, SEEK_END);
	    if (fstat(fd, &sb) != 0) {
		syslog(L_ERROR, "OVER cant stat overview file, line %d: %m", line);
		DISPOSE(dirpath);
		DISPOSE(newdirpath);
		DISPOSE(path);
		DISPOSE(newpath);
		(void)close(fd);
		(void)Fclose(f);
		return OVER_ERROR;
	    }
	    if (OVERmmap) {
		if (sb.st_size > 0 && (addr = (char *)mmap(0, sb.st_size, PROT_READ, MAP_SHARED, fd, 0)) == (MMAP_PTR)-1) {
		    syslog(L_ERROR, "OVER cant mmap overview file, line %d: %m", line);
		    DISPOSE(dirpath);
		    DISPOSE(newdirpath);
		    DISPOSE(path);
		    DISPOSE(newpath);
		    (void)close(fd);
		    (void)Fclose(f);
		    return OVER_ERROR;
		}
	    }
	}
	fp = (FILE *)NULL;
	if (OVERbuffered) {
	    if ((fp = fdopen(fd, New ? OVERnewmode : OVERmode)) == (FILE *)NULL) {
		syslog(L_ERROR, "OVER cant fdopen overview file, line %d: %m", line);
		DISPOSE(dirpath);
		DISPOSE(newdirpath);
		DISPOSE(path);
		DISPOSE(newpath);
		(void)close(fd);
		(void)Fclose(f);
		return OVER_ERROR;
	    }
	}
	CloseOnExec(fd, 1);
	if (newconfig == (UNIOVER *)NULL) {
	    newconfig = NEW(UNIOVER, 1);
	    if (New) {
		newconfig->fd = -1;
		newconfig->fp = (FILE *)NULL;
		newconfig->newfd = fd;
		newconfig->newfp = fp;
		newconfig->newsize = sb.st_size;
		newconfig->newoffset = lseek(fd, 0, SEEK_CUR);
	    } else {
		newconfig->fd = fd;
		newconfig->fp = fp;
		newconfig->newfd = -1;
		newconfig->newfp = (FILE *)NULL;
		newconfig->size = sb.st_size;
		newconfig->offset = lseek(fd, 0, SEEK_CUR);
	    }
	    newconfig->index = index;
	    newconfig->numpatterns = i;
	    newconfig->patterns = NEW(char *, i);
	    newconfig->addr = addr;
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
	    if (New) {
		newconfig->newfd = fd;
		newconfig->newfp = fp;
		newconfig->newsize = sb.st_size;
	    } else {
		newconfig->fd = fd;
		newconfig->fp = fp;
		newconfig->size = sb.st_size;
	    }
	}
    }
    (void)Fclose(f);
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
    path = NEW(char, strlen(OVERdir) + sizeof("/255") + sizeof("/overview") + 1);
    for(config=OVERconfig;config!=(UNIOVER *)NULL;config=config->next) {
	if (config->fp != (FILE *)NULL || config->fd >= 0) {
	    if (OVERmmap)
		munmap(config->addr, config->size);
	    if (config->fp != (FILE *)NULL)
		(void)fclose(config->fp);
	    else if (config->fd >= 0)
		(void)close(config->fd);
	    sprintf(path, "%s/%d/overview", OVERdir, config->index);
	    if ((config->fd = open(path, Mode, 0666)) < 0) {
		syslog(L_ERROR, "OVER cant reopen overview file, index %d: %m", config->index);
		DISPOSE(path);
	        return FALSE;
	    }
	    if (Mode == (O_WRONLY | O_APPEND | O_CREAT))
		lseek(config->fd, 0L, SEEK_END);
	    if (OVERmmap) {
		if (fstat(config->fd, &sb) != 0) {
		    syslog(L_ERROR, "OVER cant stat reopend overview file, index %d: %m", config->index);
		    DISPOSE(path);
	            return FALSE;
		}
		if (sb.st_size > 0 && (config->addr = (char *)mmap(0, sb.st_size, PROT_READ, MAP_SHARED, config->fd, 0)) == (MMAP_PTR)-1) {
		    syslog(L_ERROR, "OVER cant mmap reopend overview file, index %d: %m", config->index);
		    DISPOSE(path);
	            return FALSE;
		}
		config->size = sb.st_size;
	    }
	    if (OVERbuffered) {
		if ((config->fp = fdopen(config->fd, OVERmode)) == (FILE *)NULL) {
		    syslog(L_ERROR, "OVER cant fdopen reopend overview file, index %d: %m", config->index);
		    DISPOSE(path);
	            return FALSE;
		}
	    }
	    CloseOnExec(config->fd, 1);
	}
    }
    DISPOSE(path);
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
    BOOL retval = TRUE;

    if (!Initialized || !Newfp)
	return FALSE;
    path = NEW(char, strlen(OVERdir) + sizeof("/255") + sizeof("/overview") + 1);
    newpath = NEW(char, strlen(OVERnewdir) + sizeof("/255") + sizeof("/overview.new") + 1);
    for(config=OVERconfig;config!=(UNIOVER *)NULL;config=config->next) {
	if (config->newfp != (FILE *)NULL) {
	    (void)close(config->newfd);
	    config->newfd = -1;
	    sprintf(path, "%s/%d/overview", OVERdir, config->index);
	    sprintf(newpath, "%s/%d/overview.new", OVERnewdir, config->index);
	    if (config->fd > 0) {
		(void)close(config->fd);
		config->fd = -1;
	    }
	    if (rename(newpath, path) < 0) {
		syslog(L_ERROR, "OVER cant rename overview file, index %d: %m", config->index);
		DISPOSE(path);
		DISPOSE(newpath);
		return FALSE;
	    }
	    if ((config->fd = open(path, Mode, 0666)) < 0) {
		syslog(L_ERROR, "OVER cant reopen overview file, index %d: %m", config->index);
		DISPOSE(path);
		DISPOSE(newpath);
		return FALSE;
	    }
	    if (Mode == (O_WRONLY | O_APPEND | O_CREAT))
		lseek(config->fd, 0L, SEEK_END);
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
		OVERmaketoken(token, offset, config->index);
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
		OVERmaketoken(token, offset, config->index);
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
    if (OVERmmap) {
	if (config->size <= token->offset && !OVERreinit())
	    return (char *)NULL;
	if (config->size <= token->offset)
	    return (char *)NULL;
	addr = config->addr + token->offset;
	for (p = addr; p < config->addr+config->size; p++)
	    if ((*p == '\r') || (*p == '\n'))
		break;
	*Overlen = p - addr;
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
	if (config->fp != (FILE *)NULL) {
	    (void)fflush(config->fp);
	    (void)fclose(config->fp);
	} else if (config->fd >= 0)
	    (void)close(config->fd);
	if (config->newfp != (FILE *)NULL) {
	    (void)fflush(config->newfp);
	    (void)fclose(config->newfp);
	} else if (config->newfd >= 0)
	    (void)close(config->newfd);
	DISPOSE(config->patterns);
	DISPOSE(config);
    }
    DISPOSE(Overbuff);
    Overbuff = (char *)NULL;
    Initialized = FALSE;
    Newfp = FALSE;
}
