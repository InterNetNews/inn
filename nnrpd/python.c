/*  $Id$
**
**  python.c: Embed Python in the style of nnrpd's TCL and Perl stuff
**            (authentication and authorization hooks only at this point).
** 
**  Written by Ilya Etingof <ilya@glas.net>, 1999.
**
**  This code bases on Python work for innd filtering done by
**  G.J. Andruk <meowing@banet.net>. Also it borrows some ideas from
**  TCL/Perl work done by Bob Heiney and Christophe Wolfhugel.
*/

#include "config.h"
#include "clibrary.h"

#include "inn/innconf.h"
#include "nnrpd.h"

#if defined(DO_PYTHON)

#include "Python.h"

/* Pointers to external Python objects */
PyObject	*PYAuthObject = NULL;
PyObject	*PYAuthModule = NULL;

/* Dictionary of params to pass to authentication methods */
PyObject	*PYauthinfo = NULL;
PyObject	**PYauthitem = NULL;
PyObject	**PYauthkey = NULL;

/* Max number of items in dictionary to pass to auth methods */
#define	_PY_MAX_AUTH_ITEM	10

/* These are pointers to Python methods specified in auth module */
PyObject	*authenticate_method = NULL;
PyObject	*authorize_method = NULL;
PyObject	*close_method = NULL;

/* Forward declaration */
static PyObject *PY_set_auth_hook(PyObject *dummy, PyObject *args);

/* These variable defined in other C modules */
extern char accesslist[];

