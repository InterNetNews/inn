/*  $Id$
**
**  Parse a standard block-structured configuration file syntax.
**
**  Herein are all the parsing and access functions for the configuration
**  syntax used by INN.  See doc/config-* for additional documentation.
**
**  All entry point functions begin with config_*.  config_parse_file is
**  the entry point for most of the work done in this file; all other
**  functions access the parse tree that config_parse_file generates.
**
**  Functions are named by the structure or basic task they work on:
**
**      parameter_*     config_parameter structs.
**      group_*         config_group structs.
**      file_*          config_file structs (including all I/O).
**      token_*         The guts of the lexer.
**      parse_*         The guts of the parser.
**      error_*         Error reporting functions.
**      convert_*       Converting raw parameter values.
**
**  Each currently open file is represented by a config_file struct, which
**  contains the current parse state for that file, including the internal
**  buffer, a pointer to where in the buffer the next token starts, and the
**  current token.  Inclusion of additional files is handled by maintaining a
**  stack of config_file structs, so when one file is finished, the top struct
**  popped off the stack and parsing continues where it left off.
**
**  Since config_file structs contain the parse state, they're passed as an
**  argument to most functions.
**
**  A config_file struct contains a token struct, representing the current
**  token.  The configuration file syntax is specifically designed to never
**  require lookahead to parse; all parse decisions can be made on the basis
**  of the current state and a single token.  A token consists of a type and
**  an optional attached string.  Note that strings are allocated by the lexer
**  but are never freed by the lexer!  Any token with an associated string
**  should have that string copied into permanent storage (like the params
**  hash of a config_group) or freed.  error_unexpected_token will do the
**  latter.
**
**  Errors in the lexer are indicated by setting the token to TOKEN_ERROR.
**  All parsing errors are indicated by setting the error flag in the current
**  config_file struct.  Error recovery is *not* implemented by the current
**  algorithm; it would add a lot of complexity to the parsing algorithm and
**  the results still probably shouldn't be used by the calling program, so it
**  would only be useful to catch more than one syntax error per invocation
**  and it isn't expected that syntax errors will be that common.  Instead, if
**  something fails to parse, the whole parser unwinds and returns failure.
**
**  The config_param_* functions are used to retrieve the values of
**  parameters; each use a convert_* function to convert the raw parameter
**  value to the type specified by the user.  group_parameter_get can
**  therefore be the same for all parameter types, with all of the variations
**  encapsulated in the convert_* functions.
*/

#include "config.h"
#include "clibrary.h"
#include <errno.h>
#include <fcntl.h>

#include "inn/confparse.h"
#include "inn/hashtab.h"
#include "inn/messages.h"
#include "inn/vector.h"
#include "libinn.h"
#include "macros.h"


/* The types of tokens seen in configuration files. */
enum token_type {
    TOKEN_CRLF,
    TOKEN_STRING,
    TOKEN_QSTRING,
    TOKEN_PARAM,
    TOKEN_LBRACE,
    TOKEN_RBRACE,
    TOKEN_LANGLE,
    TOKEN_RANGLE,
    TOKEN_LBRACKET,
    TOKEN_RBRACKET,
    TOKEN_SEMICOLON,
    TOKEN_EOF,
    TOKEN_ERROR
};

/* The parse status of a file.  Variables marked internal are only used by
   file_* functions; other functions don't need to look at them.  Other
   variables are marked by what functions are responsible for maintaining
   them. */
struct config_file {
    int fd;                     /* Internal */
    char *buffer;               /* Internal */
    size_t bufsize;             /* Internal */
    const char *filename;       /* file_open */
    unsigned int line;          /* token_newline and token_quoted_string */
    bool error;                 /* Everyone */

    /* Set by file_* and token_*.  current == NULL indicates we've not yet
       read from the file. */
    char *current;

    /* Normally set by token_*, but file_read and file_read_more may set token
       to TOKEN_ERROR or TOKEN_EOF when those conditions are encountered.  In
       that situation, they also return false. */
    struct {
        enum token_type type;
        char *string;
    } token;
};

/* The types of parameters, used to distinguish the values of the union in the
   config_parameter_s struct. */
enum value_type {
    VALUE_UNKNOWN,
    VALUE_BOOL,
    VALUE_INTEGER,
    VALUE_NUMBER,
    VALUE_STRING,
    VALUE_LIST,
    VALUE_INVALID
};

