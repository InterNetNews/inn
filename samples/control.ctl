##  rone's unified control.ctl
##  control.ctl - access control for control messages
##  Format:
##	<message>:<from>:<newsgroups>:<action>
##  The last match found is used.
##	<message>	Control message or "all" if it applies
##			to all control messages.
##	<from>		Pattern that must match the From line.
##	<newsgroups>	Pattern that must match the newsgroup being
##			newgroup'd or rmgroup'd (ignored for other messages).
##      <action>        What to do:
##                          doit        Perform action (usually sends mail too)
##                          doifarg     Do if command has an arg (see sendsys)
##              doit=xxx    Do action; log to xxx (see below)
##                          drop        Ignore message
##                          log         One line to error log
##                          log=xxx     Log to xxx (see below)
##                          mail        Send mail to admin
##                          verify-pgp_userid   Do PGP verification on user.
##                          verify-pgp_userid=logfile   PGP verify and log.
##                      xxx=mail to mail; xxx= (empty) to toss; xxx=/full/path
##                      to log to /full/path; xxx=foo to log to ${LOG}/foo.log
##
## The defaults have changed.
##
## Firstly. Most things that caused mail in the past no longer do. This is
## due to proliferation of forged control messages filling up mailboxes.
##
## Secondly, the assumption now is that you have pgp on your system. If you
## don't, then you should get it to help protect youself against all the
## luser control message forgers. If you can't use pgp, then you'll have
## to fix some sections here. Search for *PGP*. At each "*PGP*" found
## you'll need to comment out the block of lines right after it (that have
## 'verify-' in their 4th field). Then uncomment the block of lines that
## comes right after that. You'll also need to change pgpverify in inn.conf.
##
## For more information on using PGP to verify control messages, upgrade
## to INN-1.5 (or later) or see: ftp://ftp.isc.org/pub/pgpcontrol/
## 
## --------------------------------------------------------------------------
##
## A number of hierarchies are for local use only but have leaked out into
## the general stream. In this config file the are set so they are easy to
## remove and will not be added by sites. Please delete them if you 
## carry them. If you wish to carry them please contact the address given to
## arrange a feed.
## 
## If you have permission to carry any of the hierachies listed in this file
## as "local only", "defunct" or "private", you should change the entry listed.
## 
## --------------------------------------------------------------------------

##	DEFAULT
all:*:*:mail

## --------------------------------------------------------------------------
##	CHECKGROUPS MESSAGES
## --------------------------------------------------------------------------

## Any newsgroup
checkgroups:*:*:mail

## --------------------------------------------------------------------------
##	IHAVE/SENDME MESSAGES
## --------------------------------------------------------------------------

ihave:*:*:drop
sendme:*:*:drop

## --------------------------------------------------------------------------
##	SENDSYS
## --------------------------------------------------------------------------

sendsys:*:*:log=sendsys

## --------------------------------------------------------------------------
##	SENDUUNAME
## --------------------------------------------------------------------------

senduuname:*:*:log=senduuname

## --------------------------------------------------------------------------
##	VERSION
## --------------------------------------------------------------------------

version:*:*:log=version

## --------------------------------------------------------------------------
##	NEWGROUP/RMGROUP MESSAGES
## --------------------------------------------------------------------------

## Default (for any group)
newgroup:*:*:mail
rmgroup:*:*:mail

## The Big 8.
## COMP, HUMANITIES, MISC, NEWS, REC, SCI, SOC, TALK

# If it *doesn't* come from group-admin@isc.org, forget it.
#newgroup:*:comp.*|humanities.*|misc.*|news.*|rec.*|sci.*|soc.*|talk.*:log
#rmgroup:*:comp.*|humanities.*|misc.*|news.*|rec.*|sci.*|soc.*|talk.*:log

# This will weed out forgeries for non-Big 8 hierarchies.
newgroup:group-admin@isc.org:*:drop
newgroup:tale@*uu.net:*:drop
rmgroup:group-admin@isc.org:*:drop
rmgroup:tale@*uu.net:*:drop

