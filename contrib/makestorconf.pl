#!/usr/local/bin/perl

# Create storage.conf script based on recently read articles.

require '/var/netnews/lib/innshellvars.pl';

$readfile="$inn'pathdb/readgroups";

$outfile="$inn'pathdb/storage.conf";
outloop: 
for ($level=9 ; $level >= 2; --$level) {
  # clear groups hash.
  foreach $i (keys %groups) {
    delete $groups{$i};
  }
  if (open(RDF, "sort $readfile|")) {
    while (<RDF>) {
      chop;
      next if (/^group/);	# bogus 
      @foo=split(/ /);		# foo[0] should be group, foo[1] lastreadtime
      @bar=split(/\./,$foo[0]);
      if ( $level >= scalar @bar) {
	$grf = join(".", @bar);
      } else {
	$grf=join(".", @bar[0..($level-1)])  . ".*";
      }
      $groups{$grf} = 1;
    }
    close(RDF);
  }
  $grlist = join(",",keys(%groups));
  last outloop if (length($grlist) < 2048);
}

open(OUT, ">$outfile") || die "cant open $outfile";
#open(OUT, ">/dev/tty");

print OUT <<"EOF" ;
method cnfs {
	newsgroups: control,control.*
	class: 1
	options: MINI
}

method timecaf {
	newsgroups: $grlist
	class: 1
}

method cnfs {
	newsgroups: *
	options: MONGO
        class: 0
}
EOF
close(OUT);
exit(0);
