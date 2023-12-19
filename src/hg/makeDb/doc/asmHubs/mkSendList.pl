#!/usr/bin/env perl

use strict;
use warnings;

my $argc = scalar(@ARGV);
if ($argc != 1) {
  printf STDERR "usage: ./mkSendList.pl something.asmId.commonName.tsv\n";
  exit 255;
}

my $asmIdFile = shift;
open (FH, "<$asmIdFile") or die "can not read $asmIdFile";
while (my $line = <FH>) {
  next if ($line =~ m/^#/);
  chomp $line;
  my @a = split('\s+', $line);
  my @b = split('_', $a[0]);
  my $GCx = $b[0];
  my $d0 = "x";
  my $d1 = "x";
  my $d2 = "x";
  my $srcPath = "x";
  if (defined($b[1])) {
    $d0 = substr($b[1],0,3);
    $d1 = substr($b[1],3,3);
    $d2 = substr($b[1],6,3);
    $srcPath = sprintf("%s/%s/%s/%s/%s_%s", $GCx, $d0, $d1, $d2, $b[0], $b[1]);
  } else {
    $srcPath = sprintf("%s/%s/%s/%s/%s", $GCx, $d0, $d1, $d2, $b[0]);
  }
  printf "%s\n", $srcPath;
}
close (FH);

# GCF_902459505.1_aGeoSer1.1

# head vgp.userTracks.tsv
# GCA_003957555.2_bCalAnn1_v1.p   Anna's hummingbird
# GCA_003957565.2_bTaeGut1_v1.p   zebra finch
# GCA_004027225.2_bStrHab1.2.pri  Kakapo
# GCF_004027225.2_bStrHab1.2.pri  Kakapo

