#!/usr/local/bin/perl -w
#
#	encodeEmail.pl - encode email addresses into HTML special chars
#
#	usage:  encodeEmail.pl <email address> [more email addresses]
#	e.g.:  encodeEmail.pl hiram@www.soe.ucsc.edu
#
#	The encoding has a bit of randomness built into it and thus
#		you will not necessarily obtain the same result each
#		time.  The randomness is that a few of the letters will
#		be printed in their plain text form, not encoded.  The
#		selection of which letters to print thusly is random.
#
#	Created: 2003-06-29 - Hiram Clawson - hiram@soe.ucsc.edu

use strict;

my $argc = @ARGV;	#	arg count

if( $argc < 1 ) {
	print "usage: $0 <email addresses>\n";
	print "\tThe email addresses will be encoded and sent to stdout\n";
	exit(255);
}

sub StringOut;		#	forward declaration of subroutine

#	For each address given:
#		Split at the @ sign into two pieces
#		encode each piece separately.
#		The dots in the name will come thru in plain text as: .
for( my $i = 0; $i < $argc; ++$i ) {
	my @atSign = split(/@/,$ARGV[$i]);
	my $lenAtSign = @atSign;
	if( $lenAtSign != 2 ) {
		print "WARNING: not a valid email address: $ARGV[$i]\n";
		print "\tFound no \@ sign.  Must have an \@ sign.\n";
	} else {
		print "\n$ARGV[$i] encoded:\n";

		#	Beginning of HTML anchor:
		print '<A HREF="mailto:';
		#	StringOut will print it, and return it
		my $str0 = &StringOut($atSign[0]);
		my $ordVal = ord("@");
		print "&#$ordVal;";
		my $str1 = &StringOut($atSign[1]);
		print '">', "\n";
		#	using the returned strings, repeat the encoding
		print "$str0&#$ordVal;$str1</A>\n";
		#	and comment what is happening here
		print "<!-- above address is $atSign[0] at $atSign[1] -->\n";
	}
}

#	Subroutine, StringOut - given one argument, a string to encode
#		Will print it out as it is encoded, and will also return
#		the encoding.
sub StringOut {
	my $strng = shift;	#	single argument, string to encode

	my $return = "";	#	to be returned
	my @string = split(//,$strng);	#	split into single characters
	my $strLen = @string;		#	count them
	my $randCount;			#	set random count based on size
	if( $strLen >= 11 ) { $randCount = 3; }
	if( $strLen < 11 ) { $randCount = 2; }
	if( $strLen < 6 ) { $randCount = 1; }
	if( $strLen < 3 ) { $randCount = 0; }
	my @randSkips;
	$randSkips[0] = -1;		#	start with none specified
	$randSkips[1] = -1;
	$randSkips[2] = -1;
	#	generate the specified number.  Some may overlap here and
	#		thus not actually have the full amount.  That is OK.
	#		The number generated here is in the range
	#		1 to strLen-1
	#		thus we will never encode the
	#		first character $string[0]
	for( my $j = 0; $j < $randCount; ++$j ) {
		$randSkips[$j] = int(rand($strLen-1)) + 1;
	}
	#	For each character in the string
	for( my $j = 0; $j < $strLen; ++$j ) {
		#	ord() converts a character to an integer value
		my $ordVal = ord($string[$j]);
		#	See if this is a character to skip
		#	Either due to random, or it is a dot: .
		#	For a . aid the line wrap with an extra CR
		if( $j == $randSkips[0] || $j == $randSkips[1] ||
			$j == $randSkips[2] || $string[$j] eq "." ) {
			print "$string[$j]";
			$return = $return . $string[$j];
			if ($string[$j] eq ".") {
				print "\n";
				$return = $return . "\n";
			}
		} else {
			#	This is the HTML special character
			print "&#$ordVal;";
			$return = $return . "&#" . $ordVal . ";";
		}
	}
	#	Have been accumulating this, return it
	return $return;
}

__END__

#	chr() is the opposite of the ord() function.
#
	for( my $j = 41; $j < 43; ++$j ) {
		my $o = chr($j);
		print "$j $o\n";
	}
