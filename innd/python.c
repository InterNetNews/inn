/* 
 * $Id$
 *
 * python.c: Embed Python in the style of innd's TCL and Perl stuff.
 * 
 * Written by G.J. Andruk <meowing@banet.net> patterned after TCL/Perl
 * work by Bob Heiney and Christophe Wolfhugel.  Meow.
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


BOOL			PythonFilterActive;
ARTHEADER		ARTheaders[], *ARTheadersENDOF;
char			*pathForPython;
PyObject		*PYFilterObject = NULL;
PyObject		*PYFilterModule = NULL;
PyObject		*PYResult;
PyObject		*PYheaditem[];
STATIC char		*PYSTARTUP = NULL;
STATIC BUFFER		PYpythonbuff;
STATIC FILE		*PYFilterFile;



void
PYfilter(value)
    BOOL value;
{
    PythonFilterActive=value;
    syslog(L_NOTICE, "%s Python filtering %s", LogName,
	   PythonFilterActive ? "enabled" : "disabled");
}



/* Reject articles we don't like. */
char *
PYHandleArticle(artBody, lines)
    char* artBody;
    int	lines;
{
    ARTHEADER	*hp;
    int		hdrnum;
    int		i;
    char	*p;
    static char	buf[256];
    PyObject	*PYheaders, *tryfunc, *result;

    if (!PythonFilterActive || PYFilterObject == NULL)
	return NULL;

    /* Fill in the dictionary */
    PYheaders = PyDict_New();	/* Hmmm. Reuse instead of this business? */
    hdrnum = 0;
    for (hp = ARTheaders; hp < ARTheadersENDOF; hp++) {
	if (hp->Found && hp->Value && strcmp (hp->Name,"Path") != 0) {
	    PYheaditem[hdrnum] = PyString_FromString(hp->Value);
	    PyDict_SetItemString(PYheaders, (char *)hp->Name,
				 PYheaditem[hdrnum++]);
	}
    }

    /* Store article body */
    if (artBody != NULL) {
	PYheaditem[hdrnum] = PyString_FromString(artBody);
	PyDict_SetItemString(PYheaders, "__BODY__", PYheaditem[hdrnum++]);
    }


    /* Store article line count */
    PYheaditem[hdrnum] = PyInt_FromLong((long) lines);
    PyDict_SetItemString(PYheaders, "__LINES__", PYheaditem[hdrnum++]);


    if (pathForPython != NULL) {
	char *p = strchr (pathForPython,'\n') ;

	*p = '\0' ;
	PYheaditem[hdrnum] = PyString_FromString(pathForPython);
	PyDict_SetItemString(PYheaders, "Path", PYheaditem[hdrnum++]);
	*p = '\n' ;
    }

    /*
     * Checking exists/callable for every article may be a little too
     * paranoid.  Perhaps better done at load/reload time?
     */
    if ((tryfunc = PyObject_GetAttrString(PYFilterObject,
					  "filter_art")) != NULL) {
	if (PyCallable_Check(tryfunc) == 1) {
	    if ((result = PyObject_CallFunction(tryfunc,
						"O", PYheaders)) != NULL) {
		if (PyObject_IsTrue(result)) {
		    strncpy(buf, PyString_AsString(result), sizeof(buf) - 1);
		    Py_DECREF(result);
		} else {
		    buf[0] = '\0';
		}
	    } else {
		buf[0] = '\0';
	    }
	}
	Py_DECREF(tryfunc);
    }
    PyDict_Clear(PYheaders);
    Py_DECREF(PYheaders);
    for (i = 0; i < hdrnum; i++)
	Py_DECREF(PYheaditem[i]);

    if (buf[0] != '\0') 
	return buf ;
    return NULL;
}


