#!/usr/bin/env perl
#
#	rmskFeatBits.pl -
#		run a simple featureBits on .bed files from rmsk tables
#
#	when file name structure follows the pattern: */file.bed
#	where that /file.bed is exactly that: '/file.bed'
#	interpret the path name as:
#	chr_name/repClass/file.bed
#	to allow sorting out repClass bed files by chrom and repClass type
#
#	 $Id: rmskFeatBits.pl,v 1.3 2005/05/02 18:00:01 hiram Exp $
#

use warnings;
use strict;
use Math::BigInt;
use Math::BigFloat;

sub usage()
{
my $progName=`basename $0`;
chomp $progName;
print "usage: $0 [options] <bed files>\n";
print "options:\n";
print "\t-verbose=N - level of verbose output, N = 1, 2, 3, ...\n";
print "\t-db=<name> - use specified db to find chromInfo\n";
print "\t-html - output html table format (default is plain ascii\n";
print "\t-tabFile=<fileName> - output tab delimited format suitable\n";
print "\t\tfor table loading\n";
print "example: $progName -verbose=1 -db=hg17 -html -tabFile=hg17.tab \\\n";
print "\tchr*/*/file.bed > hg17.summary.html 2> hg17.stderr &\n";
exit 255;
}

#	chrom name sort, strip chr or scaffold prefix, compare
#	numerically when possible, see also, in kent source:
#	hg/lib/hdb.c: int chrNameCmp(char *str1, char *str2)
sub chrSort()
{
# the two items to compare are already in $a and $b
my $c1 = $a;
my $c2 = $b;
#	strip any chr or scaffold prefixes
$c1 =~ s/^chr//i;
$c1 =~ s/^scaffold_*//i;
$c2 =~ s/^chr//i;
$c2 =~ s/^scaffold_*//i;
#	see if only numbers are left
my $isNum1 = 0;	#	test numeric each arg
my $isNum2 = 0; #	test numeric each arg
$isNum1 = 1 if ($c1 =~ m/^[0-9]+$/);
$isNum2 = 1 if ($c2 =~ m/^[0-9]+$/);
#	both numeric, simple compare for result
if ($isNum1 && $isNum2)
    {
    return $c1 <=> $c2;
    }
elsif ($isNum1)	#	only one numeric, it comes first
    {
    return -1;
    }
elsif ($isNum2)	#	only one numeric, it comes first
    {
    return 1;
    }
else
    {
    my $l1 = length($c1);
    my $l2 = length($c2);
    if ($l1 < $l2)
	{
	return -1;
	}
    elsif ($l2 < $l1)
	{
	return 1;
	}
    else
	{
	return $c1 cmp $c2;	#	not numeric, compare strings
	}
    }			#	could do more here for X,Y,M and Un
}

my $argc = scalar(@ARGV);	#	arg count

usage if ($argc < 1);		#	no arguments, show usage() message

my %repClasses;		#	key is rep class, value is index(rep class num)
my $repClassIx = 0;	#	rep class index (==rep class number)
my %repClassCounts;	#	key is rep class, value is count for that class
my %repClassBases;	#	key is rep class, value is #bases for that class
my %chrNames;		#	key is chr name, value is chrNameIx
my $chrNameCount = 0;	#	to count number of chroms as processed
my $verbose = 0;	#	verbose command line option
my $htmlOut = 0;	#	command line option to output an HTML table
my $dataBase = "";	#	database command line option
my $fileCount = 0;	#	a count of files from command line
my @fileList;		#	list of file names from command line
my $tabFile = "";	# output tab-delimited format suitable for table load
#	chrArray is going to be a multiple (3) dimensioned array.
#	Given a chrom name: $chr, find the cIx from $chrNames{$chr}
#	Then refer to $chrArray[$cIx] whose value is an array reference.
#	The array referenced in this $cIx cell is an array indexed by
#	a repClass name index.  The contents of this cell location is a
#	third array reference where each element of this third dimension
#	is merely the measurements for this two-way intersection of $chr and
#	$repClass.  The measurments here are:
#	[0] = item count for this repClass (integer)
#	[1] = bases covered for this repClass (integer)
#	[2] = % coverage of this element on this chrom (a string)
#
#	How else does Perl keep such a structure ?  Let me count the ways.
#
my @chrArray;

