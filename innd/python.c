/*
**  $Id$
**
**  python.c: Embed Python in the style of innd's TCL and Perl stuff.
** 
**  Written by G.J. Andruk <meowing@banet.net> patterned after
**  TCL/Perl work by Bob Heiney and Christophe Wolfhugel and a whole
**  bunch of other people mentioned in the docs and sources for the
**  other filters.
**
**  The astute reader may notice the commission of blatant atrocities
**  against Python's OO model here.  Don't tell Guido.
*/

#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>
#include "configdata.h"
#include "clibrary.h"
#include "innd.h"
#include "art.h"
#include "qio.h"


#if defined(DO_PYTHON)

#include "Python.h"


BOOL		PythonFilterActive;
char		*filterPath;	/* this gets set in art.c */
PyObject	*PYFilterObject = NULL;
PyObject	*PYFilterModule = NULL;

/* article filter bits and pieces */
ARTHEADER	ARTheaders[], *ARTheadersENDOF;
PyObject	*PYheaders = NULL;
PyObject	**PYheaditem;
PyObject	**PYheadkey;
PyObject	*PYpathkey, *PYlineskey, *PYbodykey;

/* external functions */
PyObject	*msgid_method = NULL;
PyObject	*art_method = NULL;
PyObject	*mode_method = NULL;
PyObject	*pre_reload_method = NULL;
PyObject	*close_method = NULL;



/*
**  Turn filtering on or off.
*/
void
PYfilter(value)
    BOOL value;
{
    PythonFilterActive = value;
    syslog(L_NOTICE, "%s Python filtering %s", LogName,
	   PythonFilterActive ? "enabled" : "disabled");
}



/*
**  Front end for PYfilter()
*/
STRING
PYcontrol(av)
    char **av;
{
    char        *p;
    extern BOOL PythonFilterActive;

    switch (av[0][0]) {
    default:
        return "1 Bad flag";
    case 'y':
        if (PythonFilterActive)
            return "1 Python filter already enabled";
        else if (PYFilterObject == NULL)
            return "1 Python filter not defined" ;
        PYfilter(TRUE);
        break;
    case 'n':
        if (!PythonFilterActive)
            return "1 Python filter already disabled";
        PYfilter(FALSE);
        break;
    }
    return NULL;
}




/*
**  Reject articles we don't like.
*/
char *
PYartfilter(artBody, artLen, lines)
    char* artBody;
    long artLen;
    int lines;
{
    ARTHEADER	*hp;
    int		hdrnum;
    int		i;
    char	*p, save;
    static char buf[256];
    PyObject	*result;

    if (!PythonFilterActive || PYFilterObject == NULL || art_method == NULL)
	return NULL;

    /* Add headers to the dictionary... */
    hdrnum = 0;
    for (hp = ARTheaders; hp < ARTheadersENDOF; hp++) {
	if (hp->Found && hp->Value && !EQ(hp->Name,"Path"))
	    PYheaditem[hdrnum] = PyBuffer_FromMemory(hp->Value, hp->Length);
	else
	    PYheaditem[hdrnum] = Py_None;
	PyDict_SetItem(PYheaders, PYheadkey[hdrnum], PYheaditem[hdrnum]);
	hdrnum++;
    }

    /* ...then the path, done separately to get the info innd adds... */
    if (filterPath != NULL) {
	p = strpbrk(filterPath,"\r\n");
	if (p) {
	    save = *p;
	    *p = '\0';
	}
	PYheaditem[hdrnum] = PyBuffer_FromMemory(filterPath,
						 strlen(filterPath));
	PyDict_SetItem(PYheaders, PYpathkey, PYheaditem[hdrnum++]);
	if (p)
	    *p = save;
    }

    /* ...then the body... */
    if (artLen && artBody != NULL)
        PYheaditem[hdrnum] = PyBuffer_FromMemory(artBody, --artLen);
    else
        PYheaditem[hdrnum] = Py_None;
    PyDict_SetItem(PYheaders, PYbodykey, PYheaditem[hdrnum++]);

    /* ...and finally, the line count. */
    PYheaditem[hdrnum] = PyInt_FromLong((long) lines);
    PyDict_SetItem(PYheaders, PYlineskey, PYheaditem[hdrnum++]);

    /* Now see if the filter likes it. */
    result = PyObject_CallFunction(art_method, "O", PYheaders);
    if ((result != NULL) && PyObject_IsTrue(result))
	strncpy(buf, PyString_AS_STRING(result), sizeof(buf) - 1);
    else
	*buf = '\0';
    Py_XDECREF(result);

    /* Clean up after ourselves */
    PyDict_Clear(PYheaders);
    for (i = 0; i < hdrnum; i++)
	if (PYheaditem[i] != Py_None)
	    Py_DECREF(PYheaditem[i]);

    if (*buf != '\0') 
	return buf;
    return NULL;
}