# *PGP*   See comment at top of file.
checkgroups:*:comp.*|humanities.*|misc.*|news.*|rec.*|sci.*|soc.*|talk.*:drop
newgroup:*:comp.*|humanities.*|misc.*|news.*|rec.*|sci.*|soc.*|talk.*:drop
rmgroup:*:comp.*|humanities.*|misc.*|news.*|rec.*|sci.*|soc.*|talk.*:drop
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
checkgroups:*:alabama.*|hsv.*:drop
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
##
## Accept all newgroup's as well as rmgroup's from trusted sources and
## process them silently.  Only the rmgroup messages from unknown sources
## will be e-mailed to the administrator. Please note that the people 
## listed here are trusted in my opinion only, you opinion my differ.
##
## Other options and comments on alt.* groups can be found on Bill 
## Hazelrig's WWW pages at http://www.tezcat.com/~haz1/alt/faqindex.html
##
newgroup:*:alt.*:doit
# Forgeries
newgroup:group-admin@isc.org:alt.*:drop
newgroup:tale@*uu.net:alt.*:drop
rmgroup:*:alt.*:drop
#rmgroup:haz1@*nwu.edu:alt.*:doit
#rmgroup:grobe@*netins.net:alt.*:doit
#rmgroup:smj@*.oro.net:alt.*:doit
#rmgroup:news@gymnet.com:alt.*:doit
#rmgroup:sjkiii@crl.com:alt.*:doit
#rmgroup:zot@ampersand.com:alt.*:doit
#rmgroup:david@home.net.nz:alt.*:doit
#rmgroup:*@*:alt.config:drop

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
checkgroups:*:aus.*:drop
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
checkgroups:*:ba.*:drop
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
checkgroups:*:baynet.*:drop
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
# Contact: usenet@innet.be
# URL: ftp://ftp.innet.be/pub/staff/stef/
# *PGP*   See comment at top of file.
checkgroups:*:be.*:drop
newgroup:*:be.*:drop
rmgroup:*:be.*:drop
checkgroups:news@*innet.be:be.*:verify-be.announce.newgroups
newgroup:news@*innet.be:be.*:verify-be.announce.newgroups
rmgroup:news@*innet.be:be.*:verify-be.announce.newgroups

# newgroup:news@innet.be:be.*:doit
# rmgroup:news@innet.be:be.*:doit

## BERMUDA
newgroup:news@*ibl.bm:bermuda.*:doit
rmgroup:news@*ibl.bm:bermuda.*:doit

## BEST ( Best Internet Communications, Inc. )
# Contact: news@best.net
# For local use only, contact the above address for information.
newgroup:*@*:best.*:mail
rmgroup:*@*:best.*:doit

## BIONET (Biology Network)
checkgroups:kristoff@*.bio.net:bionet.*:mail
checkgroups:news@*.bio.net:bionet.*:mail
newgroup:dmack@*.bio.net:bionet.*:doit
newgroup:kristoff@*.bio.net:bionet.*:doit
newgroup:shibumi@*.bio.net:bionet.*:doit
rmgroup:dmack@*.bio.net:bionet.*:doit
rmgroup:kristoff@*.bio.net:bionet.*:doit
rmgroup:shibumi@*.bio.net:bionet.*:doit

## BIT (Gatewayed Mailing lists)
# *PGP*   See comment at top of file.
checkgroups:*:bit.*:drop
newgroup:*:bit.*:drop
rmgroup:*:bit.*:drop
checkgroups:jim@american.edu:bit.*:verify-bit.admin
newgroup:jim@american.edu:bit.*:verify-bit.admin
rmgroup:jim@american.edu:bit.*:verify-bit.admin

# newgroup:jim@*american.edu:bit.*:doit
# rmgroup:jim@*american.edu:bit.*:doit

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
checkgroups:*:ch.*:drop
newgroup:*:ch.*:drop
rmgroup:*:ch.*:drop
checkgroups:felix.rauch@nice.ch:ch.*:verify-ch-news-admin@use-net.ch
newgroup:felix.rauch@nice.ch:ch.*:verify-ch-news-admin@use-net.ch
rmgroup:felix.rauch@nice.ch:ch.*:verify-ch-news-admin@use-net.ch

