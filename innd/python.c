/*  $Id$
**
**  Embed Python in the style of innd's Perl stuff.
**
**  Written by G.J. Andruk <meowing@banet.net> patterned after Tcl/Perl work
**  by Bob Heiney and Christophe Wolfhugel and a whole bunch of other people
**  mentioned in the docs and sources for the other filters.
**
**  The astute reader may notice the commission of blatant atrocities against
**  Python's OO model here.  Don't tell Guido.
**
**  A quick note regarding Python exceptions:  functions like
**      PyObject_GetAttrString(PyObject *o, const char *attr_name)
**  raise an exception when they fail, even though they return NULL.
**  And as exceptions accumulate from caller to caller and so on,
**  it generates weird issues with Python scripts afterwards.  So such
**  uses should be checked before.  For instance with:
**      PyObject_HasAttrString(PyObject *o, const char *attr_name).
*/

#include "config.h"

#ifdef DO_PYTHON

/*  Python redefines _POSIX_C_SOURCE, so undef it to suppress warnings. */
#undef _POSIX_C_SOURCE

/*  Make "s#" use Py_ssize_t rather than int. */
#define PY_SSIZE_T_CLEAN

/*  Python.h must be included after having defined PY_SSIZE_T_CLEAN,
 *  and before any standard headers are included (because Python may
 *  define some pre-processor definitions which affect the standard
 *  headers on some systems). */
#include "Python.h"

/*  Define Py_ssize_t when it does not exist (Python < 2.5.0). */
#if PY_VERSION_HEX < 0x02050000
  typedef int Py_ssize_t;
#endif

#include "clibrary.h"

#include "inn/innconf.h"
#include "inn/wire.h"
#include "innd.h"

bool		PythonFilterActive;
char		*filterPath;	/* This gets set in art.c. */
PyObject	*PYFilterObject = NULL;
PyObject	*PYFilterModule = NULL;

/*  Article filter bits and pieces. */
PyObject	*PYheaders = NULL;
PyObject	**PYheaditem;
PyObject	**PYheadkey;
PyObject	*PYpathkey, *PYlineskey, *PYbodykey;

/*  External functions. */
PyObject	*msgid_method = NULL;
PyObject	*art_method = NULL;
PyObject	*mode_method = NULL;
PyObject	*pre_reload_method = NULL;
PyObject	*close_method = NULL;



/*
**  Turn filtering on or off.
*/
void
PYfilter(bool value)
{
    PythonFilterActive = value;
    syslog(L_NOTICE, "%s Python filtering %s", LogName,
	   PythonFilterActive ? "enabled" : "disabled");
}



/*
**  Front end for PYfilter().
*/
const char *
PYcontrol(char **av)
{
    switch (av[0][0]) {
    default:
        return "1 Bad flag";
    case 'y':
        if (PythonFilterActive)
            return "1 Python filter already enabled";
        else if (PYFilterObject == NULL)
            return "1 Python filter not defined" ;
        PYfilter(true);
        break;
    case 'n':
        if (!PythonFilterActive)
            return "1 Python filter already disabled";
        PYfilter(false);
        break;
    }
    return NULL;
}