/* Refuse articles offered thru CHECK or IHAVE that we don't like.  */
char *
PYHandleMessageID(messageID)
    char *messageID;
{
    static char		buf[256];
    PyObject		*tryfunc, *result;

    if (!PythonFilterActive || PYFilterObject == NULL)
	return NULL;

    if ((tryfunc = PyObject_GetAttrString(PYFilterObject,
					  "filter_messageid")) != NULL) {
	if (PyCallable_Check(tryfunc) == 1) {
	    if ((result = PyObject_CallFunction(tryfunc,
						"s", messageID)) != NULL) {
		if (PyObject_IsTrue(result)) {
		    strncpy(buf, PyString_AsString(result), sizeof(buf) - 1);
		} else {
		    buf[0] = '\0';
		}
		Py_DECREF(result);
	    } else {
		buf[0] = '\0';
	    }
	}
	Py_DECREF(tryfunc);
    }
    if (buf[0] != '\0') 
	return buf;
    return NULL;
}



/* Tell the external module about our state. */
void
PythonMode(Mode, NewMode, reason)
    OPERATINGMODE Mode, NewMode;
    char *reason;
{
    PyObject	*tryfunc, *filtermode, *result;
    char	oldmode[10], newmode[10];

    if (!PythonFilterActive || PYFilterObject == NULL)
	return;

    if (Mode == OMrunning)
	strcpy(oldmode, "running");
    if (Mode == OMpaused)
	strcpy(oldmode, "paused");
    if (Mode == OMthrottled)
	strcpy(oldmode, "throttled");

    if (NewMode == OMrunning)
	strcpy(newmode, "running");
    if (NewMode == OMpaused)
	strcpy(newmode, "paused");
    if (NewMode == OMthrottled)
	strcpy(newmode, "throttled");

    if ((tryfunc = PyObject_GetAttrString(PYFilterObject,
					  "filter_mode")) != NULL) {
	if (PyCallable_Check(tryfunc) == 1) {
	    result = PyObject_CallFunction(tryfunc, "sss",
					   oldmode, newmode, reason);
	    Py_DECREF(result);
	}
	Py_DECREF(tryfunc);
    }
}



/* Called by the external module so it can register itself with INN. */
static PyObject *
PY_set_filter_hook(dummy, args)
    PyObject *dummy, *args;
{	
    PyObject *result = NULL;
    PyObject *temp;

    if (PyArg_ParseTuple(args, "O:set_filter_hook", &temp)) {
	Py_XINCREF(temp);
	Py_XDECREF(PYFilterObject);
	PYFilterObject = temp;
	Py_INCREF(Py_None);
	result = Py_None;
    }
    return result;
}



/* Allows external module to query innd if an ID is in history. */
static PyObject *
PY_havehist(self, args)
    PyObject *self, *args;
{
    char *msgid;

    if (!PyArg_ParseTuple(args, "s", &msgid))
	return NULL;
    if (HIShavearticle(HashMessageID(msgid))) {
	return Py_BuildValue("i", 1);
    } else {
	return Py_BuildValue("i", 0);
    }
}



/* External module can locally delete an article. */
static PyObject *
PY_cancel(self, args)
    PyObject *self, *args;
{
    char	*msgid;
    char	*parambuf[2];

    if (!PyArg_ParseTuple(args, "s", &msgid))
	return NULL;

    parambuf[0]= msgid;
    parambuf[1]= 0;

    if (!CCcancel(parambuf)) {
	return Py_BuildValue("i", 1);
    } else {
	return Py_BuildValue("i", 0);
    }
}



/* Stuff an ID into history so that it will be refused later. */
static PyObject *
PY_addhist(self, args)
    PyObject *self, *args;
{
    char*	msgid;
    char*	arrivaltime;
    char*	articletime;
    char*	expiretime;
    char*	articlepaths = "";
    char	tbuff[12];
    char*	parambuf[6];


    if (!PyArg_ParseTuple(args, "s", &msgid))
	return NULL;

    sprintf(tbuff, "%d",time((long *)0));
    arrivaltime = articletime = expiretime = tbuff;

    parambuf[0] = msgid;
    parambuf[1] = arrivaltime;
    parambuf[2] = articletime;
    parambuf[3] = expiretime;
    parambuf[4] = articlepaths;
    parambuf[5] = 0;

    if (!CCaddhist(parambuf)) {
	return Py_BuildValue("i", 1);
    } else {
	return Py_BuildValue("i", 0);
    }
}