#checkgroups:felix.rauch@nice.ch:ch.*:doit
#newgroup:felix.rauch@nice.ch:ch.*:doit
#rmgroup:felix.rauch@nice.ch:ch.*:doit


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
checkgroups:*:cityscp.*:drop
newgroup:*:cityscp.*:drop
rmgroup:*:cityscp.*:drop
checkgroups:newsmaster@demon.net:cityscp.*:verify-demon.news
newgroup:newsmaster@demon.net:cityscp.*:verify-demon.news
rmgroup:newsmaster@demon.net:cityscp.*:verify-demon.news

## CL (CL-Netz, German)
# *PGP*   See comment at top of file.
# Contact: CL-KOORDINATION@LINK-GOE.de (CL-Koordination, Link-Goe)
# URL: http://www.zerberus.de/org/cl/index.html
# Syncable server: net2.dinoex.sub.de
# Key fingerprint: 21 ED D6 CB 05 56 6E E8  F6 F1 11 E9 2F 6C D5 BB
checkgroups:*:cl.*:drop
newgroup:*:cl.*:drop
rmgroup:*:cl.*:drop
checkgroups:cl-koordination@dinoex.sub.org:cl.*:verify-cl.koordination.einstellungen
newgroup:cl-koordination@dinoex.sub.org:cl.*:verify-cl.koordination.einstellungen
rmgroup:cl-koordination@dinoex.sub.org:cl.*:verify-cl.koordination.einstellungen

# newgroup:root@cl.sub.de:cl.*:doit
# newgroup:root@cl-koordination@dinoex.sub.org:cl.*:doit
# rmgroup:root@cl.sub.de:cl.*:doit
# rmgroup:root@cl-koordination@dinoex.sub.org:cl.*:doit

## CLARINET ( Features and News, Available on a commercial basis)
# *PGP*   See comment at top of file.
checkgroups:*:clari.*:drop
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
checkgroups:*:de.*:drop
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
checkgroups:*:dk.*:drop
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
checkgroups:*:easynet.*:drop
newgroup:*:easynet.*:drop
rmgroup:*:easynet.*:drop
checkgroups:newsmaster@easynet.net:easynet.*:verify-easynet.news
newgroup:newsmaster@easynet.net:easynet.*:verify-easynet.news
rmgroup:newsmaster@easynet.net:easynet.*:verify-easynet.news

## EFN & EUG (Eugene Free Computer Network, Eugene/Springfield, Oregon, USA)
# *PGP*   See comment at top of file.
checkgroups:*:eug.*:drop
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

## ES (Spain)
# Contact: Daniel.Diaz@rediris.es
# See: http://www.rediris.es/netnews/infonews/config.es.html
# See: http://news.rediris.es/~moderador/grupos/newsgroups.es
# *PGP*   See comment at top of file.
# Key fingerprint = 3B 63 18 6F 83 EA 89 82 95 1B 7F 8D B6 ED DD 87
checkgroups:*:es.*:drop
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
checkgroups:*:esp.*:drop
newgroup:*:esp.*:drop
rmgroup:*:esp.*:drop
checkgroups:mod-ena@ennui.org:esp.*:verify-esp.news.administracion
newgroup:mod-ena@ennui.org:esp.*:verify-esp.news.administracion
rmgroup:mod-ena@ennui.org:esp.*:verify-esp.news.administracion

## EUNET ( Europe )
newgroup:news@noc.eu.net:eunet.*:doit
rmgroup:news@noc.eu.net:eunet.*:doit

## EXAMPLE ( Bogus hierarchy reserved for standards documents )
checkgroups:*@*:example.*:mail
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
checkgroups:*:fido.belg.*:drop
newgroup:*:fido.belg.*:drop
rmgroup:*:fido.belg.*:drop
checkgroups:fidobelg@mail.z2.fidonet.org:fido.belg.*:verify-fido.belg.news
newgroup:fidobelg@mail.z2.fidonet.org:fido.belg.*:verify-fido.belg.news
rmgroup:fidobelg@mail.z2.fidonet.org:fido.belg.*:verify-fido.belg.news