/*
**  Refuse message IDs offered thru CHECK or IHAVE that we don't like.
*/
char *
PYmidfilter(messageID, msglen)
    char *messageID;
    int msglen;
{
    static char		buf[256];
    PyObject		*result;

    if (!PythonFilterActive || PYFilterObject == NULL || msgid_method == NULL)
	return NULL;

    result = PyObject_CallFunction(msgid_method, "s#", messageID, msglen);
    if ((result != NULL) && PyObject_IsTrue(result))
	strncpy(buf,  PyString_AS_STRING(result), sizeof(buf) - 1);
    else
	*buf = '\0';
    Py_XDECREF(result);

    if (*buf != '\0') 
	return buf;
    return NULL;
}



/*
**  Tell the external module about innd's state.
*/
void
PYmode(Mode, NewMode, reason)
    OPERATINGMODE Mode, NewMode;
    char *reason;
{
    PyObject	*result;
    char	oldmode[10], newmode[10];

    if (!PythonFilterActive || PYFilterObject == NULL || mode_method == NULL)
	return;

    switch (Mode) {
    default:		strcpy(oldmode, "unknown");	break;
    case OMrunning:	strcpy(oldmode, "running");	break;
    case OMpaused:	strcpy(oldmode, "paused");	break;
    case OMthrottled:	strcpy(oldmode, "throttled");	break;
    }

    switch (NewMode) {
    default:		strcpy(newmode, "unknown");	break;
    case OMrunning:	strcpy(newmode, "running");	break;
    case OMpaused:	strcpy(newmode, "paused");	break;
    case OMthrottled:	strcpy(newmode, "throttled");	break;
    }

    result = PyObject_CallFunction(mode_method, "sss",
				   oldmode, newmode, reason);
    Py_DECREF(result);
}



/*
**  Called by the external module so it can register itself with innd.
*/
static PyObject *
PY_set_filter_hook(dummy, args)
    PyObject *dummy, *args;
{
    PyObject	*result = NULL;
    PyObject	*temp;

    if (PyArg_ParseTuple(args, "O:set_filter_hook", &temp)) {
	Py_XINCREF(temp);
	Py_XDECREF(PYFilterObject);
	PYFilterObject = temp;
	Py_INCREF(Py_None);
	result = Py_None;
    }
    return result;
}



/*
**  Allow external module to ask innd if an ID is in history.
*/
static PyObject *
PY_havehist(self, args)
    PyObject *self, *args;
{
    char	*msgid;
    int		msgidlen;

    if (!PyArg_ParseTuple(args, "s#", &msgid, &msgidlen))
	return NULL;

    if (HIShavearticle(HashMessageID(msgid)))
	return PyInt_FromLong(1);
    return PyInt_FromLong(0);
}



/*
**  Allow external module to locally delete an article.
*/
static PyObject *
PY_cancel(self, args)
    PyObject *self, *args;
{
    char	*msgid;
    int		msgidlen;
    char	*parambuf[2];

    if (!PyArg_ParseTuple(args, "s#", &msgid, &msgidlen))
	return NULL;

    parambuf[0]= msgid;
    parambuf[1]= 0;

    if (!CCcancel(parambuf))
	return PyInt_FromLong(1);
    return PyInt_FromLong(0);
}



