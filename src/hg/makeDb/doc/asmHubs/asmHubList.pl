#!/usr/bin/env perl

#
# asmHubList.pl - can scan the ./GCF and ./GCA directories to find the
#               - hubs that have a genomes.txt and thus are functional hubs
#  works on hgwdev and on hgdownload

use strict;
use warnings;
use File::Basename;

my $argc = scalar(@ARGV);
if ($argc != 1) {
  printf STDERR "usage: asmHubList.pl goForIt | sort > assembly.hub.list.today\n";
  printf STDERR "will scan the ./GCF and ./GCA hierarchies to find ready to\n";
  printf STDERR "go assembly hubs on hgwdev or on hgdownload.\n";
  exit 255;
}

my $goForIt = shift;
die "ERROR: need to have argument: goForIt in order to operate." if ($goForIt ne "goForIt");

my $thisMachine = `uname -n`;
chomp $thisMachine;

if ($thisMachine eq "hgwdev") {
  chdir ("/hive/data/genomes/asmHubs");
} else {
  die "ERROR: not running on hgdownload ?  Good for only hgwdev and hgdownload" if ( ! -d "/mirrordata/hubs" );
  chdir ("/mirrordata/hubs");
}

open (FH, "find ./GCF ./GCA -mindepth 4 -maxdepth 4 -type d |") or die "can not find ./GCF ./GCA";
while (my $path = <FH>) {
  chomp $path;
  my $genomes = "$path/genomes.txt";
  if ( ! -s "$genomes" ) {
    $genomes = "$path/hub.txt";
  }
  if ( -s "$genomes" ) {
    open (GE, "egrep '^description|^scientificName|^htmlPath|^taxId' $genomes|tr -d ''|") or die "can not read $genomes";;
    my $descr = "";
    my $sciName = "";
    my $asmId = "";
    my $taxId = "";
    my $asmName = "";
    while (my $line = <GE>) {
      chomp $line;
      my ($tag, $value) = split('\s+', $line, 2);
      if ($line =~ m/^description/) {
        $descr = $value;
      } elsif ($line =~ m/^scientificName/) {
        $sciName = $value;
      } elsif ($line =~ m/^taxId/) {
        $taxId = $value;
      } elsif ($line =~ m/^htmlPath/) {
        $asmId = $value;
        $asmId=~ s/.description.html//;
        $asmId=~ s#html/##;
        (undef, undef, $asmName) = split('_', $asmId, 3);
      }
    }
    close (GE);
    my $acc = basename($path);
    printf "%s\t%s\t%s\t%s\t%s\n", $acc, $asmName, $sciName, $descr, $taxId;
  }
}
close (FH);

