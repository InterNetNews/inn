## rone's unified control.ctl
## $Id$
##
## control.ctl - Access control for control messages.
##
## The canonical version of this file can be found in the latest INN release
## and at <ftp://ftp.isc.org/pub/usenet/CONFIG/control.ctl>; these two files
## will be kept in sync.  Please refer to the latest version of this file
## for the most up-to-date hierarchy control information and please use the
## latest version if you intend to carry all hierarchies.  You may wish to
## change the policy for alt.*.
##
## Format:
##     <message>:<from>:<newsgroups>:<action>
##
##     <message>      Control message or "all" if it applies to all control
##                    messages.
##     <from>         Pattern that must match the From line.
##     <newsgroups>   Pattern that must match the newsgroup being newgroup'd
##                    or rmgroup'd (ignored for other messages).
##     <action>       What to do:
##                          doit        Perform action
##                          drop        Ignore message
##                          log         One line to error log
##                          mail        Send mail to admin
##                          verify-pgp_userid   Do PGP verification on user.
##                    All actions except drop and mail can be given a log
##                    location by following the action with an = and the
##                    log ("mail" says to mail the admin, an empty location
##                    tosses the log information, and a relative path xxx
##                    logs to $LOG/xxx.log.
##
## The *last* matching entry is used.  See the expire.ctl(5) man page for
## complete information.
##
## -------------------------------------------------------------------------
##
## The defaults have changed.
##
## Firstly.  Most things that caused mail in the past no longer do.  This is
## due to proliferation of forged control messages filling up mailboxes.
##
## Secondly, the assumption now is that you have PGP on your system.  If you
## don't, then you should get it to help protect youself against all the
## luser control message forgers.  If you can't use PGP, then you'll have
## to fix some sections here. Search for *PGP*.  At each "*PGP*" found
## you'll need to comment out the block of lines right after it (that have
## 'verify-' in their 4th field).  Then uncomment the block of lines that
## comes right after that.  You'll also need to turn off pgpverify in
## inn.conf.
##
## For more information on using PGP to verify control messages, upgrade
## to INN 1.5 (or later) or see <ftp://ftp.isc.org/pub/pgpcontrol/>.
## 
## -------------------------------------------------------------------------
##
## A number of hierarchies are for local use only but have leaked out into
## the general stream.  In this config file the are set so they are easy to
## remove, marked "local only," "defunct," or "private," and will not be
## added by users of this file.  Please delete all groups in those
## hierarchies from your server if you carry them.  If you wish to carry
## them please contact the address given to arrange a feed.
## 
## If you have permission to carry any of the hierachies so listed in this
## file, you should change the entries for those hierarchies below.

## -------------------------------------------------------------------------
##	DEFAULT
## -------------------------------------------------------------------------

all:*:*:mail

## -------------------------------------------------------------------------
##	CHECKGROUPS MESSAGES
## -------------------------------------------------------------------------

checkgroups:*:*:mail

## -------------------------------------------------------------------------
##	IHAVE/SENDME MESSAGES
## -------------------------------------------------------------------------

ihave:*:*:drop
sendme:*:*:drop

## -------------------------------------------------------------------------
##	SENDSYS
## -------------------------------------------------------------------------

sendsys:*:*:log=sendsys

## -------------------------------------------------------------------------
##	SENDUUNAME
## -------------------------------------------------------------------------

senduuname:*:*:log=senduuname

## -------------------------------------------------------------------------
##	VERSION
## -------------------------------------------------------------------------

version:*:*:log=version

## -------------------------------------------------------------------------
##	NEWGROUP/RMGROUP MESSAGES
## -------------------------------------------------------------------------

## Default (for any group)
newgroup:*:*:mail
rmgroup:*:*:mail

## The Big 8.
## COMP, HUMANITIES, MISC, NEWS, REC, SCI, SOC, TALK

# If it *doesn't* come from group-admin@isc.org, ignore it silently.
newgroup:*:comp.*|humanities.*|misc.*|news.*|rec.*|sci.*|soc.*|talk.*:drop
rmgroup:*:comp.*|humanities.*|misc.*|news.*|rec.*|sci.*|soc.*|talk.*:drop

# This will weed out forgeries for non-Big 8 hierarchies.
newgroup:group-admin@isc.org:*:drop
newgroup:tale@*uu.net:*:drop
rmgroup:group-admin@isc.org:*:drop
rmgroup:tale@*uu.net:*:drop

# *PGP*   See comment at top of file.
checkgroups:group-admin@isc.org:*:verify-news.announce.newgroups
newgroup:group-admin@isc.org:comp.*|misc.*|news.*:verify-news.announce.newgroups
newgroup:group-admin@isc.org:rec.*|sci.*|soc.*:verify-news.announce.newgroups
newgroup:group-admin@isc.org:talk.*|humanities.*:verify-news.announce.newgroups
rmgroup:group-admin@isc.org:comp.*|misc.*|news.*:verify-news.announce.newgroups
rmgroup:group-admin@isc.org:rec.*|sci.*|soc.*:verify-news.announce.newgroups
rmgroup:group-admin@isc.org:talk.*|humanities.*:verify-news.announce.newgroups

# Non-pgp entries, Due to the number of forged messages these are all 
# mailed to the NewsAdmin, rmgroups are also not acted upon.
# 
# The best idea is to get PGP.
# 
# checkgroups:group-admin@isc.org:*:mail
# newgroup:group-admin@isc.org:comp.*|misc.*|news.*|rec.*|sci.*:doit
# newgroup:group-admin@isc.org:soc.*|talk.*|humanities.*:doit
# rmgroup:group-admin@isc.org:comp.*|misc.*|news.*|rec.*|sci.*:mail
# rmgroup:group-admin@isc.org:soc.*|talk.*|humanities.*:mail

## ACS & OSU (Ohio State University)
# Contact: Albert J. School <school.1@osu.edu>
# secondary contact: Harpal Chohan <chohan+@osu.edu>
# For local use only, contact the above addresses for information.
newgroup:*@*:acs.*|osu.*:mail
rmgroup:*@*:acs.*|osu.*:doit

## AHN (Athens-Clarke County, Georgia, USA)
newgroup:greg@*.ucns.uga.edu:ahn.*:doit
rmgroup:greg@*.ucns.uga.edu:ahn.*:doit

## AIR ( Internal Stanford University, USA ) 
# Contact: news@news.stanford.edu
# For local use only, contact the above address for information.
newgroup:*@*:air.*:mail
rmgroup:*@*:air.*:doit

## AKR ( Akron, Ohio, USA) 
newgroup:red@redpoll.mrfs.oh.us:akr.*:doit
rmgroup:red@redpoll.mrfs.oh.us:akr.*:doit

## ALABAMA & HSV (Huntsville, Alabama, USA)
# Contact: news@news.msfc.nasa.gov
# *PGP*   See comment at top of file.
newgroup:*:alabama.*|hsv.*:drop
rmgroup:*:alabama.*|hsv.*:drop
checkgroups:news@news.msfc.nasa.gov:alabama.*|hsv.*:verify-alabama-group-admin
newgroup:news@news.msfc.nasa.gov:alabama.*|hsv.*:verify-alabama-group-admin
rmgroup:news@news.msfc.nasa.gov:alabama.*|hsv.*:verify-alabama-group-admin

# newgroup:news@news.msfc.nasa.gov:alabama.*|hsv.*:doit
# rmgroup:news@news.msfc.nasa.gov:alabama.*|hsv.*:doit

## ALIVE
# Contact: thijs@kink.xs4all.nl
# No longer used.
newgroup:*@*:alive.*:mail
rmgroup:*@*:alive.*:doit

## ALT
#
# Accept all newgroups (except ones forged from Big 8 newgroup issuers,
# who never issue alt.* control messages) and silently ignore all
# rmgroups.  This file used to attempt to track and recommend trusted
# rmgroup issuers; it no longer does because they change too frequently.
# What policy to use for alt.* groups varies widely from site to site.
# For a small site, it is strongly recommended that this policy be changed
# to drop all newgroups for alt.* as well as rmgroups and that the news
# admin only add new alt.* groups on request, as tons of alt.* newgroups
# are sent out regularly with the intent more to create nonsense entries
# in active files than to actually create a useable newsgroup.
#
# Other options and comments on alt.* groups can be found at
# <http://www.alt-config.org/>, one of the many alt.* FAQ sites.  Be aware
# that there is no official, generally accepted alt.* policy and all
# information about alt.* groups available is essentially someone's
# opinion, including these comments.  There are nearly as many different
# policies with regard to alt.* groups as there are Usenet sites.
#
newgroup:*:alt.*:doit
newgroup:group-admin@isc.org:alt.*:drop
newgroup:tale@*uu.net:alt.*:drop
rmgroup:*:alt.*:drop

## AR (Argentina)
newgroup:jorge_f@nodens.fisica.unlp.edu.ar:ar.*:doit
rmgroup:jorge_f@nodens.fisica.unlp.edu.ar:ar.*:doit

## ARC (NASA Ames Research Center)
# Contact: news@arc.nasa.gov
# For internal use only, contact above address for questions
newgroup:*@*:arc.*:mail
rmgroup:*@*:arc.*:doit

## ARKANE (Arkane Systems, UK )
# Contact: newsbastard@arkane.demon.co.uk
# URL: http://www.arkane.demon.co.uk/Newsgroups.html
checkgroups:newsbastard@arkane.demon.co.uk:arkane.*:mail
newgroup:newsbastard@arkane.demon.co.uk:arkane.*:doit
rmgroup:newsbastard@arkane.demon.co.uk:arkane.*:doit

## AT (Austrian)
checkgroups:control@usenet.backbone.at:at.*:mail
newgroup:control@usenet.backbone.at:at.*:doit
rmgroup:control@usenet.backbone.at:at.*:doit

## AUS (Australia)
# Contact: ausadmin@aus.news-admin.org
# URL: http://aus.news-admin.org/ 
# *PGP*   See comment at top of file.
newgroup:*:aus.*:drop
rmgroup:*:aus.*:drop
checkgroups:ausadmin@aus.news-admin.org:aus.*:verify-ausadmin@aus.news-admin.org
newgroup:ausadmin@aus.news-admin.org:aus.*:verify-ausadmin@aus.news-admin.org
rmgroup:ausadmin@aus.news-admin.org:aus.*:verify-ausadmin@aus.news-admin.org

## AUSTIN (Texas, USA) 
checkgroups:fletcher@cs.utexas.edu:austin.*:doit
checkgroups:chip@unicom.com:austin.*:doit
checkgroups:pug@pug.net:austin.*:doit
newgroup:fletcher@cs.utexas.edu:austin.*:doit
newgroup:chip@unicom.com:austin.*:doit
newgroup:pug@pug.net:austin.*:doit
rmgroup:fletcher@cs.utexas.edu:austin.*:doit
rmgroup:chip@unicom.com:austin.*:doit
rmgroup:pug@pug.net:austin.*:doit

## AZ (Arizona, USA)
newgroup:system@asuvax.eas.asu.edu:az.*:doit
rmgroup:system@asuvax.eas.asu.edu:az.*:doit

## BA (San Francisco Bay Area, USA)
# Contact: <ba-mod@nas.nasa.gov>
# URL: http://ennui.org/ba/
# *PGP*   See comment at top of file.
newgroup:*:ba.*:drop
rmgroup:*:ba.*:drop
checkgroups:ba-mod@nas.nasa.gov:ba.*:verify-ba.news.config
newgroup:ba-mod@nas.nasa.gov:ba.*:verify-ba.news.config
rmgroup:ba-mod@nas.nasa.gov:ba.*:verify-ba.news.config

## BACKBONE (ruhr.de/ruhrgebiet.individual.net in Germany)
# Contact: admin@ruhr.de
# For internal use only, contact above address for questions
newgroup:*@*:backbone.*:mail
rmgroup:*@*:backbone.*:doit

