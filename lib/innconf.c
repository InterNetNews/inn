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
**   * The table in this file.
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
#include "libinn.h"
#include "paths.h"

/* Instantiation of the global innconf variable. */
struct innconf *innconf = NULL;

/* Data types used to express the mappings from the configuration parse into
   the innconf struct. */

enum type {
    TYPE_BOOLEAN,
    TYPE_NUMBER,
    TYPE_STRING
};

struct config {
    const char *name;
    size_t location;
    enum type type;
    struct {
        bool boolean;
        long integer;
        const char *string;
    } defaults;
};

/* The following macros are helpers to make it easier to define the table that
   specifies how to convert the configuration file into a struct. */

#define K(name)         (#name), offsetof(struct innconf, name)

#define BOOL(def)       TYPE_BOOLEAN, { (def),     0,  NULL }
#define NUMBER(def)     TYPE_NUMBER,  {     0, (def),  NULL }
#define STRING(def)     TYPE_STRING,  {     0,     0, (def) }

/* Accessor macros to get a pointer to a value inside a struct. */
#define CONF_BOOL(conf, offset)   (bool *) (void *)((char *) (conf) + (offset))
#define CONF_LONG(conf, offset)   (long *) (void *)((char *) (conf) + (offset))
#define CONF_STRING(conf, offset) (char **)(void *)((char *) (conf) + (offset))

/* Special notes:

   checkincludedtext and localmaxartisize are used by both nnrpd and inews,
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

   newsrequeue also uses nntplinklog, but the parameter should go away
   completely anyway.

   timer is currently used in various places, but it may be best to replace
   it with individual settings for innd and innfeed, and any other code that
   wants to use it.

   maxforks is used by rnews and nnrpd.  I'm not sure this is that useful of
   a setting to have.
*/

