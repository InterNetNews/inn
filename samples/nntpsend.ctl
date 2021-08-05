##  Sample nntpsend configuration file.
##
##  Format:
##    site:fqdn:max_size:[<args...>]
##      <site>        The name used in the newsfeeds file for this site;
##                    this determines the name of the batch file.
##      <fqdn>        A fully qualified domain name for the site,
##                    passed as the parameter to innxmit.
##      <size>        Size to truncate batch file if it gets too big;
##                    see shrinkfile(1).
##      <args>        Other args to pass to innxmit.
##
##  Everything after the number sign (#) is ignored.
##  See the nntpsend.ctl man page for more information.

#nsavax:erehwon.nsavax.gov::-t60
#group70:group70.org::
#walldrug:walldrug.com:4m-1m:-T1800 -t300
#kremvax:kremvax.cis:2m:
