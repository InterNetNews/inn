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
#include "inn/hashtab.h"

#if defined(DO_PYTHON)

/* Python redefines _POSIX_C_SOURCE, so undef it to suppress warnings. */
#undef _POSIX_C_SOURCE
#include "Python.h"

/* values relate name of hook to array index */
#define PYTHONauthen           1
#define PYTHONaccess           2
#define PYTHONdynamic          3

#define PYTHONtypes_max        4

/* values relate type of method to array index */
#define PYTHONmain             1
#define PYTHONinit             2
#define PYTHONclose            3

#define PYTHONmethods_max      4

/* key names for attributes dictionary */
#define PYTHONhostname         "hostname"
#define PYTHONipaddress        "ipaddress"
#define PYTHONport             "port"
#define PYTHONinterface        "interface"
#define PYTHONintipaddr        "intipaddr"
#define PYTHONintport          "intport"
#define PYTHONuser             "user"
#define PYTHONpass             "pass"
#define PYTHONtype             "type"
#define PYTHONnewsgroup        "newsgroup"

/* Max number of items in dictionary to pass to auth methods */
#define	_PY_MAX_AUTH_ITEM	10


/* Pointers to external Python objects */
PyObject	*PYAuthObject = NULL;

/* Dictionary of params to pass to authentication methods */
PyObject	*PYauthinfo = NULL;
PyObject	**PYauthitem = NULL;

/* Forward declaration */
static PyObject *PY_set_auth_hook(PyObject *dummy, PyObject *args);
void PY_load_python(void);
PyObject* PY_setup(int type, int method, char *file);
static const void *file_key(const void *p);
static bool file_equal(const void *k, const void *p);
static void file_free(void *p);
static void file_trav(void *data, void* null);

bool   PythonLoaded = false;

/* structure for storage of attributes for a module file */
typedef struct PyFile {
  char          *file;  
  bool          loaded[PYTHONtypes_max];
  PyObject	*procs[PYTHONtypes_max][PYTHONmethods_max];
} PyFile;

/* hash for storing files */
struct hash *files;

/* for passing the dynamic module filename from perm.c */
char*    dynamic_file;

