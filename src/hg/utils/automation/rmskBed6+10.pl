#!/usr/bin/env perl

# convert RepeatMasker entries to a bed6+10 tab separated file
# for use as a bigBed file type

use strict;
use warnings;

my $argc = scalar(@ARGV);
if ($argc < 1) {
  printf STDERR "usage: rmskBed6+10.pl rmskClass/file.tab > <db>.class.tab\n";
  printf STDERR "converts RepeatMasker entries to a bed6+10 tab separated file\n";
  printf STDERR "for use as a bigBed file type\n";
  exit 255;
}

while (my $file = shift) {

open (FH, "<$file") or die "can not read $file";
while (my $line = <FH>) {
  next if ($line =~ m/^\s+SW\s+perc.*/);
  next if ($line =~ m/^score\s+div.*/);
  next if ($line =~ m/^$/);
  chomp $line;
  my @a = split('\t', $line);
  shift @a;
  die "ERROR: not finding 16 fields in a line" if (scalar(@a) != 16);
  printf "%s\t%d\t%d\t%s\t0\t%s\t%d\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n",
    $a[4], $a[5], $a[6], $a[9], $a[8], $a[0],
    $a[1], $a[2], $a[3], $a[7], $a[10], $a[11], $a[12], $a[13], $a[14];
}
close (FH);

}