## FIDO7
# *PGP*   See comment at top of file.
checkgroups:*:fido7.*:drop
newgroup:*:fido7.*:drop
rmgroup:*:fido7.*:drop
checkgroups:newgroups-request@fido7.ru:fido7.*:verify-fido7.announce.newgroups
newgroup:newgroups-request@fido7.ru:fido7.*:verify-fido7.announce.newgroups
rmgroup:newgroups-request@fido7.ru:fido7.*:verify-fido7.announce.newgroups

# newgroup:news@wing.matsim.udmurtia.su:fido7.*:doit
# rmgroup:news@wing.matsim.udmurtia.su:fido7.*:doit

## FINET (Finland and Finnish language alternative newsgroups)
newgroup:*@*.hut.fi:finet.*:doit
rmgroup:*@*.hut.fi:finet.*:doit

## FJ (Japan and Japanese language)
# Contact: committee@fj-news.org
# URL: http://www.is.tsukuba.ac.jp/~yas/fj/
# Key URL: http://www.is.tsukuba.ac.jp/~yas/fj/fj.asc
checkgroups:*:fj.*:drop
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
checkgroups:*:flora.*:drop
newgroup:*:flora.*:drop
rmgroup:*:flora.*:drop
checkgroups:news@flora.ottawa.on.ca:flora.*:verify-flora-news
newgroup:news@news.rediris.es:flora.*:verify-flora-news
rmgroup:news@news.rediris.es:flora.*:verify-flora-news

## FR (French Language)
# *PGP*   See comment at top of file.
checkgroups:*:fr.*:drop
newgroup:*:fr.*:drop
rmgroup:*:fr.*:drop
checkgroups:control@usenet-fr.news.eu.org:fr.*:verify-control@usenet-fr.news.eu.org
newgroup:control@usenet-fr.news.eu.org:fr.*:verify-control@usenet-fr.news.eu.org
rmgroup:control@usenet-fr.news.eu.org:fr.*:verify-control@usenet-fr.news.eu.org

# newgroup:control@usenet.fr.net:fr.*:doit
# rmgroup:control@usenet.fr.net:fr.*:doit

## FREE (Open Hierarchy where anyone can create a group)
newgroup:*:free.*:doit
# Forgeries.
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
checkgroups:*:gov.*:drop
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

## HAN (Korean Hangul)
# Contact: newgroups-request@usenet.or.kr
# PGPKEY URL: ftp://ftp.usenet.or.kr/pub/korea/usenet/pgp/PGPKEY.han
# *PGP*   See comment at top of file.
checkgroups:*:han.*:drop
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

## HK (Hong Kong)
newgroup:hknews@comp.hkbu.edu.hk:hk.*:doit
rmgroup:hknews@comp.hkbu.edu.hk:hk.*:doit

## HOUSTON (Houston, Texas, USA)
# *PGP*   See comment at top of file.
checkgroups:*:houston.*:drop
newgroup:*:houston.*:drop
rmgroup:*:houston.*:drop
checkgroups:news@academ.com:houston.*:verify-houston.usenet.config
newgroup:news@academ.com:houston.*:verify-houston.usenet.config
rmgroup:news@academ.com:houston.*:verify-houston.usenet.config

# newgroup:news@academ.com:houston.*:doit
# rmgroup:news@academ.com:houston.*:doit

## HUN (Hungary)
checkgroups:*:hun.*:drop
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
newgroup:usenet@ireland.eu.net:ie.*:doit
rmgroup:usenet@ireland.eu.net:ie.*:doit

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
checkgroups:*:is.*:drop
newgroup:*:is.*:drop
rmgroup:*:is.*:drop
newgroup:news@isnet.is:is.*:verify-is.isnet
rmgroup:news@isnet.is:is.*:verify-is.isnet