## BAYNET (Bayerische Buergernetze, Deutschland)
# Contact: news@mayn.de
# URL: http://www.mayn.de/users/news/
# Key fingerprint = F7 AD 96 D8 7A 3F 7E 84  02 0C 83 9A DB 8F EB B8
# Syncable server: news.mayn.de (contact news@mayn.de if permission denied)
# *PGP*   See comment at top of file.
newgroup:*:baynet.*:drop
rmgroup:*:baynet.*:drop
checkgroups:news@mayn.de:baynet.*:verify-news.mayn.de
newgroup:news@mayn.de:baynet.*:verify-news.mayn.de
rmgroup:news@mayn.de:baynet.*:verify-news.mayn.de

# newgroup:news@mayn.de:baynet.*:mail
# rmgroup:news@mayn.de:baynet.*:mail
# checkgroups:news@mayn.de:baynet.*:mail

## BDA (German groups?)
newgroup:news@*netuse.de:bda.*:doit
rmgroup:news@*netuse.de:bda.*:doit

## BE  (Belgique/Belgie/Belgien/Belgium )
# Contact: be-hierarchy-admin@usenet.be
# URL: http://usenet.be/
# Key URL: http://usenet.be/be.announce.newgroups.asc
# Key fingerprint = 30 2A 45 94 70 DE 1F D5  81 8C 58 64 D2 F7 08 71
# *PGP*   See comment at top of file.
newgroup:*:be.*:drop
rmgroup:*:be.*:drop
checkgroups:group-admin@usenet.be:be.*:verify-be.announce.newgroups
newgroup:group-admin@usenet.be:be.*:verify-be.announce.newgroups
rmgroup:group-admin@usenet.be:be.*:verify-be.announce.newgroups

# newgroup:group-admin@usenet.be:be.*:doit
# rmgroup:group-admin@usenet.be:be.*:doit

## BERMUDA
newgroup:news@*ibl.bm:bermuda.*:doit
rmgroup:news@*ibl.bm:bermuda.*:doit

## BEST ( Best Internet Communications, Inc. )
# Contact: news@best.net
# For local use only, contact the above address for information.
newgroup:*@*:best.*:mail
rmgroup:*@*:best.*:doit

## BIONET (Biology Network)
# URL: http://www.bio.net/
# PGP: http://pgp.ai.mit.edu/~bal/pks-commands.html
#
# Biosci-control-key@net.bio.net
#     PGP  Key fingerprint =  EB C0 F1 BA 26 0B C6 D6  FB 8D ED C4 AE 5D 10 54
#
# *PGP*   See comment at top of file.
newgroup:*:bionet.*:drop
rmgroup:*:bionet.*:drop
checkgroups:Biosci-control-key@net.bio.net:bionet.*:verify-Biosci-control-key@net.bio.net
newgroup:Biosci-control-key@net.bio.net:bionet.*:verify-Biosci-control-key@net.bio.net=newgroup
rmgroup:Biosci-control-key@net.bio.net:bionet.*:verify-Biosci-control-key@net.bio.net=rmgroup

## BIRK (University of Oslo, Norway)
# Contact: birk-admin@ping.uio.no
# For private use only, contact the above address for information.
newgroup:*@*:birk.*:drop
rmgroup:*@*:birk.*:doit

## BIT (Gatewayed Mailing lists)
# *PGP*   See comment at top of file.
newgroup:*:bit.*:drop
rmgroup:*:bit.*:drop
checkgroups:bit@newsadmin.com:bit.*:verify-bit@newsadmin.com
newgroup:bit@newsadmin.com:bit.*:verify-bit@newsadmin.com
rmgroup:bit@newsadmin.com:bit.*:verify-bit@newsadmin.com

# newgroup:bit@newsadmin.com:bit.*:doit
# rmgroup:bit@newsadmin.com:bit.*:doit

## BIZ (Business Groups)
newgroup:edhew@xenitec.on.ca:biz.*:doit
rmgroup:edhew@xenitec.on.ca:biz.*:doit

## BLGTN ( Bloomington, In, USA)
newgroup:control@news.bloomington.in.us:blgtn.*:doit
rmgroup:control@news.bloomington.in.us:blgtn.*:doit

## BLN (Berlin, Germany)
checkgroups:news@*fu-berlin.de:bln.*:mail
newgroup:news@*fu-berlin.de:bln.*:doit
rmgroup:news@*fu-berlin.de:bln.*:doit

## BOFH ( Bastard Operator From Hell )
# Contact: myname@myhost.mydomain.com
# For private use only, contact the above address for information.
newgroup:*@*:bofh.*:mail
rmgroup:*@*:bofh.*:doit

## CA (California, USA)
# URL: http://www.sbay.org/ca/
# Contact: ikluft@thunder.sbay.org
newgroup:ikluft@thunder.sbay.org:ca.*:doit
rmgroup:ikluft@thunder.sbay.org:ca.*:doit

## CAIS (Capital Area Internet Services)
# Contact: news@cais.com
# For local use only, contact the above address for information.
newgroup:*@*:cais.*:mail
rmgroup:*@*:cais.*:doit

## CALSTATE (California State University)
newgroup:*@*calstate.edu:calstate.*:doit
rmgroup:*@*calstate.edu:calstate.*:doit

## CAPDIST (Albany, The Capital District, New York, USA)
newgroup:danorton@albany.net:capdist.*:doit
rmgroup:danorton@albany.net:capdist.*:doit

## CARLETON (Canadian -- Carleton University)
newgroup:news@cunews.carleton.ca:carleton.*:doit
newgroup:news@cunews.carleton.ca:carleton*class.*:mail
rmgroup:news@cunews.carleton.ca:carleton.*:doit

## CD-ONLINE
# Contact: newsmaster@worldonline.nl
# For local use only, contact the above address for information.
newgroup:*@*:cd-online.*:mail
rmgroup:*@*:cd-online.*:doit

## CENTRAL (The Internet Company of New Zealand, Wellington, NZ )
# Contact: usenet@iconz.co.nz
# For local use only, contact the above address for information.
newgroup:*@*:central.*:mail
rmgroup:*@*:central.*:doit

## CERN (CERN - European Laboratory for Particle Physics)
# Contact: Dietrich Wiegandt <News.Support@cern.ch>
# For private use only, contact the above address for information.
newgroup:News.Support@cern.ch:cern.*:doit
rmgroup:News.Support@cern.ch:cern.*:doit

## CH ( Switzerland )
# Contact: ch-news-admin@use-net.ch
# URL: http://www.use-net.ch/Usenet/
# Key URL: http://www.use-net.ch/Usenet/adminkey.html
# *PGP*   See comment at top of file.
# Key fingerprint = 71 80 D6 8C A7 DE 2C 70  62 4A 48 6E D9 96 02 DF
newgroup:*:ch.*:drop
rmgroup:*:ch.*:drop
checkgroups:felix.rauch@nice.ch:ch.*:verify-ch-news-admin@use-net.ch
newgroup:felix.rauch@nice.ch:ch.*:verify-ch-news-admin@use-net.ch
rmgroup:felix.rauch@nice.ch:ch.*:verify-ch-news-admin@use-net.ch

#checkgroups:felix.rauch@nice.ch:ch.*:doit
#newgroup:felix.rauch@nice.ch:ch.*:doit
#rmgroup:felix.rauch@nice.ch:ch.*:doit

## CHAVEN (Celestian Haven ISP, USA-Midwest ISP)
# Contact: news@chaven.com
# For internal use only, contact above address for questions
newgroup:*@*:chaven.*:drop
rmgroup:*@*:chaven.*:doit

## CHRISTNET newsgroups
checkgroups:news@fdma.com:christnet.*:mail
newgroup:news@fdma.com:christnet.*:doit
rmgroup:news@fdma.com:christnet.*:doit

## CHI (Chicago, USA)
newgroup:lisbon@*interaccess.com:chi.*:doit
newgroup:lisbon@*chi.il.us:chi.*:doit
rmgroup:lisbon@*interaccess.com:chi.*:doit
rmgroup:lisbon@*chi.il.us:chi.*:doit

## CHILE (Chile and Chilean affairs) 
# Contact: mod-cga@webhost.cl
# URL: http://www.webhost.cl/~mod-cga
checkgroups:mod-cga@*webhost.cl:chile.*:mail
newgroup:mod-cga@*webhost.cl:chile.*:doit
rmgroup:mod-cga@*webhost.cl:chile.*:doit

## CHINESE (China and Chinese language groups)
newgroup:pinghua@stat.berkeley.edu:chinese.*:doit
rmgroup:pinghua@stat.berkeley.edu:chinese.*:doit

## CITYSCAPE & DEMON (Cityscape Internet Services & Demon Internet, UK)
# Contact: Dave Williams <newsmaster@demon.net>
# URL: ftp://ftp.demon.co.uk/pub/news/doc/demon.news.txt
# *PGP*   See comment at top of file.
newgroup:*:cityscp.*:drop
rmgroup:*:cityscp.*:drop
checkgroups:newsmaster@demon.net:cityscp.*:verify-demon.news
newgroup:newsmaster@demon.net:cityscp.*:verify-demon.news
rmgroup:newsmaster@demon.net:cityscp.*:verify-demon.news

## CL (CL-Netz, German)
# *PGP*   See comment at top of file.
# Contact: koordination@cl-netz.de
# For internal use only, contact above address for questions
# URL: http://www.cl-netz.de/
# Key URL: http://www.cl-netz.de/control.txt
newgroup:*@*:cl.*:drop
rmgroup:*@*:cl.*:doit
# The following three lines are only for authorized cl.* sites.
#checkgroups:koordination@cl-netz.de:cl.*:verify-cl.netz.infos
#newgroup:koordination@cl-netz.de:cl.*:verify-cl.netz.infos
#rmgroup:koordination@cl-netz.de:cl.*:verify-cl.netz.infos

# checkgroups:koordination@cl-netz.de:cl.*:doit
# newgroup:koordination@cl-netz.de:cl.*:doit
# rmgroup:koordination@cl-netz.de:cl.*:doit

## CLARINET ( Features and News, Available on a commercial basis)
# *PGP*   See comment at top of file.
newgroup:*:clari.*:drop
rmgroup:*:clari.*:drop
checkgroups:cl*@clarinet.com:clari.*:verify-ClariNet.Group
newgroup:cl*@clarinet.com:clari.*:verify-ClariNet.Group
rmgroup:cl*@clarinet.com:clari.*:verify-ClariNet.Group

# newgroup:brad@clarinet.com:clari.*:doit
# newgroup:clarinet@clarinet.com:clari.*:doit
# newgroup:clarinet@clarinet.net:clari.*:doit
# rmgroup:brad@clarinet.com:clari.*:doit
# rmgroup:clarinet@clarinet.com:clari.*:doit
# rmgroup:clarinet@clarinet.net:clari.*:doit

## CMI (University of Illinois, Urbana-Champaign, IL, USA)
# Contact: news@uiuc.edu
# For internal use only, contact above address for questions
newgroup:*:cmi.*:drop
rmgroup:*:cmi.*:doit

## CMU (Carnegie-Mellon University, Pennsylvania, USA)
# Contact: Daniel Edward Lovinger <del+@CMU.EDU>
# For internal use only, contact above address for questions
newgroup:*:cmu.*:drop
rmgroup:*:cmu.*:doit

## CUHK (Chinese University of Hong Kong)
# Contact: shlam@ie.cuhk.edu.hk (Alan S H Lam)
# For internal use only, contact above address for questions
newgroup:*:cuhk.*:drop
rmgroup:*:cuhk.*:doit

## CO (Colorado, USA)
# Contact: coadmin@boyznoyz.com (Bill of Denver)
newgroup:coadmin@boyznoyz.com:co.*:doit
rmgroup:coadmin@boyznoyz.com:co.*:doit
checkgroups:coadmin@boyznoyz.com:co.*:doit

## CODEWARRIOR (CodeWarrior discussion)
checkgroups:news@supernews.net:codewarrior.*:doit
newgroup:news@supernews.net:codewarrior.*:doit
rmgroup:news@supernews.net:codewarrior.*:doit

## COMPUTER42 (Computer 42, Germany)
# Contact: Dirk Schmitt <news@computer42.org>
newgroup:news@computer42.org:computer42.*:doit
rmgroup:news@computer42.org:computer42.*:doit

