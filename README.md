# INN: InterNetNews

INN (InterNetNews) is a very full-featured and flexible Usenet news
server package.

`innd`, the main daemon, is a continuously running process that
receives articles from the network, files them, and records what
remote hosts should receive them. Readers can access articles directly
from the disk in the same manner as B News and C News, but an included
program, called `nnrpd`, also serves newsreaders that employ NNTP.

INN is currently maintained by Russ Allbery and Julien ÉLIE.

## Requirements

INN is written in ANSI C, so it requires a C compiler and a modern
operating system that supports ANSI C. gcc will work fine. It also
requires Perl, at least version 5.8.0, since many supporting utilities
and parts of the build system are written in Perl. To build INN, you
also need a reasonably decent make program that supports include; GNU
make will do quite well.

For support for authenticated control messages, you need either PGP or
GnuPG, the latter being recommended. For embedded filtering and
authentication hook support, you need either Perl 5.8 or later, Python
2.3.0 or later, or Python 3.3.0 or later, depending on what language
you want to write your filters and authentication hooks in.

For the ovdb overview storage method, you'll need Berkeley DB 4.4 or
later (4.7 or later recommended). If you have zlib available, you can
also compress overview before it's stored into ovdb. For support for
news reading over SSL, you'll need OpenSSL. To support SASL
authentication to nnrpd or to feed newsgroups to an IMAP server with
SASL authentication, you'll need the Cyrus SASL libraries. INN can
also check passwords against a Kerberos KDC; for this, you will need
Kerberos libraries.

## Learning More

If you are interested in receiving notice of future releases of INN,
you can
[subscribe to the inn-announce mailing list](https://lists.isc.org/mailman/listinfo/inn-announce).

[Russ Allbery’s INN site](https://www.eyrie.org/~eagle/software/inn/)
also has information on the software.

You can
[join the inn-workers mailing list](https://lists.isc.org/mailman/listinfo/inn-workers)
if you would like to follow the progress of INN development.

## History

INN was originally written by Rich Salz (read the USENIX paper Rich
wrote about it
[here](https://downloads.isc.org/isc/inn/extra-docs/innusenix.pdf)).

ISC took over development of INN in 1996 after Rich was unable to
continue supporting it and many variants of the software were forming.

INN is under active development as of 2021. The package is maintained
by volunteers, and development is hosted by the Internet Systems
Consortium.
