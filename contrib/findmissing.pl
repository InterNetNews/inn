#!/usr/local/bin/perl

#!/usr/local/bin/perl5

# findmissing
# scan news spool, and report articles that are not in the history file.

# Written by Harry Bochner, 3/95
# This program may be distributed freely, but please annotate any changes,
# and please leave these comments in place.
# - change verbose messages to go to stderr:  David Barr, 23 Feb 1996

# Adjust the next two lines to the local configuration.
$history = '/usr/local/lib/news/history';
$spool= '/var/spool/news';

# Give -v flag for verbose progress report.

$verbose = 0;
$verbose = shift if $ARGV[0] eq "-v";

chdir($spool) || die "$0: $spool: $!";

# scan the news spool recursively, looking for articles; build the internal
# bit string representation that will be checked by check_history.
#
# NB: This routine assumes that directories never have all-numeric names.
# If you want it to understand alt.2600, you'll have to treat that
# as a special case.

sub scan {
  local($group, $path, $depth) = @_;
  local($fn, @dirs, $min, $max, @nums, $size, $str, $dir);

  $0 = "scanning $group" if $depth <= 2;

  $dir = $path || ".";
  unless (opendir(DIR, $dir)) {
    warn("$0: $dir: $!");
    return;
  }
  print STDERR "scanning $path\n" if $verbose;

  $min = $max = 0;
  while ($fn = readdir(DIR)) {
    next if $fn =~ /\./ || $fn eq "lost+found";
    push(@dirs, $fn), next
      unless $fn =~ /^\d+$/;

    $min = $fn if $min == 0 || $min > $fn;
    $max = $fn if $fn > $max;
    push(@nums, $fn);
  }
  closedir(DIR);

  if ($min) {
    $min{$group} = $min;
    $size = ($max-$min+7) / 8;
    $str = "\0" x $size;
    foreach (@nums) {
      vec($str, $_-$min, 1) = 1;
    }
    $arts{$group} = $str;
  }

  $group .= "." if $group;
  $path  .= "/" if $path;
  $depth++;
  foreach $fn (@dirs) {
    &scan("$group$fn", "$path$fn", $depth);
  }
}

# Scan the history file, and clear the bit flags for all articles found there.

sub check_history {
  local($arts, @arts, $group, $num);

  print STDERR "scanning history\n"
    if $verbose;

  open(IN, "<$history") || die "$0: $history: $!";
  while (<IN>) {
    @arts = split;
    shift @arts; shift @arts;	# skip first two fields
    foreach (@arts) {
      ($group, $num) = split(m,/,);
      next unless $num;		# sanity check
      $arts++;
      # clear that bit
      vec($arts{$group}, $num-$min{$group}, 1) = 0
	if defined $min{$group} && $num >= $min{$group};
    }
    print STDERR "$. lines, $arts articles so far\n" if $verbose && $. % 50000 == 0;
    $0 = "history line $." if $. % 50000 == 0;
  }
  close(IN);

  print STDERR "done reading history, $arts articles\n" if $verbose;
}

&scan("", "", 0);

&check_history;

# now check all the bitstrings, and if there are any flags still set,
# report that article.

print STDERR "checking for missing articles\n" if $verbose;
foreach $group (sort keys %arts) {
  $min = $min{$group};
  $str = unpack("b*", $arts{$group});
  delete $arts{$group}; delete $min{$group};	# recover memory?
  next unless $str =~ /1/;

  $path = "";
  for (split(/x*/, $str)) {
    if ($_) {
      unless ($path) {
	$path = $group; $path =~ s,\.,/,g;
	$0 = "findmissing $group";
      }
      print "$path/$min\n";
    }
    $min++;
  }
}