## CONCORDIA (Concordia University, Montreal, Canada)
# URL: General University info at http://www.concordia.ca/
# Contact: newsmaster@concordia.ca
newgroup:news@newsflash.concordia.ca:concordia.*:doit
rmgroup:news@newsflash.concordia.ca:concordia.*:doit

## COURTS 
# Contact: trier@ins.cwru.edu
# This Hierarchy is defunct as of mid 1998.
newgroup:*@*:courts.*:mail
rmgroup:*@*:courts.*:doit

## CPCU/IIA (American Institute for Chartered Property Casulty
## Underwriter/Insurance Institute of America, USA )
# Contact: miller@cpcuiia.org
# URL: www.aicpcu.org
checkgroups:miller@cpcuiia.org:cpcuiia.*:mail
newgroup:miller@cpcuiia.org:cpcuiia.*:doit
rmgroup:miller@cpcuiia.org:cpcuiia.*:doit

## CU (University of Colorado)
# Contact: Doreen Petersen <news@colorado.edu>
# For local use only, contact the above address for information.
newgroup:*@*:cu.*:mail
rmgroup:*@*:cu.*:doit

## CZ newsgroups (Czech Republic)
# URL: ftp://ftp.vslib.cz/pub/news/config/cz/newsgroups (text)
# URL: http://www.ces.net/cgi-bin/newsgroups.p?cz  (HTML)
checkgroups:petr.kolar@vslib.cz:cz.*:mail
newgroup:petr.kolar@vslib.cz:cz.*:doit
rmgroup:petr.kolar@vslib.cz:cz.*:doit

## DC (Washington, D.C. , USA )
checkgroups:news@mattress.atww.org:dc.*:mail
newgroup:news@mattress.atww.org:dc.*:doit
rmgroup:news@mattress.atww.org:dc.*:doit

## DE (German language)
# *PGP*   See comment at top of file.
newgroup:*:de.*:drop
rmgroup:*:de.*:drop
checkgroups:moderator@dana.de:de.*:verify-de.admin.news.announce
newgroup:moderator@dana.de:de.*:verify-de.admin.news.announce
newgroup:*@*:de.alt.*:doit
rmgroup:moderator@dana.de:de.*:verify-de.admin.news.announce

# checkgroups:*@*dana.de:de.*:mail
# checkgroups:*@*.dana.de:de.*:mail
# newgroup:*@dana.de|*@*.dana.de:de.*:doit
# newgroup:*@*:de.alt.*:doit
# rmgroup:*@dana.de|*@*.dana.de:de.*:doit

## DFW (Dallas/Fort Worth, Texas, USA)
newgroup:eric@*cirr.com:dfw.*:doit
rmgroup:eric@*cirr.com:dfw.*:doit

## DK (Denmark)
# URL: http://www.DK.net/Usenet/
# Key URL: http://www.DK.net/Usenet/pgp.html
# Key fingerprint = 7C B2 C7 50 F3 7D 5D 73  8C EE 2E 3F 55 80 72 FF
# *PGP*   See comment at top of file.
newgroup:*:dk.*:drop
rmgroup:*:dk.*:drop
newgroup:news@news.dknet.dk:dk.*:verify-news@news.dknet.dk
rmgroup:news@news.dknet.dk:dk.*:verify-news@news.dknet.dk

# newgroup:news@news.dknet.dk:dk.*:doit
# rmgroup:news@news.dknet.dk:dk.*:doit

## DUKE ( Duke University, USA )
# Contact: news@newsgate.duke.edu
# For local use only, contact the above address for information.
newgroup:*@*:duke.*:mail
rmgroup:*@*:duke.*:doit

## Easynet PLC
# Contact: Christiaan Keet <newsmaster@easynet.net>
# URL: ftp://ftp.easynet.net/pub/usenet/easynet.control.txt
# *PGP*   See comment at top of file.
newgroup:*:easynet.*:drop
rmgroup:*:easynet.*:drop
checkgroups:newsmaster@easynet.net:easynet.*:verify-easynet.news
newgroup:newsmaster@easynet.net:easynet.*:verify-easynet.news
rmgroup:newsmaster@easynet.net:easynet.*:verify-easynet.news

## EE (Estonia)
# Contact: usenet@news.ut.ee
# URL: http://news.ut.ee/
# Key URL: http://news.ut.ee/pubkey.asc
# *PGP*   See comment at top of file.
newgroup:*:ee.*:drop
rmgroup:*:ee.*:drop
checkgroups:news@news.ut.ee:ee.*:verify-ee.news
newgroup:news@news.ut.ee:ee.*:verify-ee.news
rmgroup:news@news.ut.ee:ee.*:verify-ee.news

## EFN & EUG (Eugene Free Computer Network, Eugene/Springfield, Oregon, USA)
# *PGP*   See comment at top of file.
newgroup:*:eug.*:drop
rmgroup:*:eug.*:drop
checkgroups:newsadmin@efn.org:efn.*|eug.*:verify-eug.config
newgroup:newsadmin@efn.org:efn.*|eug.*:verify-eug.config
rmgroup:newsadmin@efn.org:efn.*|eug.*:verify-eug.config

# newgroup:newsadmin@efn.org:efn.*|eug.*:doit
# rmgroup:newsadmin@efn.org:efn.*|eug.*:doit

## EHIME-U (? University, Japan ?)
newgroup:news@cc.nias.ac.jp:ehime-u.*:doit
newgroup:news@doc.dpc.ehime-u.ac.jp:ehime-u.*:doit
rmgroup:news@cc.nias.ac.jp:ehime-u.*:doit
rmgroup:news@doc.dpc.ehime-u.ac.jp:ehime-u.*:doit

## ENGLAND
# Contact: admin@england.news-admin.org
# See: http://www.england.news-admin.org/newsadminsfaq.html
# *PGP*   See comment at top of file.
# Key fingerprint =  DA 3E C2 01 46 E5 61 CB  A2 43 09 CA 13 6D 31 1F 
newgroup:admin@england.news-admin.org:england.*:verify-england-usenet
rmgroup:admin@england.news-admin.org:england.*:verify-england-usenet
checkgroups:admin@england.news-admin.org:england.*:verify-england-usenet

## ES (Spain)
# Contact: Daniel.Diaz@rediris.es
# See: http://www.rediris.es/netnews/infonews/config.es.html
# See: http://news.rediris.es/~moderador/grupos/newsgroups.es
# *PGP*   See comment at top of file.
# Key fingerprint = 3B 63 18 6F 83 EA 89 82 95 1B 7F 8D B6 ED DD 87
newgroup:*:es.*:drop
rmgroup:*:es.*:drop
checkgroups:moderador@news.rediris.es:es.*:doit
newgroup:moderador@news.rediris.es:es.*:verify-es.news
rmgroup:moderador@news.rediris.es:es.*:verify-es.news

# checkgroups:moderador@news.rediris.es:es.*:mail
# newgroup:moderador@news.rediris.es:es.*:doit
# rmgroup:moderador@news.rediris.es:es.*:doit

## ESP (Spanish-language newsgroups)
# Contact: <mod-ena@ennui.org>
# URL: http://ennui.org/esp/
# *PGP*   See comment at top of file.
newgroup:*:esp.*:drop
rmgroup:*:esp.*:drop
checkgroups:mod-ena@ennui.org:esp.*:verify-esp.news.administracion
newgroup:mod-ena@ennui.org:esp.*:verify-esp.news.administracion
rmgroup:mod-ena@ennui.org:esp.*:verify-esp.news.administracion

## EUNET ( Europe )
newgroup:news@noc.eu.net:eunet.*:doit
rmgroup:news@noc.eu.net:eunet.*:doit

## EUROPA (Europe)
# URL: http://www.europa.usenet.eu.org/
# Key fingerprint = 3A 05 A8 49 FB 16 29 25  75 E3 DE BB 69 E0 1D B4
# *PGP*   See comment at top of file.
newgroup:group-admin@usenet.eu.org:europa.*:verify-group-admin@usenet.eu.org
rmgroup:group-admin@usenet.eu.org:europa.*:verify-group-admin@usenet.eu.org
checkgroups:group-admin@usenet.eu.org:europa.*:verify-group-admin@usenet.eu.org

## EXAMPLE ( Bogus hierarchy reserved for standards documents )
newgroup:*@*:example.*:mail
rmgroup:*@*:example.*:doit

## FA ( "From ARPA" gatewayed mailing lists)
# Removed in the "Great Renaming" of 1988.
newgroup:*@*:fa.*:mail
rmgroup:*@*:fa.*:doit

## FIDO newsgroups (FidoNet)
newgroup:root@mbh.org:fido.*:doit
rmgroup:root@mbh.org:fido.*:doit

## FIDO.BELG.* newsgroups (FidoNet)
# URL: http://www.z2.fidonet.org/news/fido.belg.news/
# *PGP*   See comment at top of file.
newgroup:*:fido.belg.*:drop
rmgroup:*:fido.belg.*:drop
checkgroups:fidobelg@mail.z2.fidonet.org:fido.belg.*:verify-fido.belg.news
newgroup:fidobelg@mail.z2.fidonet.org:fido.belg.*:verify-fido.belg.news
rmgroup:fidobelg@mail.z2.fidonet.org:fido.belg.*:verify-fido.belg.news

## FIDO7
# *PGP*   See comment at top of file.
newgroup:*:fido7.*:drop
rmgroup:*:fido7.*:drop
checkgroups:newgroups-request@fido7.ru:fido7.*:verify-fido7.announce.newgroups
newgroup:newgroups-request@fido7.ru:fido7.*:verify-fido7.announce.newgroups
rmgroup:newgroups-request@fido7.ru:fido7.*:verify-fido7.announce.newgroups

# newgroup:news@wing.matsim.udmurtia.su:fido7.*:doit
# rmgroup:news@wing.matsim.udmurtia.su:fido7.*:doit

## FINET (Finland and Finnish language alternative newsgroups)
# The alt.* of Finland
newgroup:*@*.fi:finet.*:doit
rmgroup:*@*.fi:finet.*:doit

## FJ (Japan and Japanese language)
# Contact: committee@fj-news.org
# URL: http://www.is.tsukuba.ac.jp/~yas/fj/
# Key URL: http://www.is.tsukuba.ac.jp/~yas/fj/fj.asc
newgroup:*:fj.*:drop
rmgroup:*:fj.*:drop
checkgroups:committee@fj-news.org:fj.*:verify-fj.news.announce
newgroup:committee@fj-news.org:fj.*:verify-fj.news.announce
rmgroup:committee@fj-news.org:fj.*:verify-fj.news.announce

## FL (Florida, USA )
newgroup:hgoldste@news1.mpcs.com:fl.*:doit
newgroup:scheidell@fdma.fdma.com:fl.*:doit
rmgroup:hgoldste@news1.mpcs.com:fl.*:doit
rmgroup:scheidell@fdma.fdma.com:fl.*:doit

## FLORA (FLORA Community WEB, Canada)
# Contact: russell@flora.org
# See: http://news.flora.org/  for newsgroup listings and information
# See: http://www.flora.org/russell/ for PGP keys
# *PGP*   See comment at top of file.
newgroup:*:flora.*:drop
rmgroup:*:flora.*:drop
checkgroups:news@flora.ottawa.on.ca:flora.*:verify-flora-news
newgroup:news@news.rediris.es:flora.*:verify-flora-news
rmgroup:news@news.rediris.es:flora.*:verify-flora-news

## FR (French Language)
# *PGP*   See comment at top of file.
newgroup:*:fr.*:drop
rmgroup:*:fr.*:drop
checkgroups:control@usenet-fr.news.eu.org:fr.*:verify-control@usenet-fr.news.eu.org
newgroup:control@usenet-fr.news.eu.org:fr.*:verify-control@usenet-fr.news.eu.org
rmgroup:control@usenet-fr.news.eu.org:fr.*:verify-control@usenet-fr.news.eu.org

# newgroup:control@usenet.fr.net:fr.*:doit
# rmgroup:control@usenet.fr.net:fr.*:doit

## FREE (Open Hierarchy where anyone can craete a group)
newgroup:*:free.*:doit
newgroup:group-admin@isc.org:free.*:drop
newgroup:tale@*uu.net:free.*:drop
rmgroup:*:free.*:drop