## ISC ( Japanese ?)
newgroup:news@sally.isc.chubu.ac.jp:isc.*:doit
rmgroup:news@sally.isc.chubu.ac.jp:isc.*:doit

## ISRAEL and IL newsgroups (Israel)
newgroup:news@news.biu.ac.il:israel.*:doit
rmgroup:news@news.biu.ac.il:israel.*|il.*:doit

## IT (Italian)
# *PGP*   See comment at top of file.
checkgroups:*:it.*:drop
newgroup:*:it.*:drop
rmgroup:*:it.*:drop
checkgroups:stefano@unipi.it:it.*:verify-it.announce.newgroups
newgroup:stefano@unipi.it:it.*:verify-it.announce.newgroups
rmgroup:stefano@unipi.it:it.*:verify-it.announce.newgroups

# newgroup:news@ghost.sm.dsi.unimi.it:it.*:doit
# newgroup:stefano@*unipi.it:it.*:doit
# rmgroup:news@ghost.sm.dsi.unimi.it:it.*:doit
# rmgroup:stefano@*unipi.it:it.*:doit

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
checkgroups:*:japan.*:drop
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
checkgroups:*:ka.*:drop
newgroup:*:ka.*:drop
rmgroup:*:ka.*:drop
checkgroups:usenet@karlsruhe.org:ka.*:verify-usenet@karlsruhe.org
newgroup:usenet@karlsruhe.org:ka.*:verify-usenet@karlsruhe.org
rmgroup:usenet@karlsruhe.org:ka.*:verify-usenet@karlsruhe.org


## KANTO
# *PGP*   See comment at top of file.
checkgroups:*:kanto.*:drop
rmgroup:*:kanto.*:drop
checkgroups:ty@kamoi.imasy.or.jp:kanto.*:verify-kanto.news.network
# NOTE: newgroups aren't verified...
newgroup:*@*.jp:kanto.*:doit
rmgroup:ty@kamoi.imasy.or.jp:kanto.*:verify-kanto.news.network

## KASSEL (Kassel, Germany)
# *PGP*   See comment at top of file.
# MAIL: pgp-public-keys@keys.de.pgp.net Subject: GET 0xC4D30EE5
checkgroups:*:kassel.*:drop
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

## LINUX (Newsfeed from news.lameter.com)
checkgroups:christoph@lameter.com:linux.*:doit
newgroup:christoph@lameter.com:linux.*:doit
rmgroup:christoph@lameter.com:linux.*:doit

## LOCAL (Local-only groups)
# It is not really a good idea for sites to use these since they
# may occur on many unconnect sites
newgroup:*@*:local.*:mail
rmgroup:*@*:local.*:drop

## MALTA ( Nation of Malta )
# Contact: cmeli@cis.um.edu.mt
# URL: http://www.cis.um.edu.mt/news-malta/malta-news-new-site-faq.html
# *PGP*   See comment at top of file. 
checkgroups:*:malta.*:drop
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
checkgroups:*:maus.*:drop
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
checkgroups:*:mensa.*:drop
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

## MOD (Original top level moderated hierarchy)
# Removed in the "Great Renaming" of 1988.
# Possible revival attempt in mid-97, so watch this space..
newgroup:*@*:mod.*:mail
rmgroup:*@*:mod.*:doit

## MUC (Munchen (Munich), Germany)
# *PGP*   See comment at top of file.
# Key fingerprint = 43 C7 0E 7C 45 C7 06 E0  BD 6F 76 CE 07 39 5E 66
checkgroups:*:muc.*:drop
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
checkgroups:*:net.*:drop
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
checkgroups:*:netscape.*:drop
newgroup:*:netscape.*:drop
rmgroup:*:netscape.*:drop
checkgroups:news@netscape.com:netscape.*:verify-netscape.public.admin
newgroup:news@netscape.com:netscape.*:verify-netscape.public.admin
rmgroup:news@netscape.com:netscape.*:verify-netscape.public.admin

# checkgroups:news@netscape.com:netscape.*:mail
# newgroup:news@netscape.com:netscape.*:doit
# rmgroups:news@netscape.com:netscape.*:doit

