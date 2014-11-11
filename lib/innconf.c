/*  $Id$
**
**  Manage the global innconf struct.
**
**  The functions in this file collapse the parse tree for inn.conf into the
**  innconf struct that's used throughout INN.  The code to collapse a
**  configuration parse tree into a struct is fairly generic and should
**  probably be moved into a separate library.
**
**  When adding new inn.conf parameters, make sure to add them in all of the
**  following places:
**
**   * The table in this file
**   * include/inn/innconf.h
**   * doc/pod/inn.conf.pod (and regenerate doc/man/inn.conf.5)
**   * Add the default value to samples/inn.conf.in
**
**  Please maintain the current organization of parameters.  There are two
**  different orders, one of which is a logical order used by the
**  documentation, the include file, and the sample file, and the other of
**  which is used in this file.  The order in this file is documentation of
**  where each parameter is used, for later work at breaking up this mess
**  of parameters into separate configuration groups for each INN subsystem.
*/

#include "config.h"
#include "clibrary.h"
#include <ctype.h>

#include "inn/confparse.h"
#include "inn/innconf.h"
#include "inn/messages.h"
#include "inn/vector.h"
#include "inn/libinn.h"
#include "inn/paths.h"

/* Instantiation of the global innconf variable. */
struct innconf *innconf = NULL;

/* Data types used to express the mappings from the configuration parse into
   the innconf struct. */

enum type {
    TYPE_BOOLEAN,
    TYPE_NUMBER,
    TYPE_UNUMBER,
    TYPE_STRING,
    TYPE_LIST
};

struct config {
    const char *name;
    size_t location;
    enum type type;
    struct {
        bool boolean;
        long signed_number;
        unsigned long unsigned_number;
        const char *string;
        const struct vector *list;
    } defaults;
};

/* The following macros are helpers to make it easier to define the table that
   specifies how to convert the configuration file into a struct. */

#define K(name)         (#name), offsetof(struct innconf, name)

#define BOOL(def)       TYPE_BOOLEAN,   { (def),     0,     0,  NULL,  NULL }
#define NUMBER(def)     TYPE_NUMBER,    {     0, (def),     0,  NULL,  NULL }
#define UNUMBER(def)    TYPE_UNUMBER,   {     0,     0, (def),  NULL,  NULL }
#define STRING(def)     TYPE_STRING,    {     0,     0,     0, (def),  NULL }
#define LIST(def)       TYPE_LIST,      {     0,     0,     0,  NULL, (def) }

/* Accessor macros to get a pointer to a value inside a struct. */
#define CONF_BOOL(conf, offset)     (bool *)          (void *)((char *) (conf) + (offset))
#define CONF_NUMBER(conf, offset)   (long *)          (void *)((char *) (conf) + (offset))
#define CONF_UNUMBER(conf, offset)  (unsigned long *) (void *)((char *) (conf) + (offset))
#define CONF_STRING(conf, offset)   (char **)         (void *)((char *) (conf) + (offset))
#define CONF_LIST(conf, offset)     (struct vector **)(void *)((char *) (conf) + (offset))

/* Special notes:

   checkincludedtext and localmaxartsize are used by both nnrpd and inews,
   but inews should probably just let nnrpd do that checking.

   organization is used by both nnrpd and inews.  Perhaps inews should just
   let nnrpd set it.

   mergetogroups is currently used by nnrpd for permission checking on
   posting.  I think the check should always be performed based on the
   newsgroup to which the user is actually posting, and nnrpd should let
   innd do the merging.

   useoverchan is only used in innd and overchan.  It should probably be
   something the storage system knows.  Ideally, the storage system would
   handle overchan itself, but that would require a lot of infrastructure;
   in the interim, it could be something that programs could ask the
   overview subsystem about.

   doinnwatch and docnfsstat are used by rc.news currently, but really
   should be grouped with the appropriate subsystem.

   timer is currently used in various places, but it may be best to replace
   it with individual settings for innd and innfeed, and any other code that
   wants to use it.

   maxforks is used by rnews and nnrpd.  I'm not sure this is that useful of
   a setting to have.
*/