## FUDAI (Japanese ?)
newgroup:news@picard.cs.osakafu-u.ac.jp:fudai.*:doit
rmgroup:news@picard.cs.osakafu-u.ac.jp:fudai.*:doit

## FUR ( furrynet )
# Contact: fur-config@taronga.com
# Please contact the above address before adding these groups.

## GER & HANNET & HANNOVER & HILDESHEIM & HISS (Hannover, Germany) 
newgroup:fifi@hiss.han.de:ger.*|hannover.*|hannet.*|hildesheim.*|hiss.*:doit
rmgroup:fifi@hiss.han.de:ger.*|hannover.*|hannet.*|hildesheim.*|hiss.*:doit

## GIT (Georgia Institute of Technology, USA )
newgroup:news@news.gatech.edu:git.*:doit
newgroup:news@news.gatech.edu:git*class.*:mail
rmgroup:news@news.gatech.edu:git.*:doit

## GNU ( Free Software Foundation )
newgroup:gnu@prep.ai.mit.edu:gnu.*:doit
newgroup:news@*ai.mit.edu:gnu.*:doit
rmgroup:gnu@prep.ai.mit.edu:gnu.*:doit
rmgroup:news@*ai.mit.edu:gnu.*:doit

## GOV (Government Information)
# *PGP*   See comment at top of file.
# URL: http://www.govnews.org/govnews/
# PGPKEY URL: http://www.govnews.org/govnews/site-setup/gov.pgpkeys
newgroup:*:gov.*:drop
rmgroup:*:gov.*:drop
checkgroups:gov-usenet-announce-moderator@govnews.org:gov.*:verify-gov.usenet.announce
newgroup:gov-usenet-announce-moderator@govnews.org:gov.*:verify-gov.usenet.announce
rmgroup:gov-usenet-announce-moderator@govnews.org:gov.*:verify-gov.usenet.announce

## GWU (George Washington University, Washington, DC)
# Contact: Sweth Chandramouli <news@nit.gwu.edu>
newgroup:news@nit.gwu.edu:gwu.*:doit
rmgroup:news@nit.gwu.edu:gwu.*:doit

## HAMILTON (Canadian)
newgroup:news@*dcss.mcmaster.ca:hamilton.*:doit
rmgroup:news@*dcss.mcmaster.ca:hamilton.*:doit

## HAMSTER (Hamster, a Win32 news and mail proxy server)
# Contact: hamster-contact@snafu.de
# URL: http://www.nethamster.org
# Key fingerprint = 12 75 A9 42 8A D6 1F 77 6A CF B4 0C 79 15 5F 93
# Key URL: http://www.nethamster.org/control/hamster.asc
# *PGP*   See comment at top of file.
checkgroups:hamster-control@snafu.de:hamster.*:verify-hamster-control@snafu.de
newgroup:hamster-control@snafu.de:hamster.*:verify-hamster-control@snafu.de
rmgroup:hamster-control@snafu.de:hamster.*:verify-hamster-control@snafu.de

## HAN (Korean Hangul)
# Contact: newgroups-request@usenet.or.kr
# PGPKEY URL: ftp://ftp.usenet.or.kr/pub/korea/usenet/pgp/PGPKEY.han
# *PGP*   See comment at top of file.
newgroup:*:han.*:drop
rmgroup:*:han.*:drop
checkgroups:newgroups-request@usenet.or.kr:han.*:verify-han.news.admin
newgroup:newgroups-request@usenet.or.kr:han.*:verify-han.news.admin
rmgroup:newgroups-request@usenet.or.kr:han.*:verify-han.news.admin

## HARVARD (Harvard University, Cambridge, MA)
newgroup:*@*.harvard.edu:harvard.*:doit
rmgroup:*@*.harvard.edu:harvard.*:doit

## HAWAII 
newgroup:news@lava.net:hawaii.*:doit
rmgroup:news@lava.net:hawaii.*:doit

## HIVNET (HIVNET Foundation, for HIV+/AIDS information)
# Contact: news@hivnet.org
# Syncable server: news.hivnet.org
# Key fingerprint = 5D D6 0E DC 1E 2D EA 0B  B0 56 4D D6 52 53 D7 A4
# PGPKEY URL: ftp://ftp.hivnet.org/pub/usenet/pgpkey.hivnet
# *PGP*   See comment at top of file.
newgroup:*:hiv.*:drop
rmgroup:*:hiv.*:drop
checkgroups:news@hivnet.org:hiv.*:verify-news@hivnet.org
newgroup:news@hivnet.org:hiv.*:verify-news@hivnet.org
rmgroup:news@hivnet.org:hiv.*:verify-news@hivnet.org

## HK (Hong Kong)
newgroup:hknews@comp.hkbu.edu.hk:hk.*:doit
rmgroup:hknews@comp.hkbu.edu.hk:hk.*:doit

## HOUSTON (Houston, Texas, USA)
# *PGP*   See comment at top of file.
newgroup:*:houston.*:drop
rmgroup:*:houston.*:drop
checkgroups:news@academ.com:houston.*:verify-houston.usenet.config
newgroup:news@academ.com:houston.*:verify-houston.usenet.config
rmgroup:news@academ.com:houston.*:verify-houston.usenet.config

# newgroup:news@academ.com:houston.*:doit
# rmgroup:news@academ.com:houston.*:doit

## HUMANITYQUEST
# Contact: news-admin@humanityquest.com
# URL: http://www.humanityquest.com/projects/newsgroups/
# Key fingerprint = BA3D B306 B6F5 52AA BA8F 32F0 8C4F 5040 16F9 C046
# Key URL: http://www.humanityquest.com/projects/newsgroups/PGP.htm
# *PGP*  See comment at top of file.
newgroup:*:humanityquest.*:drop
rmgroup:*:humanityquest.*:drop
checkgroups:news-admin@humanityquest.com:humanityquest.*:verify-humanityquest.admin.config
newgroup:news-admin@humanityquest.com:humanityquest.*:verify-humanityquest.admin.config
rmgroup:news-admin@humanityquest.com:humanityquest.*:verify-humanityquest.admin.config

## HUN (Hungary)
newgroup:*:hun.*:drop
rmgroup:*:hun.*:drop
checkgroups:hun-mnt@news.sztaki.hu:hun.*:verify-hun.admin.news
newgroup:hun-mnt@news.sztaki.hu:hun.*:verify-hun.admin.news
rmgroup:hun-mnt@news.sztaki.hu:hun.*:verify-hun.admin.news

## IA (Iowa, USA)
newgroup:skunz@iastate.edu:ia.*:doit
rmgroup:skunz@iastate.edu:ia.*:doit

## IBMNET
# Contact: news@ibm.net
# For local use only, contact the above address for information.
newgroup:*@*:ibmnet.*:mail
rmgroup:*@*:ibmnet.*:doit

## ICONZ (The Internet Company of New Zealand, New Zealand)
# Contact: usenet@iconz.co.nz
# For local use only, contact the above address for information.
newgroup:*@*:iconz.*:mail
rmgroup:*@*:iconz.*:doit

## IE (Ireland)
# Contact: control@usenet.ie
# *PGP*   See comment at top of file.
newgroup:*:ie.*:drop
rmgroup:*:ie.*:drop
checkgroups:control@usenet.ie:ie.*:verify-control@usenet.ie
newgroup:control@usenet.ie:ie.*:verify-control@usenet.ie
rmgroup:control@usenet.ie:ie.*:verify-control@usenet.ie

## IEEE (Institute of Electrical and Electronic Engineers)
# Contact: <postoffice@ieee.org>
# This hierarchy is now defunct.
newgroup:*@*:ieee.*:mail
rmgroup:*@*:ieee.*:doit

## INFO newsgroups
newgroup:rjoyner@uiuc.edu:info.*:doit
rmgroup:rjoyner@uiuc.edu:info.*:doit

## IS (Iceland)
# Contact: Marius Olafsson <news@isnet.is>
# *PGP*   See comment at top of file.
newgroup:*:is.*:drop
rmgroup:*:is.*:drop
checkgroups:news@isnet.is:is.*:verify-is.isnet
newgroup:news@isnet.is:is.*:verify-is.isnet
rmgroup:news@isnet.is:is.*:verify-is.isnet

## ISC ( Japanese ?)
newgroup:news@sally.isc.chubu.ac.jp:isc.*:doit
rmgroup:news@sally.isc.chubu.ac.jp:isc.*:doit

## ISRAEL and IL newsgroups (Israel)
newgroup:news@news.biu.ac.il:israel.*:doit
rmgroup:news@news.biu.ac.il:israel.*|il.*:doit

## IT (Italian)
# Contact: gcn@news.nic.it
# URL: http://www.news.nic.it/
# Key fingerprint = 94 A4 F7 B5 46 96 D6 C7  A6 73 F2 98 C4 8C D0 E0
# Key URL: http://www.news.nic.it/pgp.txt
# *PGP*   See comment at top of file.
newgroup:*:it.*:drop
rmgroup:*:it.*:drop
checkgroups:gcn@news.nic.it:it.*:verify-gcn@news.nic.it
newgroup:gcn@news.nic.it:it.*:verify-gcn@news.nic.it
rmgroup:gcn@news.nic.it:it.*:verify-gcn@news.nic.it

## ITALIA (Italy)
# Contact: news@news.cineca.it
# URL: http://news.cineca.it/italia/
# Key fingerprint = 0F BB 71 62 DA 5D 5D B8  D5 86 FC 28 02 67 1A 6B
# Key URL: http://news.cineca.it/italia/italia-pgp.txt
# *PGP*   See comment at top of file.
newgroup:*:italia.*:drop
rmgroup:*:italia.*:drop
checkgroups:news@news.cineca.it:italia.*:verify-italia.announce.newgroups
newgroup:news@news.cineca.it:italia.*:verify-italia.announce.newgroups
rmgroup:news@news.cineca.it:italia.*:verify-italia.announce.newgroups

## IU (Indiana University)
newgroup:news@usenet.ucs.indiana.edu:iu.*:doit
newgroup:root@usenet.ucs.indiana.edu:iu.*:doit
newgroup:*@usenet.ucs.indiana.edu:iu*class.*:mail
rmgroup:news@usenet.ucs.indiana.edu:iu.*:doit
rmgroup:root@usenet.ucs.indiana.edu:iu.*:doit

## JAPAN (Japan)
# Contact: Tsuneo Tanaka <tt+null@efnet.com>
# URL: http://www.asahi-net.or.jp/~AE5T-KSN/japan-e.html
# Key fingerprint = 6A FA 19 47 69 1B 10 74  38 53 4B 1B D8 BA 3E 85
# PGP Key: http://grex.cyberspace.org/~tt/japan.admin.announce.asc
# PGP Key: http://www.asahi-net.or.jp/~AE5T-KSN/japan/japan.admin.announce.asc
# *PGP*   See comment at top of file.
newgroup:*:japan.*:drop
rmgroup:*:japan.*:drop
checkgroups:japan.admin.announce@news.efnet.com:japan.*:verify-japan.admin.announce@news.efnet.com
newgroup:japan.admin.announce@news.efnet.com:japan.*:verify-japan.admin.announce@news.efnet.com
rmgroup:japan.admin.announce@news.efnet.com:japan.*:verify-japan.admin.announce@news.efnet.com

# checkgroups:news@marie.iijnet.or.jp:japan.*:log
# newgroup:*@*:japan.*:log
# rmgroup:*@*:japan.*:log

## K12 ( US Educational Network )
newgroup:braultr@*csmanoirs.qc.ca:k12.*:doit
rmgroup:braultr@*csmanoirs.qc.ca:k12.*:doit

## KA (Karlsruhe, Germany)
# Contact: usenet@karlsruhe.org
# For private use only, contact the above address for information.
#
# URL: http://www.karlsruhe.org/             (German only)
# URL: http://www.karlsruhe.org/newsgroups   (newsgroup list)
#
# *PGP*   See comment at top of file. 
# Key fingerprint =  DE 19 BB 25 76 19 81 17  F0 67 D2 23 E8 C8 7C 90
newgroup:*:ka.*:drop
rmgroup:*:ka.*:drop
checkgroups:usenet@karlsruhe.org:ka.*:verify-usenet@karlsruhe.org
newgroup:usenet@karlsruhe.org:ka.*:verify-usenet@karlsruhe.org
rmgroup:usenet@karlsruhe.org:ka.*:verify-usenet@karlsruhe.org