/* Each setting is represented by one of these structs, stored in the params
   hash of a config group.  Since all of a config_group must be in the same
   file (either group->file for regular groups or group->included for groups
   whose definition is in an included file), we don't have to stash a file
   name here for error reporting but can instead get that from the enclosing
   group. */
struct config_parameter {
    char *key;
    char *raw_value;
    unsigned int line;          /* For error reporting. */
    enum value_type type;
    union {
        bool boolean;
        long integer;
        double number;
        char *string;
        struct vector *list;
    } value;
};

/* The type of a function that converts a raw parameter value to some other
   data type, storing the result in its second argument and returning true on
   success or false on failure. */
typedef bool (*convert_func)(struct config_parameter *, const char *, void *);

/* The basic element of configuration data, a group of parameters.  This is
   the only struct that is exposed to callers, and then only as an opaque
   data structure. */
struct config_group {
    char *type;
    char *tag;
    char *file;                 /* File in which the group starts. */
    unsigned int line;          /* Line number where the group starts. */
    char *included;             /* For group <file>, the included file. */
    struct hash *params;

    struct config_group *parent;
    struct config_group *child;
    struct config_group *next;
};


/* Parameter handling, used by the hash table stored in a config_group. */
static const void *parameter_key(const void *p);
static bool parameter_equal(const void *k, const void *p);
static void parameter_free(void *p);

/* Hash traversal function to collect parameters into a vector. */
static void parameter_collect(void *, void *);

/* Group handling. */
static struct config_group *group_new(const char *file, unsigned int line,
                                      const char *type, const char *tag);
static void group_free(struct config_group *);
static bool group_parameter_get(struct config_group *group, const char *key,
                                void *result, convert_func convert);

/* Parameter type conversion functions.  All take the parameter, the file, and
   a pointer to where the result can be placed. */
static bool convert_boolean(struct config_parameter *, const char *, void *);
static bool convert_integer(struct config_parameter *, const char *, void *);
static bool convert_string(struct config_parameter *, const char *, void *);

/* File I/O.  Many other functions also manipulate config_file structs; see
   the struct definition for notes on who's responsible for what. */
static struct config_file *file_open(const char *filename);
static bool file_read(struct config_file *);
static bool file_read_more(struct config_file *, ptrdiff_t offset);
static void file_close(struct config_file *);

/* The basic lexer function.  The token is stashed in file; the return value
   is just for convenience and duplicates that information. */
static enum token_type token_next(struct config_file *);

/* Handler functions for specific types of tokens.  These should only be
   called by token_next. */
static void token_simple(struct config_file *, enum token_type type);
static void token_newline(struct config_file *);
static void token_string(struct config_file *);
static void token_quoted_string(struct config_file *);

/* Handles whitespace for the rest of the lexer. */
static bool token_skip_whitespace(struct config_file *);

/* Handles comments for the rest of the lexer. */
static bool token_skip_comment(struct config_file *);

/* Parser functions to parse the named syntactic element. */
static bool parse_group_contents(struct config_group *, struct config_file *);
static enum token_type parse_parameter(struct config_group *,
                                       struct config_file *, char *key);

/* Error reporting functions. */
static void error_bad_unquoted_char(struct config_file *, char bad);
static void error_unexpected_token(struct config_file *,
                                   const char *expecting);


/*
**  Return the key from a parameter struct, used by the hash table.
*/
static const void *
parameter_key(const void *p)
{
    const struct config_parameter *param = p;

    return param->key;
}


/*
**  Check to see if a provided key matches the key of a parameter struct,
**  used by the hash table.
*/
static bool
parameter_equal(const void *k, const void *p)
{
    const char *key = k;
    const struct config_parameter *param = p;

    return EQ(key, param->key);
}


/*
**  Free a parameter, used by the hash table.
*/
static void
parameter_free(void *p)
{
    struct config_parameter *param = p;

    free(param->key);
    free(param->raw_value);
    if (param->type == VALUE_STRING) {
        free(param->value.string);
    } else if (param->type == VALUE_LIST) {
        vector_free(param->value.list);
    }
    free(param);
}


/*
**  Report an unexpected character while parsing a regular string and set the
**  current token type to TOKEN_ERROR.
*/
static void
error_bad_unquoted_char(struct config_file *file, char bad)
{
    warn("%s:%u: invalid character '%c' in unquoted string", file->filename,
         file->line, bad);
    file->token.type = TOKEN_ERROR;
    file->error = true;
}


