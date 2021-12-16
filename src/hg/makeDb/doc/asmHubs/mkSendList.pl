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
  chomp $line;
  my @a = split('\s+', $line);
  my @b = split('_', $a[0]);
  my $GCx = $b[0];
  my $id0 = substr($b[1],0,3);
  my $id1 = substr($b[1],3,3);
  my $id2 = substr($b[1],6,3);
  my $srcPath = sprintf("%s/%s/%s/%s/%s_%s", $GCx, $id0, $id1, $id2, $b[0], $b[1]);
  printf "%s\n", $srcPath;
}
close (FH);

# GCF_902459505.1_aGeoSer1.1

# head vgp.userTracks.tsv
# GCA_003957555.2_bCalAnn1_v1.p   Anna's hummingbird
# GCA_003957565.2_bTaeGut1_v1.p   zebra finch
# GCA_004027225.2_bStrHab1.2.pri  Kakapo
# GCF_004027225.2_bStrHab1.2.pri  Kakapo