const struct config config_table[] = {
    { K(domain),                  STRING  (NULL) },
    { K(enableoverview),          BOOL    (true) },
    { K(extraoverviewadvertised), LIST    (NULL) },
    { K(extraoverviewhidden),     LIST    (NULL) },
    { K(fromhost),                STRING  (NULL) },
    { K(groupbaseexpiry),         BOOL    (true) },
    { K(mailcmd),                 STRING  (NULL) },
    { K(maxforks),                UNUMBER   (10) },
    { K(mta),                     STRING  (NULL) },
    { K(nicekids),                NUMBER     (4) },
    { K(ovmethod),                STRING  (NULL) },
    { K(pathhost),                STRING  (NULL) },
    { K(rlimitnofile),            NUMBER    (-1) },
    { K(server),                  STRING  (NULL) },
    { K(sourceaddress),           STRING  (NULL) },
    { K(sourceaddress6),          STRING  (NULL) },
    { K(timer),                   UNUMBER    (0) },

    { K(runasuser),               STRING  (RUNASUSER) },
    { K(runasgroup),              STRING  (RUNASGROUP) },

    { K(patharchive),             STRING  (NULL) },
    { K(patharticles),            STRING  (NULL) },
    { K(pathbin),                 STRING  (NULL) },
    { K(pathcontrol),             STRING  (NULL) },
    { K(pathdb),                  STRING  (NULL) },
    { K(pathetc),                 STRING  (NULL) },
    { K(pathfilter),              STRING  (NULL) },
    { K(pathhttp),                STRING  (NULL) },
    { K(pathincoming),            STRING  (NULL) },
    { K(pathlog),                 STRING  (NULL) },
    { K(pathnews),                STRING  (NULL) },
    { K(pathoutgoing),            STRING  (NULL) },
    { K(pathoverview),            STRING  (NULL) },
    { K(pathrun),                 STRING  (NULL) },
    { K(pathspool),               STRING  (NULL) },
    { K(pathtmp),                 STRING  (NULL) },

    /* The following settings are specific to innd. */
    { K(artcutoff),               UNUMBER   (10) },
    { K(badiocount),              UNUMBER    (5) },
    { K(bindaddress),             STRING  (NULL) },
    { K(bindaddress6),            STRING  (NULL) },
    { K(blockbackoff),            UNUMBER  (120) },
    { K(chaninacttime),           UNUMBER  (600) },
    { K(chanretrytime),           UNUMBER  (300) },
    { K(datamovethreshold),       UNUMBER (8192) },
    { K(dontrejectfiltered),      BOOL   (false) },
    { K(hiscachesize),            UNUMBER  (256) },
    { K(htmlstatus),              BOOL    (true) },
    { K(icdsynccount),            UNUMBER   (10) },
    { K(ignorenewsgroups),        BOOL   (false) },
    { K(incominglogfrequency),    UNUMBER  (200) },
    { K(linecountfuzz),           UNUMBER    (0) },
    { K(logartsize),              BOOL    (true) },
    { K(logcancelcomm),           BOOL   (false) },
    { K(logipaddr),               BOOL    (true) },
    { K(logsitename),             BOOL    (true) },
    { K(logstatus),               BOOL   (false) },
    { K(logtrash),                BOOL    (true) },
    { K(maxartsize),              UNUMBER (1000000) },
    { K(maxconnections),          UNUMBER   (50) },
    { K(mergetogroups),           BOOL   (false) },
    { K(nntplinklog),             BOOL   (false) },
    { K(noreader),                BOOL   (false) },
    { K(pathalias),               STRING  (NULL) },
    { K(pathcluster),             STRING  (NULL) },
    { K(pauseretrytime),          UNUMBER  (300) },
    { K(peertimeout),             UNUMBER (3600) },
    { K(port),                    UNUMBER  (119) },
    { K(readerswhenstopped),      BOOL   (false) },
    { K(refusecybercancels),      BOOL   (false) },
    { K(remembertrash),           BOOL    (true) },
    { K(stathist),                STRING  (NULL) },
    { K(status),                  UNUMBER    (0) },
    { K(verifycancels),           BOOL   (false) },
    { K(verifygroups),            BOOL   (false) },
    { K(wanttrash),               BOOL   (false) },
    { K(wipcheck),                UNUMBER    (5) },
    { K(wipexpire),               UNUMBER   (10) },
    { K(xrefslave),               BOOL   (false) },

    /* The following settings are specific to nnrpd. */
    { K(addinjectiondate),        BOOL    (true) },
    { K(addinjectionpostingaccount), BOOL (false) },
    { K(addinjectionpostinghost), BOOL    (true) },
    { K(allownewnews),            BOOL    (true) },
    { K(backoffauth),             BOOL   (false) },
    { K(backoffdb),               STRING  (NULL) },
    { K(backoffk),                UNUMBER    (1) },
    { K(backoffpostfast),         UNUMBER    (0) },
    { K(backoffpostslow),         UNUMBER    (1) },
    { K(backofftrigger),          UNUMBER (10000) },
    { K(checkincludedtext),       BOOL   (false) },
    { K(clienttimeout),           UNUMBER (1800) },
    { K(complaints),              STRING  (NULL) },
    { K(initialtimeout),          UNUMBER   (10) },
    { K(keyartlimit),             UNUMBER (100000) },
    { K(keylimit),                UNUMBER  (512) },
    { K(keymaxwords),             UNUMBER  (250) },
    { K(keywords),                BOOL   (false) },
    { K(localmaxartsize),         UNUMBER (1000000) },
    { K(maxcmdreadsize),          UNUMBER (BUFSIZ) },
    { K(msgidcachesize),          UNUMBER (16000) },
    { K(moderatormailer),         STRING  (NULL) },
    { K(nfsreader),               BOOL   (false) },
    { K(nfsreaderdelay),          UNUMBER   (60) },
    { K(nicenewnews),             UNUMBER    (0) },
    { K(nicennrpd),               UNUMBER    (0) },
    { K(nnrpdflags),              STRING    ("") },
    { K(nnrpdauthsender),         BOOL   (false) },
    { K(nnrpdloadlimit),          UNUMBER   (16) },
    { K(nnrpdoverstats),          BOOL   (false) },
    { K(organization),            STRING  (NULL) },
    { K(readertrack),             BOOL   (false) },
    { K(spoolfirst),              BOOL   (false) },
    { K(strippostcc),             BOOL   (false) },
#ifdef HAVE_OPENSSL
    { K(tlscafile),               STRING    ("") },
    { K(tlscapath),               STRING  (NULL) },
    { K(tlscertfile),             STRING  (NULL) },
    { K(tlskeyfile),              STRING  (NULL) },
    { K(tlsciphers),              STRING  (NULL) },
    { K(tlscompression),          BOOL   (false) },
    { K(tlspreferserverciphers),  BOOL    (true) },
    { K(tlsprotocols),            LIST    (NULL) },
#endif /* HAVE_OPENSSL */

    /* The following settings are used by nnrpd and rnews. */
    { K(nnrpdposthost),           STRING  (NULL) },
    { K(nnrpdpostport),           UNUMBER  (119) },

    /* The following settings are specific to the storage subsystem. */
    { K(articlemmap),             BOOL    (true) },
    { K(cnfscheckfudgesize),      UNUMBER    (0) },
    { K(immediatecancel),         BOOL   (false) },
    { K(keepmmappedthreshold),    UNUMBER (1024) },
    { K(nfswriter),               BOOL   (false) },
    { K(nnrpdcheckart),           BOOL    (true) },
    { K(overcachesize),           UNUMBER   (64) },
    { K(ovgrouppat),              STRING  (NULL) },
    { K(storeonxref),             BOOL    (true) },
    { K(tradindexedmmap),         BOOL    (true) },
    { K(useoverchan),             BOOL   (false) },
    { K(wireformat),              BOOL   (false) },

    /* The following settings are specific to the history subsystem. */
    { K(hismethod),               STRING  (NULL) },

    /* The following settings are specific to rc.news. */
    { K(docnfsstat),              BOOL   (false) },
    { K(innflags),                STRING  (NULL) },
    { K(pgpverify),               BOOL   (false) },

    /* The following settings are specific to innwatch. */
    { K(doinnwatch),              BOOL    (true) },
    { K(innwatchbatchspace),      UNUMBER (4000) },
    { K(innwatchlibspace),        UNUMBER (25000) },
    { K(innwatchloload),          UNUMBER (1000) },
    { K(innwatchhiload),          UNUMBER (2000) },
    { K(innwatchpauseload),       UNUMBER (1500) },
    { K(innwatchsleeptime),       UNUMBER  (600) },
    { K(innwatchspoolnodes),      UNUMBER  (200) },
    { K(innwatchspoolspace),      UNUMBER (25000) },

    /* The following settings are specific to scanlogs. */
    { K(logcycles),               UNUMBER    (3) },
};


