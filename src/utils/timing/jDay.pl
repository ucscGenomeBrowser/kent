#!/usr/bin/env perl
#	$Id: jDay.pl,v 1.1 2007/01/19 00:42:24 hiram Exp $
#
#	jDay.pl - display a julian date
#
#	Arguments:  when none, display julian date of the system clock

use warnings;
use strict;
use FindBin qw($Bin);
use lib "$Bin";
use julDate;	# subroutines: JulDate, CalDate, FormatCalDate

sub usage() {
    print "usage: jday.pl [options]\n";
    print "options:\n";
    print "-d YYYY-MM-DD HH:MM:SS - calculate julian date for given date\n";
    print "-h - display this help message\n";
}

my $argc = scalar(@ARGV);

my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = gmtime(time);
$year += 1900;
$mon += 1;

if ($argc > 0) {
    for (my $i = 0; $i < $argc; ++$i) {
	if ($ARGV[$i] =~ m/-h/) { usage(); die "\n"; }
	if ($ARGV[$i] =~ m/-d/) {
	    my $ymd = "";
	    my $hms = "";
	    if (length($ARGV[$i]) > 2) {
		$ymd = $ARGV[$i];
		$ymd =~ s/-d//;
		$hms = $ARGV[$i+1];
	    } else {
		$ymd = $ARGV[$i+1];
		$hms = $ARGV[$i+2];
	    }
	    ($year,$mon,$mday) = split('-',$ymd);
	    ($hour,$min,$sec) = split(':',$hms);
	}
    }
}

my %UTInstantNow;
$UTInstantNow{'YEAR'} = $year;
$UTInstantNow{'MONTH'} = $mon;
$UTInstantNow{'DAY'} = $mday;
$UTInstantNow{'HOUR'} = $hour;
$UTInstantNow{'MINUTE'} = $min;
$UTInstantNow{'SECOND'} = $sec;

my $U=\%UTInstantNow;		# $U is a pointer to the UTInstantNow
$U=\%UTInstantNow;
&JulDate($U);
printf "%.6f\n", $UTInstantNow{'J_DATE'};