/*
** Authenticate connecting host by username&password.
**
** Return NNTP reply code as returned by Python method or -1 if method
** is not defined.
*/
int
PY_authenticate(char* file, char *Username, char *Password, char *errorstring,
                char *newUser)
{
    PyObject    *result, *item, *proc;
    int         authnum;
    int         code, i;
    char        *temp;

    PY_load_python();
    proc = PY_setup(PYTHONauthen, PYTHONmain, file);

    /* Return if authentication method is not defined */
    if (proc == NULL)
        return -1;

    /* Initialize PythonAuthObject with connect method specific items */
    authnum = 0;

    /* Client hostname */
    PYauthitem[authnum] = PyBuffer_FromMemory(ClientHost, strlen(ClientHost));
    PyDict_SetItemString(PYauthinfo, PYTHONhostname, PYauthitem[authnum++]);

    /* Client IP number */
    PYauthitem[authnum] = PyBuffer_FromMemory(ClientIpString, strlen(ClientIpString));
    PyDict_SetItemString(PYauthinfo, PYTHONipaddress, PYauthitem[authnum++]);

    /* Client port number */
    PYauthitem[authnum] = PyInt_FromLong(ClientPort);
    PyDict_SetItemString(PYauthinfo, PYTHONport, PYauthitem[authnum++]);

    /* Server interface the connection comes to */
    PYauthitem[authnum] = PyBuffer_FromMemory(ServerHost, strlen(ServerHost));
    PyDict_SetItemString(PYauthinfo, PYTHONinterface, PYauthitem[authnum++]);

    /* Server IP number */
    PYauthitem[authnum] = PyBuffer_FromMemory(ServerIpString, strlen(ServerIpString));
    PyDict_SetItemString(PYauthinfo, PYTHONintipaddr, PYauthitem[authnum++]);

    /* Server port number */
    PYauthitem[authnum] = PyInt_FromLong(ServerPort);
    PyDict_SetItemString(PYauthinfo, PYTHONintport, PYauthitem[authnum++]);

    /* Username if known */
    if (Username == NULL) {
        PYauthitem[authnum] = Py_None;
    } else {
        PYauthitem[authnum] = PyBuffer_FromMemory(Username, strlen(Username));
    }
    PyDict_SetItemString(PYauthinfo, PYTHONuser, PYauthitem[authnum++]);

    /* Password if known */
    if (Password == NULL) {
        PYauthitem[authnum] = Py_None;
    } else {
        PYauthitem[authnum] = PyBuffer_FromMemory(Password, strlen(Password));
    }
    PyDict_SetItemString(PYauthinfo, PYTHONpass, PYauthitem[authnum++]);

    /* Now invoke authenticate method and see if it likes this user */
    result = PyObject_CallFunction(proc, (char *) "O", PYauthinfo);

    /* Check the response */
    if (result == NULL || !PyTuple_Check(result) 
        || ((PyTuple_Size(result) != 2) && (PyTuple_Size(result) != 3)))
    {
        syslog(L_ERROR, "python authenticate method returned wrong result");
	Reply("%d Internal Error (7).  Goodbye\r\n", NNTP_ACCESS_VAL);
	ExitWithStats(1, true);
    }

    /* Get the NNTP response code */
    item = PyTuple_GetItem(result, 0);

    /* Check the item */
    if (!PyInt_Check(item))
    {
        syslog(L_ERROR, "python authenticate method returned bad NNTP response code");
	Reply("%d Internal Error (7).  Goodbye\r\n", NNTP_ACCESS_VAL);
	ExitWithStats(1, true);
    }

    /* Store the code */
    code = PyInt_AS_LONG(item);

    /* Get the error string */
    item = PyTuple_GetItem(result, 1);

    /* Check the item */
    if (!PyString_Check(item))
    {
        syslog(L_ERROR, "python authenticate method returned bad error string");
	Reply("%d Internal Error (7).  Goodbye\r\n", NNTP_ACCESS_VAL);
	ExitWithStats(1, true);
    }

    /* Store error string */
    temp = PyString_AS_STRING(item);
    strlcpy(errorstring, temp, BIG_BUFFER);
    
    if (PyTuple_Size(result) == 3) {
        
        /* Get the username string */
        item = PyTuple_GetItem(result, 2);
        
        /* Check the item */
        if (!PyString_Check(item)) {
            syslog(L_ERROR, "python authenticate method returned bad username string");
            Reply("%d Internal Error (7).  Goodbye\r\n", NNTP_ACCESS_VAL);
            ExitWithStats(1, true);
        }

        /* Store user string */
        temp = PyString_AS_STRING(item);
        strlcpy(newUser, temp, BIG_BUFFER);
    }

    /* Clean up the dictionary object */
    PyDict_Clear(PYauthinfo);

    /* Clean up dictionary items */
    for (i = 0; i < authnum; i++) {
        if (PYauthitem[i] != Py_None) {
            Py_DECREF(PYauthitem[i]);
        }
    }

    /* Log auth result */
    syslog(L_NOTICE, "python authenticate method succeeded, return code %d, error string %s", code, errorstring);

    /* Return response code */
    return code;
}