/*
**  Stuff an ID into history so that it will be refused later.
*/
static PyObject *
PY_addhist(self, args)
    PyObject *self, *args;
{
    char	*msgid;
    int		msgidlen;
    char	*articlepaths = "";
    char	tbuff[12];
    char	*parambuf[6];

    if (!PyArg_ParseTuple(args, "s#", &msgid, &msgidlen))
	return NULL;

    sprintf(tbuff, "%d",time((long *)0));

    parambuf[0] = msgid;
    parambuf[1] = parambuf[2] = parambuf[3] = tbuff;
    parambuf[4] = articlepaths;
    parambuf[5] = 0;

    if (!CCaddhist(parambuf))
	return PyInt_FromLong(1);
    return PyInt_FromLong(0);
}



/*
**  Get a newsgroup's status flag (j, m, n, x, y, =other.group)
*/
static PyObject *
PY_newsgroup(self, args)
    PyObject *self, *args;
{
    char	*newsgroup;
    int		nglen;
    NEWSGROUP	*ngp;
    char	*end;
    char	*rest;
    int		size;

    if (!PyArg_ParseTuple(args, "s#", &newsgroup, &nglen))
	return NULL;

    ngp = NGfind(newsgroup);
    if (ngp == NULL)
	return PyString_FromStringAndSize(NULL, 0);

    /* ngp->Rest is newline-terminated; find the end. */
    end = strchr(ngp->Rest, '\n');
    if (end == NULL)
	size = strlen(ngp->Rest);
    else
	size = end - ngp->Rest;

    /* If an alias is longer than this, active is probably broken. */
    if (size > MAXHEADERSIZE) {
	syslog(L_ERROR, "too-long flag field in active for %s", newsgroup);
	size = MAXHEADERSIZE;
    }

    return PyString_FromStringAndSize(ngp->Rest, size);
}



/*
**  Return an article header to the external module as a string.  We
**  don't use a buffer object here because that would make it harder,
**  for example, to compare two on-spool articles.
*/
static PyObject *
PY_head(self, args)
    PyObject *self, *args;
{
    char	*msgid;
    int		msgidlen;
    char	*p;
    TOKEN	*token;
    ARTHANDLE	*art;
    PyObject	*header;
    int		headerlen;

    if (!PyArg_ParseTuple(args, "s#", &msgid, &msgidlen))
	return NULL;

    /* Get the article filenames/token; open the first */
    if ((token = HISfilesfor(HashMessageID(msgid))) == NULL)
	return Py_BuildValue("s", "");	
    if ((art = SMretrieve(*token, RETR_HEAD)) == NULL)
	return Py_BuildValue("s", "");	
    p = FromWireFmt(art->data, art->len, &headerlen);
    SMfreearticle(art);
    header = PyString_FromStringAndSize(p, headerlen);
    DISPOSE(p);

    return header;
}



/*
**  Return a whole article to the external module as a string.
*/
static PyObject *
PY_article(self, args)
    PyObject *self, *args;
{
    char	*msgid;
    int		msgidlen;
    char	*p;
    TOKEN	*token;
    ARTHANDLE	*arth;
    PyObject	*art;
    int		artlen;

    if (!PyArg_ParseTuple(args, "s#", &msgid, &msgidlen))
	return NULL;

    /* Get the article filenames; open the first file */
    if ((token = HISfilesfor(HashMessageID(msgid))) == NULL)
	return Py_BuildValue("s", "");
    if ((arth = SMretrieve(*token, RETR_ALL)) == NULL)
	return Py_BuildValue("s", "");	
    p = FromWireFmt(arth->data, arth->len, &artlen);
    SMfreearticle(arth);
    art = PyString_FromStringAndSize(p, artlen);
    DISPOSE(p);

    return art;
}