/*
**  Report an unexpected token.  If the token is TOKEN_ERROR, don't print an
**  additional error message.  Takes a string saying what token was expected.
**  Sets the token to TOKEN_ERROR and frees the associated string if the
**  current token type is TOKEN_STRING, TOKEN_QSTRING, or TOKEN_PARAM.
*/
static void
error_unexpected_token(struct config_file *file, const char *expecting)
{
    const char *name;
    bool string = false;

    /* If the bad token type is a string, param, or quoted string, free the
       string associated with the token to avoid a memory leak. */
    if (file->token.type != TOKEN_ERROR) {
        switch (file->token.type) {
        case TOKEN_STRING:      name = "string";        string = true; break;
        case TOKEN_QSTRING:     name = "quoted string"; string = true; break;
        case TOKEN_PARAM:       name = "parameter";     string = true; break;
        case TOKEN_CRLF:        name = "end of line";   break;
        case TOKEN_LBRACE:      name = "'{'";           break;
        case TOKEN_RBRACE:      name = "'}'";           break;
        case TOKEN_LANGLE:      name = "'<'";           break;
        case TOKEN_RANGLE:      name = "'>'";           break;
        case TOKEN_LBRACKET:    name = "'['";           break;
        case TOKEN_RBRACKET:    name = "']'";           break;
        case TOKEN_SEMICOLON:   name = "';'";           break;
        case TOKEN_EOF:         name = "end of file";   break;
        default:                name = "unknown token"; break;
        }
        warn("%s:%u: parse error: saw %s, expecting %s", file->filename,
             file->line, name, expecting);
    }
    if (string) {
        free(file->token.string);
        file->token.string = NULL;
    }
    file->token.type = TOKEN_ERROR;
    file->error = true;
}


/*
**  Handle a simple token (a single character), advancing the file->current
**  pointer past it and setting file->token as appropriate.
*/
static void
token_simple(struct config_file *file, enum token_type type)
{
    file->current++;
    file->token.type = type;
    file->token.string = NULL;
}


/*
**  Handle a newline.  Skip any number of comments after the newline,
**  including reading more data from the file if necessary, and update
**  file->line as needed.
*/
static void
token_newline(struct config_file *file)
{
    /* If we're actually positioned on a newline, update file->line and skip
       over it.  Try to handle CRLF correctly, as a single line terminator
       that only increments the line count once, while still treating either
       CR or LF alone as line terminators in their own regard. */
    if (*file->current == '\n') {
        file->current++;
        file->line++;
    } else if (*file->current == '\r') {
        if (file->current[1] == '\n')
            file->current += 2;
        else if (file->current[1] != '\0')
            file->current++;
        else {
            if (!file_read(file)) {
                file->current++;
                return;
            }
            if (*file->current == '\n')
                file->current++;
        }
        file->line++;
    }

    if (!token_skip_whitespace(file))
        return;
    while (*file->current == '#') {
        if (!token_skip_comment(file))
            return;
        if (!token_skip_whitespace(file))
            return;
    }
    file->token.type = TOKEN_CRLF;
    file->token.string = NULL;
}


/*
**  Handle a string.  Only some characters are allowed in an unquoted string;
**  check that, since otherwise it could hide syntax errors.  Any whitespace
**  ends the token.  We have to distinguish between TOKEN_PARAM and
**  TOKEN_STRING; the former ends in a colon, unlike the latter.
*/
static void
token_string(struct config_file *file)
{
    int i;
    bool status;
    ptrdiff_t offset;
    bool done = false;
    bool colon = false;

    /* Use an offset from file->current rather than a pointer that moves
       through the buffer, since the base of file->current can change during a
       file_read_more() call and we don't want to have to readjust a
       pointer.  If we have to read more, adjust our counter back one
       character, since the nul was replaced by a new, valid character. */
    i = 0;
    while (!done) {
        switch (file->current[i]) {
        case '\t':  case '\r':  case '\n':  case ' ':   case ';':
            done = true;
            break;
        case '"':   case '<':   case '>':   case '[':
        case '\\':  case ']':   case '{':   case '}':
            error_bad_unquoted_char(file, file->current[i]);
            return;
        case ':':
            if (colon) {
                error_bad_unquoted_char(file, file->current[i]);
                return;
            }
            colon = true;
            break;
        case '\0':
            offset = file->current - file->buffer;
            status = file_read_more(file, offset);
            if (status)
                i--;
            else
                done = true;
            break;
        default:
            if (colon) {
                error_bad_unquoted_char(file, ':');
                return;
            }
        }
        if (!done)
            i++;
    }
    file->token.type = colon ? TOKEN_PARAM : TOKEN_STRING;
    file->token.string = xstrndup(file->current, i - colon);
    file->current += i;
}