/* Get a newsgroup's status flag (m, y, x, etc.) */
static PyObject *
PY_newsgroup(self, args)
    PyObject *self, *args;
{
    char*	newsgroup;
    NEWSGROUP*  ngp;
    char*	end;
    char*	rest;
    int		size;

    if (!PyArg_ParseTuple(args, "s", &newsgroup))
	return NULL;

    if ((ngp = NGfind(newsgroup)) == NULL) {
	return Py_BuildValue("s", "");
    } else {
	/* ngp->Rest is newline-terminated; find the end. */
	end = strchr(ngp->Rest, '\n');
	if (end == NULL) {
	    size = strlen(ngp->Rest);
	} else {
	    size = end - ngp->Rest;
	}

	if (PYpythonbuff.Data == NULL) {
	    PYpythonbuff.Size = SITE_BUFFER_SIZE;
	    PYpythonbuff.Data = NEW(char, PYpythonbuff.Size);
	}

	/* SITE_BUFFER_SIZE should be *huge* for this, but be paranoid. */
	if (size > SITE_BUFFER_SIZE + 1)
	    size = SITE_BUFFER_SIZE;

	strncpy(PYpythonbuff.Data, ngp->Rest, size);
	PYpythonbuff.Data[size] = '\0';
	return Py_BuildValue("s", PYpythonbuff.Data);
    }
}



/* Return an article header to the external module as a string */
static PyObject *
PY_head(self, args)
    PyObject *self, *args;
{
    char	*msgid;
    char	*p;
    char	*q;
    char	*bufptr;
    QIOSTATE	*qp;


    if (!PyArg_ParseTuple(args, "s", &msgid))
	return NULL;

    /* Get the article filenames; open the first file */
    if ((q = HISfilesfor(HashMessageID(msgid))) == NULL) {
	return Py_BuildValue("s", "");	
    }
    if ((p = strchr(q, ' ')))
	*p = '\0';

    if ((qp = QIOopen(q)) == NULL) {
	return Py_BuildValue("s", "");
    }

    if (PYpythonbuff.Data == NULL) {
	PYpythonbuff.Size = SITE_BUFFER_SIZE;
	PYpythonbuff.Data = NEW(char, PYpythonbuff.Size);
    }
                                        
    strcpy(PYpythonbuff.Data, "");

    for (p = QIOread(qp); (p != NULL) && (*p != '\0'); p = QIOread(qp)) {
	if (PYpythonbuff.Size < (qp->Count + 3)) {
	    PYpythonbuff.Size += SITE_BUFFER_SIZE;
	    RENEW(PYpythonbuff.Data, char, PYpythonbuff.Size);
        }
	strncat(PYpythonbuff.Data, p, QIOlength(qp));
	strncat(PYpythonbuff.Data, "\n", 1);
    }
    QIOclose(qp);

    return Py_BuildValue("s", PYpythonbuff.Data);
}



/* Return a whole article to the external module as a string */
static PyObject *
PY_article(self, args)
    PyObject *self, *args;
{
    char	*msgid;
    char	*p;
    char	*q;
    char	*bufptr;
    QIOSTATE	*qp;


    if (!PyArg_ParseTuple(args, "s", &msgid))
	return NULL;

    /* Get the article filenames; open the first file */
    if ((q = HISfilesfor(HashMessageID(msgid))) == NULL) {
	return Py_BuildValue("s", "");	
    }
    if (p = strchr(q, ' '))
	*p = '\0';

    if ((qp = QIOopen(q)) == NULL) {
	return Py_BuildValue("s", "");
    }

    if (PYpythonbuff.Data == NULL) {
	PYpythonbuff.Size = SITE_BUFFER_SIZE;
	PYpythonbuff.Data = NEW(char, PYpythonbuff.Size);
    }
                                        
    strcpy(PYpythonbuff.Data, "");

    for (p = QIOread(qp); (p != NULL); p = QIOread(qp)) {
	if (PYpythonbuff.Size < (qp->Count + 3)) {
	    PYpythonbuff.Size += SITE_BUFFER_SIZE;
	    RENEW(PYpythonbuff.Data, char, PYpythonbuff.Size);
        }
	strncat(PYpythonbuff.Data, p, QIOlength(qp));
	strncat(PYpythonbuff.Data, "\n", 1);
    }
    QIOclose(qp);

    return Py_BuildValue("s", PYpythonbuff.Data);
}