## KANTO
# *PGP*   See comment at top of file.
rmgroup:*:kanto.*:drop
checkgroups:ty@kamoi.imasy.or.jp:kanto.*:verify-kanto.news.network
# NOTE: newgroups aren't verified...
newgroup:*@*.jp:kanto.*:doit
rmgroup:ty@kamoi.imasy.or.jp:kanto.*:verify-kanto.news.network

## KASSEL (Kassel, Germany)
# *PGP*   See comment at top of file.
# MAIL: pgp-public-keys@keys.de.pgp.net Subject: GET 0xC4D30EE5
newgroup:*:kassel.*:drop
rmgroup:*:kassel.*:drop
checkgroups:dirk.meyer@dinoex.sub.org:kassel.*:verify-kassel-admin
newgroup:dirk.meyer@dinoex.sub.org:kassel.*:verify-kassel-admin
rmgroup:dirk.meyer@dinoex.sub.org:kassel.*:verify-kassel-admin

## KC (Kansas City, Kansas/Missouri, USA)
newgroup:dan@sky.net:kc.*:doit
rmgroup:dan@sky.net:kc.*:doit

## KIEL (Kiel, Germany)
checkgroups:kris@white.schulung.netuse.de:kiel.*:mail
newgroup:kris@white.schulung.netuse.de:kiel.*:doit
rmgroup:kris@white.schulung.netuse.de:kiel.*:doit

## KRST (University of Oslo, Norway)
# Contact: jani@ifi.uio.no
# For private use only, contact the above address for information.
newgroup:*@*:krst.*:drop
rmgroup:*@*:krst.*:doit

## KWNET (Kitchener-Waterloo?)
# Contact: Ed Hew <edhew@xenitec.on.ca>
# For private use only, contact the above address for information.
newgroup:*@*:kwnet.*:mail
rmgroup:*@*:kwnet.*:doit

## LAW (?)
# Contact: Jim Burke <jburke@kentlaw.edu>
newgroup:*@*.kentlaw.edu:law.*:doit
newgroup:*@*.law.vill.edu:law.*:doit
rmgroup:*@*.kentlaw.edu:law.*:doit
rmgroup:*@*.law.vill.edu:law.*:doit

## LIU newsgroups (Sweden?)
newgroup:linus@tiny.lysator.liu.se:liu.*:doit
rmgroup:linus@tiny.lysator.liu.se:liu.*:doit

## LINUX (Gated Linux mailing lists)
# Contact: Marco d'Itri <md@toglimi.linux.it>
# Key fingerprint = 81 B3 27 99 4F CE 32 D1  1B C9 01 0D BB B3 2E 41
# *PGP*  See comment at top of file.
newgroup:*:linux.*:drop
rmgroup:*:linux.*:drop
checkgroups:linux-admin@bofh.it:linux.*:verify-linux-admin@bofh.it
newgroup:linux-admin@bofh.it:linux.*:verify-linux-admin@bofh.it
rmgroup:linux-admin@bofh.it:linux.*:verify-linux-admin@bofh.it

## LOCAL (Local-only groups)
# It is not really a good idea for sites to use these since they
# may occur on many unconnected sites.
newgroup:*@*:local.*:mail
rmgroup:*@*:local.*:drop

## MALTA ( Nation of Malta )
# Contact: cmeli@cis.um.edu.mt
# URL: http://www.malta.news-admin.org/
# Key fingerprint = 20 17 01 5C F0 D0 1A 42  E4 13 30 58 0B 14 48 A6
# *PGP*   See comment at top of file. 
newgroup:*:malta.*:drop
rmgroup:*:malta.*:drop
checkgroups:cmeli@cis.um.edu.mt:malta.*:verify-malta.config
newgroup:cmeli@cis.um.edu.mt:malta.*:verify-malta.config
rmgroup:cmeli@cis.um.edu.mt:malta.*:verify-malta.config

# newgroup:cmeli@cis.um.edu.mt:malta.*:doit
# rmgroup:cmeli@cis.um.edu.mt:malta.*:doit

## MANAWATU ( Manawatu district, New Zealand)
# Contact: alan@manawatu.gen.nz or news@manawatu.gen.nz
# For local use only, contact the above address for information.
newgroup:*@*:manawatu.*:mail
rmgroup:*@*:manawatu.*:doit

## MAUS ( MausNet, German )
# *PGP*   See comment at top of file. 
# Key fingerprint: 82 52 C7 70 26 B9 72 A1  37 98 55 98 3F 26 62 3E
newgroup:*:maus.*:drop
rmgroup:*:maus.*:drop
checkgroups:guenter@gst0hb.north.de:maus.*:verify-maus-info
checkgroups:guenter@gst0hb.hb.provi.de:maus.*:verify-maus-info
newgroup:guenter@gst0hb.north.de:maus.*:verify-maus-info
newgroup:guenter@gst0hb.hb.provi.de:maus.*:verify-maus-info
rmgroup:guenter@gst0hb.north.de:maus.*:verify-maus-info
rmgroup:guenter@gst0hb.hb.provi.de:maus.*:verify-maus-info

# newgroup:guenter@gst0hb.north.de:maus.*:doit
# rmgroup:guenter@gst0hb.north.de:maus.*:doit

## MCMASTER (McMaster University, Ontario)
# Contact: Brian Beckberger <news@informer1.cis.mcmaster.ca>
# For local use only, contact the above address for information.
newgroup:*@*:mcmaster.*:mail
rmgroup:*@*:mcmaster.*:doit

## MCOM ( Netscape Inc, USA) 
newgroup:*@*:mcom.*:mail
rmgroup:*@*:mcom.*:doit

## ME (Maine, USA)
newgroup:kerry@maine.maine.edu:me.*:doit
rmgroup:kerry@maine.maine.edu:me.*:doit

## MEDLUX ( All-Russia medical teleconferences )
# URL: ftp://ftp.medlux.ru/pub/news/medlux.grp
checkgroups:neil@new*.medlux.ru:medlux.*:mail
newgroup:neil@new*.medlux.ru:medlux.*:doit
rmgroup:neil@new*.medlux.ru:medlux.*:doit

## MELB ( Melbourne, Australia)
newgroup:kre@*mu*au:melb.*:doit
newgroup:revdoc@*uow.edu.au:melb.*:doit
rmgroup:kre@*mu*au:melb.*:doit
rmgroup:revdoc@*uow.edu.au:melb.*:doit

## MENSA (The Mensa Organisation)
# Contact: usenet@newsgate.mensa.org
# Key fingerprint:  A7 57 24 49 C0 D4 47 33  84 A0 52 6E F1 A4 00 5B
# *PGP*   See comment at top of file.
newgroup:*:mensa.*:drop
rmgroup:*:mensa.*:drop
checkgroups:usenet@newsgate.mensa.org:mensa.*:verify-mensa.config
newgroup:usenet@newsgate.mensa.org:mensa.*:verify-mensa.config
rmgroup:usenet@newsgate.mensa.org:mensa.*:verify-mensa.config

## METOCEAN (ISP in Japan)
newgroup:fwataru@*.metocean.co.jp:metocean.*:doit
rmgroup:fwataru@*.metocean.co.jp:metocean.*:doit

## METROPOLIS
# Contact: newsmaster@worldonline.nl
# For local use only, contact the above address for information.
newgroup:*@*:metropolis.*:mail
rmgroup:*@*:metropolis.*:doit

## MI (Michigan, USA)
# Contact: Steve Simmons <scs@lokkur.dexter.mi.us>
# URL: http://www.inland-sea.com/mi-news.html
# http://www.inland-sea.com/mi-news.html
checkgroups:scs@lokkur.dexter.mi.us:mi.*:mail
newgroup:scs@lokkur.dexter.mi.us:mi.*:doit
rmgroup:scs@lokkur.dexter.mi.us:mi.*:doit

## MILW (Milwaukee, Wisconsin, USA)
# Contact: milw@usenet.mil.wi.us
# URL: http://usenet.mil.wi.us
# Key fingerprint = 6E 9B 9F 70 98 AB 9C E5  C3 C0 05 82 21 5B F4 9E
# Key URL: http://usenet.mil.wi.us/pgpkey
# *PGP*   See comment at top of file.
checkgroups:milw@usenet.mil.wi.us:milw.*:verify-milw.config
newgroup:milw@usenet.mil.wi.us:milw.*:verify-milw.config
rmgroup:milw@usenet.mil.wi.us:milw.*:verify-milw.config

## MOD (Original top level moderated hierarchy)
# Removed in the "Great Renaming" of 1988.
# Possible revival attempt in mid-97, so watch this space..
newgroup:*@*:mod.*:mail
rmgroup:*@*:mod.*:doit

## MUC (Munchen (Munich), Germany)
# *PGP*   See comment at top of file.
# Key fingerprint = 43 C7 0E 7C 45 C7 06 E0  BD 6F 76 CE 07 39 5E 66
newgroup:*:muc.*:drop
rmgroup:*:muc.*:drop
checkgroups:muc-cmsg@muenchen.pro-bahn.org:muc.*:verify-muc.admin
newgroup:muc-cmsg@muenchen.pro-bahn.org:muc.*:verify-muc.admin
rmgroup:muc-cmsg@muenchen.pro-bahn.org:muc.*:verify-muc.admin

# newgroup:muc-cmsg@muenchen.pro-bahn.org:muc.*:doit
# rmgroup:muc-cmsg@muenchen.pro-bahn.org:muc.*:doit

## NAGASAKI-U ( Nagasaki University, Japan ?)
newgroup:root@*nagasaki-u.ac.jp:nagasaki-u.*:doit
rmgroup:root@*nagasaki-u.ac.jp:nagasaki-u.*:doit

## NAS (Numerican Aerodynamic Simulation Facility @ NASA Ames Research Center)
# Contact: news@nas.nasa.gov
# For internal use only, contact above address for questions
newgroup:*@*:nas.*:mail
rmgroup:*@*:nas.*:doit

## NASA (National Aeronautics and Space Administration, USA)
# Contact: news@nas.nasa.gov
# For internal use only, contact above address for questions
newgroup:*@*:nasa.*:mail
rmgroup:*@*:nasa.*:doit

## NC (North Carolina, USA)
# Tim Seaver <tas@bellsouth.net> says he hasn't had any dealings with nc.*
# for over two years and the hierarchy is basically "open to anyone who
# wants it."
# newgroup:tas@ncren.net:nc.*:doit
# rmgroup:tas@ncren.net:nc.*:doit

## NCF ( National Capital Freenet, Ottawa, Ontario, Canada )
# Contact: news@freenet.carleton.ca
# For local use only, contact the above address for information.
newgroup:*@*:ncf.*:mail
rmgroup:*@*:ncf.*:doit

## NCTU newsgroups (Taiwan)
newgroup:chen@cc.nctu.edu.tw:nctu.*:doit
rmgroup:chen@cc.nctu.edu.tw:nctu.*:doit

## NCU (National Central University, Taiwan)
# Contact: Ying-Hao Chang <aqlott@db.csie.ncu.edu.tw>
# Contact: <runn@news.ncu.edu.tw>
# For local use only, contact the above addresses for information.
newgroup:*@*:ncu.*:mail
rmgroup:*@*:ncu.*:doit

## NERSC (National Energy Research Scientific Computing Center)
# Contact: <usenet@nersc.gov>
# newgroup:*@*:nersc.*:mail
# rmgroup:*@*:nersc.*:doit

## NET newsgroups ( Usenet 2 )
# URL: http://www.usenet2.org
# *PGP*   See comment at top of file.
# Key fingerprint: D7 D3 5C DB 18 6A 29 79  BF 74 D4 58 A3 78 9D 22
newgroup:*:net.*:drop
rmgroup:*:net.*:drop
checkgroups:control@usenet2.org:net.*:verify-control@usenet2.org
newgroup:control@usenet2.org:net.*:verify-control@usenet2.org
rmgroup:control@usenet2.org:net.*:verify-control@usenet2.org