/*
**  Set some defaults that cannot be included in the table because they depend
**  on other elements or require function calls to set.  Called after the
**  configuration file is read, so any that have to override what's read have
**  to free whatever values are set by the file.
*/
static void
innconf_set_defaults(void)
{
    char *value;

    /* Some environment variables override settings in inn.conf. */
    value = getenv("FROMHOST");
    if (value != NULL) {
        if (innconf->fromhost != NULL)
            free(innconf->fromhost);
        innconf->fromhost = xstrdup(value);
    }
    value = getenv("NNTPSERVER");
    if (value != NULL) {
        if (innconf->server != NULL)
            free(innconf->server);
        innconf->server = xstrdup(value);
    }
    value = getenv("ORGANIZATION");
    if (value != NULL) {
        if (innconf->organization != NULL)
            free(innconf->organization);
        innconf->organization = xstrdup(value);
    }
    value = getenv("INND_BIND_ADDRESS");
    if (value != NULL) {
        if (innconf->bindaddress != NULL)
            free(innconf->bindaddress);
        innconf->bindaddress = xstrdup(value);
    }
    value = getenv("INND_BIND_ADDRESS6");
    if (value != NULL) {
        if (innconf->bindaddress6 != NULL)
            free(innconf->bindaddress6);
        innconf->bindaddress6 = xstrdup(value);
    }

    /* Some parameters have defaults that depend on other parameters. */
    if (innconf->fromhost == NULL)
        innconf->fromhost = xstrdup(GetFQDN(innconf->domain));
    if (innconf->pathhost == NULL)
        innconf->pathhost = xstrdup(GetFQDN(innconf->domain));
    if (innconf->pathtmp == NULL)
        innconf->pathtmp = xstrdup(INN_PATH_TMP);

    /* All of the paths are relative to other paths if not set except for
       pathnews, which is required to be set by innconf_validate. */
    if (innconf->pathbin == NULL)
        innconf->pathbin = concatpath(innconf->pathnews, "bin");
    if (innconf->pathcontrol == NULL)
        innconf->pathcontrol = concatpath(innconf->pathbin, "control");
    if (innconf->pathfilter == NULL)
        innconf->pathfilter = concatpath(innconf->pathbin, "filter");
    if (innconf->pathdb == NULL)
        innconf->pathdb = concatpath(innconf->pathnews, "db");
    if (innconf->pathetc == NULL)
        innconf->pathetc = concatpath(innconf->pathnews, "etc");
    if (innconf->pathrun == NULL)
        innconf->pathrun = concatpath(innconf->pathnews, "run");
    if (innconf->pathlog == NULL)
        innconf->pathlog = concatpath(innconf->pathnews, "log");
    if (innconf->pathhttp == NULL)
        innconf->pathhttp = concatpath(innconf->pathnews, "http");
    if (innconf->pathspool == NULL)
        innconf->pathspool = concatpath(innconf->pathnews, "spool");
    if (innconf->patharticles == NULL)
        innconf->patharticles = concatpath(innconf->pathspool, "articles");
    if (innconf->pathoverview == NULL)
        innconf->pathoverview = concatpath(innconf->pathspool, "overview");
    if (innconf->pathoutgoing == NULL)
        innconf->pathoutgoing = concatpath(innconf->pathspool, "outgoing");
    if (innconf->pathincoming == NULL)
        innconf->pathincoming = concatpath(innconf->pathspool, "incoming");
    if (innconf->patharchive == NULL)
        innconf->patharchive = concatpath(innconf->pathspool, "archive");

    /* One other parameter depends on pathbin. */
    if (innconf->mailcmd == NULL)
        innconf->mailcmd = concatpath(innconf->pathbin, "innmail");

    /* Create empty vectors of extra overview fields if they haven't already
     * been created. */
    if (innconf->extraoverviewadvertised == NULL)
        innconf->extraoverviewadvertised = vector_new();
    if (innconf->extraoverviewhidden == NULL)
        innconf->extraoverviewhidden = vector_new();

    /* Defaults used only if TLS (SSL) is supported. */
#ifdef HAVE_OPENSSL
    if (innconf->tlscapath == NULL)
        innconf->tlscapath = xstrdup(innconf->pathetc);
    if (innconf->tlscertfile == NULL)
        innconf->tlscertfile = concatpath(innconf->pathetc, "cert.pem");
    if (innconf->tlskeyfile == NULL)
        innconf->tlskeyfile = concatpath(innconf->pathetc, "key.pem");
#endif
}