/*
** Authenticate connecting host by IP address or username&password.
**
** Return NNTP reply code as returned by Python method or -1 if method
** is not defined.
*/
int PY_authenticate(char *clientHost, char *clientIpString, char *serverHost, char *Username, char *Password, char *accesslist) {
    PyObject    *result, *item;
    char        *type;
    int         authnum;
    int         code, i;

    /* Return if authentication method is not defined */
    if (authenticate_method == NULL)
        return -1;

    /* Figure out authentication type */
    if (Username == NULL)
        type = "connect";
    else
        type = "authinfo";

    /* Initialize PythonAuthObject with connect method specific items */
    authnum = 0;

    /* Authentication type */
    PYauthitem[authnum] = PyBuffer_FromMemory(type, strlen(type));
    PyDict_SetItem(PYauthinfo, PYauthkey[authnum], PYauthitem[authnum++]);

    /* Client hostname */
    PYauthitem[authnum] = PyBuffer_FromMemory(clientHost, strlen(clientHost));
    PyDict_SetItem(PYauthinfo, PYauthkey[authnum], PYauthitem[authnum++]);

    /* Client IP number */
    PYauthitem[authnum] = PyBuffer_FromMemory(clientIpString, strlen(clientIpString));
    PyDict_SetItem(PYauthinfo, PYauthkey[authnum], PYauthitem[authnum++]);

    /* Server interface the connection comes to */
    PYauthitem[authnum] = PyBuffer_FromMemory(serverHost, strlen(serverHost));
    PyDict_SetItem(PYauthinfo, PYauthkey[authnum], PYauthitem[authnum++]);

    /* Username if known */
    if (Username == NULL)
        PYauthitem[authnum] = Py_None;
    else
        PYauthitem[authnum] = PyBuffer_FromMemory(Username, strlen(Username));
    PyDict_SetItem(PYauthinfo, PYauthkey[authnum], PYauthitem[authnum++]);

    /* Password if known */
    if (Password == NULL)
        PYauthitem[authnum] = Py_None;
    else
        PYauthitem[authnum] = PyBuffer_FromMemory(Password, strlen(Password));
    PyDict_SetItem(PYauthinfo, PYauthkey[authnum], PYauthitem[authnum++]);

    /* Now invoke authenticate method and see if it likes this user */
    result = PyObject_CallFunction(authenticate_method, "O", PYauthinfo);

    /* Check the response */
    if (result == NULL || !PyTuple_Check(result))
    {
        syslog(L_ERROR, "python authenticate_method (type %s) returned wrong result", type);
	Reply("%d Internal Error (7).  Goodbye\r\n", NNTP_ACCESS_VAL);
	ExitWithStats(1, TRUE);
    }

    /* Get the NNTP response code */
    item = PyTuple_GetItem(result, 0);

    /* Check the item */
    if (!PyInt_Check(item))
    {
        syslog(L_ERROR, "python authenticate_method (type %s) returned bad NNTP response code", type);
	Reply("%d Internal Error (7).  Goodbye\r\n", NNTP_ACCESS_VAL);
	ExitWithStats(1, TRUE);
    }

    /* Store the code */
    code = PyInt_AS_LONG(item);

    /* Get the CanPost setting */
    item = PyTuple_GetItem(result, 1);

    /* Check the item */
    if (!PyInt_Check(item))
    {
        syslog(L_ERROR, "python authenticate_method (type %s) returned bad CanPost setting", type);
	Reply("%d Internal Error (7).  Goodbye\r\n", NNTP_ACCESS_VAL);
	ExitWithStats(1, TRUE);
    }

    /* Store the setting */
    PERMcanpost = PyInt_AS_LONG(item);

    /* Get the CanRead setting */
    item = PyTuple_GetItem(result, 2);

    /* Check the item */
    if (!PyInt_Check(item))
    {
        syslog(L_ERROR, "python authenticate_method (type %s) returned bad CanRead setting", type);
	Reply("%d Internal Error (7).  Goodbye\r\n", NNTP_ACCESS_VAL);
	ExitWithStats(1, TRUE);
    }

    /* Store the setting */
    PERMcanread = PyInt_AS_LONG(item);

    /* Get the access list */
    item = PyTuple_GetItem(result, 3);

    /* Check the item */
    if (!PyString_Check(item))
    {
        syslog(L_ERROR, "python authenticate_method (type %s) returned bad access list value", type);
	Reply("%d Internal Error (7).  Goodbye\r\n", NNTP_ACCESS_VAL);
	ExitWithStats(1, TRUE);
    }

    /* Store access list*/
    strcpy(accesslist, PyString_AS_STRING(item));

    /* Fix the NNTP response code */
    if ((code == NNTP_POSTOK_VAL) || (code == NNTP_NOPOSTOK_VAL))
    {
      code = PERMcanpost ? NNTP_POSTOK_VAL : NNTP_NOPOSTOK_VAL;
    }

    /* Initialize needauth flag */
    if (code == NNTP_AUTH_NEEDED_VAL) 
        PERMneedauth = TRUE;

    /* Clean up the dictionary object */
    PyDict_Clear(PYauthinfo);

    /* Clean up dictionary items */
    for (i = 0; i < authnum; i++)
    {
        if (PYauthitem[i] != Py_None)
	{
	    Py_DECREF(PYauthitem[i]);
	}
    }

    /* Log auth result */
    syslog(L_NOTICE, "python authenticate_method (type %s) succeeded, return code %d", type, code);

    /* Return response code */
    return code;
}


