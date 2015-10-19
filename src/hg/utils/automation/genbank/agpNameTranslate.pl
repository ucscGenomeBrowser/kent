#!/usr/bin/env perl

use strict;
use warnings;

my $argc = scalar(@ARGV);

if ($argc != 2) {
  printf STDERR "usage: agpNameTranslate.pl [ucscToNcbi.nameTranslate.txt] [ncbi.agp] > ucsc.agp\n";
  exit 255;
}

my $nameTranslateFile = shift;
my $agpFile = shift;

my %oldToNew;  # key is old name, value is new name

open (FH, "<$nameTranslateFile") or die "ERROR: agpNameTranslate.pl: can not read translation file $nameTranslateFile";
while (my $line = <FH>) {
  chomp $line;
  next if ($line =~ m/^#/);
  my ($newName, $oldName) = split('\s+', $line);
  $oldToNew{$oldName} = $newName;
}
close (FH);

open (FH, "<$agpFile") or die "ERROR: agpNameTranslate.pl: can not read agp file $agpFile";
while (my $line = <FH>) {
 if ($line =~ m/^#/) {
   print $line;
   next;
 }
 chomp $line;
 my ($oldName, $rest) = split('\s+', $line, 2);
 my $newName = $oldToNew{$oldName};
 if (! defined $newName) {
   die "can not find $oldName in $nameTranslateFile";
 }
 printf "%s\t%s\n", $newName, $rest;
}
close (FH);
