#!/bin/sh
echo updating doc/inn.conf.5
./autoinsert.pl -replace doc/inn.conf.5
echo updating lib/getconfig.c
./autoinsert.pl -replace lib/getconfig.c
echo updating samples/inn.conf
./autoinsert.pl -replace samples/inn.conf
