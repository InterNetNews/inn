#!/usr/local/bin/perl

# format_overview.pl: just adds the leading numbers we need on
# the script.

$counter = 0;

while (<>) {

printf "%s:%s", ++$counter, $_;

}