/*
**  Reject articles we don't like.
*/
char *
PYartfilter(const ARTDATA *data, char *artBody, long artLen, int lines)
{
    const HDRCONTENT *hc = data->HdrContent;
    int		hdrnum;
    int		i;
    static char buf[256];
    PyObject	*result;

    if (!PythonFilterActive || PYFilterObject == NULL || art_method == NULL)
	return NULL;

    /* Add headers to the dictionary... */
    hdrnum = 0;
    for (i = 0 ; i < MAX_ARTHEADER ; i++) {
	if (HDR_FOUND(i)) {
	    PYheaditem[hdrnum] = PyBuffer_FromMemory(HDR(i), HDR_LEN(i));
	} else
	    PYheaditem[hdrnum] = Py_None;
	PyDict_SetItem(PYheaders, PYheadkey[hdrnum], PYheaditem[hdrnum]);
	hdrnum++;
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
    result = PyObject_CallFunction(art_method, (char *) "O", PYheaders);
    if ((result != NULL) && PyObject_IsTrue(result))
	strlcpy(buf, PyString_AS_STRING(result), sizeof(buf));
    else
	*buf = '\0';
    Py_XDECREF(result);

    /* Clean up after ourselves. */
    PyDict_Clear(PYheaders);
    for (i = 0; i < hdrnum; i++)
	if (PYheaditem[i] != Py_None) {
	    Py_DECREF(PYheaditem[i]);
        }

    if (*buf != '\0')
	return buf;
    return NULL;
}



/*
**  Refuse message-IDs offered through CHECK, IHAVE or TAKETHIS that
**  we don't like.
*/
char *
PYmidfilter(char *messageID, int msglen)
{
    static char		buf[256];
    PyObject		*result;

    if (!PythonFilterActive || PYFilterObject == NULL || msgid_method == NULL)
	return NULL;

    result = PyObject_CallFunction(msgid_method, (char *) "s#", messageID,
                                   msglen);
    if ((result != NULL) && PyObject_IsTrue(result))
	strlcpy(buf, PyString_AS_STRING(result), sizeof(buf));
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
PYmode(OPERATINGMODE Mode, OPERATINGMODE NewMode, char *reason)
{
    PyObject	*result;
    char	oldmode[10], newmode[10];

    if (!PythonFilterActive || PYFilterObject == NULL || mode_method == NULL)
	return;

    switch (Mode) {
    default:		strlcpy(oldmode, "unknown", 10);	break;
    case OMrunning:	strlcpy(oldmode, "running", 10);	break;
    case OMpaused:	strlcpy(oldmode, "paused", 10);		break;
    case OMthrottled:	strlcpy(oldmode, "throttled", 10);	break;
    case OMshutdown:    strlcpy(oldmode, "shutdown", 10);       break;
    }

    switch (NewMode) {
    default:		strlcpy(newmode, "unknown", 10);	break;
    case OMrunning:	strlcpy(newmode, "running", 10);	break;
    case OMpaused:	strlcpy(newmode, "paused", 10);		break;
    case OMthrottled:	strlcpy(newmode, "throttled", 10);	break;
    case OMshutdown:    strlcpy(newmode, "shutdown", 10);       break;
    }

    result = PyObject_CallFunction(mode_method, (char *) "sss",
				   oldmode, newmode, reason);
    Py_XDECREF(result);
}



/*
**  Called by the external module so it can register itself with innd.
*/
static PyObject *
PY_set_filter_hook(PyObject *dummy UNUSED, PyObject *args)
{
    PyObject	*result = NULL;
    PyObject	*temp;

    /* set_filter_hook method should return a pointer to innd auth object. */
    if (PyArg_ParseTuple(args, (char *) "O:set_filter_hook", &temp)) {
	Py_XINCREF(temp);
	Py_XDECREF(PYFilterObject);
	PYFilterObject = temp;
	Py_INCREF(Py_None);
	result = Py_None;
    }

    /* Return a pointer to innd auth method. */
    return result;
}



/*
**  Allow external module to ask innd if an ID is in history.
*/
static PyObject *
PY_havehist(PyObject *self UNUSED, PyObject *args)
{
    char	*msgid;
    Py_ssize_t  msgidlen;

    if (!PyArg_ParseTuple(args, (char *) "s#", &msgid, &msgidlen))
	return NULL;

    if (HIScheck(History, msgid))
	return PyInt_FromLong(1);
    return PyInt_FromLong(0);
}



/*
**  Allow external module to locally delete an article.
*/
static PyObject *
PY_cancel(PyObject *self UNUSED, PyObject *args)
{
    char	*msgid;
    Py_ssize_t  msgidlen;
    char	*parambuf[2];

    if (!PyArg_ParseTuple(args, (char *) "s#", &msgid, &msgidlen))
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
PY_addhist(PyObject *self UNUSED, PyObject *args)
{
    char	*msgid;
    Py_ssize_t  msgidlen;
    char	*articlepaths = (char *) "";
    char	tbuff[12];
    char	*parambuf[6];

    if (!PyArg_ParseTuple(args, (char *) "s#", &msgid, &msgidlen))
	return NULL;

    snprintf(tbuff, sizeof(tbuff), "%lu", (unsigned long) time(NULL));

    parambuf[0] = msgid;
    parambuf[1] = parambuf[2] = parambuf[3] = tbuff;
    parambuf[4] = articlepaths;
    parambuf[5] = 0;

    if (!CCaddhist(parambuf))
	return PyInt_FromLong(1);
    return PyInt_FromLong(0);
}



/*
**  Get a newsgroup's status flag (j, m, n, x, y, =other.group).
*/
static PyObject *
PY_newsgroup(PyObject *self UNUSED, PyObject *args)
{
    char	*newsgroup;
    Py_ssize_t  nglen;
    NEWSGROUP	*ngp;
    char	*end;
    int		size;

    if (!PyArg_ParseTuple(args, (char *) "s#", &newsgroup, &nglen))
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
    if (size > MED_BUFFER) {
	syslog(L_ERROR, "too-long flag field in active for %s", newsgroup);
	size = MED_BUFFER;
    }

    return PyString_FromStringAndSize(ngp->Rest, size);
}



/*
**  Return an article header to the external module as a string.  We
**  don't use a buffer object here because that would make it harder,
**  for example, to compare two on-spool articles.
*/
static PyObject *
PY_head(PyObject *self UNUSED, PyObject *args)
{
    char	*msgid;
    Py_ssize_t  msgidlen;
    char	*p;
    TOKEN	token;
    ARTHANDLE	*art;
    PyObject	*header;
    size_t	headerlen;

    if (!PyArg_ParseTuple(args, (char *) "s#", &msgid, &msgidlen))
	return NULL;

    if (! HISlookup(History, msgid, NULL, NULL, NULL, &token))
	return Py_BuildValue((char *) "s", "");	
    if ((art = SMretrieve(token, RETR_HEAD)) == NULL)
	return Py_BuildValue((char *) "s", "");	
    p = wire_to_native(art->data, art->len, &headerlen);
    SMfreearticle(art);
    header = PyString_FromStringAndSize(p, headerlen);
    free(p);

    return header;
}



/*
**  Return a whole article to the external module as a string.
*/
static PyObject *
PY_article(PyObject *self UNUSED, PyObject *args)
{
    char	*msgid;
    Py_ssize_t  msgidlen;
    char	*p;
    TOKEN	token;
    ARTHANDLE	*arth;
    PyObject	*art;
    size_t	artlen;

    if (!PyArg_ParseTuple(args, (char *) "s#", &msgid, &msgidlen))
	return NULL;

    if (! HISlookup(History, msgid, NULL, NULL, NULL, &token))
	return Py_BuildValue((char *) "s", "");
    if ((arth = SMretrieve(token, RETR_ALL)) == NULL)
	return Py_BuildValue((char *) "s", "");	
    p = wire_to_native(arth->data, arth->len, &artlen);
    SMfreearticle(arth);
    art = PyString_FromStringAndSize(p, artlen);
    free(p);

    return art;
}



/*
**  Python's syslog module isn't compiled in by default.  It's easier
**  to do it this way, and the switch block looks pretty in a color
**  editor).
*/
static PyObject *
PY_syslog(PyObject *self UNUSED, PyObject *args)
{
    char	*loglevel;
    Py_ssize_t  levellen;
    char	*logmsg;
    Py_ssize_t  msglen;
    int		priority;

    /* Get loglevel and message. */
    if (!PyArg_ParseTuple(args, (char *) "s#s#",
			  &loglevel, &levellen, &logmsg, &msglen))
	return NULL;

    /* Assign syslog priority by abbreviated names. */
    switch (*loglevel) {
    case 'd': case 'D': priority = LOG_DEBUG ;		break;
    case 'i': case 'I': priority = LOG_INFO ;		break;
    case 'n': case 'N': priority = LOG_NOTICE ;		break;
    case 'w': case 'W': priority = LOG_WARNING ;	break;
    case 'e': case 'E': priority = LOG_ERR ;		break;
    case 'c': case 'C': priority = LOG_CRIT ;		break;
    case 'a': case 'A': priority = LOG_ALERT ;		break;
    default:            priority = LOG_NOTICE ;
    }

    /* Log the message. */
    syslog(priority, "python: %s", logmsg);

    /* Return None. */
    Py_INCREF(Py_None);
    return Py_None;
}



/*
**  Compute a hash digest for a string.
*/
static PyObject *
PY_hashstring(PyObject *self UNUSED, PyObject *args)
{
    char	*instring, *wpos, *p, *q;
    char	*workstring = NULL;
    Py_ssize_t  insize;
    int         worksize, i;
    bool        wasspace;
    int		lines = 0;
    HASH	myhash;

    if (!PyArg_ParseTuple(args, (char *) "s#|i", &instring, &insize, &lines))
	return NULL;

    /* If a linecount is provided, munge before hashing. */
    if (lines > 0) {
	worksize = (int) insize;

	/* Chop leading whitespace. */
	for (p=instring ; worksize>0 && isspace((unsigned char) *p) ; p++) {
	    if (*p == '\n')
		lines--;
	    worksize--;
	}
	wpos = p;

	/* And trailing. */
	for (p=&wpos[worksize] ; worksize>0 && isspace((unsigned char) *p) ; p--) {
	    if (*p == '\n')
		lines--;
	    worksize--;
	}

	/* Chop last 3 lines if we have >= 5.  From above chop the
	 * last line which has no CR so we use 1 less here. */
	if (lines >= 4) {
	    for (i=0, p=wpos+worksize ; i<2 ; p--)
		if (*p == '\n')
		    i++;
	    worksize = p - wpos;
	}

	/* Compress out multiple whitespace in the trimmed string.  We
	 * do a copy because this is probably an original art
	 * buffer. */
	workstring =  memcpy(xmalloc(worksize), wpos, worksize);
        wasspace = false;
	p = wpos;
	q = workstring;
	for (i=0 ; i<worksize ; i++) {
	    if (isspace((unsigned char) *p)) {
                if (!wasspace) {
                    *q++ = ' ';
                }
                wasspace = true;
	    } else {
                *q++ = tolower((unsigned char) *p);
                wasspace = false;
            }
            p++;
        }
	worksize = q - workstring;
	myhash = Hash(workstring, worksize);
	free(workstring);
    }
    else
	myhash = Hash(instring, (int) insize);

    return PyString_FromStringAndSize((const char *)&myhash, sizeof(myhash));
}



/*
**  Make the internal INN module's functions visible to Python.  Python
**  annoyingly doesn't use const where appropriate in its structure
**  definitions, so we have to add casts for all of the string parameters that
**  we're initializing with constant strings.
*/
#define METHOD(name, func, flags, help) \
    { (char *)(name), (func), (flags), (char *)(help) }

static PyMethodDef INNPyMethods[] = {
    METHOD("set_filter_hook", PY_set_filter_hook, METH_VARARGS, ""),
    METHOD("havehist",        PY_havehist,        METH_VARARGS, ""),
    METHOD("addhist",         PY_addhist,         METH_VARARGS, ""),
    METHOD("cancel",          PY_cancel,          METH_VARARGS, ""),
    METHOD("newsgroup",       PY_newsgroup,       METH_VARARGS, ""),
    METHOD("head",            PY_head,            METH_VARARGS, ""),
    METHOD("article",         PY_article,         METH_VARARGS, ""),
    METHOD("syslog",          PY_syslog,          METH_VARARGS, ""),
    METHOD("hashstring",      PY_hashstring,      METH_VARARGS, ""),
    METHOD(NULL,              NULL,               0,            "")
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
static void
PYdefonemethod(PyObject **methptr, const char *methname)
{
    Py_XDECREF(*methptr);

    /* We check with HasAttrString() the existence of the method because
     * otherwise, in case it does not exist, an exception is raised by Python,
     * although the result of the function is NULL. */
    if (PyObject_HasAttrString(PYFilterObject, (char *) methname) == 1) {
        /* Get a pointer to given method. */
        *methptr = PyObject_GetAttrString(PYFilterObject, (char *) methname);
    } else {
        *methptr = NULL;
    }

    /* See if such method is defined. */
    if (*methptr == NULL)
	syslog(L_NOTICE, "python method %s not found", methname);
    else {
        /* See if it is callable. */
        if (PyCallable_Check(*methptr) == 0) {
            syslog(L_ERROR, "python object %s found but not a function", methname);
            Py_DECREF(*methptr);
            *methptr = NULL;
        }
    }
}



/*
**  Look up the filter methods, so we will know what's available when
**  innd wants to call them.
*/
static void
PYdefmethods(void)
{
    PYdefonemethod(&msgid_method, "filter_messageid");
    PYdefonemethod(&art_method, "filter_art");
    PYdefonemethod(&mode_method, "filter_mode");
    PYdefonemethod(&pre_reload_method, "filter_before_reload");
    PYdefonemethod(&close_method, "filter_close");
}



/*
**  Used by "ctlinnd reload filter.python 'reason'".
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

    /* We need to reimport the module before reloading it because otherwise,
     * it might not be taken into account by Python.
     * See Python API documentation:
     *     If a module is syntactically correct but its initialization fails,
     *     the first import statement for it does not bind its name locally,
     *     but does store a (partially initialized) module object in
     *     sys.modules.  To reload the module, you must first import it again
     *     (this will bind the name to the partially initialized module object)
     *     before you can reload() it.
     */
    PYFilterModule = PyImport_ImportModule((char *) INN_PATH_PYTHON_STARTUP_M);
    if (PYFilterModule == NULL) {
        syslog(L_ERROR, "failed to reimport external python module");
    }

    if ((newmodule = PyImport_ReloadModule(PYFilterModule)) == NULL) {
	syslog(L_ERROR, "cant reload python filter module");
	PYfilter(false);
	return 0;
    }

    Py_XDECREF(PYFilterModule);
    PYFilterModule = newmodule;

    if (PYFilterObject == NULL) {
	syslog(L_ERROR, "python reload error, filter object not defined");
	PYfilter(false);
	return 0;
    }

    PYfilter(true);
    PYdefmethods();

    return 1;
}



/*
**  Called when innd first starts -- this gets the filters hooked in.
*/
void
PYsetup(void)
{
    const ARTHEADER *hp;
    size_t hdrcount;

    /* Add path for nnrpd module.  The environment variable PYTHONPATH
     * does it; one can also append innconf->pathfilter to sys.path once
     * Python has been initialized. */
    setenv("PYTHONPATH", innconf->pathfilter, 1);

    /* Load up the interpreter ;-O */
    Py_Initialize();

    /* It makes Python sad when its stdout or stderr are closed. */
    if ((fileno(stdout) == -1) || (fileno(stderr) == -1))
	PyRun_SimpleString
	    ("import sys; sys.stdout=sys.stderr=open('/dev/null', 'a')");

    /* See if Python initialized OK. */
    if (!Py_IsInitialized ()) {
	syslog(L_ERROR, "python interpreter NOT initialized");
	return;
    }

    /* Build a module interface to certain INN functions. */
    Py_InitModule((char *) "INN", INNPyMethods);

    PYFilterModule = PyImport_ImportModule((char *) INN_PATH_PYTHON_STARTUP_M);
    if (PYFilterModule == NULL)
	syslog(L_ERROR, "failed to import external python module");

    if (PYFilterObject == NULL) {
	syslog(L_ERROR, "python filter object is not defined");
	PYfilter(false);
    } else {
	PYfilter(true);
	PYdefmethods();
	syslog(L_NOTICE, "defined python methods");
    }

    /* Grab space for these so we aren't forever recreating them.  We also
     * cut the body and the line count into PYheaditem, so it needs to be
     * two elements longer than the total number of headers. */
    PYheaders = PyDict_New();
    hdrcount = ARRAY_END(ARTheaders) - ARTheaders;
    PYheaditem = xmalloc((hdrcount + 2) * sizeof(PyObject *));
    PYheadkey = xmalloc(hdrcount * sizeof(PyObject *));

    /* Preallocate keys for the article dictionary */
    for (hp = ARTheaders; hp < ARRAY_END(ARTheaders); hp++)
	PYheadkey[hp - ARTheaders] = PyString_InternFromString(hp->Name);
    PYpathkey = PyString_InternFromString("Path");
    PYlineskey = PyString_InternFromString("__LINES__");
    PYbodykey = PyString_InternFromString("__BODY__");

    syslog(L_NOTICE, "python interpreter initialized OK");
}

#endif /* defined(DO_PYTHON) */