/*
 * Python's syslog module isn't compiled in by default, and it's in
 * an 'optional' bit of the path.  Doing it this way is easier to
 * explain.
 */

static PyObject *
PY_syslog(self, args)
    PyObject *self, *args;
{
    char	*loglevel;
    char	*logmsg;
    int		priority;

    if (!PyArg_ParseTuple(args, "ss", &loglevel, &logmsg))
	return NULL;

    switch (*loglevel) {
	default:		priority = LOG_NOTICE ;
	case 'a': case 'A':	priority = LOG_ALERT ;		break;
	case 'c': case 'C':	priority = LOG_CRIT ;		break;
	case 'e': case 'E':	priority = LOG_ERR ;		break;
	case 'w': case 'W':	priority = LOG_WARNING ;	break;
	case 'n': case 'N':	priority = LOG_NOTICE ;		break;
	case 'i': case 'I':	priority = LOG_INFO ;		break;
	case 'd': case 'D':	priority = LOG_DEBUG ;		break;
    }
    syslog(priority, "python: %s", logmsg);

    return Py_BuildValue("i", 0);
}



/* Make the Python methods visible to the outside world */
static PyMethodDef INNPyMethods[] = {
    {"set_filter_hook",	PY_set_filter_hook,	METH_VARARGS},
    {"havehist",	PY_havehist,		METH_VARARGS},
    {"addhist",		PY_addhist,		METH_VARARGS},
    {"cancel",		PY_cancel,		METH_VARARGS},
    {"newsgroup",	PY_newsgroup,		METH_VARARGS},
    {"head",		PY_head,		METH_VARARGS},
    {"article",		PY_article,		METH_VARARGS},
    {"syslog",		PY_syslog,		METH_VARARGS},
    {NULL,		NULL}
};



/* used by ctlinnd reload */
int
PYreadfilter(void)
{
    PyObject   	*tryfunc, *result;
    PyObject	*newmodule = NULL;

    if (!Py_IsInitialized()) {
	syslog(L_ERROR, "Python is not initialized");
	return 0;
    }
    if ((newmodule = PyImport_ReloadModule(PYFilterModule)) == NULL) {
	syslog(L_ERROR, "Could not reload the Python filter module");
    } else {
	Py_DECREF(PYFilterModule);
	PYFilterModule = newmodule;
    }
    if (PYFilterObject == NULL) {
	syslog(L_ERROR, "Python reload: filter object is not defined");
	PYfilter(FALSE);
	return 0;
    }

    PYfilter(TRUE);
    return 1;
}



/* Called when innd first starts */
void
PYSetup(void)
{
    char	*modulename;

    setenv("PYTHONPATH", innconf->pathfilter, 1);
    Py_Initialize();

    /* It makes Python sad when its stdout and stderr are closed. */
    if ((fileno(stdout) == -1) || (fileno(stderr) == -1))
	PyRun_SimpleString
	    ("import sys; sys.stdout = sys.stderr = open('/dev/null', 'a')");

    if (Py_IsInitialized ()) {
	syslog(L_NOTICE, "Python initialized OK");
    } else {
	syslog(L_ERROR, "Python is not initialized");
	return;
    }
    (void) Py_InitModule("INN", INNPyMethods);
    strcpy(modulename, _PATH_PYTHON_STARTUP_M);
    PYFilterModule = PyImport_ImportModule(modulename);
    if (PYFilterModule == NULL) {
	syslog(L_ERROR, "Python module failed to load");
    }
    if (PYFilterObject == NULL) {
	syslog(L_ERROR, "Python filter object is not defined");
	PYfilter(FALSE);
    } else {
	PYfilter(TRUE);
    }
}



/* Maybe some day I'll even make this do something. */
void
PYclose(void)
{
}


#endif /* (DO_PYTHON) */
