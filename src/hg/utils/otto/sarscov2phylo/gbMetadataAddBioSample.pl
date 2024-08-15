#!/usr/bin/env perl

use warnings;
use strict;
use Date::Parse;


sub usage() {
  print STDERR "usage: $0 biosample.tab [gbMetadata.tab]\n";
  exit 1;
}

# Read in and store distilled BioSample metadata; stream through GenBank metadata
# (from NCBI Virus / NCBI Datasets) and add in collection date and isolate name
# from BioSample when missing from GenBank.  Report any conflicting dates.

my @months = qw(jan feb mar apr may jun jul aug sep oct nov dec);

sub normalizeDate($) {
  # Convert "25-Jan-2020" to 2020-01-25, "19-MAR-2020" to 2020-03-19...
  my ($dateIn) = @_;
  $dateIn =~ s/-00//g;
  if (! $dateIn) {
    return "";
  } elsif ($dateIn =~ /^\d\d\d\d(-\d\d)*$/) {
    return $dateIn;
  } else {
    my ($ss,$mm,$hh,$day,$month,$year,$zone) = strptime($dateIn);
    my $dateOut = "";
    if ($day) {
      $dateOut = sprintf("%04d-%02d-%02d", $year+1900, $month+1, $day);
    } elsif ($month) {
      $dateOut = sprintf("%04d-%02d", $year+1900, $month+1);
    } elsif ($year) {
      $dateOut = printf("%04d", $year+1900);
    }
    return $dateOut;
  }
}

my $biosampleFile = shift @ARGV;

open(my $BIOSAMPLE, "<$biosampleFile") || die "Can't open $biosampleFile: %!\n";

my %b2Name = ();
my %b2Date = ();
my %b2Country = ();
while (<$BIOSAMPLE>) {
  my (undef, $bAcc, $name, $date, undef, undef, $country) = split("\t");
  $b2Name{$bAcc} = $name;
  $b2Date{$bAcc} = $date;
  $b2Country{$bAcc} = $country;
}
close($BIOSAMPLE);

my $missingCount = 0;
my $maxMissing = 1000000;
while (<>) {
  my ($gbAcc, $bAcc, $gbDate, $gbGeo, $host, $gbName, $completeness, $len) = split("\t");
  if ($bAcc) {
    if (exists $b2Name{$bAcc}) {
      my ($bName, $bDate, $bCountry) = ($b2Name{$bAcc}, normalizeDate($b2Date{$bAcc}),
                                        $b2Country{$bAcc});
      if (! $gbDate || length($bDate) > length($gbDate)) {
        $gbDate = $bDate;
      } elsif ($bDate && $gbDate ne $bDate) {
        print STDERR join("\t", "dateMismatch", $gbAcc, $gbName, $gbDate, $bAcc, $bName, $bDate) .
          "\n";
      }
      if (! $gbName) {
        $gbName = $bName;
      } elsif (($gbName eq '1' || $gbName eq 'NA') && length($bName) > length($gbName)) {
        $gbName = $bName;
      } elsif ($gbName eq 'nasopharyngeal' && $bName =~ m/\d/) {
        $gbName = $bName;
      }
      if (! $gbGeo) {
        $gbGeo = $bCountry;
      }
      if ($gbName !~ m@/@ && $gbGeo ne "" && $gbDate =~ /^\d{4}/) {
        my $country = $gbGeo;
        $country =~ s/:.*//;  $country =~ s/ //g;
        my $year = $gbDate;
        $year =~ s/^(\d{4}).*/$1/;
        $gbName = "$country/$gbName/$year";
      }
      print join("\t", $gbAcc, $bAcc, $gbDate, $gbGeo, $host, $gbName, $completeness, $len);
    } else {
      # BioSample file doesn't have info for this BioSample accession
      print STDERR "Missing BioSample info for $bAcc\n";
      $missingCount++;
      if ($missingCount > $maxMissing) {
        die "Too many missing BioSamples (> $maxMissing), quitting.\n";
      }
      # Pass through as-is
      print;
    }
  } else {
    # No associated BioSample, just pass through as-is
    print;
  }
}