## NETSCAPE (Netscape Communications Corp)
# Contact: news@netscape.com
# *PGP*   See comment at top of file.
# URL: http://www.mozilla.org/community.html
# URL: http://www.mozilla.org/newsfeeds.html
# Key fingerprint = B7 80 55 12 1F 9C 17 0B  86 66 AD 3B DB 68 35 EC
newgroup:*:netscape.*:drop
rmgroup:*:netscape.*:drop
checkgroups:news@netscape.com:netscape.*:verify-netscape.public.admin
newgroup:news@netscape.com:netscape.*:verify-netscape.public.admin
rmgroup:news@netscape.com:netscape.*:verify-netscape.public.admin

# checkgroups:news@netscape.com:netscape.*:mail
# newgroup:news@netscape.com:netscape.*:doit
# rmgroups:news@netscape.com:netscape.*:doit

## NETINS ( netINS, Inc )
# Contact: news@netins.net
# For local use only, contact the above address for information.
newgroup:*@*:netins.*:mail
rmgroup:*@*:netins.*:doit

## NIAGARA (Niagara Peninsula, US/CAN)
newgroup:news@niagara.com:niagara.*:doit
rmgroup:news@niagara.com:niagara.*:doit

## NIAS (Japanese ?)
newgroup:news@cc.nias.ac.jp:nias.*:doit
rmgroup:news@cc.nias.ac.jp:nias.*:doit

## NIGERIA (Nigeria)
newgroup:news@easnet.net:nigeria.*:doit
rmgroup:news@easnet.net:nigeria.*:doit

## NIHON (Japan)
newgroup:ktomita@jade.dti.ne.jp:nihon.*:doit
rmgroup:ktomita@jade.dti.ne.jp:nihon.*:doit

## NL (Netherlands)
# Contact: nl-admin@nic.surfnet.nl
# URL: http://www.xs4all.nl/~egavic/NL/ (Dutch)
# URL: http://www.kinkhorst.com/usenet/nladmin.en.html (English)
# *PGP*   See comment at top of file.
# Key fingerprint: 45 20 0B D5 A1 21 EA 7C  EF B2 95 6C 25 75 4D 27
newgroup:*:nl.*:drop
rmgroup:*:nl.*:drop
checkgroups:nl-admin@nic.surfnet.nl:nl.*:verify-nl.newsgroups
newgroup:nl-admin@nic.surfnet.nl:nl.*:verify-nl.newsgroups
rmgroup:nl-admin@nic.surfnet.nl:nl.*:verify-nl.newsgroups

# checkgroups:nl-admin@nic.surfnet.nl:nl.*:mail
# newgroup:nl-admin@nic.surfnet.nl:nl.*:doit
# rmgroup:nl-admin@nic.surfnet.nl:nl.*:doit

## NL-ALT (Alternative Netherlands groups)
# URL: http://www.xs4all.nl/~onno/nl-alt/
# Several options are given in the FAQ for creating and removing groups.
# *PGP*   See comment at top of file.
# Key fingerprint: 6B 62 EB 53 4D 5D 2F 96  35 D9 C8 9C B0 65 0E 4C
rmgroup:*:nl-alt.*:drop
checkgroups:nl-alt-janitor@surfer.xs4all.nl:nl-alt.*:verify-nl-alt.config.admin
newgroup:*@*:nl-alt.*:doit
rmgroup:nl-alt-janitor@surfer.xs4all.nl:nl-alt.*:verify-nl-alt.config.admin
rmgroup:news@kink.xs4all.nl:nl-alt.*:verify-nl-alt.config.admin

## NLNET newsgroups (Netherlands ISP)
newgroup:beheer@nl.net:nlnet.*:doit
rmgroup:beheer@nl.net:nlnet.*:doit

## NM (New Mexico, USA)
newgroup:news@tesuque.cs.sandia.gov:nm.*:doit
rmgroup:news@tesuque.cs.sandia.gov:nm.*:doit

## NO (Norway)
# See also http://www.usenet.no/
# *PGP*   See comment at top of file.
newgroup:*:no.*:drop
rmgroup:*:no.*:drop
checkgroups:control@usenet.no:no.*:verify-no-hir-control
newgroup:control@usenet.no:no.*:verify-no-hir-control
newgroup:*@*.no:no.alt.*:doit
rmgroup:control@usenet.no:no.*:verify-no-hir-control

# checkgroups:control@usenet.no:no.*:mail
# newgroup:control@usenet.no:no.*:doit
# newgroup:*@*.no:no.alt.*:doit
# rmgroup:control@usenet.no:no.*:doit
# sendsys:news@*uninett.no:no.*:doit
# sendsys:control@usenet.no:no.*:doit

## NORD (Northern Germany)
# thilo@own.deceiver.org no longer a valid address
# newgroup:thilo@own.deceiver.org:nord.*:doit
# rmgroup:thilo@own.deceiver.org:nord.*:doit

## NV (Nevada)
newgroup:doctor@netcom.com:nv.*:doit
newgroup:cshapiro@netcom.com:nv.*:doit
rmgroup:doctor@netcom.com:nv.*:doit
rmgroup:cshapiro@netcom.com:nv.*:doit

## NY (New York State, USA)
newgroup:root@ny.psca.com:ny.*:mail
rmgroup:root@ny.psca.com:ny.*:mail

## NYC (New York City)
# Contact: Perry E. Metzger <perry@piermont.com>
newgroup:perry@piermont.com:nyc.*:doit
rmgroup:perry@piermont.com:nyc.*:doit

## NZ (New Zealand)
# *PGP*   See comment at top of file.
# Contact root@usenet.net.nz
# URL: http://usenet.net.nz
# URL: http://www.faqs.org/faqs/usenet/nz-news-hierarchy
# PGP fingerprint: 07 DF 48 AA D0 ED AA 88  16 70 C5 91 65 3D 1A 28
newgroup:*:nz.*:drop
rmgroup:*:nz.*:drop
checkgroups:root@usenet.net.nz:nz.*:verify-nz-hir-control
newgroup:root@usenet.net.nz:nz.*:verify-nz-hir-control
rmgroup:root@usenet.net.nz:nz.*:verify-nz-hir-control

# newgroup:root@usenet.net.nz:nz.*:doit
# rmgroup:root@usenet.net.nz:nz.*:doit

## OC newsgroups (Orange County, California, USA)
newgroup:bob@tsunami.sugarland.unocal.com:oc.*:doit
rmgroup:bob@tsunami.sugarland.unocal.com:oc.*:doit

## OH (Ohio, USA)
newgroup:trier@ins.cwru.edu:oh.*:doit
rmgroup:trier@ins.cwru.edu:oh.*:doit

## OK (Oklahoma, USA)
newgroup:quentin@*qns.com:ok.*:doit
rmgroup:quentin@*qns.com:ok.*:doit

## OKINAWA (Okinawa, Japan)
newgroup:news@opus.or.jp:okinawa.*:doit
rmgroup:news@opus.or.jp:okinawa.*:doit

## ONT (Ontario, Canada)
newgroup:pkern@gpu.utcc.utoronto.ca:ont.*:doit
rmgroup:pkern@gpu.utcc.utoronto.ca:ont.*:doit

## OTT (Ottawa, Ontario, Canada)
# Contact: onag@pinetree.org
# URL: http://www.pinetree.org/ONAG/
newgroup:news@bnr.ca:ott.*:doit
newgroup:news@nortel.ca:ott.*:doit
newgroup:clewis@ferret.ocunix.on.ca:ott.*:doit
newgroup:news@ferret.ocunix.on.ca:ott.*:doit
newgroup:news@*pinetree.org:ott.*:doit
newgroup:gordon@*pinetree.org:ott.*:doit
newgroup:dave@revcan.ca:ott.*:doit
rmgroup:news@bnr.ca:ott.*:doit
rmgroup:news@nortel.ca:ott.*:doit
rmgroup:clewis@ferret.ocunix.on.ca:ott.*:doit
rmgroup:news@ferret.ocunix.on.ca:ott.*:doit
rmgroup:news@*pinetree.org:ott.*:doit
rmgroup:gordon@*pinetree.org:ott.*:doit
rmgroup:dave@revcan.ca:ott.*:doit

## PA (Pennsylvania, USA)
# URL: http://www.netcom.com/~rb1000/pa_hierarchy/
newgroup:fxp@epix.net:pa.*:doit
rmgroup:fxp@epix.net:pa.*:doit

## PGH (Pittsburgh, Pennsylvania, USA)
# *PGP*   See comment at top of file.
newgroup:*:pgh.*:drop
rmgroup:*:pgh.*:drop
checkgroups:pgh-config@psc.edu:pgh.*:verify-pgh.config
newgroup:pgh-config@psc.edu:pgh.*:verify-pgh.config
rmgroup:pgh-config@psc.edu:pgh.*:verify-pgh.config

# checkgroups:pgh-config@psc.edu:pgh.*:mail
# newgroup:pgh-config@psc.edu:pgh.*:doit
# rmgroup:pgh-config@psc.edu:pgh.*:doit

## PHL (Philadelphia, Pennsylvania, USA)
newgroup:news@vfl.paramax.com:phl.*:doit
rmgroup:news@vfl.paramax.com:phl.*:doit

## PIN (Personal Internauts' NetNews)
newgroup:pin-admin@forus.or.jp:pin.*:doit
rmgroup:pin-admin@forus.or.jp:pin.*:doit

## PIPEX (UUNET WorldCom UK)
# Contact: Russell Vincent <news-control@ops.pipex.net>
newgroup:news-control@ops.pipex.net:pipex.*:doit
rmgroup:news-control@ops.pipex.net:pipex.*:doit

## PITT (University of Pittsburgh, PA)
newgroup:news+@pitt.edu:pitt.*:doit
newgroup:news@toads.pgh.pa.us:pitt.*:doit
rmgroup:news+@pitt.edu:pitt.*:doit
rmgroup:news@toads.pgh.pa.us:pitt.*:doit

## PL (Poland and Polish language)
## For more info, see http://www.ict.pwr.wroc.pl/doc/news-pl-new-site-faq.html
# *PGP*   See comment at top of file.
newgroup:*:pl.*:drop
rmgroup:*:pl.*:drop
checkgroups:michalj@*fuw.edu.pl:pl.*:verify-pl.announce.newgroups
checkgroups:newgroup@usenet.pl:pl.*:verify-pl.announce.newgroups
newgroup:michalj@*fuw.edu.pl:pl.*:verify-pl.announce.newgroups
newgroup:newgroup@usenet.pl:pl.*:verify-pl.announce.newgroups
rmgroup:michalj@*fuw.edu.pl:pl.*:verify-pl.announce.newgroups
rmgroup:newgroup@usenet.pl:pl.*:verify-pl.announce.newgroups

# newgroup:michalj@*fuw.edu.pl:pl.*:doit
# newgroup:newgroup@usenet.pl:pl.*:doit
# rmgroup:michalj@*fuw.edu.pl:pl.*:doit
# rmgroup:newgroup@usenet.pl:pl.*:doit

## PLANET ( PlaNet FreeNZ co-operative, New Zealand)
# Contact: office@pl.net
# For local use only, contact the above address for information.
newgroup:*@*:planet.*:mail
rmgroup:*@*:planet.*:doit

## PRIMA (prima.ruhr.de/Prima e.V. in Germany)
# Contact: admin@prima.ruhr.de
# For internal use only, contact above address for questions
newgroup:*@*:prima.*:mail
rmgroup:*@*:prima.*:doit

## PSU ( Penn State University, USA )
# Contact: Dave Barr (barr@math.psu.edu)
# For internal use only, contact above address for questions
newgroup:*@*:psu.*:mail
rmgroup:*@*:psu.*:doit

## PT (Portugal and Portuguese language)
newgroup:*:pt.*:drop
rmgroup:*:pt.*:drop
checkgroups:pmelo@*.inescc.pt:pt.*:verify-control@usenet-pt.org
newgroup:pmelo@*.inescc.pt:pt.*:verify-control@usenet-pt.org
rmgroup:pmelo@*.inescc.pt:pt.*:verify-control@usenet-pt.org

