##  Maintained by Simon Lyall <simon@darkmere.gen.nz>
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
##                          doit=xxx    Do action; log to xxx (see below)
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
## looser control message forgers. If you can't use pgp, then you'll have
## to fix some sections here. Search for *PGP*. At each "*PGP*" found
## you'll need to comment out the block of lines right after it (that have
## 'verify-' in their 4th field). Then uncomment the block of lines that
## comes right after that. You'll also need to change pgpverify in inn.conf.
##
## For more information on using PGP to varify control messages, upgrade
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
## The following hierachies are listed in this file as "local only",  
## "defunct" or "private" . If you carry these hierarchies you should 
## change the entry listed.
## 
## ait, alive, backbone, best, bofh, cais, cd-online, central, duke, fa, 
## fur, ibmnet, iconz, local, manawatu, mcom, metropolis, mod, ncf,
## net, netins, netaxs, planet, prima, psu, pubnet, rpi, 
## slac ,su , t-netz, tele, utwente, uva, worldonline, zer
##
## --------------------------------------------------------------------------

##	DEFAULT
all:*:*:mail

## --------------------------------------------------------------------------
##	CHECKGROUPS MESSAGES
## --------------------------------------------------------------------------

## Any newsgroup
checkgroups:*:*:log=miscctl

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
newgroup:*:*:log=newgroup
rmgroup:*:*:log=rmgroup

## The Big 8.
## COMP, HUMANITIES, MISC, NEWS, REC, SCI, SOC, TALK
#
# If it *doesn't* come from group-admin@isc.org, forget it
newgroup:*:comp.*|humanities.*|misc.*|news.*|rec.*|sci.*|soc.*|talk.*:log
rmgroup:*:comp.*|humanities.*|misc.*|news.*|rec.*|sci.*|soc.*|talk.*:log

# *PGP*   See comment at top of file.
checkgroups:group-admin@isc.org:*:verify-news.announce.newgroups=miscctl
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
# newgroup:group-admin@isc.org:comp.*|misc.*|news.*|rec.*|sci.*:doit=mail
# newgroup:group-admin@isc.org:soc.*|talk.*|humanities.*:doit=mail
# rmgroup:group-admin@isc.org:comp.*|misc.*|news.*|rec.*|sci.*:mail
# rmgroup:group-admin@isc.org:soc.*|talk.*|humanities.*:mail

## AIR ( Internal Stanford University, USA ) 
# Contact: news@news.stanford.edu
# For local use only, contact the above address for information.
newgroup:*@*:air.*:drop
rmgroup:*@*:air.*:doit=rmgroup

## AKR ( Akron, Ohio, USA) 
newgroup:red@redpoll.mrfs.oh.us:akr.*:doit=newgroup
rmgroup:red@redpoll.mrfs.oh.us:akr.*:doit=rmgroup

## ALABAMA (Alabama, USA)
# Contact: news@news.msfc.nasa.gov
# *PGP*   See comment at top of file.
newgroup:news@news.msfc.nasa.gov:alabama.*:verify-alabama-group-admin=newgroup
rmgroup:news@news.msfc.nasa.gov:alabama.*:verify-alabama-group-admin=rmgroup

# newgroup:news@news.msfc.nasa.gov:alabama.*:doit=newgroup
# rmgroup:news@news.msfc.nasa.gov:alabama.*:doit=rmgroup

## ALIVE
# Contact: thijs@kink.xs4all.nl
# No longer used.
newgroup:*@*:alive.*:drop
rmgroup:*@*:alive.*:doit=rmgroup


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
newgroup:*:alt.*:doit=newgroup
rmgroup:*:alt.*:mail
rmgroup:haz1@*nwu.edu:alt.*:doit=rmgroup
rmgroup:grobe@*netins.net:alt.*:doit=rmgroup
rmgroup:smj@*.oro.net:alt.*:doit=rmgroup
rmgroup:news@gymnet.com:alt.*:doit=rmgroup
rmgroup:sjkiii@crl.com:alt.*:doit=rmgroup
rmgroup:zot@ampersand.com:alt.*:doit=rmgroup
rmgroup:david@home.net.nz:alt.*:doit=rmgroup
rmgroup:*@*:alt.config:drop

## AR (Argentina)
newgroup:jorge_f@nodens.fisica.unlp.edu.ar:ar.*:doit=newgroup
rmgroup:jorge_f@nodens.fisica.unlp.edu.ar:ar.*:doit=rmgroup

## ARKANE (Arkane Systems, UK )
# Contact: newsbastard@arkane.demon.co.uk
# URL: http://www.arkane.demon.co.uk/Newsgroups.html
checkgroups:newsbastard@arkane.demon.co.uk:arkane.*:mail
newgroup:newsbastard@arkane.demon.co.uk:arkane.*:doit=newgroup
rmgroup:newsbastard@arkane.demon.co.uk:arkane.*:doit=rmgroup

## AT (Austrian)
checkgroups:control@usenet.backbone.at:at.*:mail
newgroup:control@usenet.backbone.at:at.*:doit=newgroup
rmgroup:control@usenet.backbone.at:at.*:doit=rmgroup

## AUS (Australia)
# NOTE: This hirarchy is in the process of setting up new alaises
#       and rules for evrything
# URL: http://aus.news-admin.org/ 
checkgroups:news@aus.news-admin.org:aus.*:mail
newgroup:news@aus.news-admin.org:aus.*:doit=newgroup
rmgroup:news@aus.news-admin.org:aus.*:doit=rmgroup

## AUSTIN (Texas) 
newgroup:pug@arlut.utexas.edu:austin.*:doit=newgroup
rmgroup:pug@arlut.utexas.edu:austin.*:doit=rmgroup

## AZ (Arizona)
newgroup:system@asuvax.eas.asu.edu:az.*:doit=newgroup
rmgroup:system@asuvax.eas.asu.edu:az.*:doit=rmgroup

## BA (San Francisco Bay Area, USA)
newgroup:ba-mod@nas.nasa.gov:ba.*:verify-ba.news.config
rmgroup:ba-mod@nas.nasa.gov:ba.*:verify-ba.news.config
checkgroups:ba-mod@nas.nasa.gov:ba.*:verify-ba.news.config

## BACKBONE (ruhr.de/ruhrgebiet.individual.net in Germany)
# Contact: admin@ruhr.de
# For internal use only, contact above address for questions
newgroup:*@*:backbone.*:drop
rmgroup:*@*:backbone.*:doit=rmgroup

## BAYNET (Bayerische Buergernetze, Deutschland)
# Contact: news@mayn.de
# URL: http://www.mayn.de/users/news/
# Key fingerprint = F7 AD 96 D8 7A 3F 7E 84  02 0C 83 9A DB 8F EB B8
# Syncable server: news.mayn.de (contact news@mayn.de if permission denied)
# *PGP*   See comment at top of file.
newgroup:news@mayn.de:baynet.*:verify-news.mayn.de=newgroup
rmgroup:news@mayn.de:baynet.*:verify-news.mayn.de=rmgroup
checkgroups:news@mayn.de:baynet.*:verify-news.mayn.de=miscctl

# newgroup:news@mayn.de:baynet.*:mail
# rmgroup:news@mayn.de:baynet.*:mail
# checkgroups:news@mayn.de:baynet.*:mail


## BE  (Belgique/Belgïe/Belgien/Belgium )
# Contact: usenet@innet.be
# URL: ftp://ftp.innet.be/pub/staff/stef/
# *PGP*   See comment at top of file.
checkgroups:news@*innet.be:be.*:verify-be.announce.newgroups
newgroup:news@*innet.be:be.*:verify-be.announce.newgroups
rmgroup:news@*innet.be:be.*:verify-be.announce.newgroups

# newgroup:news@innet.be:be.*:doit=newgroup
# rmgroup:news@innet.be:be.*:doit=rmgroup


## BERMUDA
newgroup:news@*ibl.bm:bermuda.*:doit=newgroup
rmgroup:news@*ibl.bm:bermuda.*:doit=rmgroup

## BEST ( Best Internet Communications, Inc. )
# Contact: news@best.net
# For local use only, contact the above address for information.
newgroup:*@*:best.*:drop
rmgroup:*@*:best.*:doit=rmgroup

## BIONET (Biology Network)
checkgroups:kristoff@*.bio.net:bionet.*:mail
checkgroups:news@*.bio.net:bionet.*:mail
newgroup:dmack@*.bio.net:bionet.*:doit=newgroup
newgroup:kristoff@*.bio.net:bionet.*:doit=newgroup
newgroup:shibumi@*.bio.net:bionet.*:doit=mail
rmgroup:dmack@*.bio.net:bionet.*:doit=rmgroup
rmgroup:kristoff@*.bio.net:bionet.*:doit=rmgroup
rmgroup:shibumi@*.bio.net:bionet.*:doit=mail

## BIT (Gatewayed Mailing lists)
# *PGP*   See comment at top of file.
checkgroups:jim@american.edu:bit.*:verify-bit.admin=miscctl
newgroup:jim@american.edu:bit.*:verify-bit.admin
rmgroup:jim@american.edu:bit.*:verify-bit.admin