## NETINS ( netINS, Inc )
# Contact: Kevin Houle <kevin@netins.net>
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

## NL (Netherlands)
# Contact: nl-admin@nic.surfnet.nl
# URL: http://www.xs4all.nl/~egavic/NL/ (Dutch)
# URL: http://www.kinkhorst.com/usenet/nladmin.en.html (English)
# *PGP*   See comment at top of file.
# Key fingerprint: 45 20 0B D5 A1 21 EA 7C  EF B2 95 6C 25 75 4D 27
checkgroups:*:nl.*:drop
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
checkgroups:*:nl-alt.*:drop
rmgroup:*:nl-alt.*:drop
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
checkgroups:*:no.*:drop
newgroup:*:no.*:drop
rmgroup:*:no.*:drop
checkgroups:control@usenet.no:no.*:verify-no-hir-control
newgroup:control@usenet.no:no.*:verify-no-hir-control
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
checkgroups:*:nz.*:drop
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
checkgroups:*:pgh.*:drop
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
checkgroups:*:pl.*:drop
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
checkgroups:*:pt.*:drop
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
checkgroups:*:relcom.*:drop
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
newgroup:news@alien.saar.de:saar.*:doit
rmgroup:news@alien.saar.de:saar.*:doit

## SACHSNET (German)
newgroup:root@lusatia.de:sachsnet.*:doit
rmgroup:root@lusatia.de:sachsnet.*:doit

## SAT (San Antonio, Texas, USA)
# *PGP*   See comment at top of file.
# Contact: satgroup@endicor.com
# URL: http://www.endicor.com/~satgroup/
checkgroups:*:sat.*:drop
newgroup:*:sat.*:drop
rmgroup:*:sat.*:drop
checkgroups:satgroup@endicor.com:sat.*:verify-satgroup@endicor.com
newgroup:satgroup@endicor.com:sat.*:verify-satgroup@endicor.com
rmgroup:satgroup@endicor.com:sat.*:verify-satgroup@endicor.com

#checkgroups:satgroup@endicor.com:sat.*:doit
#newgroup:satgroup@endicor.com:sat.*:doit
#rmgroup:satgroup@endicor.com:sat.*:doit

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
checkgroups:*:schule.*:drop
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
checkgroups:*:se.*:drop
newgroup:*:se.*:drop
rmgroup:*:se.*:drop
checkgroups:usenet@usenet-se.net:se.*:verify-usenet-se
newgroup:usenet@usenet-se.net:se.*:verify-usenet-se
rmgroup:usenet@usenet-se.net:se.*:verify-usenet-se

#newgroup:usenet@usenet-se.net:se.*:doit
#rmgroup:usenet@usenet-se.net:se.*:doit
#checkgroups:usenet@usenet-se.net:se.*:doit

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
checkgroups:*:si.*:drop
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

## TEST (Local test hierarchy)
# It is not really a good idea for sites to use these since they
# may occur on many unconnect sites.
newgroup:*@*:test.*:mail
rmgroup:*@*:test.*:mail

## THUR ( Thuringia, Germany )
# *PGP*   See comment at top of file.
# Key Fingerprint: 7E 3D 73 13 93 D4 CA 78  39 DE 3C E7 37 EE 22 F1
checkgroups:*:thur.*:drop
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
checkgroups:*:ucb.*:drop
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
checkgroups:*:ufra.*:drop
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
checkgroups:*:uk.*:drop
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
checkgroups:*:un.*:drop
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
# Contact: control@A470.demon.co.uk
# *PGP*   See comment at top of file.
checkgroups:*:wales.*:drop
newgroup:*:wales.*:drop
rmgroup:*:wales.*:drop
checkgroups:control@*470.demon.co.uk:wales.*:verify-wales.config
newgroup:control@*470.demon.co.uk:wales.*:verify-wales.config
rmgroup:control@*470.demon.co.uk:wales.*:verify-wales.config

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
checkgroups:*:z-netz.*:drop
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
