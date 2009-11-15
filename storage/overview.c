/*  $Id$
**
**  The implementation of the overview API.
**
**  This code handles calls to the overview API by passing them along to the
**  appropriate underlying overview method, as well as implementing those
**  portions of the overview subsystem that are independent of storage
**  method.
**
**  This is currently just a wrapper around the old API still used internally
**  by the overview methods.  Eventually, the changes in API will be pushed
**  down into the overview method implementations.
*/

#include "config.h"
#include "clibrary.h"
#include <assert.h>
#include <errno.h>

#include "inn/buffer.h"
#include "inn/innconf.h"
#include "inn/messages.h"
#include "inn/overview.h"
#include "inn/wire.h"
#include "inn/vector.h"
#include "inn/libinn.h"
#include "inn/ov.h"
#include "ovinterface.h"
#include "ovmethods.h"

/* This struct is opaque to callers and is returned by overview_open.  It
   encapsulates any internal state used by the overview subsystem. */
struct overview {
    int mode;
    bool cutoff;
    struct buffer *overdata;
    struct cvector *groups;
    struct overview_method *method;
    void *private;
};


/*
**  Open overview.  Figure out which overview method we're using and
**  initialize it.  Allocate a new overview struct, flesh it out, and return
**  it to the caller.  Takes some combination of OV_READ and OV_WRITE.
*/
struct overview *
overview_open(int mode)
{
    int i;
    struct overview *overview;
    bool status;

    /* Basic sanity checks. */
    if (innconf == NULL)
        if (!innconf_read(NULL))
            return NULL;
    if (!innconf->enableoverview) {
        warn("enableoverview is not true");
        return NULL;
    }
    if (innconf->ovmethod == NULL) {
        warn("ovmethod is not defined");
        return NULL;
    }
    assert((mode & (OV_READ | OV_WRITE)) == mode);

    /* Locate the overview method we're using. */
    for (i = 0; i < NUM_OV_METHODS; i++)
        if (strcmp(innconf->ovmethod, ov_methods[i].name) == 0)
            break;
    if (i == NUM_OV_METHODS) {
        warn("%s is not a known overview method", innconf->ovmethod);
        return NULL;
    }

    /* We have enough information to initialize the method. */
    status = ov_methods[i].open(mode);
    if (!status)
        return NULL;
    overview = xmalloc(sizeof(struct overview));
    overview->mode = mode;
    overview->cutoff = false;
    overview->overdata = NULL;
    overview->groups = NULL;
    overview->method = &ov_methods[i];
    overview->private = NULL;
    return overview;
}


/*
**  Close overview.  Takes the overview struct corresponding to the method to
**  close and frees the resources allocated to it.
*/
void
overview_close(struct overview *overview)
{
    if (overview == NULL)
        return;
    overview->method->close();
    free(overview);
}


/*
**  Retrieve information about a group and put it into the supplied struct.
**  Returns false if the group wasn't found or some other error occurred.
*/
bool
overview_group(struct overview *overview, const char *group,
               struct overview_group *stats)
{
    int lo, hi, count, flag;
    bool status;

    status = overview->method->groupstats(group, &lo, &hi, &count, &flag);
    if (!status)
        return false;
    stats->high = hi;
    stats->low = lo;
    stats->count = count;
    stats->flag = flag;
    return true;
}


/*
**  Add a new newsgroup to the overview database.  Takes the high and low
**  marks and the flag from the provided struct (count is ignored).  Returns
**  false on error.
*/
bool
overview_group_add(struct overview *overview, const char *group,
                   struct overview_group *stats)
{
    return overview->method->groupadd(group, stats->low, stats->high,
                                      &stats->flag);
}


/*
**  Remove a group from the overview database.  The overview data for that
**  group will generally be purged immediately.  Returns false on error.
*/
bool
overview_group_delete(struct overview *overview, const char *group)
{
    return overview->method->groupdel(group);
}


/*
**  Add overview data for a particular article in a particular group.  Returns
**  false on error.
*/
bool
overview_add(struct overview *overview, const char *group,
             struct overview_data *data)
{
    /* We have to add the article number to the beginning of the overview data
       and CRLF to the end.  Use the overdata buffer as temporary storage
       space. */
    if (overview->overdata == NULL) {
        overview->overdata = buffer_new();
        buffer_resize(overview->overdata, data->overlen + 13);
    }
    buffer_sprintf(overview->overdata, false, "%ld\t", data->number);
    buffer_append(overview->overdata, data->overview, data->overlen);
    buffer_append(overview->overdata, "\r\n", 2);

    /* Call the underlying method. */
    return overview->method->add(group, data->number, data->token,
                                 overview->overdata->data,
                                 overview->overdata->left,
                                 data->arrived, data->expires);
}


/*
**  Add overview data for an article using the provided Xref information (sans
**  the leading hostname) to determine which groups and article numbers.  The
**  data will be added to each.  Return true only if the overview was
**  successfully stored in every group.  Don't make a big fuss over invalid
**  Xref entries; just silently skip over them.
**
**  I hate having to support this API, but both makehistory and overchan need
**  it, so there's no point in making both of them implement it separately.
*/
bool
overview_add_xref(struct overview *overview, const char *xref,
                  struct overview_data *data)
{
    char *xref_copy;
    const char *group;
    char *p, *end;
    size_t i;
    bool success = true;

