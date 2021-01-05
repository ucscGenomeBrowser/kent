#!/usr/bin/env perl

use warnings;
use strict;

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
  my $dateOut = "";
  if ($dateIn) {
    if ($dateIn =~ /^\d\d\d\d(-\d\d)?(-\d\d)?$/) {
      $dateOut = $dateIn;
    } elsif ($dateIn =~ /^((\d\d?)-)?(\w\w\w)-(\d\d\d\d)$/) {
      my ($day, $month, $year) = ($2, lc($3), $4);
      my ($mIx) = grep { $months[$_] eq $month } (0 .. @months-1);
      if (! defined $mIx) {
        die "Unrecognized month '$month' in '$dateIn'";
      }
      $month = $mIx + 1;
      if ($day) {
        $dateOut = sprintf("%04d-%02d-%02d", $year, $month, $day);
      } else {
        $dateOut = sprintf("%04d-%02d", $year, $month);
      }
    } else {
      die "Unrecognized date format '$dateIn'";
    }
  }
  return $dateOut;
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
while (<>) {
  my ($gbAcc, $bAcc, $gbDate, $gbGeo, $host, $gbName, $completeness, $len) = split("\t");
  if ($bAcc) {
    if (exists $b2Name{$bAcc}) {
      my ($bName, $bDate, $bCountry) = ($b2Name{$bAcc}, normalizeDate($b2Date{$bAcc}),
                                        $b2Country{$bAcc});
      if (! $gbDate) {
        $gbDate = $bDate;
      } elsif ($bDate && $gbDate ne $bDate) {
        print STDERR "CONFLICT: Genbank date ($gbAcc $gbName) = $gbDate, " .
          "BioSample date ($bAcc $bName) = $bDate\n";
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
      print join("\t", $gbAcc, $bAcc, $gbDate, $gbGeo, $host, $gbName, $completeness, $len);
    } else {
      # BioSample file doesn't have info for this BioSample accession
      print STDERR "Missing BioSample info for $bAcc\n";
      $missingCount++;
      if ($missingCount >= 1000) {
        die "Too many missing BioSamples, quitting.\n";
      }
      # Pass through as-is
      print;
    }
  } else {
    # No associated BioSample, just pass through as-is
    print;
  }
}

