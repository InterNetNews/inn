#!/usr/local/bin/perl

use vars qw($opt_l $opt_h);
use Getopt::Long;
use strict;

my($conffile) = "/usr/local/news/etc/cycbuff.conf";
my($storagectl) = "/usr/local/news/etc/storage.ctl";

my(@line, %class, %buff, %stor, $c, @buffers);

my($gr, $cl, $min, $max, @storsort, $oclass, $header_printed);

GetOptions("-c=s", \$oclass, "-h", "-l:i");
if($opt_h) {
  print STDOUT "Summary tool for CNFS\n";
  print STDOUT "\nUsage:\n\t$0 [-c CLASS] [-l [seconds]]\n";
  print STDOUT "\n\tIf called without args, does a one-time status of all CNFS buffers\n";
  print STDOUT "\t-c <CLASS>: prints out status of CNFS buffers in class CLASS\n";
  print STDOUT "\t-l seconds: loops like vmstat, default seconds = 600\n";
  print STDOUT "\t-h:         This information\n";
  exit(1);
}

my($sleeptime) = (defined($opt_l) && $opt_l > 0) ? $opt_l : 600;

if( ! open(CONFFILE, "< $conffile") ) {
  print STDERR "Cannot open CycBuff Conffile $conffile ...\n";
  exit(1);
}

while(<CONFFILE>) {
  $_ =~ s/^\s*(.*?)\s*$/$1/;
  next if($_ =~ /^\s*$/ || $_ =~ /^#/);

  if($_ =~ /^metacycbuff:/) {
    @line = split(/:/, $_);
    if($class{$line[1]}) {
      print STDERR "Class $line[1] more than one time in CycBuff Conffile $conffile ...\n";
      exit(1);
    }
    $class{$line[1]} = $line[2];
    next;
  }

  if($_ =~ /^cycbuff/) {
    @line = split(/:/, $_);
    if($buff{$line[1]}) {
      print STDERR "Buff $line[1] more than one time in CycBuff Conffile $conffile ...\n";
      exit(1);
    }
    $buff{$line[1]} = $line[2];
    next;
  }

  print STDERR "Unknown config line \"$_\" in CycBuff Conffile $conffile ...\n";
}

close(CONFFILE);

if(! open(STOR, "< $storagectl") ) {
  print STDERR "Cannot open Storage Controlfile $storagectl ...\n";
  exit(1);
}
while(<STOR>) {
  $_ =~ s/^\s*(.*?)\s*$/$1/;
  next if($_ =~ /^\s*$/ || $_ =~ /^#/);

  if($_ =~ /^cnfs:/) {
    @line = split(/:/, $_);
    if($#line != 5) {
      print STDERR "Wrong Storage Control Line \"$_\" in $storagectl ...\n";
      exit(1);
    }

    if($stor{$line[5]}) {
      print STDERR "CNFS Storage Line \"$_\" more than one time in $storagectl ...\n";
      exit(1);
    }
    $stor{$line[5]} = join(":", @line[1 .. 4]);
    push(@storsort, $line[5]);
  }
}
close(STOR);

#foreach $c (keys(%class)) {
#  print "Class: $c, definition: $class{$c}\n";
#}
#foreach $c (keys(%buff)) {
#  print "Buff: $c, definition: $buff{$c}\n";
#}
#exit(0);

START:

if($oclass) {
  if($class{$oclass}) {
    if(!$header_printed) {
      print STDOUT "Class $oclass";
      ($gr, $cl, $min, $max) = split(/:/, $stor{$oclass});
      print STDOUT " for groups matching \"$gr\"";
      if($min || $max) {
        print STDOUT ", article size min/max: $min/$max";
      }
      print STDOUT "\n";
      $header_printed = 1;
    }

    @buffers = split(/,/, $class{$oclass});
    if(! @buffers) {
      print STDERR "No buffers in Class $main::ARGV[0] ...\n";
      next;
    }
  
    foreach $b (@buffers) {
      if(! $buff{$b} ) {
        print STDERR "No buffer definition for buffer $b ...\n";
        next;
      }
      &print_cycbuff_head($buff{$b});
    }
  } else {
    print STDERR "Class $ARGV[1] not found ...\n";
  }
} else { # Print all Classes

  foreach $c (@storsort) {
    print STDOUT "Class $c";
    ($gr, $cl, $min, $max) = split(/:/, $stor{$c});
    print STDOUT " for groups matching \"$gr\"";
    if($min || $max) {
      print STDOUT ", article size min/max: $min/$max";
    }
    print STDOUT "\n";
    @buffers = split(/,/, $class{$c});
    if(! @buffers) {
      print STDERR "No buffers in Class $c ...\n";
      next;
    }

    foreach $b (@buffers) {
      if(! $buff{$b} ) {
        print STDERR "No buffer definition for buffer $b ...\n";
        next;
      }
      &print_cycbuff_head($buff{$b});
    }
    print STDOUT "\n";
  }
}

if(defined($opt_l)) {
  sleep($sleeptime);
  print STDOUT "$sleeptime seconds later:\n";
  goto START;
}




sub print_cycbuff_head {
  my($buffpath) = $_[0];

  my($CNFSMASIZ)=8;
  my($CNFSNASIZ)=16;
  my($CNFSPASIZ)=64;
  my($CNFSLASIZ)=16;
  my($headerlength) = $CNFSMASIZ + $CNFSNASIZ + $CNFSPASIZ + (4 * $CNFSLASIZ);

  my($buff, @entries, $e);
  my($magic, $name, $path, $lena, $freea, $updatea, $cyclenuma);

  if(! open(BUFF, "< $buffpath") ) {
    print STDERR "Cannot open Cycbuff $buffpath ...\n";
    exit(1);
  }

  if(! read(BUFF, $buff, $headerlength) ) {
    print STDERR "Cannot read $headerlength bytes from file $buffpath...\n";
    exit(1);
  }

  ($magic, $name, $path, $lena, $freea, $updatea, $cyclenuma)  = unpack("a8 a16 a64 a16 a16 a16 a16", $buff);

  if(!$magic) {
    print STDERR "Error while unpacking header ...\n";
    exit(1);
  }

  my($len) = hex($lena);
  my($free) = hex($freea);
  my($update) = hex($updatea);
  my($cyclenum) = hex($cyclenuma) - 1;
  
  #print STDOUT "Magic: .. $magic\n";
  #print STDOUT "Name: ... $name\n";
  #print STDOUT "Path: ... $path\n";
  #print STDOUT "Len: .... $len ($lena)\n";
  #print STDOUT "Free: ... $free (";
  #print STDOUT printf("%.2f %%)\n",$free / ($len / 100));
  #print STDOUT "Last updated .. " . localtime($update) . "\n";
  #print STDOUT "Cyclenum  $cyclenum ($cyclenuma)\n";
  
  print STDOUT " Buffer $name, len: ";
  print STDOUT $len / (1024 * 1024);
  print STDOUT " Mbytes, used: ";
  printf("%.2f Mbytes", $free / (1024 * 1024));
  printf(" (%.2f %%)",$free / ($len / 100));
  print STDOUT ", last updated: " . localtime($update) . ", cycled $cyclenum times\n";

  close(BUFF);

}

