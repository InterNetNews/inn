##  $Revision$
##  control.ctl - access control for control messages
##  Format:
##	<message>:<from>:<newsgroups>:<action>
##  The last match found is used.
##	<message>	Control message or "all" if it applies
##			to all control messages.
##	<from>		Pattern that must match the From line.
##	<newsgroups>	Pattern that must match the newsgroup being
##			newgroup'd or rmgroup'd (ignored for other messages).
##	<action>	What to do:
##			    doit	Perform action (usually sends mail too)
##			    doifarg	Do if command has an arg (see sendsys)
##			    doit=xxx	Do action; log to xxx (see below)
##			    drop	Ignore message
##			    log		One line to error log
##			    log=xxx	Log to xxx (see below)
##			    mail	Send mail to admin
##			    verify-pgp_userid	Do PGP verification on user.
##			    verify-pgp_userid=logfile	PGP verify and log.
##			xxx=mail to mail; xxx= (empty) to toss; xxx=/full/path
##			to log to /full/path; xxx=foo to log to ${LOG}/foo.log
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
## comes right after that. You'll also need to change WANT_PGPVERIFY in
## config.data.
##

###########################################################################
##
##	DEFAULTS
##

all:*:*:mail
checkgroups:*:*:log=checkgroups
newgroup:*:*:log=newgroup
rmgroup:*:*:log=rmgroup
sendsys:*:*:log=sendsys
senduuname:*:*:log=senduuname
version:*:*:log=version
ihave:*:*:drop
sendme:*:*:drop


##
## We honor sendsys, senduuuname and version from *@uunet.uu.net and 
## inn@isc.org
##

sendsys:*@uunet.uu.net:*:doit=miscctl
senduuname:*@uunet.uu.net:*:doit=miscctl
version:*@uunet.uu.net:*:doit=miscctl

sendsys:inn@isc.org:*:doit=miscctl
senduuname:inn@isc.org:*:doit=miscctl
version:inn@isc.org:*:doit=miscctl


###########################################################################
##
##	HIERARCHIES
##
##

##
## BIG 8  comp, humanities, misc, news, rec, sci, soc, talk
##

# *PGP*   See comment at top of file.
checkgroups:tale@uunet.uu.net:*:verify-news.announce.newgroups=miscctl
newgroup:tale@uunet.uu.net:comp.*|misc.*|news.*:verify-news.announce.newgroups
newgroup:tale@uunet.uu.net:rec.*|sci.*|soc.*:verify-news.announce.newgroups
newgroup:tale@uunet.uu.net:talk.*|humanities.*:verify-news.announce.newgroups
rmgroup:tale@uunet.uu.net:comp.*|misc.*|news.*:verify-news.announce.newgroups
rmgroup:tale@uunet.uu.net:rec.*|sci.*|soc.*:verify-news.announce.newgroups
rmgroup:tale@uunet.uu.net:talk.*|humanities.*:verify-news.announce.newgroups

# checkgroups:tale@uunet.uu.net:*:doit=checkgroups
# newgroup:tale@*.uu.net:comp.*|misc.*|news.*|rec.*|sci.*:doit=newgroup
# newgroup:tale@*.uu.net:soc.*|talk.*|humanities.*:doit=newgroup
# rmgroup:tale@*.uu.net:comp.*|misc.*|news.*|rec.*|sci.*:doit=newgroup
# rmgroup:tale@*.uu.net:soc.*|talk.*|humanities.*:doit=newgroup


##
##	SLUDGE	(alt)
##

##
## Accept all newgroup's as well as rmgroup's from trusted sources and
## process them silently.  Only the rmgroup messages from unknown sources
## will be e-mailed to the administrator.
##
## Other options and comments on alt.* groups can be found on Bill 
## Hazelrig's WWW pages at http://www.tezcat.com/~haz1/alt/faqindex.html
##
newgroup:*:alt.*:doit=newgroup
rmgroup:*:alt.*:mail
rmgroup:haz1@*nwu.edu:alt.*:doit=rmgroup
rmgroup:grobe@*netins.net:alt.*:doit=rmgroup
rmgroup:barr@*.psu.edu:alt.*:doit=rmgroup
rmgroup:smj@*.oro.net:alt.*:doit=rmgroup
rmgroup:davidg@*.netcom.com:alt.*:doit=rmgroup
rmgroup:news@gymnet.com:alt.*:doit=rmgroup

