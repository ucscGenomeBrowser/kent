#!/usr/bin/env perl

############################################################################
### generate the hubAndGenome.txt file and the trackDb.txt from
### an existing GenArk assembly.  The trackDb.txt text file should
### have previously been checked into the source tree as, for example:
###    rat/rn8/genark.trackDb.ra
### And when this script makes this file, it should be compared to the
### source tree copy to see if it has been changing.
### This process also expects the dbDb table entry to exist for this
###   stated assembly 'db' name
############################################################################

use strict;
use warnings;

my $argc = scalar(@ARGV);
if ($argc != 3) {
  printf STDERR "usage: ./hubTxtFromGenArk.pl <db> <GCx_...> <genark.trackDb.txt> > hubAndGenome.txt\n";
  printf STDERR "   where db is the curated hub UCSC name (and dbDb table entry)\n";
  printf STDERR "   where the given GCx_ identifier is the accession name of the GenArk hub.\n";
  printf STDERR "   where genark.trackDb.txt is a file to write the hub track definitions.\n";
  printf STDERR "e.g. ./hubTxtFromGenArk.pl rn8 GCF_036323735.1 rn8.genark.trackDb.txt > hubAndGenome.txt\n";
  exit 255;
}

sub addPath($$) {
  my ($oneLine, $gbdbPath) = @_;
  my ($tag, $shortUrl) = split(/\s/, $oneLine, 2);
  my $ret = sprintf("%s %s/%s", $tag, $gbdbPath, $shortUrl);
  return $ret;
}

#############################################################################
### begin main()
#############################################################################
my $db = shift;
my $accession = shift;
my $trackDbOut = shift;
my $gcX = substr($accession, 0, 3);
my $d0 = substr($accession, 4, 3);
my $d1 = substr($accession, 7, 3);
my $d2 = substr($accession, 10, 3);
my $gbDbPath = "/gbdb/genark/${gcX}/${d0}/${d1}/${d2}/$accession";
if ( ! -d  "${gbDbPath}" ) {
  printf "ERROR: can not find directory:\n%s\n", $gbDbPath;
  exit 255;
}

my $hubTxt = "${gbDbPath}/hub.txt";
if ( ! -s "${hubTxt}" ) {
  printf STDERR "ERROR: can not find hub.txt:\n%s\n", $hubTxt;
  exit 255;
}

open (my $tdb, ">", $trackDbOut) or die "can not write to $trackDbOut";

my $stanza = "";
my $stanzaLine = 0;
my $firstTrack = 1;

my ($hubShortLabel, $hubLongLabel) = split(/\t/, `hgsql -N -e 'SELECT description, sourceName FROM dbDb WHERE name="${db}";' hgcentraltest`);
chomp $hubShortLabel;
chomp $hubLongLabel;

open (my $fh, "<", ${hubTxt}) or die "can not read $hubTxt";
while (my $line = <$fh>) {
  chomp $line;
  if (length($line) < 1) {
     next;
  }
  $line =~ s/^\s+//;
  if ($line =~ m/^hub\s/) {
    $stanza = "hub";
    $stanzaLine = 0;
  } elsif ($line =~ m/^genome\s/) {
    $stanza = "genome";
    $stanzaLine = 0;
  } elsif ($line =~ m/^track\s/) {
    $stanza = "track";
    $stanzaLine = 0;
  } elsif ($line =~ m/^include\s/) {
    $stanza = "include";
    $stanzaLine = 0;
  }
  if ($stanza eq "hub") {
    if ($stanzaLine < 1) {
      printf "hub %s genome assembly\n", $db;
      printf "shortLabel %s\n", $hubShortLabel;
      printf "longLabel %s\n", $hubLongLabel;
      printf "useOneFile on\n";
    }
    if ($line =~ m/^email\s/) {
      printf "email genome-www\@soe.ucsc.edu\n";
    } elsif ($line =~ m/^descriptionUrl\s/) {
      printf "%s\n", addPath($line, $gbDbPath);
    }
    ++$stanzaLine;
  } elsif ($stanza eq "genome") {
    printf "\n" if ($stanzaLine < 1);
    ++$stanzaLine;
    if ($line =~ m/^genome\s/) {
       printf "genome %s\n", $db;
    } elsif ($line =~ m/^groups\s|^twoBitPath\s|^twoBitBptUrl\s|^chromSizes\s|^chromAliasBb\s|^htmlPath\s|^liftOver/) {
      printf "%s\n", addPath($line, $gbDbPath);
    } else {
      printf "%s\n", $line;
    }
  } elsif ($stanza eq "track") {
    if ($firstTrack) {
      $firstTrack = 0;
    } else {
      printf $tdb "\n" if ($stanzaLine < 1);
    }
    ++$stanzaLine;
    if ($line =~ m/^html\s|^bigDataUrl\s|^linkDataUrl\s|^searchTrix\s|^summary\s|^xrefDataUrl\s/) {
      printf $tdb "%s\n", addPath($line, $gbDbPath);
    } else {
      printf $tdb "%s\n", $line;
    }
  }
}
close ($fh);
printf "\n";
close ($tdb);

__END__

  hgsql -e 'select * from dbDb where name="rn8"\G' hgcentraltest

*************************** 1. row ***************************
          name: rn8
   description: Jan. 2024 (GRCr8/rn8)
       nibPath: hub:/gbdb/rn8/hubs
      organism: Rat
    defaultPos: NC_086019.1:90172726-90182726
        active: 1
      orderKey: 18017
        genome: Rat
scientificName: Rattus norvegicus
      htmlPath: /gbdb/genark/GCF/036/323/735/GCF_036323735.1/html/GCF_036323735.1_GRCr8.description.html
      hgNearOk: 0
        hgPbOk: 0
    sourceName: BN/NHsdMcwi 2024 refseq (GCF_036323735.1)
         taxId: 10116

