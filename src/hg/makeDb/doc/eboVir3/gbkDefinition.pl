#!/usr/bin/env perl

use warnings;
use strict;

my $argc = scalar(@ARGV);

if ($argc < 1) {
  printf STDERR "usage: gbkDefinition.pl <file.gbk> [others*.gbk]\n";
  printf STDERR "output: accession descr country collectionDate\n";
  exit 255
}

while (my $gbkFile = shift) {
  my $descr = "noDescr";
  my $version = "noVersion";
  my $country = "noCountry";
  my $collectionDate = "noCollDate";
  open (FH, "<$gbkFile") or die "can not read $gbkFile";
#  printf STDERR "# processing: %s\n", $gbkFile;
  my $inDef = 0;
  while (my $line = <FH>) {
    chomp $line;
    if ($line =~m /^DEFINITION/) {
#       printf STDERR "%s", $line;
       $descr = $line;
       $descr =~ s/^DEFINITION\s+//;
       $inDef = 1;
    } elsif ($line =~ m/^[a-z]/i) {
#       printf STDERR "\n" if ($inDef == 1);
       $inDef = 0;
       if ($line =~ m/^VERSION/) {
          $version = $line;
          $version =~ s/^VERSION\s+//;
          $version =~ s/\s+.*//;
       }
    } elsif ($inDef == 1) {
       $line =~ s/^\s+//;
#       printf STDERR " %s", $line;
       $descr .= " " . $line;
    } elsif ($line =~ m#/country=#) {
          $country = $line;
          $country =~ s#/country=\"##;
          $country =~ s#\"##;
          $country =~ s#^\s+##;
    } elsif ($line =~ m#/collection_date=#) {
          $collectionDate = $line;
          $collectionDate =~ s#/collection_date=\"##;
          $collectionDate =~ s#\"##;
          $collectionDate =~ s#^\s+##;
    }
  }
  close (FH);
  printf "%s\t%s\t%s\t%s\n", $version, $descr, $country, $collectionDate;
}
