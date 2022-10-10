#!/usr/bin/env perl

use warnings;
use strict;

sub usage() {
  print STDERR "usage: $0 ncbi_dataset.plusBioSample.tsv [fasta]\n";
  exit 1;
}

# Read in metadata for GenBank virus sequences, then stream through fasta; if header already
# has a well-formed country/isolate/year name after the accession then keep that, otherwise
# add from metadata.

sub makeName($$$$) {
  my ($host, $country, $isolate, $year) = @_;
  my @components = ();
  if ($host && $isolate !~ m@^$host/@) {
    push @components, $host;
  }
  if ($isolate =~ m@^([A-Za-z ]+/)?[A-Za-z]+/.*/\d+$@) {
    push @components, $isolate;
  } else {
    if ($country) {
      push @components, $country;
    }
    if ($isolate) {
      push @components, $isolate;
    }
    if ($year) {
      push @components, $year;
    }
  }
  return join('/', @components);
}

# Replace non-human host scientific names with common names
my %sciToCommon = ( 'Canis lupus familiaris' => 'canine',
                    'Felis catus' => 'cat',
                    'Mustela lutreola' => 'mink', # Netherlands
                    'Neovison vison' => 'mink',   # Denmark
                    'Panthera leo' => 'lion',
                    'Panthera tigris' => 'tiger',
                    'Panthera tigris jacksoni' => 'tiger'
                  );

my $gbMetadataFile = shift @ARGV;

open(my $GBMETA, "<$gbMetadataFile") || die "Can't open $gbMetadataFile: $!\n";

my %accToMeta = ();
while (<$GBMETA>) {
  my ($acc, undef, $date, $geoLoc, $host, $isoName) = split("\t");
  # Trim to just the country
  $geoLoc =~ s/United Kingdom:(.*)/$1/;
  $geoLoc =~ s/:.*//;
  $geoLoc =~ s/ //g;
  if ($host eq "Homo sapiens") {
    $host = '';
  } elsif (exists $sciToCommon{$host}) {
    $host = $sciToCommon{$host};
  }
  $isoName =~ s@^SARS?[- ]Co[Vv]-?2/@@;
  $isoName =~ s@^hCo[Vv]-19/@@;
  $isoName =~ s@^BetaCoV/@@;
  $isoName =~ s@^[Hh]umans?,?/@@;
  $isoName =~ s@/ENV/@/env/@;
  $isoName =~ s@Canis lupus familiaris@canine@;
  $isoName =~ s@Felis catus@cat@;
  $isoName =~ s@Mustela lutreola@mink@;
  $isoName =~ s@Neovison vison@mink@;
  $isoName =~ s@Panthera leo@lion@;
  $isoName =~ s@Panthera tigris@tiger@;
  $isoName =~ s@Panthera tigris jacksoni@tiger@;
  $isoName =~ s@^COG-UK/@@;
  $accToMeta{$acc} = [$date, $geoLoc, $host, $isoName];
}
close($GBMETA);

while (<>) {
  if (/^>([A-Z]+\d+\.\d+)\s*(\S+.*)?\s*$/) {
    my ($acc, $fName) = ($1, $2);
    if (exists $accToMeta{$acc}) {
      my ($mDate, $mCountry, $mHost, $mName) = @{$accToMeta{$acc}};
      my $mYear;
      if ($mDate =~ /^(\d\d\d\d)/) {
        $mYear = $1;
      }
      my $name = makeName($mHost, $mCountry, $mName, $mYear);
      print ">$acc |$name\n";
    } else {
      print STDERR "No metadata for $acc\n";
      s/ / \|/;
      print;
    }
  } elsif (/^[A-Z]+$/) {
    print;
  } else {
    if (/^>/) {
      s/ / \|/;
    } else {
      warn "Passing through weird line:\n$_";
    }
    print;
  }
}