/*
** Create an access group based on the values returned by the script in file
**
*/
void
PY_access(char* file, struct vector *access_vec, char *Username)
{
    PyObject	*result, *key, *value, *proc;
    char	*skey, *svalue, *temp;
    int		authnum;
    int		i;

    PY_load_python();
    proc = PY_setup(PYTHONaccess, PYTHONmain, file);

    /* Exit if access method is not defined */
    if (proc == NULL) {
        syslog(L_ERROR, "python access method not defined");
 	Reply("%d Internal Error (7).  Goodbye\r\n", NNTP_ACCESS_VAL);
 	ExitWithStats(1, true);
     }
 
    /* Initialize PythonAuthObject with group method specific items */
    authnum = 0;

    /* Client hostname */
    PYauthitem[authnum] = PyBuffer_FromMemory(ClientHost, strlen(ClientHost));
    PyDict_SetItemString(PYauthinfo, PYTHONhostname, PYauthitem[authnum++]);

    /* Client IP number */
    PYauthitem[authnum] = PyBuffer_FromMemory(ClientIpString, strlen(ClientIpString));
    PyDict_SetItemString(PYauthinfo, PYTHONipaddress, PYauthitem[authnum++]);

    /* Client port number */
    PYauthitem[authnum] = PyInt_FromLong(ClientPort);
    PyDict_SetItemString(PYauthinfo, PYTHONport, PYauthitem[authnum++]);

    /* Server interface the connection comes to */
    PYauthitem[authnum] = PyBuffer_FromMemory(ServerHost, strlen(ServerHost));
    PyDict_SetItemString(PYauthinfo, PYTHONinterface, PYauthitem[authnum++]);

    /* Server IP number */
    PYauthitem[authnum] = PyBuffer_FromMemory(ServerIpString, strlen(ServerIpString));
    PyDict_SetItemString(PYauthinfo, PYTHONintipaddr, PYauthitem[authnum++]);

    /* Server port number */
    PYauthitem[authnum] = PyInt_FromLong(ServerPort);
    PyDict_SetItemString(PYauthinfo, PYTHONintport, PYauthitem[authnum++]);

    /* Username */
    PYauthitem[authnum] = PyBuffer_FromMemory(Username, strlen(Username));
    PyDict_SetItemString(PYauthinfo, PYTHONuser, PYauthitem[authnum++]);
 
    /* Password is not known */
    PYauthitem[authnum] = Py_None;
    PyDict_SetItemString(PYauthinfo, PYTHONpass, PYauthitem[authnum++]);

    /*
     * Now invoke newsgroup access method
     */
    result = PyObject_CallFunction(proc, (char *) "O", PYauthinfo);

    /* Check the response */
    if (result == NULL || result == Py_None || !PyDict_Check(result)) {
        syslog(L_ERROR, "python access method returned wrong result - expected a dictionary");
	Reply("%d Internal Error (7).  Goodbye\r\n", NNTP_ACCESS_VAL);
	ExitWithStats(1, true);
     }
 
    /* resize vector to dictionary length */
    vector_resize(access_vec, PyDict_Size(result) - 1);

    /* store dict values in proper format in access vector */
    i = 0;
    while(PyDict_Next(result, &i, &key, &value)) {
        if (!PyString_Check(key)) {
            syslog(L_ERROR, "python access method return dictionary key %i not a string", i);
            Reply("%d Internal Error (7).  Goodbye\r\n", NNTP_ACCESS_VAL);
            ExitWithStats(1, false);
        }
        if (!PyString_Check(value)) {
            syslog(L_ERROR, "python access method return dictionary value %i not a string", i);
            Reply("%d Internal Error (7).  Goodbye\r\n", NNTP_ACCESS_VAL);
            ExitWithStats(1, false);
        }
        
        temp = PyString_AsString(key);
        skey = xstrdup(temp);
        
        temp = PyString_AsString(value);
        svalue = xstrdup(temp);
        
        skey = strcat(skey, ": \"");
        skey = strcat(skey, svalue);
        skey = strcat(skey, "\"\n");
        vector_add(access_vec, skey);
        
        free(skey);
        free(svalue);
    }
 
    /* Clean up the dictionary object */
    PyDict_Clear(PYauthinfo);
    /* Clean up dictionary items */
    for (i = 0; i < authnum; i++) {
        if (PYauthitem[i] != Py_None) {
            Py_DECREF(PYauthitem[i]);
	}
    }

    /* Log auth result */
    syslog(L_NOTICE, "python access method succeeded");
}

/*
** Initialize dynamic access control code
*/

void
PY_dynamic_init (char* file)
{
  dynamic_file = xstrdup(file);
  PY_use_dynamic = true;
}