# newgroup:jim@*american.edu:bit.*:doit=newgroup
# rmgroup:jim@*american.edu:bit.*:doit=rmgroup

## BIZ (Business Groups)
newgroup:edhew@xenitec.on.ca:biz.*:doit=newgroup
rmgroup:edhew@xenitec.on.ca:biz.*:doit=rmgroup

## BLGTN ( Bloomington, In, USA)
newgroup:control@news.bloomington.in.us:blgtn.*:doit=newgroup
rmgroup:control@news.bloomington.in.us:blgtn.*:doit=rmgroup

## BLN (Berlin, Germany)
checkgroups:news@*fu-berlin.de:bln.*:mail
newgroup:news@*fu-berlin.de:bln.*:doit=newgroup
rmgroup:news@*fu-berlin.de:bln.*:doit=rmgroup

## BOFH ( Bastard Operator From Hell )
# Contact: myname@myhost.mydomain.com
# For private use only, contact the above address for information.
newgroup:*@*:bofh.*:drop
rmgroup:*@*:bofh.*:doit=rmgroup

## CA (California, USA)
# URL: http://www.sbay.org/ca/
# Contact: ikluft@thunder.sbay.org
newgroup:ikluft@thunder.sbay.org:ca.*:doit=newgroup
rmgroup:ikluft@thunder.sbay.org:ca.*:doit=rmgroup

## CAIS ( )
# Contact: news@cais.com
# For local use only, contact the above address for information.
newgroup:*@*:cais.*:drop
rmgroup:*@*:cais.*:doit=rmgroup

## CAPDIST (Albany, The Capital District, New York, USA)
newgroup:danorton@albany.net:capdist.*:doit=newgroup
rmgroup:danorton@albany.net:capdist.*:doit=rmgroup

## CARLETON (Canadian -- Carleton University)
newgroup:news@cunews.carleton.ca:carleton.*:doit=newgroup
newgroup:news@cunews.carleton.ca:carleton*class.*:log
rmgroup:news@cunews.carleton.ca:carleton.*:doit=rmgroup

## CD-ONLINE
# Contact: newsmaster@worldonline.nl
# For local use only, contact the above address for information.
newgroup:*@*:cd-online.*:drop
rmgroup:*@*:cd-online.*:doit=rmgroup

## CENTRAL (The Internet Company of New Zealand, Wellington, NZ )
# Contact: usenet@iconz.co.nz
# For local use only, contact the above address for information.
newgroup:*@*:central.*:drop
rmgroup:*@*:central.*:doit=rmgroup

## CH ( Switzerland )
# Contact: ch-news-admin@use-net.ch
# URL: http://www.use-net.ch/Usenet/
# Key URL: http://www.use-net.ch/Usenet/adminkey.html
# *PGP*   See comment at top of file.
# Key fingerprint = 71 80 D6 8C A7 DE 2C 70  62 4A 48 6E D9 96 02 DF
checkgroups:felix.rauch@nice.ch:ch.*:verify-ch-news-admin@use-net.ch=miscctl
newgroup:felix.rauch@nice.ch:ch.*:verify-ch-news-admin@use-net.ch
rmgroup:felix.rauch@nice.ch:ch.*:verify-ch-news-admin@use-net.ch

#checkgroups:felix.rauch@nice.ch:ch.*:doit=mail
#newgroup:felix.rauch@nice.ch:ch.*:doit=newgroup
#rmgroup:felix.rauch@nice.ch:ch.*:doit=mail



## CHRISTNET newsgroups
checkgroups:news@fdma.com:christnet.*:mail
newgroup:news@fdma.com:christnet.*:doit=newgroup
rmgroup:news@fdma.com:christnet.*:doit=rmgroup

## CHI (Chicago, USA)
newgroup:lisbon@*interaccess.com:chi.*:doit=newgroup
newgroup:lisbon@*chi.il.us:chi.*:doit=newgroup
rmgroup:lisbon@*interaccess.com:chi.*:doit=rmgroup
rmgroup:lisbon@*chi.il.us:chi.*:doit=rmgroup

## CHILE (Chile and Chilean affairs) 
# Contact: mod-cga@webhost.cl
# URL: http://www.webhost.cl/~mod-cga
checkgroups:mod-cga@*webhost.cl:chile.*:mail
newgroup:mod-cga@*webhost.cl:chile.*:doit=newgroup
rmgroup:mod-cga@*webhost.cl:chile.*:doit=rmgroup

## CHINESE (China and Chinese language groups)
newgroup:pinghua@stat.berkeley.edu:chinese.*:doit=newgroup
rmgroup:pinghua@stat.berkeley.edu:chinese.*:doit=rmgroup

## CL (CL-Netz, German)
# *PGP*   See comment at top of file.
# Contact: CL-KOORDINATION@LINK-GOE.de (CL-Koordination, Link-Goe)
# URL: http://www.zerberus.de/org/cl/index.html
# Syncable server: net2.dinoex.sub.de
# Key fingerprint: 21 ED D6 CB 05 56 6E E8  F6 F1 11 E9 2F 6C D5 BB
checkgroups:cl-koordination@dinoex.sub.org:cl.*:verify-cl.koordination.einstellungen
newgroup:cl-koordination@dinoex.sub.org:cl.*:verify-cl.koordination.einstellungen
rmgroup:cl-koordination@dinoex.sub.org:cl.*:verify-cl.koordination.einstellungen

# newgroup:root@cl.sub.de:cl.*:doit=newgroup
# newgroup:root@cl-koordination@dinoex.sub.org:cl.*:doit=newgroup
# rmgroup:root@cl.sub.de:cl.*:doit=rmgroup
# rmgroup:root@cl-koordination@dinoex.sub.org:cl.*:doit=rmgroup


## CLARINET ( Features and News, Available on a commercial basis)
# *PGP*   See comment at top of file.
checkgroups:cl*@clarinet.com:clari.*:verify-ClariNet.Group
newgroup:cl*@clarinet.com:clari.*:verify-ClariNet.Group
rmgroup:cl*@clarinet.com:clari.*:verify-ClariNet.Group

# newgroup:brad@clarinet.com:clari.*:doit=newgroup
# newgroup:clarinet@clarinet.com:clari.*:doit=newgroup
# newgroup:clarinet@clarinet.net:clari.*:doit=newgroup
# rmgroup:brad@clarinet.com:clari.*:doit=rmgroup
# rmgroup:clarinet@clarinet.com:clari.*:doit=rmgroup
# rmgroup:clarinet@clarinet.net:clari.*:doit=rmgroup

## CONCORDIA newsgroups (Concordia University, Montreal, Canada)
# URL: General University info at http://www.concordia.ca/
# Contact: newsmaster@concordia.ca
newgroup:news@newsflash.concordia.ca:concordia.*:doit=newgroup
rmgroup:news@newsflash.concordia.ca:concordia.*:doit=rmgroup

## CPCU/IIA (American Institute for Chartered Property Casulty
## Underwriter/Insurance Institute of America, USA )
# Contact: miller@cpcuiia.org
# URL: www.aicpcu.org
checkgroups:miller@cpcuiia.org:cpcuiia.*:mail
newgroup:miller@cpcuiia.org:cpcuiia.*:doit=newgroup
rmgroup:miller@cpcuiia.org:cpcuiia.*:doit=rmgroup

## CZ newsgroups (Czech Republic)
# URL: ftp://ftp.vslib.cz/pub/news/config/cz/newsgroups (text)
# URL: http://www.cesnet.cz/cgi-bin/newsgroups.p?cz (HTML)
checkgroups:petr.kolar@vslib.cz:cz.*:mail
newgroup:petr.kolar@vslib.cz:cz.*:doit=newgroup
rmgroup:petr.kolar@vslib.cz:cz.*:doit=rmgroup

## DC (Washington, D.C. , USA )
checkgroups:news@mattress.atww.org:dc.*:mail
newgroup:news@mattress.atww.org:dc.*:doit=newgroup
rmgroup:news@mattress.atww.org:dc.*:doit=rmgroup

## DE (German language)
# *PGP*   See comment at top of file.
checkgroups:moderator@dana.de:de.*:verify-de.admin.news.announce
newgroup:moderator@dana.de:de.*:verify-de.admin.news.announce
newgroup:*@*:de.alt.*:doit=newgroup
rmgroup:moderator@dana.de:de.*:verify-de.admin.news.announce

# checkgroups:*@*dana.de:de.*:mail
# checkgroups:*@*.dana.de:de.*:mail
# newgroup:*@dana.de|*@*.dana.de:de.*:doit=newgroup
# newgroup:*@*:de.alt.*:doit=newgroup
# rmgroup:*@dana.de|*@*.dana.de:de.*:doit=rmgroup

## DFW (Dallas/Fort Worth, Texas, USA)
newgroup:eric@*cirr.com:dfw.*:doit=newgroup
rmgroup:eric@*cirr.com:dfw.*:doit=rmgroup