##
##	GNU ( Free Software Foundation )
##

newgroup:gnu@ai.mit.edu:gnu.*:doit=newgroup
newgroup:gnu@prep.ai.mit.edu:gnu.*:doit=newgroup
newgroup:news@ai.mit.edu:gnu.*:doit=newgroup
newgroup:news@ai.mit.edu:gnu.*:doit=newgroup
newgroup:news@prep.ai.mit.edu:gnu.*:doit=newgroup
newgroup:tower@prep.ai.mit.edu:gnu.*:doit=newgroup
newgroup:usenet@*ohio-state.edu:gnu.*:doit=newgroup
rmgroup:gnu@ai.mit.edu:gnu.*:doit=rmgroup
rmgroup:gnu@prep.ai.mit.edu:gnu.*:doit=rmgroup
rmgroup:news@ai.mit.edu:gnu.*:doit=rmgroup
rmgroup:news@prep.ai.mit.edu:gnu.*:doit=rmgroup
rmgroup:rms@ai.mit.edu:gnu.*:doit=rmgroup
rmgroup:tower@prep.ai.mit.edu:gnu.*:doit=rmgroup
rmgroup:usenet@*ohio-state.edu:gnu.*:doit=rmgroup


##
## CLARINET ( Features and News, Available on a commercial basis)
##

# *PGP*   See comment at top of file.
newgroup:cl*@clarinet.com:clari.*:verify-ClariNet.Group
rmgroup:cl*@clarinet.com:clari.*:verify-ClariNet.Group

#newgroup:brad@clarinet.com:clari.*:doit=newgroup
#newgroup:clarinet@clarinet.com:clari.*:doit=newgroup
#newgroup:clarinet@clarinet.net:clari.*:doit=newgroup
#rmgroup:brad@clarinet.com:clari.*:doit=rmgroup
#rmgroup:clarinet@clarinet.com:clari.*:doit=rmgroup
#rmgroup:clarinet@clarinet.net:clari.*:doit=rmgroup



##
## BIONET (Biology Network)
##

checkgroups:kristoff@*.bio.net:bionet.*:doit=checkgroups
checkgroups:news@*.bio.net:bionet.*:doit=checkgroups
newgroup:dmack@*.bio.net:bionet.*:doit=newgroup
newgroup:kristoff@*.bio.net:bionet.*:doit=newgroup
newgroup:shibumi@*.bio.net:bionet.*:doit=mail
rmgroup:dmack@*.bio.net:bionet.*:doit=rmgroup
rmgroup:kristoff@*.bio.net:bionet.*:doit=rmgroup
rmgroup:shibumi@*.bio.net:bionet.*:doit=mail

## BIT (Gatewayed Mailing lists)
newgroup:jim@*american.edu:bit.*:doit=newgroup
rmgroup:jim@*american.edu:bit.*:doit=rmgroup

## BIZ (Business Groups)
newgroup:edhew@xenitec.on.ca:biz.*:doit=newgroup
rmgroup:edhew@xenitec.on.ca:biz.*:doit=rmgroup


##
## REGIONALS and other misc.
##

##  ACS hierarchy (Ohio State)
newgroup:kitw@magnus.acs.ohio-state.edu:acs.*:doit=newgroup
rmgroup:kitw@magnus.acs.ohio-state.edu:acs.*:doit=rmgroup

## ALABAMA (USA)
newgroup:news@news.msfc.nasa.gov:alabama.*:doit=newgroup
rmgroup:news@news.msfc.nasa.gov:alabama.*:doit=rmgroup

## AT (Austrian)
newgroup:gw@snoopy.cc.univie.ac.at:at.*:doit=newgroup
newgroup:ma@snoopy.cc.univie.ac.at:at.*:doit=newgroup
newgroup:news@ping.at:at.*:doit=newgroup
newgroup:lendl@cosy.sbg.ac.at:at.*:doit=newgroup
rmgroup:gw@snoopy.cc.univie.ac.at:at.*:doit=rmgroup
rmgroup:ma@snoopy.cc.univie.ac.at:at.*:doit=rmgroup
rmgroup:news@ping.at:at.*:doit=rmgroup
rmgroup:lendl@cosy.sbg.ac.at:at.*:doit=rmgroup

