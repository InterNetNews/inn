##  $Id$
##
##  This is a sample filter for the Python innd hook.
##
##  See the INN Python Filtering and Authentication Hooks documentation
##  for more information.
##
##  You have access to the following methods from the module INN:
##   - addhist(message-id)
##   - article(message-id)
##   - cancel(message-id)
##   - havehist(message-id)
##   - hashstring(string)
##   - head(message-id)
##   - newsgroup(groupname)
##   - set_filter_hook(instance)
##   - syslog(level, message)

import re
from string import *


##  The built-in intern() method has been in the sys module
##  since Python 3.0.
import sys
if sys.version_info[0] >= 3:
    def intern(headerName):
        return sys.intern(headerName)


##  This looks weird, but creating and interning these strings should
##  let us get faster access to header keys (which innd also interns) by
##  losing some strcmps under the covers.
Also_Control = intern("Also-Control")
Approved = intern("Approved")
Archive = intern("Archive")
Archived_At = intern("Archived-At")
Bytes = intern("Bytes")
Cancel_Key = intern("Cancel-Key")
Cancel_Lock = intern("Cancel-Lock")
Content_Base = intern("Content-Base")
Content_Disposition = intern("Content-Disposition")
Content_Transfer_Encoding = intern("Content-Transfer-Encoding")
Content_Type = intern("Content-Type")
Control = intern("Control")
Date = intern("Date")
Date_Received = intern("Date-Received")
Distribution = intern("Distribution")
Expires = intern("Expires")
Face = intern("Face")
Followup_To = intern("Followup-To")
From = intern("From")
In_Reply_To = intern("In-Reply-To")
Injection_Date = intern("Injection-Date")
Injection_Info = intern("Injection-Info")
Keywords = intern("Keywords")
Lines = intern("Lines")
List_ID = intern("List-ID")
Message_ID = intern("Message-ID")
MIME_Version = intern("MIME-Version")
Newsgroups = intern("Newsgroups")
NNTP_Posting_Date = intern("NNTP-Posting-Date")
NNTP_Posting_Host = intern("NNTP-Posting-Host")
NNTP_Posting_Path  = intern("NNTP-Posting-Path")
Organization = intern("Organization")
Original_Sender = intern("Original-Sender")
Originator = intern("Originator")
Path = intern("Path")
Posted = intern("Posted")
Posting_Version = intern("Posting-Version")
Received = intern("Received")
References = intern("References")
Relay_Version = intern("Relay-Version")
Reply_To = intern("Reply-To")
Sender = intern("Sender")
Subject = intern("Subject")
Summary = intern("Summary")
Supersedes = intern("Supersedes")
User_Agent = intern("User-Agent")
X_Auth = intern("X-Auth")
X_Auth_Sender = intern("X-Auth-Sender")
X_Canceled_By = intern("X-Canceled-By")
X_Cancelled_By = intern("X-Cancelled-By")
X_Complaints_To = intern("X-Complaints-To")
X_Face = intern("X-Face")
X_HTTP_UserAgent = intern("X-HTTP-UserAgent")
X_HTTP_Via = intern("X-HTTP-Via")
X_Mailer = intern("X-Mailer")
X_Modbot = intern("X-Modbot")
X_Modtrace = intern("X-Modtrace")
X_Newsposter = intern("X-Newsposter")
X_Newsreader = intern("X-Newsreader")
X_No_Archive = intern("X-No-Archive")
X_Original_Message_ID = intern("X-Original-Message-ID")
X_Original_NNTP_Posting_Host = intern("X-Original-NNTP-Posting-Host")
X_Original_Trace = intern("X-Original-Trace")
X_Originating_IP = intern("X-Originating-IP")
X_PGP_Key = intern("X-PGP-Key")
X_PGP_Sig = intern("X-PGP-Sig")
X_Poster_Trace = intern("X-Poster-Trace")
X_Postfilter = intern("X-Postfilter")
X_Proxy_User = intern("X-Proxy-User")
X_Submissions_To = intern("X-Submissions-To")
X_Trace = intern("X-Trace")
X_Usenet_Provider = intern("X-Usenet-Provider")
X_User_ID = intern("X-User-ID")
Xref = intern("Xref")
__BODY__ = intern("__BODY__")
__LINES__ = intern("__LINES__")