/*
**  Python's syslog module isn't compiled in by default.  It's easier
**  to do it this way, and the switch block looks pretty in a color
**  editor).
*/
static PyObject *
PY_syslog(self, args)
    PyObject *self, *args;
{
    char	*loglevel;
    int		levellen;
    char	*logmsg;
    int		msglen;
    int		priority;

    if (!PyArg_ParseTuple(args, "s#s#",
			  &loglevel, &levellen, &logmsg, &msglen))
	return NULL;

    switch (*loglevel) {
    default:		priority = LOG_NOTICE ;
    case 'd': case 'D': priority = LOG_DEBUG ;		break;
    case 'i': case 'I': priority = LOG_INFO ;		break;
    case 'n': case 'N': priority = LOG_NOTICE ;		break;
    case 'w': case 'W': priority = LOG_WARNING ;	break;
    case 'e': case 'E': priority = LOG_ERR ;		break;
    case 'c': case 'C': priority = LOG_CRIT ;		break;
    case 'a': case 'A': priority = LOG_ALERT ;		break;
    }

    syslog(priority, "python: %s", logmsg);

    Py_INCREF(Py_None);
    return Py_None;
}



/*
**  Compute a hash digest for a string.
*/
static PyObject *
PY_hashstring(self, args)
    PyObject *self, *args;
{
    char	*instring, *wpos, *p, *q;
    char	*workstring = NULL;
    int		insize, worksize, newsize, i, wasspace;
    int		lines = 0;
    HASH	myhash;

    if (!PyArg_ParseTuple(args, "s#|i", &instring, &insize, &lines))
	return NULL;

    /* If a linecount is provided, munge before hashing. */
    if (lines > 0) {
	worksize = insize;

	/* chop leading whitespace */
	for (p=instring ; worksize>0 && isspace(*p) ; p++) {
	    if (*p == '\n')
		lines--;
	    worksize--;
	}
	wpos = p;

	/* and trailing */
	for (p=&wpos[worksize] ; worksize>0 && isspace(*p) ; p--) {
	    if (*p == '\n')
		lines--;
	    worksize--;
	}

	/* chop last 3 lines if we have >= 5.  From above chop the
	 * last line has no CR so we use 1 less here. */
	if (lines >= 4) {
	    for (i=0, p=wpos+worksize ; i<2 ; p--)
		if (*p == '\n')
		    i++;
	    worksize = p - wpos;
	}

	/* Compress out multiple whitespace in the trimmed string.  We
	 * do a copy because this is probably an original art
	 * buffer. */
	workstring =  memcpy(NEW(char, worksize), wpos, worksize);
	newsize = wasspace = 0;
	p = wpos;
	q = workstring;
	for (i=0 ; i<worksize ; i++) {
	    if (isspace(*p)) {
		if (!wasspace)
		    *q++ = ' ';
		wasspace = 1;
	    }
	    else {
		*q++ = tolower(*p);
		wasspace = 0;
	    }
	    p++;
	}
	worksize = q - workstring;
	myhash = Hash(workstring, worksize);
	DISPOSE(workstring);
    }
    else
	myhash = Hash(instring, insize);

    return PyString_FromStringAndSize((const char *)&myhash, sizeof(myhash));
}



/*
**  Make the internal INN module's functions visible to Python.
*/
static PyMethodDef INNPyMethods[] = {
    {"set_filter_hook", PY_set_filter_hook,	METH_VARARGS},
    {"havehist",	PY_havehist,		METH_VARARGS},
    {"addhist",		PY_addhist,		METH_VARARGS},
    {"cancel",		PY_cancel,		METH_VARARGS},
    {"newsgroup",	PY_newsgroup,		METH_VARARGS},
    {"head",		PY_head,		METH_VARARGS},
    {"article",		PY_article,		METH_VARARGS},
    {"syslog",		PY_syslog,		METH_VARARGS},
    {"hashstring",	PY_hashstring,		METH_VARARGS},
    {NULL,		NULL}
};



/*
**  This runs when innd shuts down.
*/
void
PYclose(void)
{
    PyObject	*result;

    if (close_method != NULL) {
	result = PyObject_CallFunction(close_method, NULL);
	Py_XDECREF(result);
    }
}