## AUS (Australia)
newgroup:kre@*mu*au:aus.*:doit=newgroup
newgroup:revdoc@*uow.edu.au:aus.*:doit=newgroup
rmgroup:kre@*mu*au:aus.*:doit=rmgroup
rmgroup:revdoc@*uow.edu.au:aus.*:doit=rmgroup

## AUSTIN (Texas) 
newgroup:pug@arlut.utexas.edu:austin.*:doit=newgroup
rmgroup:pug@arlut.utexas.edu:austin.*:doit=rmgroup

## AZ (Arizona)
newgroup:system@asuvax.eas.asu.edu:az.*:doit=newgroup
rmgroup:system@asuvax.eas.asu.edu:az.*:doit=rmgroup

## BERMUDA
newgroup:news@*ibl.bm:bermuda.*:doit=newgroup
rmgroup:news@*ibl.bm:bermuda.*:doit=rmgroup

## BLN (Berlin, Germany)
checkgroups:news@*fu-berlin.de:bln.*:doit=checkgroups
newgroup:news@*fu-berlin.de:bln.*:doit=newgroup
rmgroup:news@*fu-berlin.de:bln.*:doit=rmgroup

## BOFH ( Bastard Operator From Hell )
newgroup:juphoff@*nrao.edu:bofh.*:doit=mail
newgroup:peter@*taronga.com:bofh.*:doit=mail
rmgroup:juphoff@*nrao.edu:bofh.*:doit=mail
rmgroup:peter@*taronga.com:bofh.*:doit=mail

## CAPDIST (Albany, The Capital District, New York, USA)
newgroup:danorton@albany.net:capdist.*:doit=newgroup
rmgroup:danorton@albany.net:capdist.*:doit=rmgroup

## CARLETON (Canadian -- Carleton University)
newgroup:news@cunews.carleton.ca:carleton.*:doit=newgroup
newgroup:news@cunews.carleton.ca:carleton*class.*:log
rmgroup:news@cunews.carleton.ca:carleton.*:doit=rmgroup

## CHRISTNET newsgroups
checkgroups:news@fdma.com:christnet.*:doit=checkgroups
newgroup:news@fdma.com:christnet.*:doit=newgroup
rmgroup:news@fdma.com:christnet.*:doit=rmgroup

## CHI (Chicago, USA)
newgroup:lisbon@*interaccess.com:chi.*:doit=newgroup
newgroup:lisbon@*chi.il.us:chi.*:doit=newgroup
rmgroup:lisbon@*interaccess.com:chi.*:doit=rmgroup
rmgroup:lisbon@*chi.il.us:chi.*:doit=rmgroup

## CHINESE (China and Chinese language groups)
newgroup:pinghua@stat.berkeley.edu:chinese.*:doit=newgroup
rmgroup:pinghua@stat.berkeley.edu:chinese.*:doit=rmgroup

## CL (CL-Netz, German)
newgroup:root@cl.sub.de:cl.*:doit=newgroup
rmgroup:root@cl.sub.de:cl.*:doit=rmgroup

## CZ newsgroups (Czech Republic)
checkgroups:petr.kolar@vslib.cz:cz.*:doit=checkgroups
newgroup:petr.kolar@vslib.cz:cz.*:doit=newgroup
rmgroup:petr.kolar@vslib.cz:cz.*:doit=rmgroup

## DC (Washington, D.C. USA )
newgroup:pete@cs.umd.edu:dc.*:doit=newgroup
rmgroup:pete@cs.umd.edu:dc.*:doit=rmgroup

## DE (German language)
checkgroups:*@*dana.de:de.*:doit=checkgroups
checkgroups:*@*.dana.de:de.*:doit=checkgroups
newgroup:*@dana.de|*@*.dana.de:de.*:doit=newgroup
newgroup:*@*:de.alt.*:doit=newgroup
rmgroup:*@dana.de|*@*.dana.de:de.*:doit=rmgroup

## DFW (Dallas/Fort Worth, Texas, USA)
newgroup:eric@*cirr.com:dfw.*:doit=newgroup
rmgroup:eric@*cirr.com:dfw.*:doit=rmgroup

## DK (Denmark)
newgroup:shj@dknet.dk:dk.*:doit=newgroup
rmgroup:shj@dknet.dk:dk.*:doit=rmgroup