/*
**  Handle a quoted string.  This token is unique as the only token that can
**  contain whitespace, even newlines if they're escaped, so we also have to
**  update file->line as we go.  Note that the quotes *are* included in the
**  string we stash in file->token, since they should be part of the raw_value
**  of a parameter.
*/
static void
token_quoted_string(struct config_file *file)
{
    int i;
    ptrdiff_t offset;
    bool status;
    bool done = false;

    /* Use an offset from file->current rather than a pointer that moves
       through the buffer, since the base of file->current can change during a
       file_read_more() call and we don't want to have to readjust a pointer.
       If we have to read more, adjust our counter back one character, since
       the nul was replaced by a new, valid character. */
    for (i = 1; !done; i++) {
        switch (file->current[i]) {
        case '"':
            done = true;
            break;
        case '\r':
        case '\n':
            warn("%s:%u: no close quote seen for quoted string",
                 file->filename, file->line);
            file->token.type = TOKEN_ERROR;
            file->error = true;
            return;
        case '\\':
            i++;
            if (file->current[i] == '\n')
                file->line++;

            /* CRLF should count as one line terminator.  Handle most cases of
               that here, but the case where CR is at the end of one buffer
               and LF at the beginning of the next has to be handled in the \0
               case below. */
            if (file->current[i] == '\r') {
                file->line++;
                if (file->current[i + 1] == '\n')
                    i++;
            }
            break;
        case '\0':
            offset = file->current - file->buffer;
            status = file_read_more(file, offset);
            if (status)
                i--;
            else {
                warn("%s:%u: end of file encountered while parsing quoted"
                     " string", file->filename, file->line);
                file->token.type = TOKEN_ERROR;
                file->error = true;
                return;
            }

            /* If the last character of the previous buffer was CR and the
               first character that we just read was LF, the CR must have been
               escaped which means that the LF is part of it, forming a CRLF
               line terminator.  Skip over the LF. */
            if (file->current[i] == '\r' && file->current[i + 1] == '\n')
                i++;

            break;
        default:
            break;
        }
    }
    file->token.type = TOKEN_QSTRING;
    file->token.string = xstrndup(file->current, i);
    file->current += i;
}


/*
**  Skip over a comment line at file->current, reading more data as necessary.
**  Stop when an end of line is encountered, positioning file->current
**  directly after the end of line.  Returns false on end of file or a read
**  error, true otherwise.
*/
static bool
token_skip_comment(struct config_file *file)
{
    char *p = file->current;

    while (*p != '\0' && *p != '\n' && *p != '\r')
        p++;
    while (*p == '\0') {
        if (!file_read(file))
            return false;
        p = file->current;
        while (*p != '\0' && *p != '\n' && *p != '\r')
            p++;
    }

    /* CRLF should count as a single line terminator, but it may be split
       across a read boundary.  Try to handle that case correctly. */
    if (*p == '\n')
        p++;
    else if (*p == '\r') {
        p++;
        if (*p == '\n')
            p++;
        else if (*p == '\0') {
            if (!file_read(file))
                return false;
            p = file->current;
            if (*p == '\n')
                p++;
        }
    }
    file->current = p;
    file->line++;
    return true;
}

/*
**  Skip over all whitespace at file->current, reading more data as
**  necessary.  Stop when the first non-whitespace character is encountered or
**  at end of file, leaving file->current pointing appropriately.  Returns
**  true if non-whitespace is found and false on end of file or a read error.
*/
static bool
token_skip_whitespace(struct config_file *file)
{
    char *p = file->current;

    while (*p == ' ' || *p == '\t')
        p++;
    while (*p == '\0') {
        if (!file_read(file))
            return false;
        p = file->current;
        while (*p == ' ' || *p == '\t')
            p++;
    }
    file->current = p;
    return true;
}


