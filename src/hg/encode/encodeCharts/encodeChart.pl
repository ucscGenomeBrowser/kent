#!/usr/bin/env perl

use strict;
use warnings;

my $reportFile = $ARGV[0] || usage();
my $keyDateFile = $ARGV[1] || 'NULL';

my $endDate = "2011-12-31";

# Hashes to hold the counts
my %submitHash;
my %releaseHash;

open(FILE, "< $reportFile") || die "Error: Can't open file '$reportFile'\n";
while(my $line = <FILE>) {
  next if ($line =~ /^Project/); # Skip first line

  chomp($line);
  my @split = split(/\t/, $line);

  # Submit and end dates in fields 6 and 7
  my $start = $split[6];
  my $end = $split[7];

  my $endFlag = 0;
  if ($end eq "none" || ! $end) {
    # Handle cases where end date is empty or "none"
    $end = $endDate;
    $endFlag = 1;
  }

  # Convert and clean up dates
  $start = stripDateDashes($start);
  $end = stripDateDashes($end);

  $start = convert_2M2D2Y_Date($start);
  $end = convert_2M2D2Y_Date($end);

  if ($start > $end) {
    print STDERR "Invalid dates on: $line\n";
    next;
  }

  # Accumulate dates in hashes
  $submitHash{$start}++;
  $releaseHash{$end}++ if (! $endFlag);
}
close(FILE);

# Read in important dates and store annotation in hash
my %keyDateHash;
if ($keyDateFile ne 'NULL') {
  open(FILE, "< $keyDateFile") || die "Error: Can't open file '$keyDateFile'\n";   
  while(my $line = <FILE>) { 
    next if ($line =~ /^\#/);
    next if ($line =~ /^\s*$/);
    chomp($line);

    my ($date, $text) = split(/\t/, $line);
    $keyDateHash{$date} = $text;
  }
  close(FILE);
}

# Determine all the dates
my @submitKeys = keys %submitHash;
my @releaseKeys = keys %releaseHash;
my @keyDateKeys = keys %keyDateHash;

my %union;
foreach my $key (@submitKeys) {
  $union{$key} = 1;
}
foreach my $key (@releaseKeys) {
  $union{$key} = 1;
}
foreach my $key (@keyDateKeys) {
  $union{$key} = 1;
}

# Sort the dates
my @dateKeys = sort {$a <=> $b} keys %union;

my $submitSum = 0;
my $releaseSum = 0;

# Loop through all the dates and print results
foreach my $key (@dateKeys) {
  my $year = substr($key, 0, 4);
  my $month = substr($key, 4, 2);
  my $day = substr($key, 6, 2);

  my $date = "$month/$day/$year";

  # Calculate cumulative
  if (defined $submitHash{$key}) {
    $submitSum += $submitHash{$key};
  }
  if (defined $releaseHash{$key}) {
    $releaseSum += $releaseHash{$key};
  }

  my $annotText = "";
  if (defined $keyDateHash{$key}) {
    $annotText = $keyDateHash{$key};
  }

  print "$date\t$releaseSum\t$submitSum\t$annotText\n";
}

exit(0);

# Convert YYYY-MM-DD to YYYYMMDD
sub stripDateDashes {
  my $date = shift;

  if ($date =~ /^\d{4}\-\d{2}\-\d{2}$/) {
    $date =~ s/\-//g;
  }

  return $date;
}

# Convert MM/DD/YY to YYYYMMDD
sub convert_2M2D2Y_Date {
  my $date = shift;

  if ($date =~ /^(\d{2})\/(\d{2})\/(\d{2})$/) {
    my ($month, $day, $year) = ($1, $2, $3);
    $year += 2000;

    $date = "$year$month$day";
  }

  return $date;
}

# Usage
sub usage {
  print "encodeChart.pl report_file\n";
  print "\twhere report_file is a file from /hive/groups/encode/dcc/reports\n";
  print "\n";
  print "\toutput: tab delimited cumulative submit/release numbers\n";

  exit(1);
}