/*
**  Given a config_group struct representing the inn.conf file, parse that
**  into an innconf struct and return the newly allocated struct.  This
**  routine should be pulled out into a library for smashing configuration
**  file parse results into structs.
*/
static struct innconf *
innconf_parse(struct config_group *group)
{
    unsigned int i, j;
    bool *bool_ptr;
    long *signed_number_ptr;
    unsigned long *unsigned_number_ptr;
    const char *char_ptr;
    char **string;
    const struct vector *vector_ptr;
    struct vector **list;
    struct innconf *config;

    config = xmalloc(sizeof(struct innconf));
    for (i = 0; i < ARRAY_SIZE(config_table); i++)
        switch (config_table[i].type) {
        case TYPE_BOOLEAN:
            bool_ptr = CONF_BOOL(config, config_table[i].location);
            if (!config_param_boolean(group, config_table[i].name, bool_ptr))
                *bool_ptr = config_table[i].defaults.boolean;
            break;
        case TYPE_NUMBER:
            signed_number_ptr = CONF_NUMBER(config, config_table[i].location);
            if (!config_param_signed_number(group, config_table[i].name, signed_number_ptr))
                *signed_number_ptr = config_table[i].defaults.signed_number;
            break;
        case TYPE_UNUMBER:
            unsigned_number_ptr = CONF_UNUMBER(config, config_table[i].location);
            if (!config_param_unsigned_number(group, config_table[i].name, unsigned_number_ptr))
                *unsigned_number_ptr = config_table[i].defaults.unsigned_number;
            break;
        case TYPE_STRING:
            if (!config_param_string(group, config_table[i].name, &char_ptr))
                char_ptr = config_table[i].defaults.string;
            string = CONF_STRING(config, config_table[i].location);
            *string = (char_ptr == NULL) ? NULL : xstrdup(char_ptr);
            break;
        case TYPE_LIST:
            /* vector_ptr contains the value taken from inn.conf or the
             * default value from config_table; *list points to the inn.conf
             * structure in memory for this parameter.
             * We have to do a deep copy of vector_ptr because, like char_ptr,
             * it is freed by config_free() called by other parts of INN. */
            if (!config_param_list(group, config_table[i].name, &vector_ptr))
                vector_ptr = config_table[i].defaults.list;
            list = CONF_LIST(config, config_table[i].location);
            *list = vector_new();
            if (vector_ptr != NULL && vector_ptr->strings != NULL) {
                vector_resize(*list, vector_ptr->count);
                for (j = 0; j < vector_ptr->count; j++) {
                    if (vector_ptr->strings[j] != NULL) {
                        vector_add(*list, vector_ptr->strings[j]);
                    }
                }
            }
            break;
        default:
            die("internal error: invalid type in row %u of config table", i);
            break;
        }
    return config;
}