class InndFilter:
    """Provide filtering callbacks to innd."""

    def __init__(self):
        """This runs every time the filter is loaded or reloaded.
        This is a good place to initialize variables and precompile
        regular expressions, or maybe reload stats from disk.
        """
        self.re_newrmgroup = re.compile('(?:new|rm)group\s')
        self.re_obsctl = re.compile('(?:sendsys|version|uuname)')
        # Message-ID pattern from a once-common spambot.
        self.re_none44 = re.compile('none\d+\.yet>')
        # There is a mad newgrouper who likes to meow.
        self.re_meow = re.compile("^Meow\!", re.M)
        # One of my silly addresses.
        self.re_fluffymorph = re.compile("andruQ@myremarQ.coM", re.I)

    def filter_before_reload(self):
        """Runs just before the filter gets reloaded.

        You can use this method to save state information to be
        restored by the __init__() method or down in the main module.
        """
        syslog('notice', "filter_before_reload executing...")

    def filter_close(self):
        """Runs when innd exits.

        You can use this method to save state information to be
        restored by the __init__() method or down in the main module.
        """
        syslog('notice', "filter_close running, bye!")

    def filter_messageid(self, msgid):
        """Filter articles just by their Message-IDs.

        This method interacts with the IHAVE and CHECK NNTP commands.
        If you return a non-empty string here, the offered article
        will be refused before you ever have to waste any bandwidth
        looking at it (make sure that such a message is properly
        encoded in UTF-8 so as to comply with the NNTP protocol).
        This is not foolproof, so you should do your ID checks both
        here and in filter_art.  (TAKETHIS does not offer the ID
        for examination, and a TAKETHIS isn't always preceded
        by a CHECK.)
        """
        return ""               # Deactivate the samples.

        if self.re_none44.search(msgid):
            return "But I don't like spam!"
        if msgid[0:8] == '<cancel.':
            return "I don't do cybercancels."

    def filter_art(self, art):
        """Decide whether to keep offered articles.

        art is a dictionary with a bunch of headers, the article's
        body, and innd's reckoning of the line count.  Items not
        in the article will have a value of None.

        The available headers are the ones listed near the top of
        innd/art.c.  At this writing, they are:

            Also-Control, Approved, Archive, Archived-At, Bytes, Cancel-Key, Cancel-Lock,
            Content-Base, Content-Disposition, Content-Transfer-Encoding,
            Content-Type, Control, Date, Date-Received, Distribution, Expires,
            Face, Followup-To, From, In-Reply-To, Injection-Date, Injection-Info,
            Keywords, Lines, List-ID, Message-ID, MIME-Version, Newsgroups,
            NNTP-Posting-Date, NNTP-Posting-Host, NNTP-Posting-Path,
            Organization, Original-Sender, Originator,
            Path, Posted, Posting-Version, Received, References, Relay-Version,
            Reply-To, Sender, Subject, Summary, Supersedes, User-Agent,
            X-Auth, X-Auth-Sender, X-Canceled-By, X-Cancelled-By, X-Complaints-To,
            X-Face, X-HTTP-UserAgent, X-HTTP-Via, X-Mailer, X-Modbot, X-Modtrace,
            X-Newsposter, X-Newsreader, X-No-Archive, X-Original-Message-ID,
            X-Original-NNTP-Posting-Host, X-Original-Trace, X-Originating-IP,
            X-PGP-Key, X-PGP-Sig, X-Poster-Trace, X-Postfilter, X-Proxy-User,
            X-Submissions-To, X-Trace, X-Usenet-Provider, X-User-ID, Xref.

        The body is the buffer in art[__BODY__] and the INN-reckoned
        line count is held as an integer in art[__LINES__].  (The
        Lines: header is often generated by the poster, and large
        differences can be a good indication of a corrupt article.)

        If you want to keep an article, return None or "".  If you
        want to reject, return a non-empty string.  The rejection
        string will appear in transfer and posting response banners,
        and local posters will see them if their messages are
        rejected (make sure that such a response is properly encoded
        in UTF-8 so as to comply with the NNTP protocol).
        """
        return ""               # Deactivate the samples.

        # Catch bad Message-IDs from articles fed with TAKETHIS but no CHECK.
        idcheck = self.filter_messageid(art[Message_ID])
        if idcheck:
            return idcheck

        # There are some control messages we don't want to process or
        # forward to other sites.
        try:
            if art[Control] is not None:
                if self.re_newrmgroup.match(art[Control]):
                    if self.re_meow.search(art[__BODY__]):
                        return "The fake tale meows again."
                    # Python 3.x uses memoryview(b'mxyzptlk') because buffers
                    # do not exist any longer.  Note that the argument is
                    # a bytes object.
                    # if art[Distribution] == memoryview(b'mxyzptlk'):
                    if art[Distribution] == buffer('mxyzptlk'):
                        return "Evil control message from the 10th dimension"
                if self.re_obsctl.match(art[Control]):
                    return "Obsolete control message"

            # If you don't know, you don't want to know.
            if self.re_fluffymorph.search(art[From]):
                return "No, you may NOT meow."
        except:
            syslog('n', str(sys.exc_info[1]))

    def filter_mode(self, oldmode, newmode, reason):
        """Capture server events and do something useful.

        When the admin throttles or pauses innd (and lets it go
        again), this method will be called.  oldmode is the state we
        just left, and newmode is where we are going.  reason is
        usually just a comment string.

        The possible values of newmode and oldmode are the five
        strings 'running', 'paused', 'throttled', 'shutdown' and
        'unknown'.  Actually 'unknown' shouldn't happen; it's there
        in case feeping creatures invade innd.
        """
        syslog('notice', 'state change from %s to %s - %s'
               % (oldmode, newmode, reason))


