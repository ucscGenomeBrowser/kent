#!/usr/bin/env perl
#
#	findPantone.pl <palette.txt>
#
#	Take rgb definitions, find nearest pantone hex values
#

use warnings;
use strict;

my $argc = @ARGV;

if( $argc != 1 )
    {
    print "usage: $0 <two column file with hex colors>\n";
    exit( 255 );
    }

my $fName = $ARGV[0];

open(IN, "<$fName") or die "Can not open $fName";

my $totColors = 0;
my $pIndex = 0;
my @palette;

while (my $line = <IN>)
    {
    chomp($line);
    ++$totColors;
    my ($rgb, $label) = split('\s+',$line);
    $palette[$pIndex++] = $rgb;
}
close(IN);

my @panName;
my @panR;
my @panG;
my @panB;
my @panHex;
my $panCount = 0;

open(IN, "< pantone.txt") or die "Can not open pantone.txt";
while( my $line = <IN> ) {
	chomp $line;
	$line =~ s/^\#.*//;
	if( length($line) ) {
($panName[$panCount], $panR[$panCount], $panG[$panCount], $panB[$panCount], $panHex[$panCount]) = split(/\s+/,$line);
		my $hexDigits = $panHex[$panCount];
		$hexDigits =~ m/^\#(..)(..)(..)$/;
		my $R = hex($1);
		my $G = hex($2);
		my $B = hex($3);
# print "$panCount: $panName[$panCount], $panR[$panCount], $panG[$panCount], $panB[$panCount], $panHex[$panCount]\n";
		if( $R != $panR[$panCount] ) {
			print "ERROR: R $R != $panR[$panCount]"; }
		if( $G != $panG[$panCount] ) {
			print "ERROR: G $G != $panG[$panCount]"; }
		if( $B != $panB[$panCount] ) {
			print "ERROR: B $B != $panB[$panCount]"; }
		++$panCount;
	}
}
close(IN);
print STDERR "Read $panCount pantone colors\n";
print STDERR "Checking $totColors colors in palette $fName\n";

for( my $i = 0; $i < $totColors; ++$i )
    {
    my $hexDigits = $palette[$i];
    $hexDigits =~ m/^\#(..)(..)(..)$/;
    my $Red = hex($1);
    my $Green = hex($2);
    my $Blue = hex($3);
    my $minIndex = $panCount + 1;
    my $minDistance = (256 * 256) * 3;
    my $dist;
    for( my $j = 0; $j < $panCount; ++$j )
	{
	my $dR = $Red - $panR[$j];
	my $dG = $Green - $panG[$j];
	my $dB = $Blue - $panB[$j];
	$dR *= $dR;
	$dG *= $dG;
	$dB *= $dB;
	$dist = $dR + $dG + $dB;
	if( $dist <= $minDistance )
	    {
	    if( $dist == $minDistance )
		{
print STDERR "Equivalent: $palette[$i] $panHex[$j] $panName[$j] $dist\n";
		} else {
		$minDistance = $dist;
		$minIndex = $j;
		}
	    }
	}
print STDERR "Minimum: $i: $palette[$i] $panHex[$minIndex] $panName[$minIndex] $dist\n";
print "$panHex[$minIndex] $panName[$minIndex]\n";
    }

__END__
# Pantone   R       G       B        Hex   Colour*
100     244     237     124     #F4ED7C       