## DK (Denmark)
# URL: http://www.DK.net/Usenet/
# Key URL: http://www.DK.net/Usenet/pgp.html
# Key fingerprint = 7C B2 C7 50 F3 7D 5D 73  8C EE 2E 3F 55 80 72 FF
# *PGP*   See comment at top of file.
newgroup:news@news.dknet.dk:dk.*:verify-news@news.dknet.dk
rmgroup:news@news.dknet.dk:dk.*:verify-news@news.dknet.dk

# newgroup:news@news.dknet.dk:dk.*:doit=newgroup
# rmgroup:news@news.dknet.dk:dk.*:doit=rmgroup


## DUKE ( Duke University, USA )
# Contact: news@newsgate.duke.edu
# For local use only, contact the above address for information.
newgroup:*@*:duke.*:drop
rmgroup:*@*:duke.*:doit=rmgroup

## EFN 
##
# *PGP*   See comment at top of file.
checkgroups:newsadmin@efn.org:efn.*:verify-eug.config=miscctl
newgroup:newsadmin@efn.org:efn.*:verify-eug.config
rmgroup:newsadmin@efn.org:efn.*:verify-eug.config

# newgroup:newsadmin@efn.org:efn.*:doit=newgroup
# rmgroup:newsadmin@efn.org:efn.*:doit=rmgroup

## EHIME-U (? University, Japan ?)
newgroup:news@cc.nias.ac.jp:ehime-u.*:doit=newgroup
newgroup:news@doc.dpc.ehime-u.ac.jp:ehime-u.*:doit=newgroup
rmgroup:news@cc.nias.ac.jp:ehime-u.*:doit=rmgroup
rmgroup:news@doc.dpc.ehime-u.ac.jp:ehime-u.*:doit=rmgroup

## ES (Spain)
# Contact: Juan.Garcia@rediris.es
# See: http://www.rediris.es/netnews/infonews/config.es.html
# See: http://news.rediris.es/infonews/docs/news_config/newsgroups.es
# *PGP*   See comment at top of file.
# Key fingerprint = 3B 63 18 6F 83 EA 89 82 95 1B 7F 8D B6 ED DD 87
checkgroups:news@news.rediris.es:es.*:verify-es.news
newgroup:news@news.rediris.es:es.*:verify-es.news
rmgroup:news@news.rediris.es:es.*:verify-es.news

# checkgroups:news@news.rediris.es:es.*:mail
# newgroup:news@news.rediris.es:es.*:doit=newgroup
# rmgroup:news@news.rediris.es:es.*:doit=rmgroup

## ESP (Spanish-language newsgroups)
# YRL: http://www.ennui.org/esp
newgroup:mod-ena@ennui.org:esp.*:verify-esp.news.administracion
rmgroup:mod-ena@ennui.org:esp.*:verify-esp.news.administracion
checkgroups:mod-ena@ennui.org:esp.*:verify-esp.news.administracion

## EUG (Eugene/Springfield, Oregon, USA)
# *PGP*   See comment at top of file.
checkgroups:newsadmin@efn.org:eug.*:verify-eug.config=miscctl
newgroup:newsadmin@efn.org:eug.*:verify-eug.config
rmgroup:newsadmin@efn.org:eug.*:verify-eug.config

# newgroup:newsadmin@efn.org:eug.*:doit=newgroup
# rmgroup:newsadmin@efn.org:eug.*:doit=rmgroup

## EUNET ( Europe )
newgroup:news@noc.eu.net:eunet.*:doit=newgroup
rmgroup:news@noc.eu.net:eunet.*:doit=rmgroup

## EXAMPLE ( Bogus hierarchy reserved for standards documents )
# checkgroups:*@*:example.*:drop
newgroup:*@*:example.*:drop
rmgroup:*@*:example.*:doit=rmgroup

## FA ( "From ARPA" gatewayed mailing lists)
# Removed in the "Great Renaming" of 1988.
newgroup:*@*:fa.*:drop
rmgroup:*@*:fa.*:doit=rmgroup

## FIDO newsgroups (FidoNet)
newgroup:root@mbh.org:fido.*:doit=newgroup
rmgroup:root@mbh.org:fido.*:doit=rmgroup

## FIDO.BELG.* newsgroups (FidoNet)
# URL: http://www.z2.fidonet.org/news/fido.belg.news/
# *PGP*   See comment at top of file.
checkgroups:fidobelg@mail.z2.fidonet.org:fido.belg.*:verify-fido.belg.news
newgroup:fidobelg@mail.z2.fidonet.org:fido.belg.*:verify-fido.belg.news
rmgroup:fidobelg@mail.z2.fidonet.org:fido.belg.*:verify-fido.belg.news

## FIDO7
# *PGP*   See comment at top of file.
checkgroups:newgroups-request@fido7.ru:fido7.*:verify-fido7.announce.newgroups
newgroup:newgroups-request@fido7.ru:fido7.*:verify-fido7.announce.newgroups
rmgroup:newgroups-request@fido7.ru:fido7.*:verify-fido7.announce.newgroups

# newgroup:news@wing.matsim.udmurtia.su:fido7.*:doit=newgroup
# rmgroup:news@wing.matsim.udmurtia.su:fido7.*:doit=rmgroup

## FJ (Japan and Japanese language)
newgroup:fj-committee@etl.go.jp:fj.*:doit=newgroup
newgroup:fj-committee@cow.nara.sharp.co.jp:fj.*:doit=newgroup
rmgroup:fj-committee@etl.go.jp:fj.*:doit=rmgroup
rmgroup:fj-committee@cow.nara.sharp.co.jp:fj.*:doit=rmgroup

## FL (Florida, USA )
newgroup:hgoldste@news1.mpcs.com:fl.*:doit=newgroup
newgroup:scheidell@fdma.fdma.com:fl.*:doit=newgroup
rmgroup:hgoldste@news1.mpcs.com:fl.*:doit=rmgroup
rmgroup:scheidell@fdma.fdma.com:fl.*:doit=rmgroup

## FLORA (FLORA Community WEB, Canada)
# Contact: russell@flora.org
# See: http://news.flora.org/  for newsgroup listings and information
# See: http://www.flora.org/russell/ for PGP keys
# *PGP*   See comment at top of file.
checkgroups:news@flora.ottawa.on.ca:flora.*:verify-flora-news
newgroup:news@news.rediris.es:flora.*:verify-flora-news
rmgroup:news@news.rediris.es:flora.*:verify-flora-news

## FR (French Language)
# *PGP*   See comment at top of file.
checkgroups:control@usenet.fr.net:fr.*:verify-fr.announce.newgroups
newgroup:control@usenet.fr.net:fr.*:verify-fr.announce.newgroups
rmgroup:control@usenet.fr.net:fr.*:verify-fr.announce.newgroups

# newgroup:control@usenet.fr.net:fr.*:doit=newgroup
# rmgroup:control@usenet.fr.net:fr.*:doit=rmgroup


## FREE
newgroup:*:free.*:doit=mail
rmgroup:*:free.*:doit=mail

## FUDAI (Japanese ?)
newgroup:news@picard.cs.osakafu-u.ac.jp:fudai.*:doit=newgroup
rmgroup:news@picard.cs.osakafu-u.ac.jp:fudai.*:doit=rmgroup

## FUR ( furrynet )
# Contact: fur-config@taronga.com
# Please contact the above address before adding these groups.

## GIT (Georgia Institute of Technology, USA )
newgroup:news@news.gatech.edu:git.*:doit=newgroup
newgroup:news@news.gatech.edu:git*class.*:log
rmgroup:news@news.gatech.edu:git.*:doit=rmgroup

## GNU ( Free Software Foundation )
newgroup:gnu@prep.ai.mit.edu:gnu.*:doit=newgroup
newgroup:news@prep.ai.mit.edu:gnu.*:doit=newgroup
newgroup:news@ai.mit.edu:gnu.*:doit=newgroup
rmgroup:gnu@prep.ai.mit.edu:gnu.*:doit=rmgroup
rmgroup:news@prep.ai.mit.edu:gnu.*:doit=rmgroup
rmgroup:news@ai.mit.edu:gnu.*:doit=rmgroup

## GOV (Government Information)
# *PGP*   See comment at top of file.
# URL: http://www.govnews.org/govnews/
# PGPKEY URL: http://www.govnews.org/govnews/site-setup/gov.pgpkeys
checkgroups:gov-usenet-announce-moderator@govnews.org:gov.*:verify-gov.usenet.announce
newgroup:gov-usenet-announce-moderator@govnews.org:gov.*:verify-gov.usenet.announce
rmgroup:gov-usenet-announce-moderator@govnews.org:gov.*:verify-gov.usenet.announce

## HAMILTON (Canadian)
newgroup:news@*dcss.mcmaster.ca:hamilton.*:doit=newgroup
rmgroup:news@*dcss.mcmaster.ca:hamilton.*:doit=rmgroup

## HAN (Korean Hangul)
# Contact: newgroups-request@usenet.or.kr
# PGPKEY URL: ftp://ftp.usenet.or.kr/pub/korea/usenet/pgp/PGPKEY.han
newgroup:newgroups-request@usenet.or.kr:han.*:verify-han.news.admin
rmgroup:newgroups-request@usenet.or.kr:han.*:verify-han.news.admin

