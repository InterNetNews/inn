#!/usr/bin/perl -w

# Parse log files created by innd history profiler
# 2001/01/29 - Fabien Tassin

use strict;
use FileHandle;

my $file = shift || "stathist.log";
if ($file eq '-h' || $file eq '--help') {
  print "Usage: stathist [logfile]\n";
  exit 0;
}

sub parse {
  my $file = shift;

  my $f = new FileHandle $file;
  unless (defined $f) {
    print STDERR "Can't open file: $!\n";
    return {};
  }
  my $data = {};
  my $begin = 1;
  my @stack = ();
  while (defined (my $line = <$f>)) {
    next if $begin && $line !~ / HIS(havearticle|write|setup) begin/;
    $begin = 0;
    chomp $line;
    my @c = split /[\[\]\(\) ]+/, $line;
    ($c[4] eq 'begin') && do {
      push @stack, $c[3];
      my $d = $data;
      for my $l (@stack) {
	unless (defined $$d{$l}) {
	  $$d{$l}{'min'} = 1E10;
	  $$d{$l}{'total'} = $$d{$l}{'count'} = $$d{$l}{'max'} = 0;
	}
	$d = $$d{$l}
      }
    } ||
    ($c[4] eq 'end') && do {
      my $d = $data;
      for my $l (@stack) {
	$d = $$d{$l};
      }
      $$d{'count'}++;
      $$d{'total'} += $c[5];
      $$d{'min'} = $c[5] if $$d{'min'} > $c[5];
      $$d{'max'} = $c[5] if $$d{'max'} < $c[5];
      pop @stack;
    };
  }
  $f->close;
  $data;
}

sub report {
  my $data = shift;
  my $inc = shift;

  unless (defined $inc) {
    printf "%-16s %10s %14s %10s %10s %10s\n\n", "Function", "Invoked",
      "Total(s)", "Min(ms)", "Avg(ms)", "Max(ms)";
    $inc = 0;
  }

  for my $key (sort keys %$data) {
    next unless $key =~ m/^HIS/;
    printf "%-16s %10d %14.6f %10.3f %10.3f %10.3f\n", ('  ' x $inc) . $key,
      $$data{$key}{'count'}, $$data{$key}{'total'}, $$data{$key}{'min'} * 1000,
      $$data{$key}{'total'} / $$data{$key}{'count'} * 1000,
      $$data{$key}{'max'} * 1000;
    &report($$data{$key}, $inc + 1)
  }
}

my $data = &parse($file);
&report($data);