/*
**  Check the configuration file for consistency and ensure that mandatory
**  settings are present.  Returns true if the file is okay and false
**  otherwise.
*/
static bool
innconf_validate(struct config_group *group)
{
    bool okay = true;

    if (GetFQDN(innconf->domain) == NULL) {
        warn("hostname does not resolve or domain not set in inn.conf");
        okay = false;
    }
    if (innconf->mta == NULL) {
        warn("must set mta in inn.conf");
        okay = false;
    }
    if (innconf->pathnews == NULL) {
        warn("must set pathnews in inn.conf");
        okay = false;
    }
    if (innconf->hismethod == NULL) {
        warn("must set hismethod in inn.conf");
        okay = false;
    }
    if (innconf->enableoverview && innconf->ovmethod == NULL) {
        warn("ovmethod must be set in inn.conf if enableoverview is true");
        okay = false;
    }

    if (innconf->datamovethreshold > 1024 * 1024) {
        config_error_param(group, "datamovethreshold",
                           "maximum value for datamovethreshold is 1MB");
        innconf->datamovethreshold = 1024 * 1024;
    }

    if (innconf->keywords) {
        bool found = false;
        unsigned int i;
        if (innconf->extraoverviewadvertised->strings != NULL) {
            for (i = 0; i < innconf->extraoverviewadvertised->count; i++) {
                if (innconf->extraoverviewadvertised->strings[i] != NULL &&
                    (strcasecmp(innconf->extraoverviewadvertised->strings[i], "Keywords") == 0)) {
                    found = true;
                    break;
                }
            }
        }
        if (innconf->extraoverviewhidden->strings != NULL) {
            for (i = 0; i < innconf->extraoverviewhidden->count; i++) {
                if (innconf->extraoverviewhidden->strings[i] != NULL &&
                    (strcasecmp(innconf->extraoverviewhidden->strings[i], "Keywords") == 0)) {
                    found = true;
                    break;
                }
            }
        }

        if (!found) {
            config_error_param(group, "keywords",
                               "keyword generation is useless if the Keywords:"
                               " header is not stored in the overview");
            innconf->keywords = false;
        }
    }

    return okay;
}


/*
**  Read in inn.conf.  Takes a single argument, which is either NULL to read
**  the default configuration file or a path to an alternate configuration
**  file to read.  Returns true if the file was read successfully and false
**  otherwise.
*/
bool
innconf_read(const char *path)
{
    struct config_group *group;
    char *tmpdir;

    if (innconf != NULL)
        innconf_free(innconf);
    if (path == NULL)
        path = getenv("INNCONF");
    group = config_parse_file(path == NULL ? INN_PATH_CONFIG : path);
    if (group == NULL)
        return false;

    innconf = innconf_parse(group);
    if (!innconf_validate(group))
        return false;
    config_free(group);
    innconf_set_defaults();

    /* It's not clear that this belongs here, but it was done by the old
       configuration parser, so this is a convenient place to do it. */
    tmpdir = getenv("TMPDIR");
    if (tmpdir == NULL || strcmp(tmpdir, innconf->pathtmp) != 0)
        if (setenv("TMPDIR", innconf->pathtmp, true) != 0) {
            warn("cannot set TMPDIR in the environment");
            return false;
        }

    return true;
}


/*
**  Check an inn.conf file.  This involves reading it in and then additionally
**  making sure that there are no keys defined in the inn.conf file that
**  aren't recognized.  This doesn't have to be very fast (and isn't).
**  Returns true if everything checks out successfully, and false otherwise.
**
**  A lot of code is duplicated with innconf_read here and should be
**  refactored.
*/
bool
innconf_check(const char *path)
{
    struct config_group *group;
    struct vector *params;
    size_t set, known;
    bool found;
    bool okay = true;

    if (innconf != NULL)
        innconf_free(innconf);
    if (path == NULL)
        path = getenv("INNCONF");
    group = config_parse_file(path == NULL ? INN_PATH_CONFIG : path);
    if (group == NULL)
        return false;

    innconf = innconf_parse(group);
    if (!innconf_validate(group))
        return false;

    /* Now, do the work that innconf_read doesn't do.  Get a list of
       parameters defined in innconf and then walk our list of valid
       parameters and see if there are any set that we don't recognize. */
    params = config_params(group);
    for (set = 0; set < params->count; set++) {
        found = false;
        for (known = 0; known < ARRAY_SIZE(config_table); known++)
            if (strcmp(params->strings[set], config_table[known].name) == 0)
                found = true;
        if (!found) {
            config_error_param(group, params->strings[set],
                               "unknown parameter %s", params->strings[set]);
            okay = false;
        }
    }

    /* Check and warn about a few other parameters. */
    if (innconf->peertimeout < 3 * 60)
        config_error_param(group, "peertimeout",
                           "warning: NNTP RFC 3977 states inactivity"
                           " timeouts MUST be at least three minutes");
    if (innconf->clienttimeout < 3 * 60)
        config_error_param(group, "clienttimeout",
                           "warning: NNTP RFC 3977 states inactivity"
                           " timeouts MUST be at least three minutes");

    /* All done.  Free the parse tree and return. */
    config_free(group);
    return okay;
}