## HANNET & HANNOVER (Hannover, Germany) 
newgroup:fifi@hiss.han.de:hannover.*|hannet.*:doit=newgroup
rmgroup:fifi@hiss.han.de:hannover.*|hannet.*:doit=rmgroup

## HAWAII 
newgroup:news@lava.net:hawaii.*:doit=newgroup
rmgroup:news@lava.net:hawaii.*:doit=rmgroup

## HK (Hong Kong)
newgroup:hknews@comp.hkbu.edu.hk:hk.*:doit=newgroup
rmgroup:hknews@comp.hkbu.edu.hk:hk.*:doit=rmgroup

## HOUSTON (Houston, Texas, USA)
# *PGP*   See comment at top of file.
checkgroups:news@academ.com:houston.*:verify-houston.usenet.config=miscctl
newgroup:news@academ.com:houston.*:verify-houston.usenet.config
rmgroup:news@academ.com:houston.*:verify-houston.usenet.config

# newgroup:news@academ.com:houston.*:doit=newgroup
# rmgroup:news@academ.com:houston.*:doit=rmgroup


## HUN (Hungary)
checkgroups:kissg@*sztaki.hu:hun.*:mail
checkgroups:hg@*.elte.hu:hun.org.elte.*:mail
newgroup:kissg@*sztaki.hu:hun.*:doit=newgroup
newgroup:hg@*.elte.hu:hun.org.elte.*:doit=newgroup
rmgroup:kissg@*sztaki.hu:hun.*:doit=rmgroup
rmgroup:hg@*.elte.hu:hun.org.elte.*:doit=rmgroup

## HSV (Huntsville, Alabama, USA)
# *PGP*   See comment at top of file.
# Contact: news@news.msfc.nasa.gov
newgroup:news@news.msfc.nasa.gov:hsv.*:verify-alabama-group-admin=newgroup
rmgroup:news@news.msfc.nasa.gov:hsv.*:verify-alabama-group-admin=rmgroup

# newgroup:news@news.msfc.nasa.gov:hsv.*:doit=newgroup
# rmgroup:news@news.msfc.nasa.gov:hsv.*:doit=rmgroup

## IA (Iowa, USA)
newgroup:skunz@iastate.edu:ia.*:doit=newgroup
rmgroup:skunz@iastate.edu:ia.*:doit=rmgroup

## IBMNET
# Contact: news@ibm.net
# For local use only, contact the above address for information.
newgroup:*@*:ibmnet.*:drop
rmgroup:*@*:ibmnet.*:doit=rmgroup

## ICONZ (The Internet Company of New Zealand, New Zealand)
# Contact: usenet@iconz.co.nz
# For local use only, contact the above address for information.
newgroup:*@*:iconz.*:drop
rmgroup:*@*:iconz.*:doit=rmgroup

## IE (Ireland)
newgroup:usenet@ireland.eu.net:ie.*:doit=newgroup
rmgroup:usenet@ireland.eu.net:ie.*:doit=rmgroup

## IEEE 
newgroup:burt@ieee.org:ieee.*:doit=newgroup
rmgroup:burt@ieee.org:ieee.*:doit=rmgroup

## INFO newsgroups
newgroup:rjoyner@uiuc.edu:info.*:doit=newgroup
rmgroup:rjoyner@uiuc.edu:info.*:doit=rmgroup

## ISC ( Japanese ?)
newgroup:news@sally.isc.chubu.ac.jp:isc.*:doit=newgroup
rmgroup:news@sally.isc.chubu.ac.jp:isc.*:doit=rmgroup

## ISRAEL and IL newsgroups (Israel)
newgroup:news@news.biu.ac.il:israel.*:doit=newgroup
rmgroup:news@news.biu.ac.il:israel.*|il.*:doit=rmgroup

## IT (Italian)
# *PGP*   See comment at top of file.
checkgroups:stefano@unipi.it:it.*:verify-it.announce.newgroups=miscctl
newgroup:stefano@unipi.it:it.*:verify-it.announce.newgroups
rmgroup:stefano@unipi.it:it.*:verify-it.announce.newgroups

# newgroup:news@ghost.sm.dsi.unimi.it:it.*:doit=newgroup
# newgroup:stefano@*unipi.it:it.*:doit=newgroup
# rmgroup:news@ghost.sm.dsi.unimi.it:it.*:doit=rmgroup
# rmgroup:stefano@*unipi.it:it.*:doit=rmgroup

## IU (Indiana University)
newgroup:news@usenet.ucs.indiana.edu:iu.*:doit=newgroup
newgroup:root@usenet.ucs.indiana.edu:iu.*:doit=newgroup
newgroup:*@usenet.ucs.indiana.edu:iu*class.*:log
rmgroup:news@usenet.ucs.indiana.edu:iu.*:doit=rmgroup
rmgroup:root@usenet.ucs.indiana.edu:iu.*:doit=rmgroup

## JAPAN (Japan)
# Contact: Tsuneo Tanaka <tt+null@efnet.com>
# URL: http://www.asahi-net.or.jp/~AE5T-KSN/japan-e.html
# Key fingerprint = 6A FA 19 47 69 1B 10 74  38 53 4B 1B D8 BA 3E 85
# PGP Key: http://grex.cyberspace.org/~tt/japan.admin.announce.asc
# PGP Key: http://www.asahi-net.or.jp/~AE5T-KSN/japan/japan.admin.announce.asc
# *PGP*   See comment at top of file.
checkgroups:japan.admin.announce@news.efnet.com:japan.*:verify-japan.admin.announce@news.efnet.com
newgroup:japan.admin.announce@news.efnet.com:japan.*:verify-japan.admin.announce@news.efnet.com
rmgroup:japan.admin.announce@news.efnet.com:japan.*:verify-japan.admin.announce@news.efnet.com

# checkgroups:news@marie.iijnet.or.jp:japan.*:log
# newgroup:*@*:japan.*:log
# rmgroup:*@*:japan.*:log


## K12 ( US Educational Network )
newgroup:braultr@csmanoirs.qc.ca:k12.*:doit=newgroup
rmgroup:braultr@csmanoirs.qc.ca:k12.*:doit=rmgroup

## KA (Karlsruhe, Germany)
# Contact: usenet@karlsruhe.org
# For private use only, contact the above address for information.
#
# URL: http://www.karlsruhe.org/             (German only)
# URL: http://www.karlsruhe.org/newsgroups   (newsgroup list)
#
# *PGP*   See comment at top of file. 
# Key fingerprint =  DE 19 BB 25 76 19 81 17  F0 67 D2 23 E8 C8 7C 90
checkgroups:usenet@karlsruhe.org:ka.*:verify-usenet@karlsruhe.org
newgroup:usenet@karlsruhe.org:ka.*:verify-usenet@karlsruhe.org
rmgroup:usenet@karlsruhe.org:ka.*:verify-usenet@karlsruhe.org



## KANTO
# *PGP*   See comment at top of file.
checkgroups:ty@kamoi.imasy.or.jp:kanto.*:verify-kanto.news.network=miscctl
# NOTE: newgroups aren't verified...
newgroup:*@*.jp:kanto.*:doit=newgroup
rmgroup:ty@kamoi.imasy.or.jp:kanto.*:verify-kanto.news.network

## KASSEL (Kassel, Germany)
# *PGP*   See comment at top of file.
# MAIL: pgp-public-keys@keys.de.pgp.net Subject: GET 0xC4D30EE5
checkgroups:dirk.meyer@dinoex.sub.org:kassel.*:verify-kassel-admin
newgroup:dirk.meyer@dinoex.sub.org:kassel.*:verify-kassel-admin
rmgroup:dirk.meyer@dinoex.sub.org:kassel.*:verify-kassel-admin

## KIEL 
checkgroups:kris@white.schulung.netuse.de:kiel.*:mail
newgroup:kris@white.schulung.netuse.de:kiel.*:doit=newgroup
rmgroup:kris@white.schulung.netuse.de:kiel.*:doit=rmgroup

## LIU newsgroups (Sweden?)
newgroup:linus@tiny.lysator.liu.se:liu.*:doit=newgroup
rmgroup:linus@tiny.lysator.liu.se:liu.*:doit=rmgroup

## LINUX (gatewayed mailing lists for the Linux OS)
newgroup:hpa@yggdrasil.com:linux.*:doit=newgroup
rmgroup:hpa@yggdrasil.com:linux.*:doit=rmgroup

## LOCAL (Local-only groups)
# It is not really a good idea for sites to use these since they
# may occur on many unconnect sites
newgroup:*@*:local.*:drop
rmgroup:*@*:local.*:drop

## MALTA ( Nation of Malta )
# Contact: cmeli@cis.um.edu.mt
# URL: http://www.cis.um.edu.mt/news-malta/malta-news-new-site-faq.html
# *PGP*   See comment at top of file. 
checkgroups:cmeli@cis.um.edu.mt:malta.*:verify-malta.config=miscctl
newgroup:cmeli@cis.um.edu.mt:malta.*:verify-malta.config
rmgroup:cmeli@cis.um.edu.mt:malta.*:verify-malta.config

