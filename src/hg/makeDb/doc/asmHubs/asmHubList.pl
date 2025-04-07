#!/usr/bin/env perl

#
# asmHubList.pl - can scan the ./GCF and ./GCA directories to find the
#               - hubs that have a hub.txt and thus are functional hubs
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

my $thisMachine = `hostname -s`;
chomp $thisMachine;

if ($thisMachine eq "hgwdev") {
  chdir ("/hive/data/genomes/asmHubs");
} elsif ($thisMachine eq "hgdownload1") {
  chdir ("/mirrordata/hubs");
} elsif ($thisMachine eq "hgdownload2") {
  chdir ("/mirrordata/hubs");
} elsif ($thisMachine eq "hgdownload3") {
  chdir ("/mirrordata/hubs");
} else {
  die "ERROR: not running on hgdownload ?  Good for only hgwdev and hgdownload[123]";
}

my %cladeListings;	# key is assemblyName GCA/GCF identifier, value is clade
# one off until this group gets out here
$cladeListings{"GCF_000007345.1"} = "archaea";

open (FH, "ls /dev/shm/clade.*.list|grep -v legacy|") or die "can not read /dev/shm/clade.*.list";
while (my $cladeList = <FH>) {
  chomp $cladeList;
  my $clade = basename($cladeList);
  $clade =~ s/clade.//;
  $clade =~ s/.list//;
  my $genomeCount = 0;
  open (CL, "<$cladeList") or die "can not read $cladeList\n";
  while (my $genome = <CL>) {
    chomp $genome;
    if (defined($cladeListings{$genome})) {
      printf STDERR "# warning: already defined clade for %s '%s' again '%s'\n", $genome, $cladeListings{$genome}, $clade;
    }
    $cladeListings{$genome} = $clade;
    ++$genomeCount;
  }
  printf STDERR "# clade %s has %d genomes\n", $clade, $genomeCount;
  close (CL);
}
close (FH);

open (FH, "</home/qateam/cronJobs/legacy.clade.txt") or die "can not read legacy.clade.txt";
while (my $line = <FH>) {
  chomp $line;
  my ($genome, $clade) = split('\t', $line);
  my @a = split("_", $genome);
  $genome = sprintf("%s_%s", $a[0], $a[1]);
  if (defined($cladeListings{$genome})) {
    printf STDERR "# warning: already defined clade for %s '%s' again '%s' legacy\n", $genome, $cladeListings{$genome}, $clade;
  } else {
    $cladeListings{$genome} = $clade . "(L)";
  }
}
close (FH);

my $missCount = 0;

open (FH, "find ./GCF ./GCA -mindepth 4 -maxdepth 4 -type d |") or die "can not find ./GCF ./GCA";
while (my $path = <FH>) {
  chomp $path;
  my $hubTxt = "$path/hub.txt";
  if ( ! -s "$hubTxt" ) {
    printf STDERR "# ERROR: can not find hub.txt in\n%s\n", $hubTxt;
  }
  if ( -s "$hubTxt" ) {
    open (GE, "egrep '^description|^scientificName|^htmlPath|^taxId' $hubTxt|tr -d ''|") or die "can not read $hubTxt";
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
    my $clade = "n/a";
    $clade = $cladeListings{$acc} if (defined($cladeListings{$acc}));
    printf "%s\t%s\t%s\t%s\t%s\t%s\n", $acc, $asmName, $sciName, $descr, $taxId, $clade;
  } else {
    ++$missCount;
    printf STDERR "# missing '%s'\n", $hubTxt;
  }
}
close (FH);

if ($missCount > 0) {
  printf STDERR "# failed to find %d genomes in clade listings vs. genome files here\n", $missCount;
  exit 255;
}