/*
**  Free innconf, requiring some complexity since all strings stored in the
**  innconf struct are allocated memory.  This routine is mostly generic to
**  any struct smashed down from a configuration file parse.
*/
void
innconf_free(struct innconf *config)
{
    unsigned int i;
    char *p;
    struct vector *q;

    for (i = 0; i < ARRAY_SIZE(config_table); i++) {
        if (config_table[i].type == TYPE_STRING) {
            p = *CONF_STRING(config, config_table[i].location);
            if (p != NULL)
                free(p);
        }
        if (config_table[i].type == TYPE_LIST) {
            q = *CONF_LIST(config, config_table[i].location);
            if (q != NULL)
                vector_free(q);
        }
    }
    free(config);
}


/*
**  Print a single boolean value with appropriate quoting.
*/
static void
print_boolean(FILE *file, const char *key, bool value,
              enum innconf_quoting quoting)
{
    char *upper, *p;

    switch (quoting) {
    case INNCONF_QUOTE_NONE:
        fprintf(file, "%s\n", value ? "true" : "false");
        break;
    case INNCONF_QUOTE_SHELL:
        upper = xstrdup(key);
        for (p = upper; *p != '\0'; p++)
            *p = toupper((unsigned char) *p);
        fprintf(file, "%s=%s; export %s;\n", upper, value ? "true" : "false",
                upper);
        free(upper);
        break;
    case INNCONF_QUOTE_PERL:
        fprintf(file, "$%s = '%s';\n", key, value ? "true" : "false");
        break;
    case INNCONF_QUOTE_TCL:
        fprintf(file, "set inn_%s \"%s\"\n", key, value ? "true" : "false");
        break;
    }
}


/*
**  Print a single signed integer value with appropriate quoting.
*/
static void
print_signed_number(FILE *file, const char *key, long value,
                    enum innconf_quoting quoting)
{
    char *upper, *p;

    switch (quoting) {
    case INNCONF_QUOTE_NONE:
        fprintf(file, "%ld\n", value);
        break;
    case INNCONF_QUOTE_SHELL:
        upper = xstrdup(key);
        for (p = upper; *p != '\0'; p++)
            *p = toupper((unsigned char) *p);
        fprintf(file, "%s=%ld; export %s;\n", upper, value, upper);
        free(upper);
        break;
    case INNCONF_QUOTE_PERL:
        fprintf(file, "$%s = %ld;\n", key, value);
        break;
    case INNCONF_QUOTE_TCL:
        fprintf(file, "set inn_%s %ld\n", key, value);
        break;
    }
}


/*
**  Print a single unsigned integer value with appropriate quoting.
*/
static void
print_unsigned_number(FILE *file, const char *key, unsigned long value,
                      enum innconf_quoting quoting)
{
    char *upper, *p;

    switch (quoting) {
        case INNCONF_QUOTE_NONE:
            fprintf(file, "%lu\n", value);
            break;
        case INNCONF_QUOTE_SHELL:
            upper = xstrdup(key);
            for (p = upper; *p != '\0'; p++)
                *p = toupper((unsigned char) *p);
            fprintf(file, "%s=%lu; export %s;\n", upper, value, upper);
            free(upper);
            break;
        case INNCONF_QUOTE_PERL:
            fprintf(file, "$%s = %lu;\n", key, value);
            break;
        case INNCONF_QUOTE_TCL:
            fprintf(file, "set inn_%s %lu\n", key, value);
            break;
    }
}


/*
**  Print a single string value with appropriate quoting.
*/
static void
print_string(FILE *file, const char *key, const char *value,
             enum innconf_quoting quoting)
{
    char *upper, *p;
    const char *letter;
    static const char tcl_unsafe[] = "$[]{}\"\\";

    switch (quoting) {
    case INNCONF_QUOTE_NONE:
        /* Do not output NULL values.  They are not empty strings. */
        if (value == NULL) {
            return;
        }
        fprintf(file, "%s\n", value);
        break;
    case INNCONF_QUOTE_SHELL:
        /* Do not output NULL values.  They are not empty strings. */
        if (value == NULL) {
            return;
        }
        upper = xstrdup(key);
        for (p = upper; *p != '\0'; p++)
            *p = toupper((unsigned char) *p);
        fprintf(file, "%s='", upper);
        for (letter = value; letter != NULL && *letter != '\0'; letter++) {
            if (*letter == '\'')
                fputs("'\\''", file);
            else if (*letter == '\\')
                fputs("\\\\", file);
            else
                fputc(*letter, file);
        }
        fprintf(file, "'; export %s;\n", upper);
        free(upper);
        break;
    case INNCONF_QUOTE_PERL:
        if (value == NULL) {
            fprintf(file, "$%s = undef;\n", key);
            return;
        }
        fprintf(file, "$%s = '", key);
        for (letter = value; letter != NULL && *letter != '\0'; letter++) {
            if (*letter == '\'' || *letter == '\\')
                fputc('\\', file);
            fputc(*letter, file);
        }
        fputs("';\n", file);
        break;
    case INNCONF_QUOTE_TCL:
        /* Do not output NULL values.  They are not empty strings. */
        if (value == NULL) {
            return;
        }
        fprintf(file, "set inn_%s \"", key);
        for (letter = value; letter != NULL && *letter != '\0'; letter++) {
            if (strchr(tcl_unsafe, *letter) != NULL)
                fputc('\\', file);
            fputc(*letter, file);
        }
        fputs("\"\n", file);
        break;
    }
}