## EHIME-U (? University, Japan ?)
newgroup:news@cc.nias.ac.jp:ehime-u.*:doit=newgroup
newgroup:news@doc.dpc.ehime-u.ac.jp:ehime-u.*:doit=newgroup
rmgroup:news@cc.nias.ac.jp:ehime-u.*:doit=rmgroup
rmgroup:news@doc.dpc.ehime-u.ac.jp:ehime-u.*:doit=rmgroup

## EUNET ( Europe )
newgroup:news@noc.eu.net:eunet.*:doit=newgroup
rmgroup:news@noc.eu.net:eunet.*:doit=rmgroup

## FIDO newsgroups (FidoNet)
newgroup:root@mbh.org:fido.*:doit=newgroup
rmgroup:root@mbh.org:fido.*:doit=rmgroup

## FIDO7
newgroup:news@wing.matsim.udmurtia.su:fido7.*:doit=newgroup
rmgroup:news@wing.matsim.udmurtia.su:fido7.*:doit=rmgroup

## FJ (Japan and Japanese language)
newgroup:fj-committee@etl.go.jp:fj.*:doit=newgroup
rmgroup:fj-committee@etl.go.jp:fj.*:doit=rmgroup

## FR (French Language)

# *PGP*   See comment at top of file.
newgroup:control@usenet.fr.net:fr.*:verify-fr.announce.newgroups
rmgroup:control@usenet.fr.net:fr.*:verify-fr.announce.newgroups

#newgroup:control@usenet.fr.net:fr.*:doit=newgroup
#rmgroup:control@usenet.fr.net:fr.*:doit=rmgroup


## FREE
newgroup:*:free.*:doit=mail
rmgroup:*:free.*:doit=mail

## FUDAI (Japanese ?)
newgroup:news@picard.cs.osakafu-u.ac.jp:fudai.*:doit=newgroup
rmgroup:news@picard.cs.osakafu-u.ac.jp:fudai.*:doit=rmgroup

## GIT (Georgia Institute of Technology, USA )
newgroup:news@news.gatech.edu:git.*:doit=newgroup
newgroup:news@news.gatech.edu:git*class.*:log
rmgroup:news@news.gatech.edu:git.*:doit=rmgroup

## HAMILTON (Canadian)
newgroup:news@*dcss.mcmaster.ca:hamilton.*:doit=newgroup
rmgroup:news@*dcss.mcmaster.ca:hamilton.*:doit=rmgroup

## HAN (Korean Hangul)
newgroup:news@usenet.hana.nm.kr:han.*:doit=newgroup
rmgroup:news@usenet.hana.nm.kr:han.*:doit=rmgroup

## HANNET & HANNOVER (Hannover, Germany) 
newgroup:fifi@hiss.han.de:hannover.*|hannet.*:doit=newgroup
rmgroup:fifi@hiss.han.de:hannover.*|hannet.*:doit=rmgroup

## HAWAII 
newgroup:news@lava.net:hawaii.*:doit=newgroup
rmgroup:news@lava.net:hawaii.*:doit=rmgroup

## HOUSTON (Houston, Texas, USA)
newgroup:usenet@academ.com:houston.*:doit=newgroup
rmgroup:usenet@academ.com:houston.*:doit=rmgroup

## HUN (Hungary)
checkgroups:kissg@*sztaki.hu:hun.*:doit=checkgroups
checkgroups:hg@*.elte.hu:hun.org.elte.*:doit=checkgroups
newgroup:kissg@*sztaki.hu:hun.*:doit=newgroup
newgroup:hg@*.elte.hu:hun.org.elte.*:doit=newgroup
rmgroup:kissg@*sztaki.hu:hun.*:doit=rmgroup
rmgroup:hg@*.elte.hu:hun.org.elte.*:doit=rmgroup

## IA (Iowa, USA)
newgroup:skunz@iastate.edu:ia.*:doit=newgroup
rmgroup:skunz@iastate.edu:ia.*:doit=rmgroup

## IEEE 
newgroup:burt@ieee.org:ieee.*:doit=newgroup
rmgroup:burt@ieee.org:ieee.*:doit=rmgroup

## INFO newsgroups
newgroup:rjoyner@uiuc.edu:info.*:doit=newgroup
rmgroup:rjoyner@uiuc.edu:info.*:doit=rmgroup