#	process command line arguments
for (my $i = 0; $i < $argc; ++$i)
{
if ($ARGV[$i] =~ m/^-/)
    {
    if ($ARGV[$i] =~ m/^-verbose=/)
	{
	my ($junk, $level) = split('=',$ARGV[$i]);
	$verbose = $level;
	}
    elsif ($ARGV[$i] =~ m/^-html/)
	{
	$htmlOut = 1;
	}
    elsif ($ARGV[$i] =~ m/^-db=/)
	{
	my ($junk, $db) = split('=',$ARGV[$i]);
	$dataBase = $db;
	}
    elsif ($ARGV[$i] =~ m/^-tabFile=/)
	{
	my ($junk, $file) = split('=',$ARGV[$i]);
	$tabFile = $file;
	}
    }
else
    {
    $fileList[$fileCount++] = $ARGV[$i];
    }
}

#	no files to process ?
if ($fileCount < 1)
    {
    print STDERR "ERROR: no bed files specified\n";
    usage;
    }

print STDERR "#\tworking on $fileCount files\n" if ($verbose > 0);

my $chromCount = 0;	#	chrom count from chromInfo
my %chromInfo;		#	key will be chrom name, value is chrom size
my $genomeSize = Math::BigInt->new("0");
my $genomeGapSize = Math::BigInt->new("0");
my $percentGap = "";	#	calculated result from division gap/size
my %chromGaps;		#	key is chrom name, value is gap size

#	if given a database, fetch its chromInfo business
if (length($dataBase) > 0)
{
print STDERR "#\tdatabase: $dataBase\n" if ($verbose > 0);
open (FH,"hgsql -N $dataBase -e 'select chrom,size from chromInfo;'|") or die
	"Can not run: hgsql -N $dataBase -e 'select * from chromInfo;'";
while (my $line = <FH>)
    {
    chomp $line;
    ++$chromCount;
    my ($chr, $size) = split('\s+',$line);
    $chromInfo{$chr} = $size;
    $genomeSize->badd($size);
    }
close (FH);
open (FH,"hgsql -N $dataBase -e 'show tables like \"%gap%\";'|") or die
	"Can not run: hgsql -N $dataBase -e 'show tables like \"%gap%\";'";
while (my $table = <FH>)
    {
    chomp $table;
    my $sumSize=`hgsql -N $dataBase -e "select sum(size) from $table;" 2> /dev/null`;
    chomp $sumSize;
    if ((length($sumSize)>0) && ($sumSize !~ m/NULL/) && ($sumSize > 0))
	{
	if ($table =~ m/_gap$/)
	    {
	    my $chrName = $table;
	    $chrName =~ s/_gap//;
	    $chromGaps{$chrName} = $sumSize if (exists($chromInfo{$chrName}));
	    }
	my $tmpSize = Math::BigInt->new($sumSize);
	$genomeGapSize->badd($tmpSize);
    printf STDERR "#\ttable: $table, sumSize: $sumSize, %s\n", $genomeGapSize->bstr() if ($verbose>1);
	}
    }
close (FH);

my $copyGapSize = Math::BigFloat->new("$genomeGapSize");
$copyGapSize->precision(-8);
$copyGapSize->bdiv($genomeSize);
$percentGap = sprintf("%.6f",100.0*$copyGapSize->bsstr());
printf STDERR "#\ttotal genome size: %s, gapSize: %s, gap %% %s\n",
	$genomeSize->bstr(),$genomeGapSize->bstr(),$percentGap if ($verbose > 0);
}