# newgroup:cmeli@cis.um.edu.mt:malta.*:doit=newgroup
# rmgroup:cmeli@cis.um.edu.mt:malta.*:doit=rmgroup

## MANAWATU ( Manawatu district, New Zealand)
# Contact: alan@manawatu.gen.nz or news@manawatu.gen.nz
# For local use only, contact the above address for information.
newgroup:*@*:manawatu.*:drop
rmgroup:*@*:manawatu.*:doit=rmgroup

## MAUS ( MausNet, German )
# *PGP*   See comment at top of file. 
# Key fingerprint: 82 52 C7 70 26 B9 72 A1  37 98 55 98 3F 26 62 3E
checkgroups:guenter@gst0hb.north.de:maus.*:verify-maus-info=miscctl
checkgroups:guenter@gst0hb.hb.provi.de:maus.*:verify-maus-info=miscctl
newgroup:guenter@gst0hb.north.de:maus.*:verify-maus-info
newgroup:guenter@gst0hb.hb.provi.de:maus.*:verify-maus-info
rmgroup:guenter@gst0hb.north.de:maus.*:verify-maus-info
rmgroup:guenter@gst0hb.hb.provi.de:maus.*:verify-maus-info

# newgroup:guenter@gst0hb.north.de:maus.*:doit=newgroup
# rmgroup:guenter@gst0hb.north.de:maus.*:doit=rmgroup

## MCOM ( Netscape Inc, USA) 
newgroup:*@*:mcom.*:drop
rmgroup:*@*:mcom.*:doit=rmgroup

## ME (Maine, USA)
newgroup:kerry@maine.maine.edu:me.*:doit=newgroup
rmgroup:kerry@maine.maine.edu:me.*:doit=rmgroup

## MEDLUX ( All-Russia medical teleconferences )
# URL: ftp://ftp.medlux.ru/pub/news/medlux.grp
checkgroups:neil@new*.medlux.ru:medlux.*:mail
newgroup:neil@new*.medlux.ru:medlux.*:doit=newgroup
rmgroup:neil@new*.medlux.ru:medlux.*:doit=rmgroup

## MELB ( Melbourne, Australia)
newgroup:kre@*mu*au:melb.*:doit=newgroup
newgroup:revdoc@*uow.edu.au:melb.*:doit=newgroup
rmgroup:kre@*mu*au:melb.*:doit=rmgroup
rmgroup:revdoc@*uow.edu.au:melb.*:doit=rmgroup

## MENSA (The Mensa Organisation)
# Contact: usenet@newsgate.mensa.org
# Key fingerprint:  A7 57 24 49 C0 D4 47 33  84 A0 52 6E F1 A4 00 5B
# *PGP*   See comment at top of file.
checkgroups:usenet@newsgate.mensa.org:mensa.*:verify-mensa.config=miscctl
newgroup:usenet@newsgate.mensa.org:mensa.*:verify-mensa.config
rmgroup:usenet@newsgate.mensa.org:mensa.*:verify-mensa.config

## METOCEAN (ISP in Japan)
newgroup:fwataru@*.metocean.co.jp:metocean.*:doit=newgroup
rmgroup:fwataru@*.metocean.co.jp:metocean.*:doit=rmgroup

## METROPOLIS
# Contact: newsmaster@worldonline.nl
# For local use only, contact the above address for information.
newgroup:*@*:metropolis.*:drop
rmgroup:*@*:metropolis.*:doit=rmgroup

## MI (Michigan, USA)
# Contact: Steve Simmons <scs@lokkur.dexter.mi.us>
# URL: http://www.inland-sea.com/mi-news.html
# http://www.inland-sea.com/mi-news.html
checkgroups:scs@lokkur.dexter.mi.us:mi.*:mail
newgroup:scs@lokkur.dexter.mi.us:mi.*:doit=newgroup
rmgroup:scs@lokkur.dexter.mi.us:mi.*:doit=rmgroup

## MOD (Original top level moderated hierarchy)
# Removed in the "Great Renaming" of 1988.
# Possible revival attempt in mid-97, so watch this space..
newgroup:*@*:mod.*:drop
rmgroup:*@*:mod.*:doit=rmgroup

## MUC (Munchen, Germany. Gatewayed mailing lists??)
# *PGP*   See comment at top of file.
checkgroups:muc-cmsg@muenchen.pro-bahn.org:muc.*:verify-muc.admin
newgroup:muc-cmsg@muenchen.pro-bahn.org:muc.*:verify-muc.admin
rmgroup:muc-cmsg@muenchen.pro-bahn.org:muc.*:verify-muc.admin

# newgroup:muc-cmsg@muenchen.pro-bahn.org:muc.*:doit=newgroup
# rmgroup:muc-cmsg@muenchen.pro-bahn.org:muc.*:doit=rmgroup


## NAGASAKI-U ( Nagasaki University, Japan ?)
newgroup:root@*nagasaki-u.ac.jp:nagasaki-u.*:doit=newgroup
rmgroup:root@*nagasaki-u.ac.jp:nagasaki-u.*:doit=rmgroup

## NCF ( National Capital Freenet, Ottawa, Ontario, Canada )
# Contact: news@freenet.carleton.ca
# For local use only, contact the above address for information.
newgroup:*@*:ncf.*:drop
rmgroup:*@*:ncf.*:doit=rmgroup

## NCTU newsgroups (Taiwan)
newgroup:chen@cc.nctu.edu.tw:nctu.*:doit=newgroup
rmgroup:chen@cc.nctu.edu.tw:nctu.*:doit=rmgroup

## NET newsgroups ( Usenet 2 )
# URL: http://www.usenet2.org
# *PGP*   See comment at top of file.
# Key fingerprint: D7 D3 5C DB 18 6A 29 79  BF 74 D4 58 A3 78 9D 22
checkgroups:control@usenet2.org:net.*:verify-control@usenet2.org
newgroup:control@usenet2.org:net.*:verify-control@usenet2.org
rmgroup:control@usenet2.org:net.*:verify-control@usenet2.org

## NETSCAPE (Netscape Communications Corp)
# Contact: news@netscape.com
# *PGP*   See comment at top of file.
# URL: http://www.mozilla.org/community.html
# URL: http://www.mozilla.org/newsfeeds.html
# Key fingerprint = B7 80 55 12 1F 9C 17 0B  86 66 AD 3B DB 68 35 EC
checkgroups:news@netscape.com:netscape.*:verify-netscape.public.admin
newgroup:news@netscape.com:netscape.*:verify-netscape.public.admin
rmgroup:news@netscape.com:netscape.*:verify-netscape.public.admin

# checkgroups:news@netscape.com:netscape.*:mail
# newgroup:news@netscape.com:netscape.*:doit=newgroup
# rmgroups:news@netscape.com:netscape.*:doit=rmgroup

## NETINS ( netINS, Inc )
# Contact: Kevin Houle <kevin@netins.net>
# For local use only, contact the above address for information.
newgroup:*@*:netins.*:drop
rmgroup:*@*:netins.*:doit=rmgroup

## NIAGARA (Niagara Peninsula, US/CAN)
newgroup:news@niagara.com:niagara.*:doit=newgroup
rmgroup:news@niagara.com:niagara.*:doit=rmgroup

## NIAS (Japanese ?)
newgroup:news@cc.nias.ac.jp:nias.*:doit=newgroup
rmgroup:news@cc.nias.ac.jp:nias.*:doit=rmgroup

## NL (Netherlands)
# Contact: nl-admin@nic.surfnet.nl
# URL: http://www.xs4all.nl/~egavic/NL/ (Dutch)
# URL: http://www.kinkhorst.com/usenet/nladmin.en.html (English)
# *PGP*   See comment at top of file.
# Key fingerprint: 45 20 0B D5 A1 21 EA 7C  EF B2 95 6C 25 75 4D 27
checkgroups:nl-admin@nic.surfnet.nl:nl.*:verify-nl.newsgroups
newgroup:nl-admin@nic.surfnet.nl:nl.*:verify-nl.newsgroups
rmgroup:nl-admin@nic.surfnet.nl:nl.*:verify-nl.newsgroups

# checkgroups:nl-admin@nic.surfnet.nl:nl.*:mail
# newgroup:nl-admin@nic.surfnet.nl:nl.*:doit=newgroup
# rmgroup:nl-admin@nic.surfnet.nl:nl.*:doit=rmgroup

## NL-ALT (Alternative Netherlands groups)
# URL: http://www.xs4all.nl/~onno/nl-alt/
# Several options are given in the FAQ for creating and removing groups.
# *PGP*   See comment at top of file.
# Key fingerprint: 6B 62 EB 53 4D 5D 2F 96  35 D9 C8 9C B0 65 0E 4C
newgroup:*@*:nl-alt.*:doit=newgroup
rmgroup:nl-alt-janitor@surfer.xs4all.nl:nl-alt.*:verify-nl-alt.config.admin
rmgroup:news@kink.xs4all.nl:nl-alt.*:verify-nl-alt.config.admin