## ISC ( Japanese ?)
newgroup:news@sally.isc.chubu.ac.jp:isc.*:doit=newgroup
rmgroup:news@sally.isc.chubu.ac.jp:isc.*:doit=newgroup

## ISRAEL and IL newsgroups (Israel)
newgroup:news@news.biu.ac.il:israel.*:doit=newgroup
rmgroup:news@news.biu.ac.ul:israel.*|il.*:doit=rmgroup

## IT (Italian)
newgroup:news@ghost.sm.dsi.unimi.it:it.*:doit=newgroup
newgroup:stefano@*unipi.it:it.*:doit=newgroup
rmgroup:news@ghost.sm.dsi.unimi.it:it.*:doit=rmgroup
rmgroup:stefano@*unipi.it:it.*:doit=rmgroup

## IU (Indiana University)
newgroup:news@usenet.ucs.indiana.edu:iu.*:doit=newgroup
newgroup:root@usenet.ucs.indiana.edu:iu.*:doit=newgroup
newgroup:*@usenet.ucs.indiana.edu:iu*class.*:log
rmgroup:news@usenet.ucs.indiana.edu:iu.*:doit=rmgroup
rmgroup:root@usenet.ucs.indiana.edu:iu.*:doit=rmgroup

## K12 ( US Educational Network )
newgroup:*@psg.com:k12.*:doit=mail
rmgroup:*@psg.com:k12.*:doit=mail

## KIEL newsgroups
checkgroups:kris@white.schulung.netuse.de:kiel.*:doit=checkgroups
newgroup:kris@white.schulung.netuse.de:kiel.*:doit=newgroup
rmgroup:kris@white.schulung.netuse.de:kiel.*:doit=rmgroup

## LIU newsgroups (Sweden?)
newgroup:linus@tiny.lysator.liu.se:liu.*:doit=newgroup
rmgroup:linus@tiny.lysator.liu.se:liu.*:doit=rmgroup

## LINUX (gatewayed mailing lists for the Linux OS)
newgroup:hpa@yggdrasil.com:linux.*:doit=newgroup
rmgroup:hpa@yggdrasil.com:linux.*:doit=rmgroup

## MAUS ( MausNet, German )
newgroup:guenter@gst0hb.north.de:maus.*:doit=newgroup
rmgroup:guenter@gst0hb.north.de:maus.*:doit=rmgroup

## ME (Maine, USA)
newgroup:kerry@maine.maine.edu:me.*:doit=newgroup
rmgroup:kerry@maine.maine.edu:me.*:doit=rmgroup

## MELB ( Melbourne, Australia)
newgroup:kre@*mu*au:melb.*:doit=newgroup
newgroup:revdoc@*uow.edu.au:melb.*:doit=newgroup
rmgroup:kre@*mu*au:melb.*:doit=rmgroup
rmgroup:revdoc@*uow.edu.au:melb.*:doit=rmgroup

## METOCEAN newsgroups (ISP in Japan)
newgroup:fwataru@*.metocean.co.jp:metocean.*:doit=newgroup
rmgroup:fwataru@*.metocean.co.jp:metocean.*:doit=rmgroup

## MUC (Munchen, Germany. Gatewayed mailing lists??)
newgroup:newsgate@mystery.muc.de:muc.*:doit=newgroup
rmgroup:newsgate@mystery.muc.de:muc.*:doit=rmgroup

## NAGASAKI-U ( Nagasaki University, Japan ?)
newgroup:root@*nagasaki-u.ac.jp:nagasaki-u.*:doit=newgroup
rmgroup:root@*nagasaki-u.ac.jp:nagasaki-u.*:doit=rmgroup

## NCTU newsgroups (Taiwan)
newgroup:chen@cc.nctu.edu.tw:nctu.*:doit=newgroup
rmgroup:chen@cc.nctu.edu.tw:nctu.*:doit=rmgroup

## NIAGARA (Niagara Peninsula, US/CAN)
newgroup:news@niagara.com:niagara.*:doit=newgroup
rmgroup:news@niagara.com:niagara.*:doit=rmgroup

# NIAS (Japanese ?)
newgroup:news@cc.nias.ac.jp:nias.*:doit=newgroup
rmgroup:news@cc.nias.ac.jp:nias.*:doit=rmgroup