#	if we are given a database, we can obtain the total featureBits
#	of the rmsk tables via a call to featureBits.  No DB, then start
#	at zero
my $fbTotalLen = "0";

if (length($dataBase)>0)
    {
    open (FH,"featureBits $dataBase rmsk|") or
	die "Can not featureBits $dataBase rmsk";
    while (my $line = <FH>)
	{
	chomp $line;
	my @a = split('\s+',$line);
	$fbTotalLen = $a[0];
	}
    close (FH);
    }

#	overall total length covered by all repClass elements on all chroms.
#	either just measured above by featureBits or will accumulate
#	below.
my $totalLen =  Math::BigInt->new($fbTotalLen);
my $totalClassCount = 0;

if (length($tabFile)>0)
    {
    open (TF,">$tabFile") or die "Can not open for writing to $tabFile";
    }

##########################################################################
#	primary processing loop
#	for each file on the command line, process them
for (my $i = 0; $i < $fileCount; ++$i)
    {
    my $fileName = $fileList[$i];
    my $lineCount = 0;		# count of lines read in this current file
    my $chromStart = -1;	# current chromStart and chromEnd of
    my $chromEnd = -1;		# this element being processed
    my $prevChrName = "";	# previous chrom name processed
    my $chrName = "";	# current chrom name being processed
    my $chrLen = 0;	# bases covered on this chrom by this repClass element
    my $chrSize = 0;	# this chrom's size from chromInfo (when in use)
    my $chrCovered = 0;	# fraction of this chrom covered (chrLen/chrSize)
    my $chrCoveredNoGaps = 0;	# same as chrCovered not counting gaps
    my $chrGapSize = 0;	#	this chrom's gap size
    my $genomeCovered = 0;
    my $repClass = "";
    my $itemCount = 0;	#	count of instances of this repClass

    if ($fileName =~ m#/file.bed$#)
	{
	my @a = split('/',$fileName);
	my $repIx = scalar(@a) - 2;
	$repClass = $a[$repIx] if ($repIx > 0);
	$repClasses{$repClass} = $repClassIx++
		if (!exists($repClasses{$repClass}));
	}
    open (FH,"sort -k1,1 -k2,2n -k3,3n $fileName|") or die "Can not open $fileName";
    while (my $line = <FH>)
	{
	chomp $line;
	++$lineCount;
	++$itemCount;
	++$repClassCounts{$repClass} if (length($repClass)>0);
	++$totalClassCount;
	my ($chr, $start, $end) = split('\s+',$line);
	if (length($chrName) < 1)
	    {
	    $chrName = $chr;
	    $prevChrName = $chr;
	    $chrSize = $chromInfo{$chr} if ($chromCount > 0);
	    }
	if ($chromStart == -1) { $chromStart = $start; $chromEnd = $end; }
	#	encountered gap or next chrom, done with bed element
	if (($start > $chromEnd) || ($chrName ne $prevChrName))
	    {
	    my $len = $chromEnd - $chromStart;
	    if ($len < 1)
		{
		printf STDERR "ERROR: illegal length $len at line $lineCount in $fileName\n";
		printf STDERR "$line\n";
		exit 255;
		}
	    $totalLen->badd("$len") if (0 == length($dataBase));
	    if (length($repClass)>0)
		{
		if (!exists($repClassBases{$repClass}))
		    {
		    $repClassBases{$repClass}=0;
		    }
		$repClassBases{$repClass} += $len;
		}
	    $chrLen += $len;
	    if (($verbose > 2) || ($chrName ne $prevChrName))
		{
		$chrCovered = 0;
		$chrCoveredNoGaps = 0;
		$chrCovered = $chrLen/$chrSize if ($chrSize > 0);
		$chrGapSize = 0;
		$chrGapSize = $chromGaps{$chr} if (exists($chromGaps{$chr}));
		$chrCoveredNoGaps = $chrLen/($chrSize-$chrGapSize)
			if (($chrSize-$chrGapSize) > 0);
		$genomeCovered = "0";
		if (! $genomeSize->is_zero())
		    {
		    my $copyTotalLen = Math::BigFloat->new("$totalLen");
		    $copyTotalLen->precision(-8);
		    $copyTotalLen->bdiv($genomeSize);
		    $genomeCovered = sprintf("%.6f",100.0*$copyTotalLen->bsstr());
		    }
		my $chrRepClass = $chr;
		if (length($repClass) > 0)
		    {
		    $chrRepClass = "$chr:$repClass";
		    }
		printf STDERR "%s\t%d\t%d\t%d\t%d\t%s\t%.2f\t%.2f\t%s\n",
		    $chrRepClass,$chromStart,$chromEnd,$len,$chrLen,
			$totalLen->bstr(),$chrCovered,
			    $chrCoveredNoGaps,$genomeCovered;
		}
	    $chromStart = $start;
	    $chromEnd = $end;
	    $chrLen = 0 if ($chrName ne $prevChrName);
	    }
	$chromEnd = $end if ($end > $chromEnd);
	$chrSize = $chromInfo{$chr} if (($chromCount > 0) &&
						($chr ne $prevChrName));
	$prevChrName = $chr;
	$chrNames{$chr} = $chrNameCount++ if (!exists($chrNames{$chr}));
	}
    close (FH);
    my $len = $chromEnd - $chromStart;
    if ($len < 1)
	{
	printf STDERR "ERROR: illegal length $len at line $lineCount in $fileName\n";
	exit 255;
	}
    if (length($repClass)>0)
	{
	if (!exists($repClassBases{$repClass}))
	    {
	    $repClassBases{$repClass}=0;
	    }
	$repClassBases{$repClass} += $len;
	}
    $chrGapSize = 0;
    $chrGapSize = $chromGaps{$chrName} if (exists($chromGaps{$chrName}));
    $totalLen->badd("$len") if (0 == length($dataBase));
    $chrLen += $len;
    $chrCovered = 0;
    $chrCoveredNoGaps = 0;
    $genomeCovered = "0";
    $chrCovered = $chrLen/$chrSize if ($chrSize > 0);
    $chrCoveredNoGaps =
	$chrLen/($chrSize-$chrGapSize) if (($chrSize-$chrGapSize) > 0);
    if (! $genomeSize->is_zero())
	{
	my $copyTotalLen = Math::BigFloat->new("$totalLen");
	$copyTotalLen->precision(-8);
	$copyTotalLen->bdiv($genomeSize);
	$genomeCovered = sprintf("%.6f",100.0*$copyTotalLen->bsstr());
	}
    my $chrRepClass = $chrName;
    if (length($repClass) > 0)
	{
	$chrRepClass = "$chrName:$repClass";
	printf TF "%s\t%s\t%d\t%d\n", $chrName,$repClass,$itemCount,$chrLen
		if (length($tabFile)>0);
	}
    printf STDERR "%s\t%d\t%d\t%d\t%d\t%s\t%.2f\t\t%.2f%s\n",
	    $chrRepClass,$chromStart,$chromEnd,$len,$chrLen,$totalLen->bstr(),
		    $chrCovered,$chrCoveredNoGaps,$genomeCovered if ($verbose > 1);
    printf STDERR "%s\t%d\t%d\t%s\t%.6f\t%.6f\t%s\n", $chrRepClass,$itemCount,
	$chrLen,$totalLen->bstr(), $chrCovered,$chrCoveredNoGaps,$genomeCovered;


    if (length($repClass) > 0)
	{
	#	The two indexes into chrArray
	my $cIx = $chrNames{$chrName};
	my $rcIx = $repClasses{$repClass};
	if (!defined($chrArray[$cIx]))
	    {
	    my @repA = ();
	    $chrArray[$cIx] = \@repA;
	    }
	my $repArray = $chrArray[$cIx];
	if (!defined(@$repArray[$rcIx]))
	    {
	    my @repB = ();
	    @$repArray[$rcIx] = \@repB;
	    }
	my $countsArray = @$repArray[$rcIx];
	@$countsArray[0] = $itemCount;
	@$countsArray[1] = $chrLen;
	@$countsArray[2] = $chrCovered;
	@$countsArray[3] = $chrCoveredNoGaps;
	}
    }	#	for each file

#########################################################################
#	Primary processing complete
#	Now for summary outputs

#	Close tabFile if it was in use
if (length($tabFile)>0)
    {
    close (TF);
    }

#	HTML page starts
if ($htmlOut)
    {
    print "<HTML><HEAD><TITLE>DB: $dataBase Repeat Masker ",
	"Analysis</TITLE></HEAD>\n<BODY>\n<H1>DB: $dataBase Repeat Masker ",
	"Analysis</H1>\n";

    if (! $genomeSize->is_zero())
	{
	printf "<H2>Total genome size: %s bases</H2>\n", $genomeSize->bstr();
	my $tmp = Math::BigFloat->new("$genomeSize");
	if (! $genomeGapSize->is_zero())
	    {
	    printf "<H2>Bases in gaps: %s</H2>\n", $genomeGapSize->bstr();
	    printf "<H2>Percent gap: %s</H2>\n", $percentGap;
	    $tmp->bsub($genomeGapSize);
	    printf "<H2>Size without gaps: %s</H2>\n", $tmp->bstr();
	    }
	}

    print "<H2>Each table cell contents:</H2>\n";
    print "<UL><LI>#&nbsp;item&nbsp;count</LI>",
	"<LI>bases&nbsp;covered&nbsp;b</LI>",
	"<LI>%&nbsp;percent&nbsp;covered&nbsp;including&nbsp;gaps</LI>",
	"<LI>%&nbsp;percent&nbsp;covered&nbsp;without&nbsp;gaps</LI>",
	"</UL>\n";
    print "<TABLE BORDER=1>\n";
    print "<TR><TH>chrom&nbsp;name<BR>chrom&nbsp;size<BR>gap&nbsp;size<BR>size&nbsp;no&nbsp;gaps<BR>%&nbsp;gap</TH><TH>this&nbsp;chrom<BR>sum</TH>";
    }
else
    {
    print "chrom\tsums\t";
    }

#	top row column labels
for my $repClass (sort {$repClassCounts{$b} <=> $repClassCounts{$a} } keys(%repClassCounts))
    {
    if ($htmlOut)
	{
	printf "<TH>%s</TH>", $repClass;
	}
    else
	{
	printf "%s\t", $repClass;
	}
    }
print "</TR>" if ($htmlOut);
printf "\n";

#########################################################################
#	For each chromosome, output a row of data information
#
for my $chr (sort chrSort keys(%chrNames))
    {
    if (exists($chrNames{$chr}))
	{
	my $cIx = $chrNames{$chr};
	if ($htmlOut)
	    {
	    my $chrSize = 0;
	    $chrSize = $chromInfo{$chr} if (exists($chromInfo{$chr}));
	    my $chrGapSize = 0;
	    $chrGapSize = $chromGaps{$chr} if (exists($chromGaps{$chr}));
	    my $chrNoGaps = $chrSize - $chrGapSize;
	    my $chrPercentGap = 0;
	    $chrPercentGap = 100.0*$chrGapSize/$chrSize if ($chrSize>0);
	    printf "<TR><TH ALIGN=LEFT>%s<BR>%d&nbsp;b<BR>%d&nbsp;b<BR>%d&nbsp;b<BR>%%&nbsp;%.4f</TH>", $chr,$chrSize,$chrGapSize,$chrNoGaps,$chrPercentGap;
	    }
	else
	    {
	    printf "%s\t", $chr;
	    }
	my $repArray = $chrArray[$cIx];
	#	Before we walk across the columns of this row, sum up
	#	the item count and bases count for this row so those
	#	numbers can go into the first column of data
	#	Turns out this isn't a featureBits measurement because
	#	some of these repeat masker class items overlap each
	#	other, therefore, we need to actually call featureBits
	#	to do the measurement.
	my $fbCount = 0;
	if (length($dataBase)>0)
	    {
	    open (FH,"featureBits $dataBase -chrom=$chr rmsk|") or
		die "Can not featureBits $dataBase -chrom=$chr rmsk";
	    while (my $line = <FH>)
		{
		chomp $line;
		my @a = split('\s+',$line);
		$fbCount = $a[0];
		}
	    close (FH);
	    }
	my $totalBasesThisChr = 0;
	my $totalItemsThisChr = 0;
	for my $repClass (sort {$repClassCounts{$b} <=> $repClassCounts{$a} } keys(%repClassCounts))
	    {
	    my $rcIx = $repClasses{$repClass};
	    if (defined(@$repArray[$rcIx]))
		{
		my $countsArray = @$repArray[$rcIx];
		my $itemCount = @$countsArray[0];
		my $chrLen = @$countsArray[1];
		$totalBasesThisChr += $chrLen;
		$totalItemsThisChr += $itemCount;
		}
	    }
	my $percentChromCovered = 0;	# with gaps
	my $percentChromNoGaps = 0;	# without gaps
	if ($chromCount > 0)
	    {
	    my $chrSize = 0;
	    if (exists($chromInfo{$chr}))
		{
		$chrSize = $chromInfo{$chr};
#		$percentChromCovered =
#			100.0*$totalBasesThisChr/$chrSize if ($chrSize > 0);
		$percentChromCovered =
			100.0*$fbCount/$chrSize if ($chrSize > 0);
		}
	    if (exists($chromGaps{$chr}))
		{
		my $gapSize = $chromGaps{$chr};
#		$percentChromNoGaps =
#			100.0*$totalBasesThisChr/($chrSize - $gapSize) if ($chrSize - $gapSize > 0);
		$percentChromNoGaps =
			100.0*$fbCount/($chrSize - $gapSize) if ($chrSize - $gapSize > 0);
		}
	    else
		{
		$percentChromNoGaps = $percentChromCovered;
		}
	    }
	if ($htmlOut)
	    {
	    printf "<TH ALIGN=RIGHT>#&nbsp;%d<BR>%d&nbsp;b<BR>%%&nbsp;%.4f<BR>%%&nbsp;%.4f</TH>", $totalItemsThisChr,$fbCount,$percentChromCovered,$percentChromNoGaps;
	    }
	else
	    {
	    printf "%d,%d,%.4f\t", $totalItemsThisChr,$fbCount,$percentChromCovered;
	    }
	for my $repClass (sort {$repClassCounts{$b} <=> $repClassCounts{$a} } keys(%repClassCounts))
	    {
	    my $rcIx = $repClasses{$repClass};
	    if (defined(@$repArray[$rcIx]))
		{
		my $countsArray = @$repArray[$rcIx];
		my $itemCount = @$countsArray[0];
		my $chrLen = @$countsArray[1];
		my $chrCovered = @$countsArray[2];
		my $chrCoveredNoGaps = @$countsArray[3];
		if ($htmlOut)
		    {
		    printf "<TD ALIGN=RIGHT>#&nbsp;%d<BR>%d&nbsp;b<BR>%%&nbsp;%.4f<BR>%%&nbsp;%.4f</TD>", $itemCount,$chrLen,$chrCovered,$chrCoveredNoGaps;
		    }
	    	else { printf "#%d,%d b,%%%.4f,%%%.4f\t", $itemCount,$chrLen,$chrCovered,$chrCoveredNoGaps; }
		}
	    else
		{
		if ($htmlOut) { print "<TD ALIGN=RIGHT>N/A</TD>"; }
		    else { print "N/A\t" }
		}
	    }
	print "</TR>" if ($htmlOut);
	printf "\n";
	}
    }	#	for each chrom name

########################################################################
#	last row is an overall summary count, if we know the genome size
if (! $genomeSize->is_zero())
    {
    my $genomeCovered = "0";
    my $genomeCoveredNoGaps = "0";
    my $genomeSizeNoGaps = Math::BigInt->new("$genomeSize");
    my $copyTotalLen = Math::BigFloat->new("$totalLen");
    $copyTotalLen->precision(-8);
    $copyTotalLen->bdiv($genomeSize);
    $genomeCovered = sprintf("%.6f",100.0*$copyTotalLen->bsstr());
    if (!$genomeGapSize->is_zero())
	{
	$genomeSizeNoGaps->bsub($genomeGapSize);
	if (!$genomeSizeNoGaps->is_zero())
	    {
	    my $copyTotalLen = Math::BigFloat->new("$totalLen");
	    $copyTotalLen->precision(-8);
	    $copyTotalLen->bdiv($genomeSizeNoGaps);
	    $genomeCoveredNoGaps =
		    sprintf("%.6f",100.0*$copyTotalLen->bsstr());
	    }
	}
    else
	{
	$genomeCoveredNoGaps = $genomeCovered;
	}
    if ($htmlOut)
	{
	printf "<TR><TH>Total<BR>Genome<BR>Summary</TH><TH>#&nbsp;%d<BR>%s&nbsp;b<BR>%%&nbsp;%s<BR>%%&nbsp;%s</TH>",
	    $totalClassCount,$totalLen->bstr(),$genomeCovered,
		$genomeCoveredNoGaps;
	}
    else
	{
	printf "Overall:\t%d,%s,%s,%s\t",
	    $totalClassCount,$totalLen->bstr(),$genomeCovered,
		$genomeCoveredNoGaps;
	}

    for my $repClass (sort {$repClassCounts{$b} <=> $repClassCounts{$a} } keys(%repClassCounts))
	{
	my $classCount = $repClassCounts{$repClass};
	$genomeCovered = "0";
	my $len = $repClassBases{$repClass};
	my $copyLen = Math::BigFloat->new("$len");
	$copyLen->precision(-8);
	$copyLen->bdiv($genomeSize);
	$genomeCovered = sprintf("%.6f",100.0*$copyLen->bsstr());
	$copyLen = Math::BigFloat->new("$len");
	$copyLen->precision(-8);
	$copyLen->bdiv($genomeSizeNoGaps);
	$genomeCoveredNoGaps = sprintf("%.6f",100.0*$copyLen->bsstr());
	printf "<TD ALIGN=RIGHT>#&nbsp;%d<BR>%d&nbsp;b<BR>%%&nbsp;%.4f<BR>%%&nbsp;%.4f</TD>", $classCount,$len,$genomeCovered,$genomeCoveredNoGaps;
	}
    print "</TR>" if ($htmlOut);
    printf "\n";
    }	# last row when we have genomeSize

#	And the last row are the column headings again
if ($htmlOut)
    {
    print "<TR><TH>chrom<BR>name</TH><TH>this&nbsp;chrom<BR>sum</TH>"
    }
else
    {
    print "chrom\tsums\t";
    }

for my $repClass (sort {$repClassCounts{$b} <=> $repClassCounts{$a} } keys(%repClassCounts))
    {
    if ($htmlOut)
	{
	printf "<TH>%s</TH>", $repClass;
	}
    else
	{
	printf "%s\t", $repClass;
	}
    }
print "</TR>" if ($htmlOut);
printf "\n";

print "</TABLE></BODY></HTML>\n" if ($htmlOut);
