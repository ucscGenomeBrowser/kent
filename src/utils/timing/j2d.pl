#!/usr/bin/env perl

#	$Id: j2d.pl,v 1.1 2007/01/19 00:42:24 hiram Exp $
#
#	j2d.pl - convert a julian date to a calendar date
#
#	Arguments:  must have one argument: the julian date to convert

use warnings;
use strict;
use FindBin qw($Bin);
use lib "$Bin";
use julDate;	# subroutines: JulDate, CalDate, FormatCalDate

sub usage() {
printf("usage: j2d.pl <julian date>\n" );
}

my $argc = $#ARGV + 1;

if( 1 != $argc ) {
	&usage();
	die "ERROR: must supply a julian date\n";
}

if ( $ARGV[0] =~ /^-h.*/ ) {
	&usage();
	die "\n";
}

my %UTInstantNow;
$UTInstantNow{'J_DATE'} = $ARGV[0];

&CalDate(\%UTInstantNow);
my $D=&FormatCalDate(\%UTInstantNow);
printf("%s\n", $D);