## NLNET newsgroups (Netherlands ISP)
newgroup:beheer@nl.net:nlnet.*:doit=newgroup
rmgroup:beheer@nl.net:nlnet.*:doit=rmgroup

## NM (New Mexico, USA)
newgroup:news@tesuque.cs.sandia.gov:nm.*:doit=newgroup
rmgroup:news@tesuque.cs.sandia.gov:nm.*:doit=rmgroup

## NO (Norway)
# See also http://www.usenet.no/
# *PGP*   See comment at top of file.
checkgroups:control@usenet.no:no.*:verify-no-hir-control
newgroup:control@usenet.no:no.*:verify-no-hir-control
newgroup:*@*.no:no.alt.*:doit=newgroup
rmgroup:control@usenet.no:no.*:verify-no-hir-control

# checkgroups:control@usenet.no:no.*:mail
# newgroup:control@usenet.no:no.*:doit=newgroup
# newgroup:*@*.no:no.alt.*:doit=newgroup
# rmgroup:control@usenet.no:no.*:doit=rmgroup
# sendsys:news@*uninett.no:no.*:doit=miscctl
# sendsys:control@usenet.no:no.*:doit=miscctl

## NV (Nevada)
newgroup:doctor@netcom.com:nv.*:doit=newgroup
newgroup:cshapiro@netcom.com:nv.*:doit=newgroup
rmgroup:doctor@netcom.com:nv.*:doit=rmgroup
rmgroup:cshapiro@netcom.com:nv.*:doit=rmgroup

## NY (New York State, USA)
newgroup:root@ny.psca.com:ny.*:mail
rmgroup:root@ny.psca.com:ny.*:mail

## NZ (New Zealand)
# *PGP*   See comment at top of file.
# Contact root@usenet.net.nz
# URL: http://usenet.net.nz
# URL: http://www.faqs.org/faqs/usenet/nz-news-hierarchy
# PGP fingerprint: 07 DF 48 AA D0 ED AA 88  16 70 C5 91 65 3D 1A 28
checkgroups:root@usenet.net.nz:nz.*:verify-nz-hir-control
newgroup:root@usenet.net.nz:nz.*:verify-nz-hir-control
rmgroup:root@usenet.net.nz:nz.*:verify-nz-hir-control

# newgroup:root@usenet.net.nz:nz.*:doit=newgroup
# rmgroup:root@usenet.net.nz:nz.*:doit=rmgroup


## OC newsgroups (?)
newgroup:bob@tsunami.sugarland.unocal.com:oc.*:doit=newgroup
rmgroup:bob@tsunami.sugarland.unocal.com:oc.*:doit=rmgroup

## OH (Ohio, USA)
newgroup:trier@ins.cwru.edu:oh.*:doit=newgroup
rmgroup:trier@ins.cwru.edu:oh.*:doit=rmgroup

## OK (Oklahoma, USA)
newgroup:quentin@*qns.com:ok.*:doit=newgroup
rmgroup:quentin@*qns.com:ok.*:doit=rmgroup

## OTT (Ottawa, Ontario, Canada)
# Contact: onag@pinetree.org
# URL: http://www.pinetree.org/ONAG/
newgroup:news@bnr.ca:ott.*:doit=mail
newgroup:news@nortel.ca:ott.*:doit=mail
newgroup:clewis@ferret.ocunix.on.ca:ott.*:doit=mail
newgroup:news@ferret.ocunix.on.ca:ott.*:doit=mail
newgroup:news@*pinetree.org:ott.*:doit=mail
newgroup:gordon@*pinetree.org:ott.*:doit=mail
newgroup:dave@revcan.ca:ott.*:doit=mail
rmgroup:news@bnr.ca:ott.*:doit=mail
rmgroup:news@nortel.ca:ott.*:doit=mail
rmgroup:clewis@ferret.ocunix.on.ca:ott.*:doit=mail
rmgroup:news@ferret.ocunix.on.ca:ott.*:doit=mail
rmgroup:news@*pinetree.org:ott.*:doit=mail
rmgroup:gordon@*pinetree.org:ott.*:doit=mail
rmgroup:dave@revcan.ca:ott.*:doit=mail


## PA (Pennsylvania, USA)
# URL: http://www.netcom.com/~rb1000/pa_hierarchy/
newgroup:fxp@epix.net:pa.*:doit=newgroup
rmgroup:fxp@epix.net:pa.*:doit=rmgroup

## PGH (Pittsburgh, Pennsylvania, USA)
# *PGP*   See comment at top of file.
checkgroups:pgh-config@psc.edu:pgh.*:verify-pgh.config=miscctl
newgroup:pgh-config@psc.edu:pgh.*:verify-pgh.config
rmgroup:pgh-config@psc.edu:pgh.*:verify-pgh.config

# checkgroups:pgh-config@psc.edu:pgh.*:mail
# newgroup:pgh-config@psc.edu:pgh.*:doit=newgroup
# rmgroup:pgh-config@psc.edu:pgh.*:doit=rmgroup

## PHL (Philadelphia, Pennsylvania, USA)
newgroup:news@vfl.paramax.com:phl.*:doit=newgroup
rmgroup:news@vfl.paramax.com:phl.*:doit=rmgroup

## PIN (Personal Internauts' NetNews)
newgroup:pin-admin@forus.or.jp:pin.*:doit=newgroup
rmgroup:pin-admin@forus.or.jp:pin.*:doit=rmgroup

## PL (Poland and Polish language)
## For more info, see http://www.ict.pwr.wroc.pl/doc/news-pl-new-site-faq.html
# *PGP*   See comment at top of file.
checkgroups:michalj@*fuw.edu.pl:pl.*:verify-pl.announce.newgroups
checkgroups:newgroup@usenet.pl:pl.*:verify-pl.announce.newgroups
newgroup:michalj@*fuw.edu.pl:pl.*:verify-pl.announce.newgroups
newgroup:newgroup@usenet.pl:pl.*:verify-pl.announce.newgroups
rmgroup:michalj@*fuw.edu.pl:pl.*:verify-pl.announce.newgroups
rmgroup:newgroup@usenet.pl:pl.*:verify-pl.announce.newgroups

# newgroup:michalj@*fuw.edu.pl:pl.*:doit=newgroup
# newgroup:newgroup@usenet.pl:pl.*:doit=newgroup
# rmgroup:michalj@*fuw.edu.pl:pl.*:doit=rmgroup
# rmgroup:newgroup@usenet.pl:pl.*:doit=rmgroup

## PLANET ( PlaNet FreeNZ co-operative, New Zealand)
# Contact: office@pl.net
# For local use only, contact the above address for information.
newgroup:*@*:planet.*:drop
rmgroup:*@*:planet.*:doit=rmgroup


## PRIMA (prima.ruhr.de/Prima e.V. in Germany)
# Contact: admin@prima.ruhr.de
# For internal use only, contact above address for questions
newgroup:*@*:prima.*:drop
rmgroup:*@*:prima.*:doit=rmgroup

# PSU ( Penn State University, USA )
# Contact: Dave Barr (barr@math.psu.edu)
# For internal use only, contact above address for questions
newgroup:*@*:psu.*:drop
rmgroup:*@*:psu.*:doit=rmgroup

## PT (Portugal and Portuguese language)
newgroup:pmelo@*inescc.pt:pt.*:doit=newgroup
rmgroup:pmelo@*inescc.pt:pt.*:doit=rmgroup

## PUBNET 
## This Hierarchy is now defunct.
## URL: ftp://ftp.uu.net/usenet/control/pubnet/pubnet.config.Z
newgroup:*@*:pubnet.*:drop
rmgroup:*@*:pubnet.*:doit=rmgroup

## RELCOM ( Commonwealth of Independent States)
## The official list of relcom groups is supposed to be available from
## URL: ftp://ftp.kiae.su/relcom/netinfo/telconfs.txt
checkgroups:dmart@new*.relcom.ru:relcom.*:mail
newgroup:dmart@new*.relcom.ru:relcom.*:doit=newgroup
rmgroup:dmart@new*.relcom.ru:relcom.*:doit=rmgroup

## RPI ( Rensselaer Polytechnic Institute, Troy, NY, USA)
# Contact: sofkam@rpi.edu
# For local use only, contact the above address for information.
newgroup:*@*:rpi.*:drop
rmgroup:*@*:rpi.*:doit=rmgroup

## SACHSNET (German)
newgroup:root@lusatia.de:sachsnet.*:doit=newgroup
rmgroup:root@lusatia.de:sachsnet.*:doit=rmgroup

## SAT (San Antonio, Texas, USA)
# *PGP*   See comment at top of file.
# Contact: satgroup@endicor.com
# URL: http://www.endicor.com/~satgroup/
checkgroups:satgroup@endicor.com:sat.*:verify-satgroup@endicor.com
newgroup:satgroup@endicor.com:sat.*:verify-satgroup@endicor.com
rmgroup:satgroup@endicor.com:sat.*:verify-satgroup@endicor.com

#checkgroups:satgroup@endicor.com:sat.*:doit=checkgroups
#newgroup:satgroup@endicor.com:sat.*:doit=newgroup
#rmgroup:satgroup@endicor.com:sat.*:doit=rmgroup