/*
**  Print a single list value with appropriate quoting.
*/
static void
print_list(FILE *file, const char *key, const struct vector *value,
           enum innconf_quoting quoting)
{
    char *upper, *p;
    const char *letter;
    unsigned int i;
    static const char tcl_unsafe[] = "$[]{}\"\\";

    switch (quoting) {
    case INNCONF_QUOTE_NONE:
        /* Do not output NULL values.  They are not empty lists. */
        if (value == NULL || value->strings == NULL) {
            return;
        }
        fprintf(file, "[ ");
        if (value != NULL && value->strings != NULL) {
            for (i = 0; i < value->count; i++) {
                /* No separation between strings. */
                fprintf(file, "%s ",
                        value->strings[i] != NULL ? value->strings[i] : "");
            }
        }
        fprintf(file, "]\n");
        break;
    case INNCONF_QUOTE_SHELL:
        /* Do not output NULL values.  They are not empty lists. */
        if (value == NULL || value->strings == NULL) {
            return;
        }
        upper = xstrdup(key);
        for (p = upper; *p != '\0'; p++)
            *p = toupper((unsigned char) *p);
        /* For interoperability reasons, we return a space-separated string
         * representing an array (pure Bourne shell does not have the notion
         * of an array for instance). */
        fprintf(file, "%s='", upper);
        if (value != NULL && value->strings != NULL) {
            for (i = 0; i < value->count; i++) {
                fprintf(file, "\"");
                for (letter = value->strings[i]; letter != NULL
                     && *letter != '\0'; letter++) {
                    if (*letter == '\'')
                        fputs("'\\''", file);
                    else if (*letter == '"')
                        fputs("\\\"", file);
                    else if (*letter == '\\')
                        fputs("\\\\", file);
                    else
                        fputc(*letter, file);
                }
                if (i == value->count - 1) {
                    fprintf(file, "\"");
                } else {
                    fprintf(file, "\" ");
                }
            }
        }
        fprintf(file, "'; export %s;\n", upper);
        free(upper);
        break;
    case INNCONF_QUOTE_PERL:
        /* Consider that an empty list is undefined. */
        if (value == NULL || value->strings == NULL) {
            fprintf(file, "@%s = undef;\n", key);
            return;
        }   
        fprintf(file, "@%s = ( ", key);
        if (value != NULL && value->strings != NULL) {
            for (i = 0; i < value->count; i++) {
                fprintf(file, "'");
                for (letter = value->strings[i]; letter != NULL
                     && *letter != '\0'; letter++) {
                    if (*letter == '\'' || *letter == '\\')
                        fputc('\\', file);
                    fputc(*letter, file);
                }
                if (i == value->count - 1) {
                    fprintf(file, "' ");
                } else {
                    fprintf(file, "', ");
                }
            }
        }
        fprintf(file, ");\n");
        break;
    case INNCONF_QUOTE_TCL:
        /* Do not output NULL values.  They are not empty lists. */
        if (value == NULL || value->strings == NULL) {
            return;
        }   
        fprintf(file, "set inn_%s { ", key);
        if (value != NULL && value->strings != NULL) {
            for (i = 0; i < value->count; i++) {
                fprintf(file, "\"");
                for (letter = value->strings[i]; letter != NULL
                     && *letter != '\0'; letter++) {
                    if (strchr(tcl_unsafe, *letter) != NULL)
                        fputc('\\', file);
                    fputc(*letter, file);
                }
                fprintf(file, "\" ");
            }
        }
        fprintf(file, "}\n");
        break;
    }
}



/*
**  Print a single parameter to the given file.  Take an index into the table
**  specifying the attribute to print and the quoting.
*/
static void
print_parameter(FILE *file, size_t i, enum innconf_quoting quoting)
{
    bool bool_val;
    long signed_number_val;
    unsigned long unsigned_number_val;
    const char *string_val;
    const struct vector *list_val;

    switch (config_table[i].type) {
    case TYPE_BOOLEAN:
        bool_val = *CONF_BOOL(innconf, config_table[i].location);
        print_boolean(file, config_table[i].name, bool_val, quoting);
        break;
    case TYPE_NUMBER:
        signed_number_val = *CONF_NUMBER(innconf, config_table[i].location);
        print_signed_number(file, config_table[i].name, signed_number_val, quoting);
        break;
    case TYPE_UNUMBER:
        unsigned_number_val = *CONF_UNUMBER(innconf, config_table[i].location);
        print_unsigned_number(file, config_table[i].name, unsigned_number_val, quoting);
        break;
    case TYPE_STRING:
        string_val = *CONF_STRING(innconf, config_table[i].location);
        print_string(file, config_table[i].name, string_val, quoting);
        break;
    case TYPE_LIST:
        list_val = *CONF_LIST(innconf, config_table[i].location);
        print_list(file, config_table[i].name, list_val, quoting);
        break;
    default:
        die("internal error: invalid type in row %lu of config table",
            (unsigned long) i);
        break;
    }
}


/*
**  Given a single parameter, find it in the table and print out its value.
*/
bool
innconf_print_value(FILE *file, const char *key, enum innconf_quoting quoting)
{
    size_t i;

    for (i = 0; i < ARRAY_SIZE(config_table); i++)
        if (strcmp(key, config_table[i].name) == 0) {
            print_parameter(file, i, quoting);
            return true;
        }
    return false;
}