/*
**  The basic lexer function.  Read the next token from a configuration file.
**  Returns the token, which is also stored in file.  Lexer failures set the
**  token to TOKEN_ERROR.
*/
static enum token_type
token_next(struct config_file *file)
{
    /* If file->current is NULL, we've never read from the file.  There is
       special handling for a comment at the very beginning of a file, since
       normally we only look for comments after newline tokens.

       If we do see a # at the beginning of the first line, let token_newline
       deal with it.  That function can cope with file->current not pointing
       at a newline.  We then return the newline token as the first token in
       the file. */
    if (file->current == NULL) {
        if (!file_read(file))
            return file->token.type;
        if (!token_skip_whitespace(file))
            return file->token.type;
        if (*file->current == '#') {
            token_newline(file);
            return file->token.type;
        }
    } else {
        if (!token_skip_whitespace(file))
            return file->token.type;
    }

    /* Almost all of our tokens can be recognized by the first character; the
       only exception is telling strings from parameters.  token_string
       handles both of those and sets file->token.type appropriately.
       Comments are handled by token_newline. */
    switch (*file->current) {
    case '{':   token_simple(file, TOKEN_LBRACE);       break;
    case '}':   token_simple(file, TOKEN_RBRACE);       break;
    case '<':   token_simple(file, TOKEN_LANGLE);       break;
    case '>':   token_simple(file, TOKEN_RANGLE);       break;
    case '[':   token_simple(file, TOKEN_LBRACKET);     break;
    case ']':   token_simple(file, TOKEN_RBRACKET);     break;
    case ';':   token_simple(file, TOKEN_SEMICOLON);    break;
    case '\r':  token_newline(file);                    break;
    case '\n':  token_newline(file);                    break;
    case '"':   token_quoted_string(file);              break;
    default:    token_string(file);                     break;
    }

    return file->token.type;
}


/*
**  Open a new configuration file and return config_file representing the
**  parse state of that file.  We assume that we don't have to make a copy of
**  the filename argument.  Default to stdio BUFSIZ for our buffer size, since
**  it's generally reasonably chosen with respect to disk block sizes, memory
**  consumption, and the like.
*/
static struct config_file *
file_open(const char *filename)
{
    struct config_file *file;

    file = xmalloc(sizeof(*file));
    file->filename = filename;
    file->fd = open(filename, O_RDONLY);
    if (file->fd < 0) {
        free(file);
        return NULL;
    }
    file->buffer = xmalloc(BUFSIZ);
    file->bufsize = BUFSIZ;
    file->current = NULL;
    file->line = 1;
    file->token.type = TOKEN_ERROR;
    file->error = false;
    return file;
}


/*
**  Read some data from a configuration file, handling errors (by reporting
**  them with warn) and returning true if there's data left and false on EOF
**  or a read error.
*/
static bool
file_read(struct config_file *file)
{
    ssize_t status;

    status = read(file->fd, file->buffer, file->bufsize - 1);
    if (status < 0) {
        syswarn("%s: read error", file->filename);
        file->token.type = TOKEN_ERROR;
        file->error = true;
    } else if (status == 0) {
        file->token.type = TOKEN_EOF;
    }
    if (status <= 0)
        return false;
    file->buffer[status] = '\0';
    file->current = file->buffer;

    /* Reject nuls, since otherwise they would cause strange problems. */
    if (strlen(file->buffer) != (size_t) status) {
        warn("%s: invalid NUL character found in file", file->filename);
        return false;
    }
    return true;
}


/*
**  Read additional data from a configuration file when there's some partial
**  data in the buffer already that we want to save.  Takes the config_file
**  struct and an offset from file->buffer specifying the start of the data
**  that we want to preserve.  Resizes the buffer if offset is 0.  Returns
**  false on EOF or a read error, true otherwise.
*/
static bool
file_read_more(struct config_file *file, ptrdiff_t offset)
{
    char *start;
    size_t amount;
    ssize_t status;

    if (offset > 0) {
        size_t left;

        left = file->bufsize - offset - 1;
        memmove(file->buffer, file->buffer + offset, left);
        file->current -= offset;
        start = file->buffer + left;
        amount = offset;
    } else {
        file->buffer = xrealloc(file->buffer, file->bufsize + BUFSIZ);
        file->current = file->buffer;
        start = file->buffer + file->bufsize - 1;
        amount = BUFSIZ;
        file->bufsize += BUFSIZ;
    }
    status = read(file->fd, start, amount);
    if (status < 0)
        syswarn("%s: read error", file->filename);
    if (status <= 0)
        return false;
    start[status] = '\0';

    /* Reject nuls, since otherwise they would cause strange problems. */
    if (strlen(start) != (size_t) status) {
        warn("%s: invalid NUL character found in file", file->filename);
        return false;
    }
    return true;
}


/*
**  Close a file and free the resources associated with it.
*/
static void
file_close(struct config_file *file)
{
    close(file->fd);
    free(file->buffer);
    free(file);
}