## SBAY (South Bay/Silicon Valley, California)
newgroup:steveh@grafex.sbay.org:sbay.*:doit=newgroup
newgroup:ikluft@thunder.sbay.org:sbay.*:doit=newgroup
rmgroup:steveh@grafex.sbay.org:sbay.*:mail
rmgroup:ikluft@thunder.sbay.org:sbay.*:mail

## SCHULE
##
# *PGP*   See comment at top of file.
checkgroups:newsctrl@schule.de:schule.*:verify-schule.konfig=miscctl
newgroup:newsctrl@schule.de:schule.*:verify-schule.konfig
rmgroup:newsctrl@schule.de:schule.*:verify-schule.konfig

## SDNET (Greater San Diego Area, California, USA)
# URL: http://www-rohan.sdsu.edu/staff/wk/w/sdnet.html
# URL: http://www-rohan.sdsu.edu/staff/wk/w/config.html
# URL: ftp://ftp.csu.net/pub/news/active
newgroup:wkronert@sunstroke.sdsu.edu:sdnet.*:doit=newgroup
rmgroup:wkronert@sunstroke.sdsu.edu:sdnet.*:doit=rmgroup

## SE (Sweden)
# Contact: usenet@usenet-se.net
# URL: http://www.usenet-se.net/
# URL: http://www.usenet-se.net/index_eng.html  (English version)
# Key URL:  http://www.usenet-se.net/pgp-key.txt
# Key fingerprint = 68 03 F0 FD 0C C3 4E 69  6F 0D 0C 60 3C 58 63 96
# *PGP*   See comment at top of file.
newgroup:usenet@usenet-se.net:se.*:verify-usenet-se
rmgroup:usenet@usenet-se.net:se.*:verify-usenet-se
checkgroups:usenet@usenet-se.net:se.*:verify-usenet-se

#newgroup:usenet@usenet-se.net:se.*:doit=newgroup
#rmgroup:usenet@usenet-se.net:se.*:doit=rmgroup
#checkgroups:usenet@usenet-se.net:se.*:doit=checkgroups


## SEATTLE (Seattle, Washington, USA)
newgroup:billmcc@akita.com:seattle.*:doit=newgroup
newgroup:graham@ee.washington.edu:seattle.*:doit=newgroup
rmgroup:billmcc@akita.com:seattle.*:doit=rmgroup
rmgroup:graham@ee.washington.edu:seattle.*:doit=rmgroup

## SFNET newsgroups (Finland)
newgroup:hmj@*cs.tut.fi:sfnet.*:doit=newgroup
rmgroup:hmj@*cs.tut.fi:sfnet.*:doit=rmgroup

## SHAMASH (Jewish)
newgroup:archives@israel.nysernet.org:shamash.*:doit=newgroup
rmgroup:archives@israel.nysernet.org:shamash.*:doit=rmgroup

## SI (The Republic of Slovenia)
# *PGP*   See comment at top of file.
checkgroups:news-admin@arnes.si:si.*:verify-si.news.announce.newsgroups=miscctl
newgroup:news-admin@arnes.si:si.*:verify-si.news.announce.newsgroups
rmgroup:news-admin@arnes.si:si.*:verify-si.news.announce.newsgroups

# newgroup:news-admin@arnes.si:si.*:doit=newgroup
# rmgroup:news-admin@arnes.si:si.*:doit=rmgroup

## SK (Slovakia)
checkgroups:uhlar@ccnews.ke.sanet.sk:sk.*:mail
newgroup:uhlar@ccnews.ke.sanet.sk:sk.*:doit=newgroup
rmgroup:uhlar@ccnews.ke.sanet.sk:sk.*:doit=rmgroup

## SLAC ( Stanford Linear Accelerator Center, Stanford, USA )
# Contact: news@news.stanford.edu
# Limited distribution hierarchy, contact the above address for information.
newgroup:news@news.stanford.edu:slac.*:log
rmgroup:news@news.stanford.edu:slac.*:doit=rmgroup

## SOLENT (Solent region, England)
newgroup:news@tcp.co.uk:solent.*:doit=newgroup
rmgroup:news@tcp.co.uk:solent.*:doit=rmgroup

## STGT (Stuttgart, Germany)
checkgroups:news@news.uni-stuttgart.de:stgt.*:mail
newgroup:news@news.uni-stuttgart.de:stgt.*:doit=newgroup
rmgroup:news@news.uni-stuttgart.de:stgt.*:doit=rmgroup

## STL (Saint Louis, Missouri, USA)
newgroup:news@icon-stl.net:stl.*:doit=newgroup
rmgroup:news@icon-stl.net:stl.*:doit=rmgroup

## SU ( Stanford University, USA )
# Contact: news@news.stanford.edu
# For local use only, contact the above address for information.
newgroup:*@*:su.*:drop
rmgroup:*@*:su.*:doit=rmgroup

## SURFNET (Dutch Universities network)
newgroup:news@info.nic.surfnet.nl:surfnet.*:doit=newgroup
rmgroup:news@info.nic.surfnet.nl:surfnet.*:doit=rmgroup

## SWNET (Sverige, Sweden)
newgroup:ber@sunic.sunet.se:swnet.*:doit=newgroup
rmgroup:ber@sunic.sunet.se:swnet.*:doit=rmgroup

## TAOS (Taos, New Mexico, USA)
# Contact: "Chris Gunn" <cgunn@laplaza.org>
newgroup:cgunn@laplaza.org:taos.*:doit=newgroup
rmgroup:cgunn@laplaza.org:taos.*:doit=rmgroup

## TCFN (Toronto Free Community Network, Canada)
newgroup:news@t-fcn.net:tcfn.*:doit=newgroup
rmgroup:news@t-fcn.net:tcfn.*:doit=rmgroup

## T-NETZ (German Email Network)
# Defunct, use z-netz.*
newgroup:*@*:t-netz.*:drop
rmgroup:*@*:t-netz.*:doit=rmgroup

## TELE (Tele Danmark Internet)
# Contact: usenet@tdk.net
# For internal use only, contact above address for questions
newgroup:*@*:tele.*:drop
rmgroup:*@*:tele.*:doit=rmgroup

## TEST (Local test hierarchy)
# It is not really a good idea for sites to use these since they
# may occur on many unconnect sites.
newgroup:*@*:test.*:drop
rmgroup:*@*:test.*:drop

## THUR ( Thuringia, Germany )
# *PGP*   See comment at top of file.
# Key Fingerprint: 7E 3D 73 13 93 D4 CA 78  39 DE 3C E7 37 EE 22 F1
checkgroups:usenet@thur.de:thur.*:verify-thur.net.news.groups
newgroup:usenet@thur.de:thur.*:verify-thur.net.news.groups
rmgroup:usenet@thur.de:thur.*:verify-thur.net.news.groups

## TNN ( The Network News, Japan )
newgroup:tnn@iij-mc.co.jp:tnn.*:doit=newgroup
newgroup:netnews@news.iij.ad.jp:tnn.*:doit=newgroup
rmgroup:tnn@iij-mc.co.jp:tnn.*:doit=rmgroup
rmgroup:netnews@news.iij.ad.jp:tnn.*:doit=rmgroup

## TRIANGLE (Central North Carolina, USA )
newgroup:jfurr@acpub.duke.edu:triangle.*:doit=newgroup
newgroup:tas@concert.net:triangle.*:doit=newgroup
newgroup:news@news.duke.edu:triangle.*:doit=newgroup
rmgroup:jfurr@acpub.duke.edu:triangle.*:doit=rmgroup
rmgroup:tas@concert.net:triangle.*:doit=rmgroup
rmgroup:news@news.duke.edu:triangle.*:doit=rmgroup

## TW (Taiwan)
newgroup:ltc@news.cc.nctu.edu.tw:tw.*:doit=newgroup
newgroup:k-12@news.nchu.edu.tw:tw.k-12.*:doit=newgroup
rmgroup:ltc@news.cc.nctu.edu.tw:tw.*:doit=rmgroup
rmgroup:k-12@news.nchu.edu.tw:tw.k-12*:doit=rmgroup

## TX (Texas, USA)
newgroup:eric@cirr.com:tx.*:doit=newgroup
newgroup:fletcher@cs.utexas.edu:tx.*:doit=newgroup
newgroup:usenet@academ.com:tx.*:doit=newgroup
rmgroup:eric@cirr.com:tx.*:doit=rmgroup
rmgroup:fletcher@cs.utexas.edu:tx.*:doit=rmgroup
rmgroup:usenet@academ.com:tx.*:doit=rmgroup

## UCB ( University of California Berkeley, USA)
# I don't know what happens here, Rob creates the groups
# one minute and then usenet creates them as moderated...
# 
# newgroup:rob@agate.berkeley.edu:ucb.*:doit=newgroup
# newgroup:usenet@agate.berkeley.edu:ucb.*:doit=newgroup
# rmgroup:rob@agate.berkeley.edu:ucb.*:doit=rmgroup
# rmgroup:usenet@agate.berkeley.edu:ucb.*:doit=rmgroup

