#!/usr/local/bin/perl
#
# count_overview.pl:  Count the groups in a bunch of xref records.

while (<>) {

chop;
@xreflist = split(/\t/); # split apart record

$_ = $xreflist[$#xreflist];  # xref is last.

@xreflist = reverse(split(/ /));  #break part xref line.

pop @xreflist;  # get rid xref header
pop @xreflist;

while ($current = pop @xreflist) {
	($current) = split(/:/,$current);  #get newsgroup name
	$groups{$current}++;  #tally
}

}

# display accumulated groups and counts.
foreach $current (sort keys %groups) {
	printf "%-50s\t%5d\n", $current, $groups{$current};
}