## PUBNET 
# This Hierarchy is now defunct.
# URL: ftp://ftp.isc.org/pub/usenet/control/pubnet/pubnet.config.Z
newgroup:*@*:pubnet.*:mail
rmgroup:*@*:pubnet.*:doit

## RELCOM ( Commonwealth of Independent States)
# The official list of relcom groups is supposed to be available from
# URL: ftp://ftp.kiae.su/relcom/netinfo/telconfs.txt
# *PGP*   See comment at top of file.
newgroup:*:relcom.*:drop
rmgroup:*:relcom.*:drop
checkgroups:coord@new*.relcom.ru:relcom.*:verify-relcom.newsgroups
newgroup:coord@new*.relcom.ru:relcom.*:verify-relcom.newsgroups
rmgroup:coord@new*.relcom.ru:relcom.*:verify-relcom.newsgroups

## RPI ( Rensselaer Polytechnic Institute, Troy, NY, USA)
# Contact: sofkam@rpi.edu
# For local use only, contact the above address for information.
newgroup:*@*:rpi.*:mail
rmgroup:*@*:rpi.*:doit

## SAAR (Saarbruecke, Germany)
checkgroups:thomas.rachel@gmx.de:saar.*:verify-saar-control
newgroup:thomas.rachel@gmx.de:saar.*:verify-saar-control
rmgroup:thomas.rachel@gmx.de:saar.*:verify-saar-control

## SACHSNET (German)
newgroup:root@lusatia.de:sachsnet.*:doit
rmgroup:root@lusatia.de:sachsnet.*:doit

## SAT (San Antonio, Texas, USA)
# *PGP*   See comment at top of file.
# Contact: satgroup@endicor.com
# URL: http://www.endicor.com/~satgroup/
newgroup:*:sat.*:drop
rmgroup:*:sat.*:drop
checkgroups:satgroup@endicor.com:sat.*:verify-satgroup@endicor.com
newgroup:satgroup@endicor.com:sat.*:verify-satgroup@endicor.com
rmgroup:satgroup@endicor.com:sat.*:verify-satgroup@endicor.com

# checkgroups:satgroup@endicor.com:sat.*:doit
# newgroup:satgroup@endicor.com:sat.*:doit
# rmgroup:satgroup@endicor.com:sat.*:doit

## SBAY (South Bay/Silicon Valley, California)
newgroup:steveh@grafex.sbay.org:sbay.*:doit
newgroup:ikluft@thunder.sbay.org:sbay.*:doit
rmgroup:steveh@grafex.sbay.org:sbay.*:mail
rmgroup:ikluft@thunder.sbay.org:sbay.*:mail

## SCHULE
# Contact: schule-admin@roxel.ms.sub.org
# URL: http://home.pages.de/~schule-admin/
# Key fingerprint = 64 06 F0 AE E1 46 85 0C  BD CA 0E 53 8B 1E 73 D2
# Key URL: http://home.pages.de/~schule-admin/schule.asc
# *PGP*   See comment at top of file.
newgroup:*:schule.*:drop
rmgroup:*:schule.*:drop
checkgroups:newsctrl@schule.de:schule.*:verify-schule.konfig
newgroup:newsctrl@schule.de:schule.*:verify-schule.konfig
rmgroup:newsctrl@schule.de:schule.*:verify-schule.konfig

## SDNET (Greater San Diego Area, California, USA)
# URL: http://www-rohan.sdsu.edu/staff/wk/w/sdnet.html
# URL: http://www-rohan.sdsu.edu/staff/wk/w/config.html
# URL: ftp://ftp.csu.net/pub/news/active
newgroup:wkronert@sunstroke.sdsu.edu:sdnet.*:doit
rmgroup:wkronert@sunstroke.sdsu.edu:sdnet.*:doit

## SDSU (San Diego State University, CA)
# Contact: Craig R. Sadler <usenet@sdsu.edu>
# For local use only, contact the above address for information.
newgroup:*@*:sdsu.*:mail
rmgroup:*@*:sdsu.*:doit

## SE (Sweden)
# Contact: usenet@usenet-se.net
# URL: http://www.usenet-se.net/
# URL: http://www.usenet-se.net/index_eng.html  (English version)
# Key URL:  http://www.usenet-se.net/pgp-key.txt
# Key fingerprint = 68 03 F0 FD 0C C3 4E 69  6F 0D 0C 60 3C 58 63 96
# *PGP*   See comment at top of file.
newgroup:*:se.*:drop
rmgroup:*:se.*:drop
checkgroups:usenet@usenet-se.net:se.*:verify-usenet-se
newgroup:usenet@usenet-se.net:se.*:verify-usenet-se
rmgroup:usenet@usenet-se.net:se.*:verify-usenet-se

# newgroup:usenet@usenet-se.net:se.*:doit
# rmgroup:usenet@usenet-se.net:se.*:doit
# checkgroups:usenet@usenet-se.net:se.*:doit

## SEATTLE (Seattle, Washington, USA)
newgroup:billmcc@akita.com:seattle.*:doit
newgroup:graham@ee.washington.edu:seattle.*:doit
rmgroup:billmcc@akita.com:seattle.*:doit
rmgroup:graham@ee.washington.edu:seattle.*:doit

## SFNET newsgroups (Finland)
newgroup:sfnet@*.cs.tut.fi:sfnet.*:doit
rmgroup:sfnet@*.cs.tut.fi:sfnet.*:doit

## SHAMASH (Jewish)
newgroup:archives@israel.nysernet.org:shamash.*:doit
rmgroup:archives@israel.nysernet.org:shamash.*:doit

## SI (The Republic of Slovenia)
# *PGP*   See comment at top of file.
newgroup:*:si.*:drop
rmgroup:*:si.*:drop
checkgroups:news-admin@arnes.si:si.*:verify-si.news.announce.newsgroups
newgroup:news-admin@arnes.si:si.*:verify-si.news.announce.newsgroups
rmgroup:news-admin@arnes.si:si.*:verify-si.news.announce.newsgroups

# newgroup:news-admin@arnes.si:si.*:doit
# rmgroup:news-admin@arnes.si:si.*:doit

## SK (Slovakia)
checkgroups:uhlar@ccnews.ke.sanet.sk:sk.*:mail
newgroup:uhlar@ccnews.ke.sanet.sk:sk.*:doit
rmgroup:uhlar@ccnews.ke.sanet.sk:sk.*:doit

## SLAC ( Stanford Linear Accelerator Center, Stanford, USA )
# Contact: news@news.stanford.edu
# Limited distribution hierarchy, contact the above address for information.
newgroup:news@news.stanford.edu:slac.*:mail
rmgroup:news@news.stanford.edu:slac.*:doit

## SLO (San Luis Obispo, CA)
newgroup:news@punk.net:slo.*:doit
rmgroup:news@punk.net:slo.*:doit

## SOLENT (Solent region, England)
newgroup:news@tcp.co.uk:solent.*:doit
rmgroup:news@tcp.co.uk:solent.*:doit

## StarOffice (Sun Microsystems, Inc.)
# For the StarOffice business suite.
# Contact: news@stardivision.de
# URL: http://www.sun.com/products/staroffice/newsgroups.html
# Syncable server: starnews.sun.com
# Key fingerprint = F6 6A 5C 57 77 1F 63 26  F2 43 02 41 75 2A 04 1C
# *PGP*   See comment at top of file.
newgroup:*:staroffice.*:drop
rmgroup:*:staroffice.*:drop
checkgroups:news@stardivision.de:staroffice.*:verify-staroffice.admin
newgroup:news@stardivision.de:staroffice.*:verify-staroffice.admin
rmgroup:news@stardivision.de:staroffice.*:verify-staroffice.admin

## STGT (Stuttgart, Germany)
checkgroups:news@news.uni-stuttgart.de:stgt.*:mail
newgroup:news@news.uni-stuttgart.de:stgt.*:doit
rmgroup:news@news.uni-stuttgart.de:stgt.*:doit

## STL (Saint Louis, Missouri, USA)
newgroup:news@icon-stl.net:stl.*:doit
rmgroup:news@icon-stl.net:stl.*:doit

## SU ( Stanford University, USA )
# Contact: news@news.stanford.edu
# For local use only, contact the above address for information.
newgroup:*@*:su.*:mail
rmgroup:*@*:su.*:doit

## SUNET (Swedish University Network)
newgroup:ber@*.sunet.se:sunet.*:doit
rmgroup:ber@*.sunet.se:sunet.*:doit

## SURFNET (Dutch Universities network)
newgroup:news@info.nic.surfnet.nl:surfnet.*:doit
rmgroup:news@info.nic.surfnet.nl:surfnet.*:doit

## SWNET (Sverige, Sweden)
newgroup:ber@sunic.sunet.se:swnet.*:doit
rmgroup:ber@sunic.sunet.se:swnet.*:doit

## TAMU (Texas A&M University)
# Contact: Philip Kizer <news@tamu.edu>
newgroup:news@tamsun.tamu.edu:tamu.*:doit
rmgroup:news@tamsun.tamu.edu:tamu.*:doit

## TAOS (Taos, New Mexico, USA)
# Contact: "Chris Gunn" <cgunn@laplaza.org>
newgroup:cgunn@laplaza.org:taos.*:doit
rmgroup:cgunn@laplaza.org:taos.*:doit

## TCFN (Toronto Free Community Network, Canada)
newgroup:news@t-fcn.net:tcfn.*:doit
rmgroup:news@t-fcn.net:tcfn.*:doit

## T-NETZ (German Email Network)
# Defunct, use z-netz.*
newgroup:*@*:t-netz.*:mail
rmgroup:*@*:t-netz.*:doit

## TELE (Tele Danmark Internet)
# Contact: usenet@tdk.net
# For internal use only, contact above address for questions
newgroup:*@*:tele.*:mail
rmgroup:*@*:tele.*:doit

## TERMVAKT (University of Oslo, Norway)
# Contact: jani@ifi.uio.no
# For private use only, contact the above address for information.
newgroup:*@*:termvakt.*:drop
rmgroup:*@*:termvakt.*:doit

## TEST (Local test hierarchy)
# It is not really a good idea for sites to use these since they
# may occur on many unconnect sites.
newgroup:*@*:test.*:mail
rmgroup:*@*:test.*:mail

## THUR ( Thuringia, Germany )
# *PGP*   See comment at top of file.
# Key Fingerprint: 7E 3D 73 13 93 D4 CA 78  39 DE 3C E7 37 EE 22 F1
newgroup:*:thur.*:drop
rmgroup:*:thur.*:drop
checkgroups:usenet@thur.de:thur.*:verify-thur.net.news.groups
newgroup:usenet@thur.de:thur.*:verify-thur.net.news.groups
rmgroup:usenet@thur.de:thur.*:verify-thur.net.news.groups

## TNN ( The Network News, Japan )
newgroup:tnn@iij-mc.co.jp:tnn.*:doit
newgroup:netnews@news.iij.ad.jp:tnn.*:doit
rmgroup:tnn@iij-mc.co.jp:tnn.*:doit
rmgroup:netnews@news.iij.ad.jp:tnn.*:doit

## TRIANGLE (Central North Carolina, USA )
newgroup:jfurr@acpub.duke.edu:triangle.*:doit
newgroup:tas@concert.net:triangle.*:doit
newgroup:news@news.duke.edu:triangle.*:doit
rmgroup:jfurr@acpub.duke.edu:triangle.*:doit
rmgroup:tas@concert.net:triangle.*:doit
rmgroup:news@news.duke.edu:triangle.*:doit

## TUM (Technische Universitaet Muenchen)
newgroup:news@informatik.tu-muenchen.de:tum.*:doit
rmgroup:news@informatik.tu-muenchen.de:tum.*:doit

## TW (Taiwan)
newgroup:ltc@news.cc.nctu.edu.tw:tw.*:doit
newgroup:k-12@news.nchu.edu.tw:tw.k-12.*:doit
rmgroup:ltc@news.cc.nctu.edu.tw:tw.*:doit
rmgroup:k-12@news.nchu.edu.tw:tw.k-12*:doit