/*
** Authorize user access to a newsgroup.
**
** Return 0 if requested privelege is granted or positive value
** and a reply_message pointer initialized with reply message.
** Return negative value if authorize method is not defined.
*/
int PY_authorize(char *clientHost, char *clientIpString, char *serverHost, char *Username, char *NewsGroup, int PostFlag, char **reply_message) {
    PyObject	*result, *item;
    char	*string;
    int		authnum;
    int		i;

    /* Return if authorize_method is not defined */
    if (authorize_method == NULL)
        return -1;

    /* Initialize PythonAuthObject with group method specific items */
    authnum = 0;

    /* Assign authentication type */
    PYauthitem[authnum] = PyBuffer_FromMemory(PostFlag ? "post" : "read", 4);
    PyDict_SetItem(PYauthinfo, PYauthkey[authnum], PYauthitem[authnum++]);

    /* Client hostname */
    PYauthitem[authnum] = PyBuffer_FromMemory(clientHost, strlen(clientHost));
    PyDict_SetItem(PYauthinfo, PYauthkey[authnum], PYauthitem[authnum++]);

    /* Client IP number */
    PYauthitem[authnum] = PyBuffer_FromMemory(clientIpString, strlen(clientIpString));
    PyDict_SetItem(PYauthinfo, PYauthkey[authnum], PYauthitem[authnum++]);

    /* Server interface the connection comes to */
    PYauthitem[authnum] = PyBuffer_FromMemory(serverHost, strlen(serverHost));
    PyDict_SetItem(PYauthinfo, PYauthkey[authnum], PYauthitem[authnum++]);

    /* Username */
    PYauthitem[authnum] = PyBuffer_FromMemory(Username, strlen(Username));
    PyDict_SetItem(PYauthinfo, PYauthkey[authnum], PYauthitem[authnum++]);

    /* Password is not known */
    PYauthitem[authnum] = Py_None;
    PyDict_SetItem(PYauthinfo, PYauthkey[authnum], PYauthitem[authnum++]);

    /* Newsgroup user tries to access */
    PYauthitem[authnum] = PyBuffer_FromMemory(NewsGroup, strlen(NewsGroup));;
    PyDict_SetItem(PYauthinfo, PYauthkey[authnum], PYauthitem[authnum++]);

    /*
     * Now invoke newsgroup access authorization method and see if
     * it likes this user to access this newsgroup.
     */
    result = PyObject_CallFunction(authorize_method, "O", PYauthinfo);

    /* Check the response */
    if (result == NULL || result != Py_None && !PyString_Check(result))
    {
        syslog(L_ERROR, "python authorize_method (%s access) returned wrong result", PostFlag ? "post" : "read");
	Reply("%d Internal Error (7).  Goodbye\r\n", NNTP_ACCESS_VAL);
	ExitWithStats(1, FALSE);
    }

    /* Get the response string */
    if (result == Py_None)
        string = NULL;
    else
        string = PyString_AS_STRING(result);

    /* Clean up the dictionary object */
    PyDict_Clear(PYauthinfo);

    /* Clean up dictionary items */
    for (i = 0; i < authnum; i++)
    {
        if (PYauthitem[i] != Py_None)
	{
	    Py_DECREF(PYauthitem[i]);
	}
    }

    /* Log auth result */
    syslog(L_NOTICE, "python authorize_method (%s access) succeeded, refusion string: %s", PostFlag ? "post" : "read", string == NULL ? "<empty>" : string);

    /* Initialize reply string */
    if (reply_message != NULL)
        *reply_message = string;

    /* Return result */
    return string == NULL ? 0 : 1;
}


/*
**  This runs when nnrpd shuts down.
*/
void
PY_close(void)
{
    PyObject	*result;

    if (close_method != NULL) {
        result = PyObject_CallFunction(close_method, NULL);
        Py_XDECREF(result);
    }
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
    char        *loglevel;
    int         levellen;
    char        *logmsg;
    int         msglen;
    int         priority;

    /* Get loglevel and message */
    if (!PyArg_ParseTuple(args, "s#s#", &loglevel, &levellen, &logmsg, &msglen))
        return NULL;

    /* Assign syslog priority by abbreviated names */
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

    /* Log the message */
    syslog(priority, "python: %s", logmsg);

    /* Return None */
    Py_INCREF(Py_None);
    return Py_None;
}


/*
**  Make the internal nnrpd module's functions visible to Python.
*/
static PyMethodDef nnrpdPyMethods[] = {
    {"set_auth_hook",   PY_set_auth_hook,	METH_VARARGS},
    {"syslog",		PY_syslog,		METH_VARARGS},
    {NULL,		NULL}
};


/*
**  Called by the external module so it can register itself with nnrpd.
*/
static PyObject *
PY_set_auth_hook(dummy, args)
    PyObject *dummy, *args;
{
    PyObject    *result = NULL;
    PyObject    *temp;

    /* set_auth_hook method should return a pointer to nnrpd auth object */
    if (PyArg_ParseTuple(args, "O:set_auth_hook", &temp)) {
        Py_XINCREF(temp);
        Py_XDECREF(PYAuthObject);
        PYAuthObject = temp;
        Py_INCREF(Py_None);
        result = Py_None;
    }

    /* Return a pointer to nnrpd auth method */
    return result;
}