/*
**  Dump the entire inn.conf configuration with appropriate quoting.
*/
void
innconf_dump(FILE *file, enum innconf_quoting quoting)
{
    size_t i;

    for (i = 0; i < ARRAY_SIZE(config_table); i++)
        print_parameter(file, i, quoting);
}


/*
**  Compare two innconf structs to see if they represent identical
**  configurations.  This routine is mostly used for testing.  Prints warnings
**  about where the two configurations differ and return false if they differ,
**  true if they match.  This too should be moved into a config smashing
**  library.
*/
bool
innconf_compare(struct innconf *conf1, struct innconf *conf2)
{
    unsigned int i, j;
    bool bool1, bool2;
    long signed_number1, signed_number2;
    unsigned long unsigned_number1, unsigned_number2;
    const char *string1, *string2;
    const struct vector *list1, *list2;
    bool okay = true;

    for (i = 0; i < ARRAY_SIZE(config_table); i++)
        switch (config_table[i].type) {
        case TYPE_BOOLEAN:
            bool1 = *CONF_BOOL(conf1, config_table[i].location);
            bool2 = *CONF_BOOL(conf2, config_table[i].location);
            if (bool1 != bool2) {
                warn("boolean variable %s differs: %d != %d",
                     config_table[i].name, bool1, bool2);
                okay = false;
            }
            break;
        case TYPE_NUMBER:
            signed_number1 = *CONF_NUMBER(conf1, config_table[i].location);
            signed_number2 = *CONF_NUMBER(conf2, config_table[i].location);
            if (signed_number1 != signed_number2) {
                warn("integer variable %s differs: %ld != %ld",
                     config_table[i].name, signed_number1, signed_number2);
                okay = false;
            }
            break;
        case TYPE_UNUMBER:
            unsigned_number1 = *CONF_UNUMBER(conf1, config_table[i].location);
            unsigned_number2 = *CONF_UNUMBER(conf2, config_table[i].location);
            if (unsigned_number1 != unsigned_number2) {
                warn("integer variable %s differs: %lu  != %lu",
                     config_table[i].name, unsigned_number1, unsigned_number2);
                okay = false;
            }
            break;
        case TYPE_STRING:
            string1 = *CONF_STRING(conf1, config_table[i].location);
            string2 = *CONF_STRING(conf2, config_table[i].location);
            if (string1 == NULL && string2 != NULL) {
                warn("string variable %s differs: NULL != %s",
                     config_table[i].name, string2);
                okay = false;
            } else if (string1 != NULL && string2 == NULL) {
                warn("string variable %s differs: %s != NULL",
                     config_table[i].name, string1);
                okay = false;
            } else if (string1 != NULL && string2 != NULL) {
                if (strcmp(string1, string2) != 0) {
                    warn("string variable %s differs: %s != %s",
                         config_table[i].name, string1, string2);
                    okay = false;
                }
            }
            break;
        case TYPE_LIST:
            list1 = *CONF_LIST(conf1, config_table[i].location);
            list2 = *CONF_LIST(conf2, config_table[i].location);
            /* Vectors are not resized when created in inn.conf.
             * Therefore, we can compare their length. */
            if (  (list1 == NULL && list2 != NULL)
               || (list1 != NULL && list2 == NULL)) {
                 warn("list variable %s differs: one is NULL",
                      config_table[i].name);
                 okay = false;
            } else if (list1 != NULL && list2 != NULL) {
                if (  (list1->strings == NULL && list2->strings != NULL)
                   || (list1->strings != NULL && list2->strings == NULL)) {
                    warn("list strings variable %s differs: one is NULL",
                         config_table[i].name);
                    okay = false;
                } else if (list1->strings != NULL && list2->strings != NULL) {
                    if (list1->count != list2->count) {
                        warn("list variable %s differs in length: %lu != %lu",
                             config_table[i].name, (unsigned long) list1->count,
                             (unsigned long) list2->count);
                        okay = false;
                    } else {
                        for (j = 0; j < list1->count; j++) {
                            if (list1->strings[j] == NULL
                                && list2->strings[j] != NULL) {
                                warn("list variable %s differs: NULL != %s",
                                     config_table[i].name, list2->strings[j]);
                                okay = false;
                                break;
                            } else if (list1->strings[j] != NULL
                                       && list2->strings[j] == NULL) {
                                warn("list variable %s differs: %s != NULL",
                                     config_table[i].name, list1->strings[j]);
                                okay = false;
                                break;
                            } else if (list1->strings[j] != NULL
                                       && list2->strings[j] != NULL) {
                                if (strcmp(list1->strings[j], list2->strings[j]) != 0) {
                                    warn("list variable %s differs at element %u: %s != %s",
                                         config_table[i].name, j+1,
                                         list1->strings[j], list2->strings[j]);
                                    okay = false;
                                    break;
                                }
                            }
                        }
                    }
                }
            }
            break;
        default:
            die("internal error: invalid type in row %d of config table", i);
            break;
        }
    return okay;
}
