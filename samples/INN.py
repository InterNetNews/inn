# This module supplies stub Python functions corresponding to the ones
# provided by innd.  It is not used by the server; it is only here so
# that you can test your filter scripts before loading.

from types import *

def set_filter_hook(anObject):
    if type(anObject) == InstanceType:
        print "** set_filter_hook for " + repr(anObject)
    else:
        print "** <Your object is not a class instance.>"

def havehist(messageid):
    print "** havehist message id: " + messageid

def cancel(messageid):
    print "** cancel message id: " + messageid

def newsgroup(groupname):
    print "** newsgroup: " + messageid

def head(messageid):
    print "** head  message id: " + messageid

def article(messageid):
    print "** article message id: " + messageid

def syslog(level, message):
    print "-- syslog level: %s message: %s" % (level, message)