## NL (Netherlands)
newgroup:nl-authority@a3.xs4all.nl:nl.*:doit=newgroup
newgroup:news@a3.xs4all.nl:nl.*:doit=newgroup
rmgroup:nl-authority@a3.xs4all.nl:nl.*:doit=rmgroup
rmgroup:news@a3.xs4all.nl:nl.*:doit=rmgroup

## NLNET newsgroups (Netherlands ISP)
newgroup:beheer@nl.net:nlnet.*:doit=newgroup
rmgroup:beheer@nl.net:nlnet.*:doit=rmgroup

## NM (New Mexico, USA)
newgroup:news@tesuque.cs.sandia.gov:nm.*:doit=newgroup
rmgroup:news@tesuque.cs.sandia.gov:nm.*:doit=rmgroup

## NO (Norway)
## See also http://www.usenet.no
checkgroups:control@usenet.no:no.*:doit=checkgroups
newgroup:control@usenet.no:no.*:doit=newgroup
newgroup:*@*.no:no.alt.*:doit=newgroup
rmgroup:control@usenet.no:no.*:doit=rmgroup

## NV (Nevada)
newgroup:doctor@netcom.com:nv.*:doit=newgroup
newgroup:cshapiro@netcom.com:nv.*:doit=newgroup
rmgroup:doctor@netcom.com:nv.*:doit=rmgroup
rmgroup:cshapiro@netcom.com:nv.*:doit=rmgroup

## NY (New York State, USA)
newgroup:root@ny.psca.com:ny.*:mail
rmgroup:root@ny.psca.com:ny.*:mail

## NZ (New Zealand)
newgroup:mark@comp.vuw.ac.nz:nz.*:doit=newgroup
newgroup:root@usenet.net.nz:nz.*:doit=newgroup
rmgroup:mark@comp.vuw.ac.nz:nz.*:doit=rmgroup
rmgroup:root@usenet.net.nz:nz.*:doit=rmgroup

## OC newsgroups (?)
newgroup:bob@tsunami.sugarland.unocal.com:oc.*:doit=newgroup
rmgroup:bob@tsunami.sugarland.unocal.com:oc.*:doit=rmgroup

## OH (Ohio, USA)
newgroup:trier@ins.cwru.edu:oh.*:doit=newgroup
rmgroup:trier@ins.cwru.edu:oh.*:doit=rmgroup

## OK (Oklahoma, USA)
newgroup:quentin@*qns.com:ok.*:doit=newgroup
rmgroup:quentin@*qns.com:ok.*:doit=rmgroup

## PA (Pennsylvania, USA)
newgroup:mhw@canal.org:pa.*:doit=newgroup
rmgroup:mhw@canal.org:pa.*:doit=rmgroup

## PGH (Pittsburgh, Pennsylvania, USA)
newgroup:news@telerama.lm.com:pgh.*:doit=newgroup
newgroup:jgm+@cmu.edu:pgh.*:doit=newgroup
rmgroup:news@telerama.lm.com:pgh.*:doit=rmgroup
rmgroup:jgm+@cmu.edu:pgh.*:doit=rmgroup

## PHL (Philadelphia, Pennsylvania, USA)
newgroup:mhw@canal.org:phl.*:doit=newgroup
newgroup:news@vfl.paramax.com:phl.*:doit=newgroup
rmgroup:news@vfl.paramax.com:phl.*:doit=rmgroup
rmgroup:mhw@canal.org:phl.*:doit=rmgroup

## PIN (Personal Internauts' NetNews)
newgroup:pin-admin@forus.or.jp:pin.*:doit=newgroup
rmgroup:pin-admin@forus.or.jp:pin.*:doit=rmgroup

## PL (Poland and Polish language)
## For more info, see http://www.ict.pwr.wroc.pl/doc/news-pl-new-site-faq.html
newgroup:michalj@*fuw.edu.pl:pl.*:doit=newgroup
newgroup:newgroup@usenet.pl:pl.*:doit=newgroup
rmgroup:michalj@*fuw.edu.pl:pl.*:doit=rmgroup
rmgroup:newgroup@usenet.pl:pl.*:doit=rmgroup

## PT (Portugal and Portuguese language)
newgroup:pmelo@*inescc.pt:pt.*:doit=newgroup
rmgroup:pmelo@*inescc.pt:pt.*:doit=rmgroup