## UCD ( University of California Davis, USA )
newgroup:usenet@rocky.ucdavis.edu:ucd.*:doit=newgroup
newgroup:usenet@mark.ucdavis.edu:ucd.*:doit=newgroup
rmgroup:usenet@rocky.ucdavis.edu:ucd.*:doit=rmgroup
rmgroup:usenet@mark.ucdavis.edu:ucd.*:doit=rmgroup


## UFRA (Unterfranken, Deutschland)
# Contact: news@mayn.de
# URL: http://www.mayn.de/users/news/
# Key fingerprint = F7 AD 96 D8 7A 3F 7E 84  02 0C 83 9A DB 8F EB B8
# Syncable server: news.mayn.de (contact news@mayn.de if permission denied)
# *PGP*   See comment at top of file.
newgroup:news@mayn.de:ufra.*:verify-news.mayn.de=newgroup
rmgroup:news@mayn.de:ufra.*:verify-news.mayn.de=rmgroup
checkgroups:news@mayn.de:ufra.*:verify-news.mayn.de=misctl

# newgroup:news@mayn.de:ufra.*:verify-news.mayn.de=mail
# rmgroup:news@mayn.de:ufra.*:verify-news.mayn.de=mail
# checkgroups:news@mayn.de:ufra.*:verify-news.mayn.de=mail


## UIUC (University of Illinois, USA )
newgroup:p-pomes@*.cso.uiuc.edu:uiuc.*:doit=newgroup
newgroup:paul@*.cso.uiuc.edu:uiuc.*:doit=newgroup
rmgroup:p-pomes@*.cso.uiuc.edu:uiuc.*:doit=rmgroup
rmgroup:paul@*.cso.uiuc.edu:uiuc.*:doit=rmgroup

## UK (United Kingdom of Great Britain and Northern Ireland)
# *PGP*   See comment at top of file.
checkgroups:control@usenet.org.uk:uk.*:verify-uk.net.news.announce
newgroup:control@usenet.org.uk:uk.*:verify-uk.net.news.announce
rmgroup:control@usenet.org.uk:uk.*:verify-uk.net.news.announce

# checkgroups:control@usenet.org.uk:uk.*:mail
# newgroup:control@usenet.org.uk:uk.*:doit=newgroup
# rmgroup:control@usenet.org.uk:uk.*:doit=rmgroup


## UKR ( Ukraine )
newgroup:news-server@sita.kiev.ua:ukr.*:doit=newgroup
rmgroup:news-server@sita.kiev.ua:ukr.*:doit=rmgroup

## UMN (University of Minnesota, USA )
newgroup:edh@*.tc.umn.edu:umn.*:doit=newgroup
newgroup:news@*.tc.umn.edu:umn.*:doit=newgroup
newgroup:Michael.E.Hedman-1@umn.edu:umn.*:doit=newgroup
newgroup:edh@*.tc.umn.edu:umn*class.*:log
newgroup:news@*.tc.umn.edu:umn*class.*:log
newgroup:Michael.E.Hedman-1@umn.edu:umn*class.*:log
rmgroup:news@*.tc.umn.edu:umn.*:doit=rmgroup
rmgroup:edh@*.tc.umn.edu:umn.*:doit=rmgroup
rmgroup:Michael.E.Hedman-1@umn.edu:umn.*:doit=rmgroup

## UN (The United Nations)
# URL: http://www.itu.int/Conferences/un/
# *PGP*   See comment at top of file.
checkgroups:news@news.itu.int:un.*:verify-ungroups@news.itu.int
newgroup:news@news.itu.int:un.*:verify-ungroups@news.itu.int
rmgroup:news@news.itu.int:un.*:verify-ungroups@news.itu.int

# checkgroups:news@news.itu.int:un.*:mail
# newgroup:news@news.itu.int:un.*:doit=newgroup
# rmgroup:news@news.itu.int:un.*:doit=rmgroup

## UO (University of Oregon, Eugene, Oregon, USA )
newgroup:newsadmin@news.uoregon.edu:uo.*:doit=newgroup
rmgroup:newsadmin@news.uoregon.edu:uo.*:doit=rmgroup

## US (United States of America)
# *PGP*   See comment at top of file.
checkgroups:usadmin@wwa.com:us.*:mail
newgroup:usadmin@wwa.com:us.*:doit=newgroup
rmgroup:usadmin@wwa.com:us.*:doit=rmgroup

## UT (U. of Toronto)
# newgroup:news@ecf.toronto.edu:ut.*:doit=newgroup
# newgroup:news@ecf.toronto.edu:ut.class.*:log
# rmgroup:news@ecf.toronto.edu:ut.*:doit=rmgroup

## UTA (Finnish)
newgroup:news@news.cc.tut.fi:uta.*:doit=newgroup
rmgroup:news@news.cc.tut.fi:uta.*:doit=rmgroup

## UTEXAS (University of Texas, USA )
newgroup:fletcher@cs.utexas.edu:utexas.*:doit=newgroup
newgroup:news@geraldo.cc.utexas.edu:utexas.*:doit=newgroup
newgroup:fletcher@cs.utexas.edu:utexas*class.*:log
newgroup:news@geraldo.cc.utexas.edu:utexas*class.*:log
rmgroup:fletcher@cs.utexas.edu:utexas.*:doit=rmgroup
rmgroup:news@geraldo.cc.utexas.edu:utexas.*:doit=rmgroup

## UTWENTE (University of Twente, Netherlands)
# Contact: newsmaster@utwente.nl
# For internal use only, contact above address for questions
newgroup:*@*:utwente.*:drop
rmgroup:*@*:utwente.*:doit=rmgroup

## UVA (virginia.edu - University of Virginia)
# Contact: usenet@virginia.edu
# For internal use only, contact above address for questions
newgroup:*@*:uva.*:drop
rmgroup:*@*:uva.*:doit=rmgroup

## UW (University of Waterloo, Canada)
newgroup:bcameron@math.uwaterloo.ca:uw.*:doit=newgroup
rmgroup:bcameron@math.uwaterloo.ca:uw.*:doit=rmgroup

## UWO (University of Western Ontario, London, Canada)
newgroup:reggers@julian.uwo.ca:uwo.*:doit=newgroup
rmgroup:reggers@julian.uwo.ca:uwo.*:doit=rmgroup

## VEGAS (Las Vegas, Nevada, USA)
newgroup:cshapiro@netcom.com:vegas.*:doit=newgroup
newgroup:doctor@netcom.com:vegas.*:doit=newgroup
rmgroup:cshapiro@netcom.com:vegas.*:doit=rmgroup
rmgroup:doctor@netcom.com:vegas.*:doit=rmgroup

## VMSNET ( VMS Operating System )
newgroup:cts@dragon.com:vmsnet.*:doit=newgroup
rmgroup:cts@dragon.com:vmsnet.*:doit=rmgroup

## WADAI (Japanese ?) 
newgroup:kohe-t@*wakayama-u.ac.jp:wadai.*:doit=newgroup
rmgroup:kohe-t@*wakayama-u.ac.jp:wadai.*:doit=rmgroup

## WASH (Washington State, USA)
newgroup:graham@ee.washington.edu:wash.*:doit=newgroup
rmgroup:graham@ee.washington.edu:wash.*:doit=rmgroup

## WORLDONLINE
# Contact: newsmaster@worldonline.nl
# For local use only, contact the above address for information.
newgroup:*@*:worldonline.*:drop
rmgroup:*@*:worldonline.*:doit=rmgroup

## WPI (Worcester Polytechnic Institute, Worcester, MA)
newgroup:aej@*.wpi.edu:wpi.*:doit=newgroup
rmgroup:aej@*.wpi.edu:wpi.*:doit=rmgroup

## Z-NETZ (German non internet based network.)
# *PGP*   See comment at top of file.
# MAIL: pgp-public-keys@informatik.uni-hamburg.de Subject: GET 0x40145FC9
checkgroups:dirk.meyer@dinoex.sub.org:z-netz.*:verify-checkgroups-dinoex
newgroup:dirk.meyer@dinoex.sub.org:z-netz.*:verify-checkgroups-dinoex
newgroup:*@*.de:z-netz.alt.*:doit=newgroup
newgroup:*@*.sub.org:z-netz.alt.*:doit=newgroup
rmgroup:dirk.meyer@dinoex.sub.org:z-netz.*:verify-checkgroups-dinoex

# newgroup:*@*.de:z-netz.*:mail
# newgroup:*@*.sub.org:z-netz.*:mail
# rmgroup:*@*.de:z-netz.*:mail

## ZA (South Africa)
newgroup:root@duvi.eskom.co.za:za.*:doit=newgroup
newgroup:ccfj@hippo.ru.ac.za:za.*:doit=newgroup
rmgroup:root@duvi.eskom.co.za:za.*:doit=rmgroup
rmgroup:ccfj@hippo.ru.ac.za:za.*:doit=rmgroup

## ZER (German Email Network)
## Defunct, use z-netz.*
newgroup:*@*:zer.*:drop
rmgroup:*@*:zer.*:doit=rmgroup
