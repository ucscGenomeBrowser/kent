#!/usr/local/bin/perl -w
#
#	sortKg.pl - sort knownGenes data by chrom then txStart
#

use strict;

my $argc = @ARGV;	#	arg count

if ($argc < 1) {
	my $BN=`basename $0`;
	chomp($BN);
	print "usage: $BN knownGene.tab dnaGene.tab\n";
	print "\tprints a sorted listing by chrom and txStart\n";
	exit 255;
}

my %chrIndex;
my @data;
my $totalLines = 0;

#	Read in each file given, keep their data in the @data array
#	create a has of chrom names in the %chrIndex hash

while (my $FName = shift) {
	open (FH, "<$FName") or die "Can not open $FName";
	my $fileLines = 0;
	while (my $line = <FH>) {
		chomp($line);
		++$fileLines;
		$data[$totalLines] = $line;	#	save input
		my @a = split('\t',$line);
		if (length($a[3]) < 1) {	# a[3] is the txStart column
	print STDERR "ERROR: null TxStart at line $fileLines of $FName\n";
			print STDERR "$line\n";
			exit 255;
		}
		if (length($a[1]) > 0) {	# a[1] is the chr column
			my $noChr = $a[1];
			$noChr =~ s/^chr//;
	# create an artifical unique # key for all data lines
	#	That will sort in chrom order
			my $key = sprintf("%s.%d", $noChr, $totalLines);
			if (exists($chrIndex{$key})) {
		    print STDERR "ERROR: duplicate key: $key at line $fileLines of $FName\n";
		    exit 255;
			}
			$chrIndex{$key} = $totalLines;
		} else {
		    print STDERR "ERROR: null chr name at line $fileLines of $FName\n";
		    exit 255;
		}
		++$totalLines;
	}
	close (FH);
	print STDERR "processed $fileLines lines in $FName\n";
	my $chrLineCount = 0;
	foreach my $key (sort keys %chrIndex) {
		++$chrLineCount;
	}
	print STDERR "total $chrLineCount lines in the hash\n";
}

#	All data is in @data and the chroms are indexed in %chrIndex hash
#	Now going to go through that in chr sorted order creating a new
#	hash of the txStart positions.
my $prevKey = "";
my %txStartIndex;
my $chrLineCount = 0;
my $processedLines = 0;
my $accountedFor = 0;
#	This sort will produce a chrom order
foreach my $key (sort keys %chrIndex) {
	++$processedLines;
	(my $chr, my $lineNo) = split('\.',$key);
	if (length($lineNo)<1) {
		print STDERR "ERROR: lineNo empty for key: $key\n";
		exit 255;
	}
	if (length($prevKey) > 0) {	#	first time through situation
		if ($prevKey ne $chr) {	#	one chr is done, print it
					#	this sort is the txStart order
			foreach my $key2 (sort {$a <=> $b} keys %txStartIndex) {
				my $lineData = $data[$txStartIndex{$key2}];
				print "$lineData\n";
				++$accountedFor;
			}
			#	Clean up the txStartIndex has for next use
			foreach my $key2 (sort keys %txStartIndex) {
				$txStartIndex{$key2} = undef;
			}
			#	Really clean it up.  It is now empty.
			undef %txStartIndex;
			$chrLineCount = 0;
		}
	}
	$prevKey = $chr;
	my $line = $data[$lineNo];
	my @a = split('\t',$line);
	my $lenTxStart = length($a[3]);	#	a[3] is the txStart number
	if ($lenTxStart > 0) {
		my $ixKey = $a[3] . "." . $chrLineCount;
		if (exists($txStartIndex{$ixKey})) {
		print STDERR "ERROR: already exists: $ixKey at line chr$chr $lineNo\n";
			exit 255;
		} else {
#	keep the line number of each data line in this hash
			$txStartIndex{$ixKey} = $lineNo;
		}
	} else {
			printf STDERR "ERROR: null TxStart '$a[3]' %d at line $lineNo\n", $lenTxStart;
			print STDERR "$line\n";
			exit 255;
	}
	++$chrLineCount;
}

#	And the last chromosome needs to be done.  The loop above exits
#	with this last bit of data to be printed
foreach my $key2 (sort {$a <=> $b} keys %txStartIndex) {
	my $lineData = $data[$txStartIndex{$key2}];
	print "$lineData\n";
	++$accountedFor;
}

if ($processedLines != $accountedFor) {
print "ERROR: Processed $processedLines lines, accounted for $accountedFor\n";
printf "\tnot accounted for: %d\n", $processedLines - $accountedFor;
	exit 255;
} else {
print STDERR "Processed $processedLines lines, accounted for $accountedFor\n";
}


__END__
		my @a = split('\t',$chrIndex{$key});
		if (length($a[3]) > 0) {
			$txStartIndex{$a[3]} = $key;
		} else {
		$txStartIndex
