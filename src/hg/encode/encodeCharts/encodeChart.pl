#!/usr/bin/env perl

use strict;
use warnings;

my $reportFile = $ARGV[0] || usage();

my $endDate = "2011-12-31";

my %submitHash;
my %releaseHash;

open(FILE, "< $reportFile") || die "Error: Can't open file '$reportFile'\n";
while(my $line = <FILE>) {
    next if ($line =~ /^Project/); # Skip first line

    chomp($line);
    my @split = split(/\t/, $line);

    my $start = $split[6];
    my $end = $split[7];

    my $endFlag = 0;
    if ($end eq "none" || ! $end) {
        $end = $endDate;
        $endFlag = 1;
    }

    $start = stripDateDashes($start);
    $end = stripDateDashes($end);

    $start = convert_2M2D2Y_Date($start);
    $end = convert_2M2D2Y_Date($end);

    if ($start > $end) {
        print STDERR "Invalid dates on: $line\n";
        next;
    }

    $submitHash{$start}++;
    $releaseHash{$end}++ if (! $endFlag);
}
close(FILE);

my @submitKeys = keys %submitHash;
my @releaseKeys = keys %releaseHash;

my %union;
foreach my $key (@submitKeys) {
    $union{$key} = 1;
}
foreach my $key (@releaseKeys) {
    $union{$key} = 1;
}

my @dateKeys = sort {$a <=> $b} keys %union;

my $submitSum = 0;
my $releaseSum = 0;

foreach my $key (@dateKeys) {
    my $year = substr($key, 0, 4);
    my $month = substr($key, 4, 2);
    my $day = substr($key, 6, 2);

    my $date = "$month/$day/$year";

    if (defined $submitHash{$key}) {
        $submitSum += $submitHash{$key};
    }
    if (defined $releaseHash{$key}) {
        $releaseSum += $releaseHash{$key};
    }

    print "$date\t$submitSum\t$releaseSum\n";
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
