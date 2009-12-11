#!/usr/bin/env perl
# findScores.pl - search our hive hierarchy for traces of -minScore
#	and -linearGap used in blastz/lastz axtChain operation

#	$Id: findScores.pl,v 1.1 2009/12/11 21:36:03 hiram Exp $

use strict;
use warnings;

my $hiveData = "/hive/data/genomes";
my $argc = scalar(@ARGV);

sub usage() {
    printf "findScores.pl - search hive genomes directory log files for\n";
    printf "         evidence of -minScore and -linearGap used in axtChain\n";
    printf "usage: findScores.pl <targetDb> <queryDb>\n";
    printf "  will look for arguments in:\n";
    printf "  $hiveData/targetDb/bed/*astz.queryDb/axtChain/run/chain.csh\n";
}

if ($argc != 2) {
    usage;
    exit 255;
}

my $targetDb = shift;
my $queryDb = shift;

my $targetDir = "$hiveData/$targetDb";

if ( ! -d $targetDir ) {
    printf STDERR "ERROR: can not find target genome directory:\n\n";
    printf STDERR "$targetDir\n\n";
    usage;
    exit 255;
}

my $searchDir = "$targetDir/bed/*lastz.$queryDb/axtChain/run";
my $chainFile = `ls -rtd $searchDir/chain.csh 2> /dev/null | tail -1`;
chomp $chainFile;
if (length($chainFile) < 1) {
    printf STDERR "ERROR: do not find any chain.csh in:\n\n$searchDir\n\n";
    printf STDERR "maybe try: findScores.pl $queryDb $targetDb\n\n";
    usage;
    exit 255;
}

printf "looking in file:\n  $chainFile\n";

open (FH, "<$chainFile") or die "can not read $chainFile";
while (my $line = <FH>) {
    chomp $line;
    my @a = split('\s+', $line);
    for (my $i = 0; $i < scalar(@a); ++$i) {
	if ($a[$i] =~ m/-minScore=|-scoreScheme=|-linearGap/ ) {
	    printf "%s\n", $a[$i];
	}
    }
}
close (FH);


__END__

[hiram@hgwdev /hive/data/genomes/hg19/bed/lastz.panTro2/axtChain/run] grep -i min *.csh
chain.csh:| axtChain -psl -verbose=0 -scoreScheme=/hive/data/genomes/hg19/bed/lastzHg19Haps.2009-03-09/human_chimp.v2.q  -minScore=5000 -linearGap=medium stdin \
[hiram@hgwdev /hive/data/genomes/hg19/bed/lastz.panTro2/axtChain/run] 