## TX (Texas, USA)
newgroup:eric@cirr.com:tx.*:doit
newgroup:fletcher@cs.utexas.edu:tx.*:doit
newgroup:usenet@academ.com:tx.*:doit
rmgroup:eric@cirr.com:tx.*:doit
rmgroup:fletcher@cs.utexas.edu:tx.*:doit
rmgroup:usenet@academ.com:tx.*:doit

## UA (Ukraine)
# probable tale mistype - meant ukr.*
# newgroup:*@sita.kiev.ua:ua.*:doit
# rmgroup:*@sita.kiev.ua:ua.*:doit

## UCB ( University of California Berkeley, USA)
# Contact: Chris van den Berg <usenet@agate.berkeley.edu>
newgroup:*:ucb.*:drop
rmgroup:*:ucb.*:drop
checkgroups:usenet@agate.berkeley.edu:ucb.*:verify-ucb.news
newgroup:usenet@agate.berkeley.edu:ucb.*:verify-ucb.news
rmgroup:usenet@agate.berkeley.edu:ucb.*:verify-ucb.news

## UCD ( University of California Davis, USA )
newgroup:usenet@rocky.ucdavis.edu:ucd.*:doit
newgroup:usenet@mark.ucdavis.edu:ucd.*:doit
rmgroup:usenet@rocky.ucdavis.edu:ucd.*:doit
rmgroup:usenet@mark.ucdavis.edu:ucd.*:doit

## UFRA (Unterfranken, Deutschland)
# Contact: news@mayn.de
# URL: http://www.mayn.de/users/news/
# Key fingerprint = F7 AD 96 D8 7A 3F 7E 84  02 0C 83 9A DB 8F EB B8
# Syncable server: news.mayn.de (contact news@mayn.de if permission denied)
# *PGP*   See comment at top of file.
newgroup:*:ufra.*:drop
rmgroup:*:ufra.*:drop
checkgroups:news@mayn.de:ufra.*:verify-news.mayn.de
newgroup:news@mayn.de:ufra.*:verify-news.mayn.de
rmgroup:news@mayn.de:ufra.*:verify-news.mayn.de

# newgroup:news@mayn.de:ufra.*:verify-news.mayn.de
# rmgroup:news@mayn.de:ufra.*:verify-news.mayn.de
# checkgroups:news@mayn.de:ufra.*:verify-news.mayn.de

## UIUC (University of Illinois, USA )
newgroup:p-pomes@*.cso.uiuc.edu:uiuc.*:doit
newgroup:paul@*.cso.uiuc.edu:uiuc.*:doit
rmgroup:p-pomes@*.cso.uiuc.edu:uiuc.*:doit
rmgroup:paul@*.cso.uiuc.edu:uiuc.*:doit

## UK (United Kingdom of Great Britain and Northern Ireland)
# *PGP*   See comment at top of file.
newgroup:*:uk.*:drop
rmgroup:*:uk.*:drop
checkgroups:control@usenet.org.uk:uk.*:verify-uk.net.news.announce
newgroup:control@usenet.org.uk:uk.*:verify-uk.net.news.announce
rmgroup:control@usenet.org.uk:uk.*:verify-uk.net.news.announce

# checkgroups:control@usenet.org.uk:uk.*:mail
# newgroup:control@usenet.org.uk:uk.*:doit
# rmgroup:control@usenet.org.uk:uk.*:doit

## UKR ( Ukraine )
newgroup:ay@sita.kiev.ua:ukr.*:doit
rmgroup:ay@sita.kiev.ua:ukr.*:doit

## UMICH (University of Michigan)
newgroup:*@*.umich.edu:umich.*:doit
rmgroup:*@*.umich.edu:umich.*:doit

## UMN (University of Minnesota, USA )
newgroup:edh@*.tc.umn.edu:umn.*:doit
newgroup:news@*.tc.umn.edu:umn.*:doit
newgroup:Michael.E.Hedman-1@umn.edu:umn.*:doit
newgroup:edh@*.tc.umn.edu:umn*class.*:mail
newgroup:news@*.tc.umn.edu:umn*class.*:mail
newgroup:Michael.E.Hedman-1@umn.edu:umn*class.*:mail
rmgroup:news@*.tc.umn.edu:umn.*:doit
rmgroup:edh@*.tc.umn.edu:umn.*:doit
rmgroup:Michael.E.Hedman-1@umn.edu:umn.*:doit

## UN (The United Nations)
# URL: http://www.itu.int/Conferences/un/
# *PGP*   See comment at top of file.
newgroup:*:un.*:drop
rmgroup:*:un.*:drop
checkgroups:news@news.itu.int:un.*:verify-ungroups@news.itu.int
newgroup:news@news.itu.int:un.*:verify-ungroups@news.itu.int
rmgroup:news@news.itu.int:un.*:verify-ungroups@news.itu.int

# checkgroups:news@news.itu.int:un.*:mail
# newgroup:news@news.itu.int:un.*:doit
# rmgroup:news@news.itu.int:un.*:doit

## UO (University of Oregon, Eugene, Oregon, USA )
newgroup:newsadmin@news.uoregon.edu:uo.*:doit
rmgroup:newsadmin@news.uoregon.edu:uo.*:doit

## US (United States of America)
# *PGP*   See comment at top of file.
checkgroups:usadmin@wwa.com:us.*:mail
newgroup:usadmin@wwa.com:us.*:doit
rmgroup:usadmin@wwa.com:us.*:doit

## UT (U. of Toronto)
# newgroup:news@ecf.toronto.edu:ut.*:doit
# newgroup:news@ecf.toronto.edu:ut.class.*:mail
# rmgroup:news@ecf.toronto.edu:ut.*:doit

## UTA (Finnish)
newgroup:news@news.cc.tut.fi:uta.*:doit
rmgroup:news@news.cc.tut.fi:uta.*:doit

## UTEXAS (University of Texas, USA )
newgroup:fletcher@cs.utexas.edu:utexas.*:doit
newgroup:news@geraldo.cc.utexas.edu:utexas.*:doit
newgroup:fletcher@cs.utexas.edu:utexas*class.*:mail
newgroup:news@geraldo.cc.utexas.edu:utexas*class.*:mail
rmgroup:fletcher@cs.utexas.edu:utexas.*:doit
rmgroup:news@geraldo.cc.utexas.edu:utexas.*:doit

## UTWENTE (University of Twente, Netherlands)
# Contact: newsmaster@utwente.nl
# For internal use only, contact above address for questions
newgroup:*@*:utwente.*:mail
rmgroup:*@*:utwente.*:doit

## UVA (virginia.edu - University of Virginia)
# Contact: usenet@virginia.edu
# For internal use only, contact above address for questions
newgroup:*@*:uva.*:mail
rmgroup:*@*:uva.*:doit

## UW (University of Waterloo, Canada)
newgroup:bcameron@math.uwaterloo.ca:uw.*:doit
rmgroup:bcameron@math.uwaterloo.ca:uw.*:doit

## UWARWICK (University of Warwick, UK)
# Contact: Jon Harley <news@csv.warwick.ac.uk>
# For internal use only, contact above address for questions
newgroup:*@*:uwarwick.*:mail
rmgroup:*@*:uwarwick.*:doit

## UWO (University of Western Ontario, London, Canada)
newgroup:reggers@julian.uwo.ca:uwo.*:doit
rmgroup:reggers@julian.uwo.ca:uwo.*:doit

## VEGAS (Las Vegas, Nevada, USA)
newgroup:cshapiro@netcom.com:vegas.*:doit
newgroup:doctor@netcom.com:vegas.*:doit
rmgroup:cshapiro@netcom.com:vegas.*:doit
rmgroup:doctor@netcom.com:vegas.*:doit

## VGC (Japan groups?)
newgroup:news@isl.melco.co.jp:vgc.*:doit
rmgroup:news@isl.melco.co.jp:vgc.*:doit

## VMSNET ( VMS Operating System )
newgroup:cts@dragon.com:vmsnet.*:doit
rmgroup:cts@dragon.com:vmsnet.*:doit

## WADAI (Japanese ?) 
newgroup:kohe-t@*wakayama-u.ac.jp:wadai.*:doit
rmgroup:kohe-t@*wakayama-u.ac.jp:wadai.*:doit

## WALES (Wales)
# Contact: committee@wales-usenet.org
# URL: http://www.wales-usenet.org/
# Key fingerprint = 2D 9E DE DF 12 DA 34 5C  49 E1 EE 28 E3 AB 0D AD
# *PGP*   See comment at top of file.
newgroup:*:wales.*:drop
rmgroup:*:wales.*:drop
newgroup:control@wales-usenet.org:wales.*:verify-wales-usenet
rmgroup:control@wales-usenet.org:wales.*:verify-wales-usenet
checkgroups:control@wales-usenet.org:wales.*:verify-wales-usenet

## WASH (Washington State, USA)
newgroup:graham@ee.washington.edu:wash.*:doit
rmgroup:graham@ee.washington.edu:wash.*:doit

## WEST-VIRGINIA (West Virginia, USA)
# Note: checkgroups only by bryan27, not mark.
checkgroups:bryan27@hgo.net:west-virginia.*:doit
newgroup:mark@bluefield.net:west-virginia.*:doit
newgroup:bryan27@hgo.net:west-virginia.*:doit
rmgroup:mark@bluefield.net:west-virginia.*:doit
rmgroup:bryan27@hgo.net:west-virginia.*:doit

## WORLDONLINE
# Contact: newsmaster@worldonline.nl
# For local use only, contact the above address for information.
newgroup:*@*:worldonline.*:mail
rmgroup:*@*:worldonline.*:doit

## WPG (Winnipeg, Manitoba, Canada)
# Contact: Gary Mills <mills@cc.umanitoba.ca>
newgroup:mills@cc.umanitoba.ca:wpg.*:doit
rmgroup:mills@cc.umanitoba.ca:wpg.*:doit

## WPI (Worcester Polytechnic Institute, Worcester, MA)
newgroup:aej@*.wpi.edu:wpi.*:doit
rmgroup:aej@*.wpi.edu:wpi.*:doit

## WU (Washington University at St. Louis, MO)
newgroup:*@*.wustl.edu:wu.*:doit
rmgroup:*@*.wustl.edu:wu.*:doit

## XS4ALL (XS4ALL, Netherlands)
# Contact: Cor Bosman <news@xs4all.nl>
newgroup:news@*xs4all.nl:xs4all.*:doit
rmgroup:news@*xs4all.nl:xs4all.*:doit

## YORK (York University, Toronto, ON)
# Contact: Peter Marques <news@yorku.ca>
# For local use only, contact the above address for information.
newgroup:*@*:york.*:mail
rmgroup:*@*:york.*:doit

## Z-NETZ (German non internet based network.)
# *PGP*   See comment at top of file.
# MAIL: pgp-public-keys@informatik.uni-hamburg.de Subject: GET 0x40145FC9
newgroup:*:z-netz.*:drop
rmgroup:*:z-netz.*:drop
checkgroups:dirk.meyer@dinoex.sub.org:z-netz.*:verify-checkgroups-dinoex
newgroup:dirk.meyer@dinoex.sub.org:z-netz.*:verify-checkgroups-dinoex
newgroup:*@*.de:z-netz.alt.*:doit
newgroup:*@*.sub.org:z-netz.alt.*:doit
rmgroup:dirk.meyer@dinoex.sub.org:z-netz.*:verify-checkgroups-dinoex

# newgroup:*@*.de:z-netz.*:mail
# newgroup:*@*.sub.org:z-netz.*:mail
# rmgroup:*@*.de:z-netz.*:mail

## ZA (South Africa)
newgroup:root@duvi.eskom.co.za:za.*:doit
newgroup:ccfj@hippo.ru.ac.za:za.*:doit
rmgroup:root@duvi.eskom.co.za:za.*:doit
rmgroup:ccfj@hippo.ru.ac.za:za.*:doit

## ZER (German Email Network)
# Defunct, use z-netz.*
newgroup:*@*:zer.*:mail
rmgroup:*@*:zer.*:doit