"""
Okay, that's the end of our class definition.  What follows is the
stuff you need to do to get it all working inside innd.
"""

##  This import must succeed, or your filter won't work.  I'll repeat
##  that: You MUST import INN.
from INN import *

##  Some of the stuff below is gratuitous, just demonstrating how the
##  INN.syslog call works.  That first thingy tells the Unix syslogger
##  what severity to use; you can abbreviate down to one letter and
##  it's case insensitive.  Available levels are (in increasing levels
##  of seriousness) Debug, Info, Notice, Warning, Err, Crit, and
##  Alert.  If you provide any other string, it will be defaulted to
##  Notice.  You'll find the entries in the same log files innd itself
##  uses, with an 'innd: python:' prefix.
##
##  The native Python syslog module seems to clash with INN, so use
##  INN's.  Oh yeah -- you may notice that stdout and stderr have been
##  redirected to /dev/null -- if you want to print stuff, open your
##  own files.

try:
    import sys

except Exception, errmsg:    # Syntax for Python 2.x.
#except Exception as errmsg: # Syntax for Python 3.x.
    syslog('Error', "import boo-boo: " + errmsg[0])


##  If you want to do something special when the server first starts
##  up, this is how to find out when it's time.

if 'spamfilter' not in dir():
    syslog('n', "First load, so I can do initialization stuff.")
    # You could unpickle a saved hash here, so that your hard-earned
    # spam scores aren't lost whenever you shut down innd.
else:
    syslog('NoTicE', "I'm just reloading, so skip the formalities.")


##  Finally, here is how we get our class on speaking terms with innd.
##  The hook is refreshed on every reload, so that you can change the
##  methods on a running server.  Don't forget to test your changes
##  before reloading!
spamfilter = InndFilter()
try:
    set_filter_hook(spamfilter)
    syslog('n', "spamfilter successfully hooked into INN")
except Exception, errmsg:    # Syntax for Python 2.x.
#except Exception as errmsg: # Syntax for Python 3.x.
    syslog('e', "Cannot obtain INN hook for spamfilter: %s" % errmsg[0])
