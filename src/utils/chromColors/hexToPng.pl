#!/usr/local/bin/perl -w
#
#	Read a palette definition, create ImageMagick "convert"
#		commands that will draw the color key png(/gif)
#
#	The input palette definition file has lines defined as such:
#996600	1
#	and yes, they do begin with the # character
#	That is the hex value 996600 for chrom 1

use strict;

my $argc = @ARGV;

if( $argc != 1 ) {
	print "usage: $0 <file with 26 hex colors>\n";
	exit( 255 );
}

my $fName = $ARGV[0];

open(IN, "<$fName") or die "Can not open $fName";

my $width = 21;		#	width of a block
my $height = 18;	#	height of a block
my $border = 2;		#	white pixels between blocks
my $totColors = 0;	#	count the colors input
my @palette;		#	to save the hex values
my @names;		#	and their associated names

while( my $line = <IN> ) {
    chomp($line);
    my @a = split(/\s/,$line);
    $palette[$totColors] = $a[0];	#	hex value
    $names[$totColors] = $a[1];		#	chrom name
    ++$totColors;
}
close(IN);

my $xSize = (($totColors) * ($width + 2)) - 2;

printf "import -window root -crop %dx%d-0+0 -quality 50 chromColor.png\n",
	$xSize, $height;
printf "mogrify -fill white -draw 'rectangle 0,0 %d,%d' chromColor.png\n",
	$xSize-1, $height-1;

print "convert \\\n";

my $x = 0;
for( my $i = 0; $i < $totColors; ++$i ) {
	my $hexDigits = $palette[$i];
	$hexDigits =~ m/^\#(..)(..)(..)$/;
	my $Red = hex($1);
	my $Green = hex($2);
	my $Blue = hex($3);
	my $xNext = $x + $width - 1;
	my $totalValue = $Red + $Green + $Blue;
# printf STDERR "RGB: %d %d %d = %d at $names[$i]\n", $Red, $Green, $Blue, $totalValue;
	printf "\t-fill '%s' -draw 'rectangle %d,0 %d,%d reset' \\\n",
		$palette[$i], $x, $xNext, $height-1;
	my $xText = $x + 5;
	if( length($names[$i]) == 2 ) {
	$xText = $x + 2;
	}
        #	color the digits based on the the lightness of the block
	my $fontColor = "#ffffff";
	#	The green at #1 needs black letters
	if( ($i == 11) || ($totalValue >= (256 * 3) / 2 )) {
		$fontColor = "#000000";
	}
	printf "\t-font x:9x15bold -fill '%s' -draw 'text %d,%d \"%s\"' \\\n",
		$fontColor, $xText, ($width / 2) + 1, $names[$i];
	$x += $width + $border;
}

print "\tchromColor.png palette.png\n";

__END__
