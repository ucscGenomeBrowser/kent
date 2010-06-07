#!/usr/bin/env perl

#	$Id: fixup.hbrc.pl,v 1.1 2006/02/07 19:51:29 hiram Exp $"

use warnings;
use strict;

my $argc = scalar(@ARGV);

sub usage() {
    print "usage: ./fixup.hbrc.pl hbrc.txt \\\n";
    print "\t/cluster/data/hg18/bed/fishClones/seq_clone.pmd > fixed.hbrc.txt\n";
    print "the seq_clone.pmd file was obtained via email from Wonhee Jang\n";
    print 'jang@ncbi.nlm.nih.gov - I have asked for clarification where', "\n";
    print "such a file can be fetched without resorting to email.\n";
    exit 255;
}

if ($argc != 2) {
    usage;
}

my $hbrcFile = shift;
my $seqCloneFile = shift;

open (hbrcFH,"<$hbrcFile") or die "Can not open $hbrcFile";
open (seqCloneFH,"grep ref_assembly $seqCloneFile|") or die "Can not open $seqCloneFile";

my %cloneToContig;

my $linesRead = 0;
while (my $line = <seqCloneFH>) {
    ++$linesRead;
    chomp $line;
    my ($contig, $num9606, $chr, $start, $end, $strand, $cloneName, $rest) =
	split('\t',$line);
    if (exists($cloneToContig{$cloneName})) {
	if ($cloneToContig{$cloneName} !~ m/$contig/) {
	    my $addTo = "$cloneToContig{$cloneName};$contig";
	    $cloneToContig{$cloneName} = $addTo;
printf STDERR "$linesRead: $cloneName\t$addTo\n";
	}
	else { next; }
    }
    $cloneToContig{$cloneName} = $contig;
    if (0 == ($linesRead % 1000)) {
	print STDERR "$linesRead: $cloneName \t$contig\n";
    }
}
close(seqCloneFH);

printf STDERR "read $linesRead lines from $seqCloneFile\n";

$linesRead = 0;
while (my $line = <hbrcFH>) {
    ++$linesRead;
    chomp $line;
    if (1 == $linesRead) {
print "#fish_chr\tclone_name\tDistributor\tFISH\tInsert_accession\tSTSname_UniSTSid\tBAC-end\tContig\tflag\n";
	next;
    }
    if ($line =~ m/^#/) { print "$line\n"; next }
    my ($fishChr, $cloneName, $distributor, $FISH, $placeGenome,
	$placeSummary, $insertAcc, $stsName, $bacEnd, $flag) =split('\t',$line);
    if (!defined($cloneName)) {
printf STDERR "no clone name ?  at line $linesRead in $hbrcFile\n";
printf STDERR "$line\n";
	exit 255
    }
    if ($cloneName =~ m/std_name/) { next; }
    my $contig = "";
    if (defined($cloneName) && exists($cloneToContig{$cloneName})) {
	$contig = $cloneToContig{$cloneName};
    }
    if (!defined($fishChr)) {
	$fishChr = ""; 
printf STDERR "no fish_chr at line $linesRead in $hbrcFile\n";
    }
    if (!defined($bacEnd)) {
	$fishChr = ""; 
printf STDERR "no BAC-end at line $linesRead in $hbrcFile\n";
    }
    if (!defined($flag)) {
	$flag = ""; 
    }
    $bacEnd =~ s/, /;/g;
    $bacEnd =~ s/multiple_placements_on_genome//;
#    $stsName =~ s/, /;/g;
#    $placeSummary =~ s/, /;/g;
    my @b;
    $b[0] = $placeSummary;
    $b[1] = $stsName;
    for (my $j = 0; $j < 2; ++$j) {
	my @names = split(', ',$b[$j]);
	my $nameCount = scalar(@names);
	for (my $i = 0; $i < $nameCount; ++$i) {
	    my ($acc, $ver) = split('\.', $names[$i]);
	    $ver =~ s/[A-Z]+.*//i;
	    $ver =~ s/\*.*//i;
	    $names[$i] = "$acc.$ver";
	}
	if ($nameCount > 0) { $b[$j] = $names[0]; } else
	    { $b[$j] = ""; }
	for (my $i = 1; $i < $nameCount; ++$i) {
	    $b[$j] = "$b[$j];$names[$i]";
	}
    }
    $placeSummary = $b[0];
    $stsName = $b[1];
    $FISH =~ s/^multiple centromere.*//;
    $FISH =~ s/ and at //;
    $FISH =~ s/ //g;
    printf "%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n", $fishChr, $cloneName,
	$distributor, $FISH, $placeSummary, $stsName, $bacEnd, $contig,
	$flag;
}
printf STDERR "read $linesRead lines from $hbrcFile\n";

close(hbrcFH);

__END__

#  columns remaining the same:
1.  fish_chr
2.  clone_name
3.  Distributor
4.  FISH

#  rearranged columns
old:new
5:6  Insert_accession, Placement summary
6:8  STSname_UniSTSid
7:9  BAC-end
9:10  flag

#  missing column:
8 Contig

Previous hbrc file format:


#fish_chr	clone_name	Distributor	FISH	Insert_accession	STSname_UniSTSid	BAC-end	Contig	flag
1	RP4-703E10	S	1p36.32~36.33(SC)	AL139823.11			NT_004547	
1	RP11-671C15	C	1p36.3(FHCRC)	AC079860.3		AQ407341.1;AQ430410.1	NT_079624	map location ambiguous
1	RP11-33M12	S	1p36.31~36.33(SC)	AL356693.37		AQ046869.1	NT_021937	
1	RP11-58A11	C	1p36.3(FHCRC)	AL591866.13	R34121 44503	AQ195777.1;AQ195780.1	NT_021937	
1	RP11-181G12	C	1p36.3(FHCRC)	AC026932.2;AC069581.1;AL590822.36				

And this current hbrc file format

#fish_chr	clone_name	Distributor	FISH	Placement on genome	Placement summary	Insert_accession	STSname_UniSTSid	BAC-end	flag
1	CTC-232B23		1pter(UChicago)					
1	RP4-703E10	S	1p36.32~36.33(SC)	chr1 5Mb	AL139823.11			
1	RP1-187A9	S	1p36.31~36.33(SC)	chr1 3Mb	AL359826.6			
1	RP11-181G12	C	1p36.3(FHCRC)	chr1 2Mb	AL590822.36, AC026932.2*u, AC069581.1*u			
1	RP11-201E15	C	1p36.3(FHCRC)					
