#!/bin/sh
jade -t sgml -d /usr/local/share/sgml/docbook/dsssl/modular/html/docbook.dsl $1.sgml
mv x1.html $1.html
jade -t rtf -d /usr/local/share/sgml/docbook/dsssl/modular/print/docbook.dsl $1.sgml