/*
**  Check that a method exists and is callable.	 Set up a pointer to
**  the corresponding PyObject, or NULL if not found.
*/
void
PYdefonemethod(methptr, methname)
    PyObject    **methptr;
    char        *methname;
{
    Py_XDECREF(*methptr);

    /* Get a pointer to given method */
    *methptr = PyObject_GetAttrString(PYAuthObject, methname);

    /* See if such method is defined */
    if (*methptr == NULL)
        syslog(L_NOTICE, "python method %s not found", methname);
    else {
        /* See if it is callable */
        if (PyCallable_Check(*methptr) == 0) {
	    syslog(L_ERROR, "python object %s found but not a function", methname);
	    Py_DECREF(*methptr);
	    *methptr = NULL;
	}
    }
}


/*
**  Look up all the known authentication/authorization methods and set up
**  pointers to them so that we could call them from nnrpd.
*/
void
PYdefmethods(void)
{
    /* Get a reference to authenticate() method */
    PYdefonemethod(&authenticate_method, "authenticate");

    /* Get a reference to authorize() method */
    PYdefonemethod(&authorize_method, "authorize");

    /* Get a reference to close() method */
    PYdefonemethod(&close_method, "close");
}


/*
**  Called when nnrpd starts -- this gets the scripts hooked in.
*/
void
PY_setup(void)
{
    int  authnum;

    /* Export $PYTHONPATH to let Python find the scripts */
    setenv("PYTHONPATH", innconf->pathfilter, 1);

    /* Load up the interpreter ;-O */
    Py_Initialize();

    /* It makes Python sad when its stdout and stderr are closed. */
    if (feof(stdout) || feof(stderr))
        PyRun_SimpleString
	  ("import sys; sys.stdout=sys.stderr=open('/dev/null', 'a')");

    /* See it Python initialized OK */
    if (!Py_IsInitialized ()) {
        syslog(L_ERROR, "python interpreter NOT initialized");
	return;
    }
    syslog(L_NOTICE, "python interpreter initialized OK");

    /* Build a module interface to certain nnrpd functions */
    Py_InitModule("nnrpd", nnrpdPyMethods);

    /* Load up external nntpd auth module */
    PYAuthModule = PyImport_ImportModule(_PATH_PYTHON_AUTH_M);
    if (PYAuthModule == NULL)
        syslog(L_ERROR, "failed to import external python module");

    /* See if nnrpd auth object is defined in auth module */
    if (PYAuthObject == NULL)
        syslog(L_ERROR, "python auth object is not defined");
    else {
        /* Set up pointers to known Python methods */
        PYdefmethods();
        syslog(L_NOTICE, "some python methods defined. good.");
    }

    /*
    ** Grab space for authinfo dictionary so we aren't forever
    ** recreating them.
    */
    PYauthinfo = PyDict_New();
    PYauthitem = NEW(PyObject *, _PY_MAX_AUTH_ITEM);
    PYauthkey = NEW(PyObject *, _PY_MAX_AUTH_ITEM);

    /* Preallocate keys for the authinfo dictionary (up to PY_MAX_AUTH_ITEM) */
    authnum = 0;
    PYauthkey[authnum++] = PyString_InternFromString("type");
    PYauthkey[authnum++] = PyString_InternFromString("hostname");
    PYauthkey[authnum++] = PyString_InternFromString("ipaddress");
    PYauthkey[authnum++] = PyString_InternFromString("interface");
    PYauthkey[authnum++] = PyString_InternFromString("user");
    PYauthkey[authnum++] = PyString_InternFromString("pass");
    PYauthkey[authnum++] = PyString_InternFromString("newsgroup");
}
#endif /* defined(DO_PYTHON) */