/*
**  Given a config_group with the type and tag already filled in and a
**  config_file with the buffer positioned after the opening brace of the
**  group, read and add parameters to the group until encountering a close
**  brace.  Returns true on a successful parse, false on an error that
**  indicates the group should be discarded.
*/
static bool
parse_group_contents(struct config_group *group, struct config_file *file)
{
    enum token_type token;

    token = token_next(file);
    while (!file->error) {
        switch (token) {
        case TOKEN_PARAM:
            token = parse_parameter(group, file, file->token.string);
            while (token == TOKEN_CRLF || token == TOKEN_SEMICOLON)
                token = token_next(file);
            break;
        case TOKEN_CRLF:
            token = token_next(file);
            break;
        case TOKEN_EOF:
            return true;
        default:
            error_unexpected_token(file, "parameter");
            break;
        }
    }
    return false;
}


/*
**  Parse a parameter.  Takes the group we're currently inside, the
**  config_file parse state, and the key of the parameter.  Returns the next
**  token after the parameter, and also checks to make sure that it's
**  something legal (end of line, end of file, or a semicolon).
*/
static enum token_type
parse_parameter(struct config_group *group, struct config_file *file,
                char *key)
{
    enum token_type token;

    token = token_next(file);
    if (token == TOKEN_STRING || token == TOKEN_QSTRING) {
        struct config_parameter *param;
        unsigned int line;
        char *value;

        /* Before storing the parameter, check to make sure that the next
           token is valid.  If it isn't, chances are high that the user has
           tried to set a parameter to a value containing spaces without
           quoting the value. */
        value = file->token.string;
        line = file->line;
        token = token_next(file);
        switch (token) {
        default:
            error_unexpected_token(file, "semicolon or newline");
            free(value);
            break;
        case TOKEN_CRLF:
        case TOKEN_SEMICOLON:
        case TOKEN_EOF:
            param = xmalloc(sizeof(*param));
            param->key = key;
            param->raw_value = value;
            param->type = VALUE_UNKNOWN;
            param->line = line;
            if (!hash_insert(group->params, key, param)) {
                warn("%s:%u: duplicate parameter %s", file->filename, line,
                     key);
                free(param->raw_value);
                free(param->key);
                free(param);
            }
            return token;
        }
    } else {
        error_unexpected_token(file, "parameter value");
    }

    /* If we fell through, we encountered some sort of error.  Free allocated
       memory and return an error token. */
    free(key);
    return TOKEN_ERROR;
}


/*
**  Allocate a new config_group and set the initial values of all of the
**  struct members.
*/
static struct config_group *
group_new(const char *file, unsigned int line, const char *type,
          const char *tag)
{
    struct config_group *group;

    group = xmalloc(sizeof(*group));
    group->type = xstrdup(type);
    group->tag = (tag == NULL) ? NULL : xstrdup(tag);
    group->file = xstrdup(file);
    group->included = NULL;
    group->line = line;
    group->params = hash_create(4, hash_string, parameter_key,
                                parameter_equal, parameter_free);
    group->parent = NULL;
    group->child = NULL;
    group->next = NULL;
    return group;
}


/*
**  Free a config_group and all associated storage.
*/
static void
group_free(struct config_group *group)
{
    free(group->type);
    if (group->tag != NULL)
        free(group->tag);
    free(group->file);
    if (group->included != NULL)
        free(group->included);
    hash_free(group->params);
    free(group);
}


/*
**  Accessor function for the group type.
*/
const char *
config_group_type(struct config_group *group)
{
    return group->type;
}


/*
**  Accessor function for the group tag.
*/
const char *
config_group_tag(struct config_group *group)
{
    return group->tag;
}


/*
**  Parse a configuration file, returning the config_group that's the root of
**  the tree represented by that file (and any other files that it includes).
**  Returns NULL on a parse failure.
*/
struct config_group *
config_parse_file(const char *filename, ...)
{
    struct config_group *group;
    struct config_file *file;
    bool success;

    file = file_open(filename);
    if (file == NULL) {
        syswarn("open of %s failed", filename);
        return NULL;
    }
    group = group_new(filename, 1, "GLOBAL", NULL);
    success = parse_group_contents(group, file);
    file_close(file);
    return success ? group : NULL;
}


/*
**  Given a config_group representing the root of a configuration structure,
**  recursively free the entire structure.
*/
void
config_free(struct config_group *group)
{
    group_free(group);
}