/*
** Determine dynamic user access rights to a given newsgroup.
**
** Return 0 if requested privelege is granted or positive value
** and a reply_message pointer initialized with reply message.
** Return negative value if dynamic method is not defined.
*/
int
PY_dynamic(char *Username, char *NewsGroup, int PostFlag, char **reply_message)
{
    PyObject	*result, *proc;
    char	*string, *temp;
    int		authnum;
    int		i;

    PY_load_python();
    proc = PY_setup(PYTHONdynamic, PYTHONmain, dynamic_file);

    /* Return if dynamic method is not defined */
    if (proc == NULL)
        return -1;

    /* Initialize PythonAuthObject with group method specific items */
    authnum = 0;

    /* Client hostname */
    PYauthitem[authnum] = PyBuffer_FromMemory(ClientHost, strlen(ClientHost));
    PyDict_SetItemString(PYauthinfo, PYTHONhostname, PYauthitem[authnum++]);

    /* Client IP number */
    PYauthitem[authnum] = PyBuffer_FromMemory(ClientIpString, strlen(ClientIpString));
    PyDict_SetItemString(PYauthinfo, PYTHONipaddress, PYauthitem[authnum++]);

    /* Client port number */
    PYauthitem[authnum] = PyInt_FromLong(ClientPort);
    PyDict_SetItemString(PYauthinfo, PYTHONport, PYauthitem[authnum++]);

    /* Server interface the connection comes to */
    PYauthitem[authnum] = PyBuffer_FromMemory(ServerHost, strlen(ServerHost));
    PyDict_SetItemString(PYauthinfo, PYTHONinterface, PYauthitem[authnum++]);

    /* Server IP number */
    PYauthitem[authnum] = PyBuffer_FromMemory(ServerIpString, strlen(ServerIpString));
    PyDict_SetItemString(PYauthinfo, PYTHONintipaddr, PYauthitem[authnum++]);

    /* Server port number */
    PYauthitem[authnum] = PyInt_FromLong(ServerPort);
    PyDict_SetItemString(PYauthinfo, PYTHONintport, PYauthitem[authnum++]);
    
    /* Username */
    PYauthitem[authnum] = PyBuffer_FromMemory(Username, strlen(Username));
    PyDict_SetItemString(PYauthinfo, PYTHONuser, PYauthitem[authnum++]);
    
    /* Password is not known */
    PYauthitem[authnum] = Py_None;
    PyDict_SetItemString(PYauthinfo, PYTHONpass, PYauthitem[authnum++]);

    /* Assign authentication type */
    PYauthitem[authnum] =
        PyBuffer_FromMemory((char *)(PostFlag ? "post" : "read"), 4);
    PyDict_SetItemString(PYauthinfo, PYTHONtype, PYauthitem[authnum++]);
 
    /* Newsgroup user tries to access */
    PYauthitem[authnum] = PyBuffer_FromMemory(NewsGroup, strlen(NewsGroup));
    PyDict_SetItemString(PYauthinfo, PYTHONnewsgroup,  PYauthitem[authnum++]);
    
    /*
     * Now invoke newsgroup dynamic access method and see if
     * it likes this user to access this newsgroup.
     */
    result = PyObject_CallFunction(proc, (char *) "O", PYauthinfo);

    /* Check the response */
    if (result == NULL || (result != Py_None && !PyString_Check(result)))
    {
        syslog(L_ERROR, "python dyanmic method (%s access) returned wrong result", PostFlag ? "post" : "read");
	Reply("%d Internal Error (7).  Goodbye\r\n", NNTP_ACCESS_VAL);
	ExitWithStats(1, false);
    }

    /* Get the response string */
    if (result == Py_None) {
        string = NULL;
    } else {
        temp = PyString_AS_STRING(result);
        string = xstrdup(temp);
    }
    /* Clean up the dictionary object */
    PyDict_Clear(PYauthinfo);

    /* Clean up dictionary items */
    for (i = 0; i < authnum; i++) {
        if (PYauthitem[i] != Py_None) {
            Py_DECREF(PYauthitem[i]);
        }
    }

    /* Log auth result */
    syslog(L_NOTICE, "python dynamic method (%s access) succeeded, refusion string: %s", PostFlag ? "post" : "read", string == NULL ? "<empty>" : string);

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
PY_close_python(void)
{
    if (files != NULL) {
	hash_traverse(files, file_trav, NULL);
	hash_free(files);
    }
    if (dynamic_file != NULL)
	free(dynamic_file);
}

/*
** Traversal function for PY_close_python
*/
void
file_trav(void *data, void* null UNUSED)
{
    PyFile *fp = data;
    int j;
    PyObject	*result, *func;

    for (j = 1; j < PYTHONtypes_max; j++) {
        if (fp->loaded[j] != false) {
            func = fp->procs[j][PYTHONclose];
              if (func != NULL) {
                  result = PyObject_CallFunction(func, NULL);
                  Py_XDECREF(result);
              }
        }
    }
}

/*
**  Python's syslog module isn't compiled in by default.  It's easier
**  to do it this way, and the switch block looks pretty in a color
**  editor).
*/
static PyObject *
PY_syslog(PyObject *self UNUSED, PyObject *args)
{
    char        *loglevel;
    int         levellen;
    char        *logmsg;
    int         msglen;
    int         priority;

    /* Get loglevel and message */
    if (!PyArg_ParseTuple(args, (char *) "s#s#", &loglevel, &levellen,
                          &logmsg, &msglen))
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
**  Make the internal nnrpd module's functions visible to Python.  Python
**  annoyingly doesn't use const where appropriate in its structure
**  definitions, so we have to add casts for all of the string parameters that
**  we're initializing with constant strings.
*/
#define METHOD(name, func, flags, help) \
    { (char *)(name), (func), (flags), (char *)(help) }

static PyMethodDef nnrpdPyMethods[] = {
    METHOD("set_auth_hook", PY_set_auth_hook, METH_VARARGS, ""),
    METHOD("syslog",        PY_syslog,        METH_VARARGS, ""),
    METHOD(NULL,            NULL,             0,            "")
};


/*
**  Called by the external module so it can register itself with nnrpd.
*/
static PyObject *
PY_set_auth_hook(PyObject *dummy UNUSED, PyObject *args)
{
    PyObject    *result = NULL;
    PyObject    *temp;

    /* set_auth_hook method should return a pointer to nnrpd auth object */
    if (PyArg_ParseTuple(args, (char *) "O:set_auth_hook", &temp)) {
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
** Load the Python interpreter
*/
void
PY_load_python(void)
{
    if (!PythonLoaded) {
        /* add path for nnrpd module */    
        setenv("PYTHONPATH", innconf->pathfilter, 1);

        /* Load up the interpreter ;-O */
        Py_Initialize();
    
        /* It makes Python sad when its stdout and stderr are closed. */
        if (feof(stdout) || feof(stderr))
            PyRun_SimpleString("import sys; sys.stdout=sys.stderr=open('/dev/null', 'a')");
   
        /* See if Python initialized OK */
        if (!Py_IsInitialized ()) {
            syslog(L_ERROR, "python interpreter NOT initialized");
            return;
        }


        /* Build a module interface to certain nnrpd functions */
        Py_InitModule((char *) "nnrpd", nnrpdPyMethods);

        /*
        ** Grab space for authinfo dictionary so we aren't forever
        ** recreating them.
        */
        PYauthinfo = PyDict_New();
        PYauthitem = xcalloc(_PY_MAX_AUTH_ITEM, sizeof(PyObject *));

        /* create hash to store file attributes */
        
        files = hash_create(4, hash_string, file_key,
                            file_equal, file_free);

        PythonLoaded = true;

        syslog(L_NOTICE, "python interpreter initialized OK");
    }
}

/*
**  Check that a method exists and is callable.	 Set up a pointer to
**  the corresponding PyObject, or NULL if not found.
*/
static void
PYdefonemethod(PyFile *fp, int type, int method, const char *methname)
{
    PyObject **methptr;

    methptr = &fp->procs[type][method];

    /* Get a pointer to given method */
    *methptr = PyObject_GetAttrString(PYAuthObject, (char *) methname);

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
**  Look up all the known python methods and set up
**  pointers to them so that we could call them from nnrpd.
*/
static void
PYdefmethods(PyFile *fp)
{
    /* Get a reference to authenticate() method */
    PYdefonemethod(fp, PYTHONauthen, PYTHONmain, "authenticate");

    /* Get a reference to authen_init() method */
    PYdefonemethod(fp, PYTHONauthen, PYTHONinit, "authen_init");
    
    /* Get a reference to authen_close() method */
    PYdefonemethod(fp, PYTHONauthen, PYTHONclose, "authen_close");

    /* Get a reference to access() method */
    PYdefonemethod(fp, PYTHONaccess, PYTHONmain, "access");
    
    /* Get a reference to access_init() method */
    PYdefonemethod(fp, PYTHONaccess, PYTHONinit, "access_init");
    
    /* Get a reference to access_close() method */
    PYdefonemethod(fp, PYTHONaccess, PYTHONclose, "access_close");
    
    /* Get a reference to dynamic() method */
    PYdefonemethod(fp, PYTHONdynamic, PYTHONmain, "dynamic");
    
    /* Get a reference to dynamic_init() method */
    PYdefonemethod(fp, PYTHONdynamic, PYTHONinit, "dynamic_init");
    
    /* Get a reference to dynamic_close() method */
    PYdefonemethod(fp, PYTHONdynamic, PYTHONclose, "dynamic_close");
}


/*
**  Called when a python hook is needed -- this gets the scripts hooked in.
*/
PyObject*
PY_setup(int type, int method, char *file)
{
    int  i;
    PyFile *fp;
    PyObject    *result;

    /* check to see if this file is in files */
    if (!(hash_lookup(files, file))) {
        fp = xmalloc(sizeof(PyFile));
        fp->file = xstrdup(file);

        for (i = 1; i < PYTHONtypes_max; i++) {
            fp->loaded[i] = false;
        }
        
        /* Load up external module */
        (void) PyImport_ImportModule(file);

        /* See if nnrpd auth object is defined in auth module */
        if (PYAuthObject == NULL) {
            syslog(L_ERROR, "python auth object is not defined");
            Reply("%d Internal Error (7).  Goodbye\r\n", NNTP_ACCESS_VAL);
            PY_close_python();
            ExitWithStats(1, false);
        } else {
            /* Set up pointers to known Python methods */
            PYdefmethods(fp);
        }
        hash_insert(files, file, fp);

        if ((!fp->loaded[type]) && (fp->procs[type][PYTHONinit] != NULL)) {
            result = PyObject_CallFunction(fp->procs[type][PYTHONinit], NULL);
            if (result != NULL) {
                Py_XDECREF(result);
            }
            fp->loaded[type] = true;
        }
        return fp->procs[type][method];
    }
    return NULL;
}

/*
**  Return the key (filename) from a file struct, used by the hash table.
*/
static const void *
file_key(const void *p)
{
    const struct PyFile *f = p;

    return f->file;
}

/*
**  Check to see if a provided key matches the key of a PyFile struct,
**  used by the hash table.
*/
static bool
file_equal(const void *k, const void *p)
{
    const char *key = k;
    const struct PyFile *f = p;

    return strcmp(key, f->file) == 0;
}

/*
**  Free a file, used by the hash table.
*/
static void
file_free(void *p)
{
    struct PyFile *fp = p;
    int i, j;

    free(fp->file);

    for (i = 1; i < PYTHONtypes_max; i++) {
        for (j = 1; j < PYTHONmethods_max; j++) {
            if (fp->procs[i][j] != NULL) {
                Py_DECREF(fp->procs[i][j]);
            }
        }
    }

    free(fp);
}

#endif /* defined(DO_PYTHON) */
