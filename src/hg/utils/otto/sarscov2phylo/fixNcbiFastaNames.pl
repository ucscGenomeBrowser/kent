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
  if ($host) {
    push @components, $host;
  }
  if ($country) {
    push @components, $country;
  }
  if ($isolate) {
    push @components, $isolate;
  }
  if ($year) {
    push @components, $year;
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
  $isoName =~ s@^human/@@;
  $isoName =~ s@/ENV/@/env/@;
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
      my $name = $fName;
      my $year = $mYear;
      if (! $fName) {
        $name = makeName($mHost, $mCountry, $mName, $mYear);
      } else {
        # If fasta name contains host, country, isolate name, and/or year, use those,
        # otherwise take from metadata.
        if ($fName =~ m@^((\w+)/)?(\w+)/[^/]+/(\d+)$@) {
          # Well-formed; use it.
          $name = $fName;
        } else {
          if ($fName =~ m@/(\d\d\d\d)$@) {
            if ($1 && $mYear && $1 ne $mYear) {
              print $STDERR "Year mismatch for $acc: name $name, metadata $mDate";
            }
            $fName =~ s@/(\d\d\d\d)$@@;
            $year = $1;
          }
          if ($fName =~ m@^[A-Z]{3}/@) {
            # Not really well-formed, but at least it starts with a country code so
            # don't mess it up further.
            $name = $fName;
          } else {
            $name = makeName($mHost, $mCountry, $fName, $year);
          }
        }
      }
      print ">$acc |$name\n";
    } else {
      print STDERR "No metadata for $acc\n";
      s/ /\|/;
      print;
    }
  } elsif (/^[A-Z]+$/) {
    print;
  } else {
    if (/^>/) {
      s/ /\|/;
    } else {
      warn "Passing through weird line:\n$_";
    }
    print;
  }
}
