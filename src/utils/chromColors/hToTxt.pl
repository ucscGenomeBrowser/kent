#!/usr/local/bin/perl -w
#
#	hToTxt - read the definition of the UCSC chromColors from
#		the source tree file chromColors.h and rearrange into
#		a simple text file specifying each color hex value.

use strict;

my $argc = @ARGV;	#	arg count

if ( $argc != 1 ) {
	print "usage: $0 <chromColors.h>\n";
	exit( 255 );
}

my $fName = shift;	#	path to chromColors.h file
			#	A typical definition in that file:
#define CHROM_1_R       0x99
#define CHROM_1_G       0x66
#define CHROM_1_B       0x00
			#	to be translated by this program into:
#996600	1

open(IN, "< $fName") or die "Can not open $fName";

my $previousChrName = "";	#	to keep track when name changes
my $hexString = "";		#	to build string: #123def

while( my $line = <IN> ) {
	chomp($line);
	if ( $line =~ m/^\#define\sCHROM_[^C]/ ) {
		my @a = split(/\s/,$line);
		my @n = split(/_/,$a[1]);	# extract the name
		my $hex = $a[2];		# the 0xhh hex value
		$hex =~ s/0x//;			# remove the 0x
		#	See if the name is changing, time to finish one
		if( $previousChrName ne $n[1] ) {
			if( length($previousChrName) > 0 ) {
				print "$hexString\t$previousChrName\n";
			}
			$previousChrName = $n[1];	# new name
			$hexString = "#" . $hex;	# initial two digits
		} else {
			$hexString = $hexString . $hex;  # accumulating
		}
	}
}
print "$hexString\t$previousChrName\n";		# last one

close(IN);