## PUBNET 
## This Hierarchy is now defunct.
## See ftp://ftp.uu.net/usenet/control/pubnet/pubnet.config.Z
newgroup:*@*:pubnet.*:drop
rmgroup:*@*:pubnet.*:doit=rmgroup

## RELCOM ( Commonwealth of Independent States)
## The official list of relcom groups is supposed to be available from
## ftp://ftp.kiae.su/relcom/netinfo/telconfs.txt
checkgroups:dmart@*kiae.su:relcom.*:doit=checkgroups
newgroup:dmart@*kiae.su:relcom.*:doit=newgroup
rmgroup:dmart@*kiae.su:relcom.*:doit=rmgroup

## SACHSNET (German)
newgroup:root@lusatia.de:sachsnet.*:doit=newgroup
rmgroup:root@lusatia.de:sachsnet.*:doit=rmgroup

## SAT (San Antonio, Texas, USA)

# *PGP*   See comment at top of file.
newgroup:satgroup@endicor.com:sat.*:verify-satgroup@endicor.com
rmgroup:satgroup@endicor.com:sat.*:verify-satgroup@endicor.com

#newgroup:magnus@empire.texas.net:sat.*:doit=newgroup
#newgroup:satgroup@endicor.com:sat.*:doit=newgroup
#rmgroup:magnus@empire.texas.net:sat.*:doit=rmgroup
#rmgroup:satgroup@endicor.com:sat.*:doit=rmgroup


## SBAY (South Bay/Silicon Valley, California)
newgroup:steveh@grafex.sbay.org:sbay.*:doit=newgroup
newgroup:ikluft@thunder.sbay.org:sbay.*:doit=newgroup
rmgroup:steveh@grafex.sbay.org:sbay.*:mail
rmgroup:ikluft@thunder.sbay.org:sbay.*:mail

## SDNET (Greater San Diego Area, California, USA)
newgroup:news@network.ucsd.edu:sdnet.*:doit=newgroup
newgroup:brian@nothing.ucsd.edu:sdnet.*:doit=newgroup
rmgroup:news@network.ucsd.edu:sdnet.*:doit=rmgroup
rmgroup:brian@nothing.ucsd.edu:sdnet.*:doit=rmgroup

## SEATTLE (Seattle, Washington, USA)
newgroup:billmcc@akita.com:seattle.*:doit=newgroup
newgroup:graham@ee.washington.edu:seattle.*:doit=newgroup
rmgroup:billmcc@akita.com:seattle.*:doit=rmgroup
rmgroup:graham@ee.washington.edu:seattle.*:doit=rmgroup

## SFNET newsgroups (Finland)
newgroup:wirzeniu@cs.helsinki.fi:sfnet.*:doit=newgroup
rmgroup:wirzeniu@cs.helsinki.fi:sfnet.*:doit=rmgroup

## SHAMASH (Jewish)
newgroup:archives@israel.nysernet.org:shamash.*:doit=newgroup
rmgroup:archives@israel.nysernet.org:shamash.*:doit=rmgroup

## SK (Slovakia)
checkgroups:uhlar@ccnews.ke.sanet.sk:sk.*:doit=checkgroups
newgroup:uhlar@ccnews.ke.sanet.sk:sk.*:doit=newgroup
rmgroup:uhlar@ccnews.ke.sanet.sk:sk.*:doit=rmgroup

## SLAC newsgroups (Stanford Linear Accelerator Center)
newgroup:bebo@slacvm.slac.stanford.edu:slac.*:doit=newgroup
rmgroup:bebo@slacvm.slac.stanford.edu:slac.*:doit=rmgroup

## SOLENT (Solent region, England)
newgroup:news@tcp.co.uk:solent.*:doit=newgroup
rmgroup:news@tcp.co.uk:solent.*:doit=rmgroup

## STGT (Stuttgart, Germany)
checkgroups:news@news.uni-stuttgart.de:stgt.*:doit=checkgroups
newgroup:news@news.uni-stuttgart.de:stgt.*:doit=newgroup
rmgroup:news@news.uni-stuttgart.de:stgt.*:doit=rmgroup

## STL (Saint Louis, Missouri, USA)
newgroup:news@icon-stl.net:stl.*:doit=newgroup
rmgroup:news@icon-stl.net:stl.*:doit=rmgroup