/*
**  Convert a given parameter value to a boolean, returning true if successful
**  and false otherwise.
*/
static bool
convert_boolean(struct config_parameter *param, const char *file,
                void *result)
{
    static const char *const truevals[] = { "yes", "on", "true", NULL };
    static const char *const falsevals[] = { "no", "off", "false", NULL };
    bool *value = result;
    int i;

    if (param->type == VALUE_BOOL) {
        *value = param->value.boolean;
        return true;
    } else if (param->type != VALUE_UNKNOWN) {
        warn("%s:%u: %s is not a boolean", file, param->line, param->key);
        return false;
    }
    param->type = VALUE_BOOL;
    for (i = 0; truevals[i] != NULL; i++)
        if (EQ(param->raw_value, truevals[i])) {
            param->value.boolean = true;
            *value = true;
            return true;
        }
    for (i = 0; falsevals[i] != NULL; i++)
        if (EQ(param->raw_value, falsevals[i])) {
            param->value.boolean = false;
            *value = false;
            return true;
        }
    param->type = VALUE_INVALID;
    warn("%s:%u: %s is not a boolean", file, param->line, param->key);
    return false;
}


/*
**  Convert a given parameter value to an integer, returning true if
**  successful and false otherwise.
*/
static bool
convert_integer(struct config_parameter *param, const char *file,
                void *result)
{
    long *value = result;
    char *p;

    if (param->type == VALUE_INTEGER) {
        *value = param->value.integer;
        return true;
    } else if (param->type != VALUE_UNKNOWN) {
        warn("%s:%u: %s is not an integer", file, param->line, param->key);
        return false;
    }

    /* Do a syntax check even though strtol would do some of this for us,
       since otherwise some syntax errors may go silently undetected. */
    p = param->raw_value;
    if (*p == '-')
        p++;
    for (; *p != '\0'; p++)
        if (*p < '0' || *p > '9')
            break;
    if (*p != '\0') {
        warn("%s:%u: %s is not an integer", file, param->line, param->key);
        return false;
    }

    /* Do the actual conversion with strtol. */
    errno = 0;
    param->value.integer = strtol(param->raw_value, NULL, 10);
    if (errno != 0) {
        warn("%s:%u: %s doesn't convert to an integer", file, param->line,
             param->key);
        return false;
    }
    *value = param->value.integer;
    param->type = VALUE_INTEGER;
    return true;
}


/*
**  Convert a parameter value to a string, interpreting it as a quoted string,
**  and returning true if successful and false otherwise.  Does none of the
**  initial type checking, since convert_string should have already done that.
*/
static bool
convert_string_quoted(struct config_parameter *param, const char *file,
                      void *result)
{
    const char **value = result;
    size_t length;
    char *src, *dest;

    length = strlen(param->raw_value) - 2;
    param->value.string = xmalloc(length + 1);
    src = param->raw_value + 1;
    dest = param->value.string;
    for (; *src != '"' && *src != '\0'; src++) {
        if (*src != '\\') {
            *dest++ = *src;
        } else {
            src++;

            /* This should implement precisely the semantics of backslash
               escapes in quoted strings in C. */
            switch (*src) {
            case 'a':   *dest++ = '\a'; break;
            case 'b':   *dest++ = '\b'; break;
            case 'f':   *dest++ = '\f'; break;
            case 'n':   *dest++ = '\n'; break;
            case 'r':   *dest++ = '\r'; break;
            case 't':   *dest++ = '\t'; break;
            case 'v':   *dest++ = '\v'; break;

            case '\n':  break;  /* Escaped newlines disappear. */

            case '\\':
            case '\'':
            case '"':
            case '?':
                *dest++ = *src;
                break;

            case '\0':
                /* Should never happen; the tokenizer should catch this. */
                warn("%s:%u: unterminated string", file, param->line);
                goto fail;

            default:
                /* FIXME: \<octal>, \x, \u, and \U not yet implemented; the
                   last three could use the same basic code.  Think about
                   whether the escape should generate a single 8-bit character
                   or a UTF-8 encoded character; maybe the first two generate
                   the former and \u and \U generate the latter? */
                warn("%s:%u: unrecognized escape '\\%c'", file, param->line,
                     *src);
                goto fail;
            }
        }
    }
    *dest = '\0';

    /* The tokenizer already checked this for most cases but could miss the
       case where the final quote mark is escaped with a backslash. */
    if (*src != '"') {
        warn("%s:%u: unterminated string (no closing quote)", file,
             param->line);
        goto fail;
    }

    param->type = VALUE_STRING;
    *value = param->value.string;
    return true;

 fail:
    free(param->value.string);
    return false;
}