const struct config config_table[] = {
    { K(domain),                STRING  (NULL) },
    { K(enableoverview),        BOOL    (true) },
    { K(fromhost),              STRING  (NULL) },
    { K(groupbaseexpiry),       BOOL    (true) },
    { K(mailcmd),               STRING  (NULL) },
    { K(maxforks),              NUMBER  (10) },
    { K(mta),                   STRING  (NULL) },
    { K(nicekids),              NUMBER  (4) },
    { K(ovmethod),              STRING  (NULL) },
    { K(pathhost),              STRING  (NULL) },
    { K(rlimitnofile),          NUMBER  (-1) },
    { K(server),                STRING  (NULL) },
    { K(sourceaddress),         STRING  (NULL) },
    { K(sourceaddress6),        STRING  (NULL) },
    { K(timer),                 NUMBER  (0) },

    { K(patharchive),           STRING  (NULL) },
    { K(patharticles),          STRING  (NULL) },
    { K(pathbin),               STRING  (NULL) },
    { K(pathcontrol),           STRING  (NULL) },
    { K(pathdb),                STRING  (NULL) },
    { K(pathetc),               STRING  (NULL) },
    { K(pathfilter),            STRING  (NULL) },
    { K(pathhttp),              STRING  (NULL) },
    { K(pathincoming),          STRING  (NULL) },
    { K(pathlog),               STRING  (NULL) },
    { K(pathnews),              STRING  (NULL) },
    { K(pathoutgoing),          STRING  (NULL) },
    { K(pathoverview),          STRING  (NULL) },
    { K(pathrun),               STRING  (NULL) },
    { K(pathspool),             STRING  (NULL) },
    { K(pathtmp),               STRING  (NULL) },

    /* The following settings are specific to innd. */
    { K(artcutoff),             NUMBER  (10) },
    { K(badiocount),            NUMBER  (5) },
    { K(bindaddress),           STRING  (NULL) },
    { K(bindaddress6),          STRING  (NULL) },
    { K(blockbackoff),          NUMBER  (120) },
    { K(chaninacttime),         NUMBER  (600) },
    { K(chanretrytime),         NUMBER  (300) },
    { K(datamovethreshold),     NUMBER  (8192) },
    { K(dontrejectfiltered),    BOOL    (false) },
    { K(hiscachesize),          NUMBER  (0) },
    { K(icdsynccount),          NUMBER  (10) },
    { K(ignorenewsgroups),      BOOL    (false) },
    { K(linecountfuzz),         NUMBER  (0) },
    { K(logartsize),            BOOL    (true) },
    { K(logcancelcomm),         BOOL    (false) },
    { K(logipaddr),             BOOL    (true) },
    { K(logsitename),           BOOL    (true) },
    { K(maxartsize),            NUMBER  (1000000) },
    { K(maxconnections),        NUMBER  (50) },
    { K(mergetogroups),         BOOL    (false) },
    { K(nntpactsync),           NUMBER  (200) },
    { K(nntplinklog),           BOOL    (false) },
    { K(noreader),              BOOL    (false) },
    { K(pathalias),             STRING  (NULL) },
    { K(pathcluster),           STRING  (NULL) },
    { K(pauseretrytime),        NUMBER  (300) },
    { K(peertimeout),           NUMBER  (3600) },
    { K(port),                  NUMBER  (119) },
    { K(readerswhenstopped),    BOOL    (false) },
    { K(refusecybercancels),    BOOL    (false) },
    { K(remembertrash),         BOOL    (true) },
    { K(stathist),              STRING  (NULL) },
    { K(status),                NUMBER  (0) },
    { K(verifycancels),         BOOL    (false) },
    { K(wanttrash),             BOOL    (false) },
    { K(wipcheck),              NUMBER  (5) },
    { K(wipexpire),             NUMBER  (10) },
    { K(xrefslave),             BOOL    (false) },

    /* The following settings are specific to nnrpd. */
    { K(addnntppostingdate),    BOOL    (true) },
    { K(addnntppostinghost),    BOOL    (true) },
    { K(allownewnews),          BOOL    (true) },
    { K(backoffauth),           BOOL    (false) },
    { K(backoffdb),             STRING  (NULL) },
    { K(backoffk),              NUMBER  (1) },
    { K(backoffpostfast),       NUMBER  (0) },
    { K(backoffpostslow),       NUMBER  (1) },
    { K(backofftrigger),        NUMBER  (10000) },
    { K(checkincludedtext),     BOOL    (false) },
    { K(clienttimeout),         NUMBER  (600) },
    { K(complaints),            STRING  (NULL) },
    { K(initialtimeout),        NUMBER  (10) },
    { K(keyartlimit),           NUMBER  (100000) },
    { K(keylimit),              NUMBER  (512) },
    { K(keymaxwords),           NUMBER  (250) },
    { K(keywords),              BOOL    (false) },
    { K(localmaxartsize),       NUMBER  (1000000) },
    { K(maxcmdreadsize),        NUMBER  (BUFSIZ) },
    { K(msgidcachesize),        NUMBER  (10000) },
    { K(moderatormailer),       STRING  (NULL) },
    { K(nfsreader),             BOOL    (false) },
    { K(nfsreaderdelay),        NUMBER  (60) },
    { K(nicenewnews),           NUMBER  (0) },
    { K(nicennrpd),             NUMBER  (0) },
    { K(nnrpdauthsender),       BOOL    (false) },
    { K(nnrpdloadlimit),        NUMBER  (16) },
    { K(nnrpdoverstats),        BOOL    (false) },
    { K(organization),          STRING  (NULL) },
    { K(readertrack),           BOOL    (false) },
    { K(spoolfirst),            BOOL    (false) },
    { K(strippostcc),           BOOL    (false) },

    /* The following settings are used by nnrpd and rnews. */
    { K(nnrpdposthost),         STRING  (NULL) },
    { K(nnrpdpostport),         NUMBER  (119) },

    /* The following settings are specific to the storage subsystem. */
    { K(articlemmap),           BOOL    (false) },
    { K(cnfscheckfudgesize),    NUMBER  (0) },
    { K(immediatecancel),       BOOL    (false) },
    { K(keepmmappedthreshold),  NUMBER  (1024) },
    { K(nfswriter),             BOOL    (false) },
    { K(nnrpdcheckart),         BOOL    (true) },
    { K(overcachesize),         NUMBER  (15) },
    { K(ovgrouppat),            STRING  (NULL) },
    { K(storeonxref),           BOOL    (true) },
    { K(tradindexedmmap),       BOOL    (true) },
    { K(useoverchan),           BOOL    (false) },
    { K(wireformat),            BOOL    (false) },

    /* The following settings are specific to the history subsystem. */
    { K(hismethod),             STRING  (NULL) },

    /* The following settings are specific to rc.news. */
    { K(docnfsstat),            BOOL    (false) },
    { K(innflags),              STRING  (NULL) },
    { K(pgpverify),             BOOL    (false) },

    /* The following settings are specific to innwatch. */
    { K(doinnwatch),            BOOL    (true) },
    { K(innwatchbatchspace),    NUMBER  (800) },
    { K(innwatchlibspace),      NUMBER  (25000) },
    { K(innwatchloload),        NUMBER  (1000) },
    { K(innwatchhiload),        NUMBER  (2000) },
    { K(innwatchpauseload),     NUMBER  (1500) },
    { K(innwatchsleeptime),     NUMBER  (600) },
    { K(innwatchspoolnodes),    NUMBER  (200) },
    { K(innwatchspoolspace),    NUMBER  (8000) },

    /* The following settings are specific to scanlogs. */
    { K(logcycles),             NUMBER  (3) },
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
        innconf->pathtmp = xstrdup(_PATH_TMP);

    /* All of the paths are relative to other paths if not set except for
       pathnews, which is required to be set by innconf_validate. */
    if (innconf->pathbin == NULL)
        innconf->pathbin = concatpath(innconf->pathnews, "bin");
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
        innconf->pathhttp = xstrdup(innconf->pathlog);
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
    unsigned int i;
    bool *bool_ptr;
    long *long_ptr;
    const char *char_ptr;
    char **string;
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
            long_ptr = CONF_LONG(config, config_table[i].location);
            if (!config_param_integer(group, config_table[i].name, long_ptr))
                *long_ptr = config_table[i].defaults.integer;
            break;
        case TYPE_STRING:
            if (!config_param_string(group, config_table[i].name, &char_ptr))
                char_ptr = config_table[i].defaults.string;
            string = CONF_STRING(config, config_table[i].location);
            *string = (char_ptr == NULL) ? NULL : xstrdup(char_ptr);
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
    long threshold;

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
    threshold = innconf->datamovethreshold;
    if (threshold <= 0 || threshold > 1024 * 1024) {
        config_error_param(group, "datamovethreshold",
                           "maximum value for datamovethreshold is 1MB");
        innconf->datamovethreshold = 1024 * 1024;
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
    group = config_parse_file(path == NULL ? _PATH_CONFIG : path);
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
    group = config_parse_file(path == NULL ? _PATH_CONFIG : path);
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
                           "warning: NNTP draft (15) states inactivity"
                           " timeouts MUST be at least three minutes");
    if (innconf->clienttimeout < 3 * 60)
        config_error_param(group, "clienttimeout",
                           "warning: NNTP draft (15) states inactivity"
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

    for (i = 0; i < ARRAY_SIZE(config_table); i++)
        if (config_table[i].type == TYPE_STRING) {
            p = *CONF_STRING(config, config_table[i].location);
            if (p != NULL)
                free(p);
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
            *p = toupper(*p);
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
**  Print a single integer value with appropriate quoting.
*/
static void
print_number(FILE *file, const char *key, long value,
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
            *p = toupper(*p);
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
        fprintf(file, "%s\n", value != NULL ? value : "");
        break;
    case INNCONF_QUOTE_SHELL:
        upper = xstrdup(key);
        for (p = upper; *p != '\0'; p++)
            *p = toupper(*p);
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
        fprintf(file, "$%s = '", key);
        for (letter = value; letter != NULL && *letter != '\0'; letter++) {
            if (*letter == '\'' || *letter == '\\')
                fputc('\\', file);
            fputc(*letter, file);
        }
        fputs("';\n", file);
        break;
    case INNCONF_QUOTE_TCL:
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
**  Print a single paramter to the given file.  Takes an index into the table
**  specifying the attribute to print and the quoting.
*/
static void
print_parameter(FILE *file, size_t i, enum innconf_quoting quoting)
{
    bool bool_val;
    long long_val;
    const char *string_val;

    switch (config_table[i].type) {
    case TYPE_BOOLEAN:
        bool_val = *CONF_BOOL(innconf, config_table[i].location);
        print_boolean(file, config_table[i].name, bool_val, quoting);
        break;
    case TYPE_NUMBER:
        long_val = *CONF_LONG(innconf, config_table[i].location);
        print_number(file, config_table[i].name, long_val, quoting);
        break;
    case TYPE_STRING:
        string_val = *CONF_STRING(innconf, config_table[i].location);
        print_string(file, config_table[i].name, string_val, quoting);
        break;
    default:
        die("internal error: invalid type in row %d of config table", i);
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
    unsigned int i;
    bool bool1, bool2;
    long long1, long2;
    const char *string1, *string2;
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
            long1 = *CONF_LONG(conf1, config_table[i].location);
            long2 = *CONF_LONG(conf2, config_table[i].location);
            if (long1 != long2) {
                warn("integer variable %s differs: %ld != %ld",
                     config_table[i].name, long1, long2);
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
        default:
            die("internal error: invalid type in row %d of config table", i);
            break;
        }
    return okay;
}