## SURFNET (? in the Netherlands)
newgroup:news@info.nic.surfnet.nl:surfnet.*:doit=newgroup
rmgroup:news@info.nic.surfnet.nl:surfnet.*:doit=rmgroup

## SWNET (Sverige, Sweden)
newgroup:ber@sunic.sunet.se:swnet.*:doit=newgroup
rmgroup:ber@sunic.sunet.se:swnet.*:doit=rmgroup

## TNN ( The Network News, Japan )
newgroup:netnews@news.iij.ad.jp:tnn.*:doit=newgroup
newgroup:tnn@iij-mc.co.jp:tnn.*:doit=newgroup
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

#checkgroups:control@usenet.org.uk:uk.*:doit=checkgroups
#newgroup:control@usenet.org.uk:uk.*:doit=newgroup
#rmgroup:control@usenet.org.uk:uk.*:doit=rmgroup


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

## US (United States)
# newgroup:de5@*ornl.gov:us.*:doit=newgroup
# rmgroup:de5@*ornl.gov:us.*:doit=rmgroup

## UT (U. of Toronto)
# newgroup:news@ecf.toronto.edu:ut.*:doit=newgroup
# newgroup:news@ecf.toronto.edu:ut.class.*:log
# rmgroup:news@ecf.toronto.edu:ut.*:doit=rmgroup

## UTA (Finnish)
newgroup:news@news.cc.tut.fi:uta.*:doit=newgroup
rmgroup:news@news.cc.tut.fi:uta.*:doit=rmgroup

## UTEXAS (University of Texas, USA )
newgroup:fletcher@cs.utexas.edu:utexas.*:doit=newgroup
newgroup:fletcher@cs.utexas.edu:utexas*class.*:log
newgroup:news@geraldo.cc.utexas.edu:utexas.*:doit=newgroup
newgroup:news@geraldo.cc.utexas.edu:utexas*class.*:log
rmgroup:news@geraldo.cc.utexas.edu:utexas.*:doit=rmgroup
rmgroup:fletcher@cs.utexas.edu:utexas.*:doit=rmgroup

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

## WPI (Worcester Polytechnic Institute, Worcester, MA)
newgroup:aej@*.wpi.edu:wpi.*:doit=newgroup
rmgroup:aej@*.wpi.edu:wpi.*:doit=rmgroup

## Z-NETZ (German email network.)
newgroup:*@*.de:z-netz.*:mail
newgroup:*@*.de:z-netz.alt.*:doit=newgroup
rmgroup:*@*.de:z-netz.*:mail

## ZA (South Africa)
newgroup:root@duvi.eskom.co.za:za.*:doit=newgroup
newgroup:ccfj@hippo.ru.ac.za:za.*:doit=newgroup
rmgroup:root@duvi.eskom.co.za:za.*:doit=rmgroup
rmgroup:ccfj@hippo.ru.ac.za:za.*:doit=rmgroup


##
## People we'd rather just send over the falls in a barrel
## (aka. The Idiot List) Keep this at the end. 
##

## "Real" People
all:*@*michigan.com:*:drop
all:*@*rabbit.net:*:drop
all:*@*anatolia.org:*:drop
all:djk@*:*:drop
all:*@*tasp*:*:drop
all:*@espuma*:*:drop
all:cosar@*:*:drop
all:*@*caprica.com:*:drop
all:gritton@montana.et.byu.edu:*:drop
all:riley@planet*:*:drop
all:biff@bit.net:*:drop

## "Open" sites
all:*@*infi.net:*:drop
all:*@*.cs.du.edu:*:drop
all:*@*netcom*:alt.*:drop
all:*@*.penet.*:*:drop
all:*@utmb.edu:*:drop
all:*@lbss.fcps*:*:drop
all:*@freenet.carleton.ca:*:drop
all:*@kaiwan.com:*:drop
all:*@ripco.com:*:drop

## The usual aliases
all:*@*heaven*:*:drop
all:*@*hell*:*:drop
all:*@*nowhere*:*:drop
all:noone@*:*:drop
all:nobody@*:*:drop
all:*d00d*@*:*:drop
all:*dude@*:*:drop
all:*warez*@*:*:drop
all:*@*warez*:*:drop
all:hacker@*:*:drop
all:guest@*:*:drop
all:cracker@*:*:drop
all:news@mail:*:drop
