#!/usr/local/bin/perl -w
#
#	Read a palette definition, create HTML to display those colors
#		in a pattern so they can all be seen next to each other
#		to determine if any of them are so similar they meld together
#
#	The input palette definition file has lines defined as such:
#996600	1
#	and yes, they do begin with the # character
#	That is the hex value 996600 for chrom 1

use strict;

my $argc = @ARGV;	#	argument count

if( $argc != 1 ) {
	print "usage: $0 <file with 26 hex colors>\n";
	exit( 255 );
}

my $fName = $ARGV[0];

open(IN, "<$fName") or die "Can not open $fName";

#	start the HTML page

print "<html>\n", "<body>\n", "<CENTER>\n<H1>  UCSC Genome browser rearranged </H1>\n",
	"Following pattern Blk Brn Red Org Ylo Grn Blu Vio Gray<BR>\n",
	"Demonstrating each color next to all others\n</CENTER>\n";

my $totColors = 0;	#	to count the colors input
my @palette;		#	will be the hex values for each
my @names;		#	and corresponding name
my $pIndex = 0;		#	index into the palette array

while( my $line = <IN> ) {
    chomp($line);
    my @a = split(/\s/,$line);
    $palette[$totColors] = $a[0];	#	hex value
    $names[$totColors] = $a[1];		#	chrom name
    ++$totColors;
}
close(IN);

for( my $k = 0; $k < 12; ++$k ) {
	print "<hr>\n", '<table ALIGN="CENTER">', "\n";
	$pIndex = ($k * 2) + 1;

	my $fgColor = "";
	print "<tr>\n";
	for( my $i = 0; $i < 26; ++$i ) {
		if( $palette[$pIndex] eq "#000000" ) {
			$fgColor = "";
		} else { $fgColor = ""; }
	print '<td WIDTH=20 ALIGN=CENTER HEIGHT=20 ', $fgColor, ' bgcolor="',
		"$palette[$pIndex]", '"> ', "$names[$pIndex] </td>";
		$pIndex = ($pIndex + 1) % 26;
	}
	print "</tr>\n";

	print "<tr>\n";
	for( my $i = 0; $i < 26; ++$i ) {
		if( $palette[$pIndex] eq "#000000" ) {
			$fgColor = "";
		} else { $fgColor = ""; }
	print '<td WIDTH=20 ALIGN=CENTER HEIGHT=20 bgcolor="',
		"$palette[$i]", '"> ', "$names[$i] </td>";
	}
	print "</tr>\n";

	print "<tr>\n";
	$pIndex = (($k * 2) + 2) % 26;
	if( $pIndex > 24 ) {
		die "pIndex: $pIndex > 24 at k: $k";
	}
	for( my $i = 0; $i < 26; ++$i ) {
		if( $palette[$pIndex] eq "#000000" ) {
			$fgColor = "";
		} else { $fgColor = ""; }
	print '<td WIDTH=20 ALIGN=CENTER HEIGHT=20 ', $fgColor, ' bgcolor="',
		"$palette[$pIndex]", '"> ', "$names[$pIndex] </td>";
		$pIndex = ($pIndex + 1) % 26;
	}
	print "</tr>\n";

	print "</table>\n\n";
}

print "</table>\n<hr>\n";

open(IN0, "<$fName") or die "Can not open $fName";
print "<TABLE BORDER=1 ALIGN=CENTER><TR><TH> Hex Code </TH><TH> Chrom Name </TH>\n";
while( my $line = <IN0> ) {
	chomp $line;
	my @a = split(/\s/,$line);
	print '<TR><TD ALIGN=RIGHT> <FONT="fixed"> <B>', " $a[0] </B> </FONT> </TD><TD ALIGN=CENTER> $a[1] </TD>\n";
}
close(IN0);

print "</table>\n<hr>\n", "</body>\n", "</html>\n";