/*
**  Check that a method exists and is callable.	 Set a pointer to
**  the corresponding PyObject, or NULL if not found.
*/
void
PYdefonemethod(methptr, methname)
    PyObject	**methptr;
    char	*methname;
{
    Py_XDECREF(*methptr);
    *methptr = PyObject_GetAttrString(PYFilterObject, methname);
    if (*methptr == NULL)
	syslog(L_NOTICE, "python method %s not found", methname);
    else if (PyCallable_Check(*methptr) == 0) {
	syslog(L_ERROR, "python object %s found but not a function", methname);
	Py_DECREF(*methptr);
	*methptr = NULL;
    }
}



/*
**  Look up the filter methods, so we will know what's available when
**  innd wants to call them.
*/
void
PYdefmethods(void)
{
    PYdefonemethod(&msgid_method, "filter_messageid");
    PYdefonemethod(&art_method, "filter_art");
    PYdefonemethod(&mode_method, "filter_mode");
    PYdefonemethod(&pre_reload_method, "filter_before_reload");
    PYdefonemethod(&close_method, "filter_close");
}



/*
**  Used by `ctlinnd reload filter.python'
*/
int
PYreadfilter(void)
{
    PyObject	*newmodule = NULL;
    PyObject	*result;

    if (!Py_IsInitialized()) {
	syslog(L_ERROR, "python is not initialized");
	return 0;
    }

    /* If there is a filter running, let it clean up first. */
    if (pre_reload_method != NULL) {
	result = PyObject_CallFunction(pre_reload_method, NULL);
	Py_XDECREF(result);
    }

    if ((newmodule = PyImport_ReloadModule(PYFilterModule)) == NULL) {
	syslog(L_ERROR, "cant reload python filter module");
	PYfilter(FALSE);
	return 0;
    }
    Py_DECREF(PYFilterModule);
    PYFilterModule = newmodule;
    if (PYFilterObject == NULL) {
	syslog(L_ERROR, "python reload error, filter object not defined");
	PYfilter(FALSE);
	return 0;
    }
    PYfilter(TRUE);
    PYdefmethods();
    return 1;
}



/*
**  Called when innd first starts -- this gets the filters hooked in.
*/
void
PYsetup(void)
{
    ARTHEADER	*hp;
    int		hdrindex;

    setenv("PYTHONPATH", innconf->pathfilter, 1);
    Py_Initialize();

    /* It makes Python sad when its stdout and stderr are closed. */
    if ((fileno(stdout) == -1) || (fileno(stderr) == -1))
	PyRun_SimpleString
	    ("import sys; sys.stdout=sys.stderr=open('/dev/null', 'a')");

    if (!Py_IsInitialized ()) {
	syslog(L_ERROR, "python interpreter NOT initialized");
	return;
    }
    syslog(L_NOTICE, "python interpreter initialized OK");

    (void) Py_InitModule("INN", INNPyMethods);

    PYFilterModule = PyImport_ImportModule(_PATH_PYTHON_STARTUP_M);
    if (PYFilterModule == NULL)
	syslog(L_ERROR, "failed to import external python module");

    if (PYFilterObject == NULL) {
	syslog(L_ERROR, "python filter object is not defined");
	PYfilter(FALSE);
    } else {
	PYfilter(TRUE);
	PYdefmethods();
	syslog(L_NOTICE, "defined python methods");
    }

    /* Grab space for these so we aren't forever recreating them. */
    PYheaders = PyDict_New();
    PYheaditem = NEW(PyObject *, ARTheadersENDOF - ARTheaders);
    PYheadkey = NEW(PyObject *, ARTheadersENDOF - ARTheaders);

    /* Preallocate keys for the article dictionary */
    for (hp = ARTheaders; hp < ARTheadersENDOF; hp++)
	PYheadkey[hp - ARTheaders] = PyString_InternFromString(hp->Name);
    PYpathkey = PyString_InternFromString("Path");
    PYlineskey = PyString_InternFromString("__LINES__");
    PYbodykey = PyString_InternFromString("__BODY__");
}

#endif /* defined(DO_PYTHON) */