    xref_copy = xstrdup(xref);
    p = strchr(xref_copy, '\n');
    if (p != NULL)
        *p = '\0';
    overview->groups = cvector_split_space(xref_copy, overview->groups);
    for (i = 0; i < overview->groups->count; i++) {
        group = overview->groups->strings[i];
        p = (char *) strchr(group, ':');
        if (p == NULL || p == group || p[1] == '-')
            continue;
        *p = '\0';
        errno = 0;
        data->number = strtoul(p + 1, &end, 10);
        if (data->number == 0 || *end != '\0' || errno == ERANGE)
            continue;
        success = success && overview_add(overview, group, data);
    }
    return success;
}


/*
**  Cancel a message from a particular group.
*/
bool
overview_cancel(struct overview *overview, const char *group, ARTNUM artnum)
{
    return overview->method->cancel(group, artnum);
}


/*
**  Cancel a message from all groups based on Xref information.  This is
**  hideously ugly, since there's no easy way to go from a token to a
**  newsgroup name and article number.  We retrieve the head of the article,
**  find the Xref header, and then parse it.  Articles without an Xref header
**  lose.
*/
bool
overview_cancel_xref(struct overview *overview, TOKEN token)
{
    ARTHANDLE *art;
    const char *xref, *xrefend, *group;
    size_t xreflen, i;
    char *xref_copy, *p, *end;
    ARTNUM artnum;

    art = SMretrieve(token, RETR_HEAD);
    if (art == NULL)
        return false;
    xref = wire_findheader(art->data, art->len, "Xref", true);
    if (xref == NULL)
        goto fail;
    xrefend = wire_endheader(xref, art->data + art->len - 1);
    if (xrefend == NULL)
        goto fail;
    xreflen = xrefend - xref + 1;
    xref_copy = xstrndup(xref, xreflen);
    SMfreearticle(art);
    overview->groups = cvector_split_space(xref_copy, overview->groups);
    for (i = 0; i < overview->groups->count; i++) {
        group = overview->groups->strings[i];
        p = (char *) strchr(group, ':');
        if (p == NULL || p == group || p[1] == '-')
            continue;
        *p = '\0';
        errno = 0;
        artnum = strtoul(p + 1, &end, 10);
        if (artnum == 0 || *end != '\0' || errno == ERANGE)
            continue;

        /* Don't worry about the return status; the article may have already
           expired out of some or all of the groups. */
        overview_cancel(overview, group, artnum);
    }
    free(xref_copy);
    return true;

fail:
    SMfreearticle(art);
    return false;
}


/*
**  Open a new overview search, which is used to retrieve overview records
**  between the given low and high article numbers.  Returns an opaque handle
**  or NULL if the search fails.
*/
void *
overview_search_open(struct overview *overview, const char *group, ARTNUM low,
                     ARTNUM high)
{
    return overview->method->opensearch(group, low, high);
}


/*
**  Retrieve the next overview record in the given overview search.  Returns
**  false if no more articles are found.
*/
bool
overview_search(struct overview *overview, void *handle,
                struct overview_data *data)
{
    ARTNUM number;
    char *overdata;
    int length;
    TOKEN token;
    time_t arrived;
    bool status;

    status = overview->method->search(handle, &number, &overdata, &length,
                                      &token, &arrived);
    if (!status)
        return false;
    data->number = number;
    data->overview = overdata;
    data->overlen = length;
    data->token = token;
    data->arrived = arrived;
    data->expires = 0;
    return true;
}


/*
**  Close an open search.
*/
void
overview_search_close(struct overview *overview, void *handle)
{
    overview->method->closesearch(handle);
}


/*
**  Given the group and article number, retrieve the token for the article
**  from overview.  Returns false if the article isn't found.
*/
bool
overview_token(struct overview *overview, const char *group, ARTNUM number,
               TOKEN *token)
{
    return overview->method->getartinfo(group, number, token);
}


/*
**  Expire a single group and flesh out the statistics portion of the provided
**  expiration configuration struct.  Returns false if group expiration fails
**  for some reason.
*/
bool
overview_expire(struct overview *overview, const char *group, ARTNUM *low,
                struct overview_expire *data)
{
    int newlow;
    bool status;

    EXPprocessed = 0;
    EXPunlinked = 0;
    EXPoverindexdrop = 0;
    status = overview->method->expiregroup(group, &newlow, data->history);
    data->processed += EXPprocessed;
    data->dropped += EXPunlinked;
    data->indexdropped += EXPoverindexdrop;
    if (status)
        *low = newlow;
    return status;
}


/*
**  Get the current overview configuration, filling out the provided struct.
*/
void
overview_config_get(struct overview *overview, struct overview_config *config)
{
    int i;
    OVSORTTYPE sort;

    config->mode = overview->mode;
    overview->method->ctl(OVSORT, &sort);
    config->sorted = (sort == OVNEWSGROUP);
    overview->method->ctl(OVSTATICSEARCH, &i);
    config->persistant = i;
    config->cutoff = overview->cutoff;
}


/*
**  Set the current overview configuration (which right now means only the
**  cutoff setting).  Right now, this can never fail; the return status is for
**  future work.
*/
bool
overview_config_set(struct overview *overview, struct overview_config *config)
{
    overview->cutoff = config->cutoff;
    return overview->method->ctl(OVCUTOFFLOW, &overview->cutoff);
}


/*
**  Return the free space of the overview method as a percentage, or -1 if
**  that concept isn't meaningful for the overview method.
*/
float
overview_free_space(struct overview *overview)
{
    int space;

    if (overview->method->ctl(OVSPACE, &space))
        return space;
    else
        return -1;
}