/*
**  Convert a given parameter value to a string, returning true if successful
**  and false otherwise.
*/
static bool
convert_string(struct config_parameter *param, const char *file, void *result)
{
    const char **value = result;

    if (param->type == VALUE_STRING) {
        *value = param->value.string;
        return true;
    } else if (param->type != VALUE_UNKNOWN) {
        warn("%s:%u: %s is not an string", file, param->line, param->key);
        return false;
    }

    if (*param->raw_value == '"') {
        return convert_string_quoted(param, file, result);
    } else {
        param->value.string = xstrdup(param->raw_value);
        param->type = VALUE_STRING;
        *value = param->value.string;
        return true;
    }
}


/*
**  Given a group, query it for the given parameter and then when the
**  parameter is found, check to see if it's already marked invalid.  If so,
**  fail quietly; otherwise, hand it off to the conversion function to do
**  type-specific work, returning the result.  Returns true if the parameter
**  is found in the group or one of its parents and convert can successfully
**  convert the raw value and put it in result, false otherwise (either for
**  the parameter not being found or for it being the wrong type).
*/
static bool
group_parameter_get(struct config_group *group, const char *key, void *result,
                    convert_func convert)
{
    struct config_group *current = group;

    while (current != NULL) {
        struct config_parameter *param;

        param = hash_lookup(group->params, key);
        if (param != NULL) {
            if (param->type == VALUE_INVALID)
                return false;
            else
                return (*convert)(param, group->file, result);
        }
        current = group->parent;
    }
    return false;
}


/*
**  All of the config_param_* functions do the following:
**
**  Given a group, query it for the given parameter, interpreting its value as
**  the appropriate type and returning it in the third argument.  Returns true
**  on success, false on failure (such as the parameter not being set or an
**  error), and report errors via warn.
*/
bool
config_param_boolean(struct config_group *group, const char *key,
                     bool *result)
{
    return group_parameter_get(group, key, result, convert_boolean);
}

bool
config_param_integer(struct config_group *group, const char *key,
                     long *result)
{
    return group_parameter_get(group, key, result, convert_integer);
}

bool
config_param_string(struct config_group *group, const char *key,
                    const char **result)
{
    return group_parameter_get(group, key, result, convert_string);
}


/*
**  A hash traversal function to add all parameter keys to the vector provided
**  as the second argument.
*/
static void
parameter_collect(void *element, void *cookie)
{
    struct config_parameter *param = element;
    struct vector *params = cookie;

    vector_add(params, param->key);
}


/*
**  Returns a newly allocated vector of all of the config parameters in a
**  group, including the inherited ones (not implemented yet).
*/
struct vector *
config_params(struct config_group *group)
{
    struct vector *params;
    size_t size;

    /* Size the vector, which we can do accurately for now. */
    params = vector_new();
    size = hash_count(group->params);
    vector_resize(params, size);

    /* Now, walk the hash to build the vector of params. */
    hash_traverse(group->params, parameter_collect, params);
    return params;
}


/*
**  Report an error in a given parameter.  Used so that the file and line
**  number can be included in the error message.
*/
void
config_error_param(struct config_group *group, const char *key,
                   const char *fmt, ...)
{
    va_list args;
    ssize_t length;
    char *message, *file;
    struct config_parameter *param;

    va_start(args, fmt);
    length = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    if (length < 0)
        return;
    message = xmalloc(length + 1);
    va_start(args, fmt);
    snprintf(message, length + 1, fmt, args);
    va_end(args);

    param = hash_lookup(group->params, key);
    if (param == NULL)
        warn("%s", message);
    else {
        file = (group->included != NULL ? group->included : group->file);
        warn("%s:%u: %s", file, param->line, message);
    }

    free(message);
}


/*
**  Stubs for functions not yet implemented.
*/
struct config_group *
config_find_group(struct config_group *group UNUSED, const char *type UNUSED)
{
    return NULL;
}

struct config_group *
config_next_group(struct config_group *group UNUSED)
{
    return NULL;
}

bool
config_param_real(struct config_group *group UNUSED, const char *key UNUSED,
                  double *result UNUSED)
{
    return false;
}

bool
config_param_list(struct config_group *group UNUSED, const char *key UNUSED,
                  struct vector *result UNUSED)
{
    return false;
}

void
config_error_group(struct config_group *group UNUSED, const char *fmt UNUSED,
                   ...)
{
}
