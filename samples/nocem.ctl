##  Configuration file for perl-nocem(8).
##
##  This file lists the NoCeM issuers and types of notices you want to process.
##
##  Based on Rosalind Hengeveld's NoCeM Registry:
##      <https://rosalind.home.xs4all.nl/nocemreg/nocemreg.html>
##
##  Format:
##      issuer:type1[,type2...]
##
##  You also have to properly configure your newsfeeds file and add the PGP
##  public keys of the issuers you trust to your keyring situated in
##  <pathetc>/pgp/ncmring.gpg by default (a different location can be specified
##  to perl-nocem with the -k flag).
##  You are encouraged to review and change the policy below.
##
##  See the perl-nocem manual page for more information.

bleachbot@httrack.com:spam,site
news@uni-berlin.de:Admincancel
nocem@aioe.org:aioe-spam
nocem@arcor.de:spam
nocem@novabbs.com:spam
pgpmoose@killfile.org:pgpmoose-forged-moderation
robot@pasdenom.info:spam,site
