#!/usr/bin/env perl
# findScores.pl - search our hive hierarchy for traces of -minScore
#	and -linearGap used in blastz/lastz axtChain operation

#	$Id: findScores.pl,v 1.2 2009/12/22 20:05:10 hiram Exp $

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
    printf "If no scores are found but you know the lastz directories\n";
    printf "exist, go to /hive/data/genomes/organism/bed and construct\n";
    printf "the symlink to lastz.otherOrg, e.g.:\n";
    printf "ln -s lastzMelGal1.2011-03-30 lastz.melGal1\n";
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
# maybe reverse target and query to find result
if (length($chainFile) < 1) {
    $targetDir = "$hiveData/$queryDb";
    $searchDir = "$targetDir/bed/*lastz.$targetDb/axtChain/run";
    $chainFile = `ls -rtd $searchDir/chain.csh 2> /dev/null | tail -1`;
    chomp $chainFile;
}
# ancient style of running blastz
if (length($chainFile) < 1) {
    $targetDir = "$hiveData/$targetDb";
    $searchDir = "$targetDir/bed/*lastz.$queryDb/axtChain/run1";
    $chainFile = `ls -rtd $searchDir/doChain 2> /dev/null | tail -1`;
}
# ancient style of running blastz reverse query and target
if (length($chainFile) < 1) {
    $targetDir = "$hiveData/$queryDb";
    $searchDir = "$targetDir/bed/*lastz.$targetDb/axtChain/run1";
    $chainFile = `ls -rtd $searchDir/doChain 2> /dev/null | tail -1`;
}

if (length($chainFile) < 1) {
    $targetDir = "$hiveData/$targetDb";
    $searchDir = "$targetDir/bed/*lastz.$queryDb/axtChain/run";
    printf STDERR "ERROR: do not find any chain.csh in:\n\n$searchDir\n\n";
    $targetDir = "$hiveData/$queryDb";
    $searchDir = "$targetDir/bed/*lastz.$targetDb/axtChain/run";
    printf STDERR "ERROR: do not find any chain.csh in:\n\n$searchDir\n\n";
    usage;
    exit 255;
}

printf "looking in file:\n  $chainFile\n";
my %foundSome;

open (FH, "<$chainFile") or die "can not read $chainFile";
while (my $line = <FH>) {
    chomp $line;
    my @a = split('\s+', $line);
    for (my $i = 0; $i < scalar(@a); ++$i) {
	if ($a[$i] =~ m/-minScore=|-scoreScheme=|-linearGap/ ) {
	    my ($tag, $value) = split('=',$a[$i]);
	    $foundSome{$tag} = $value;
	    printf "%s\n", $a[$i];
	    if ( $a[$i] =~ m/-scoreScheme/ ) {
		if ( -s $value ) {
		    my $matrix=`cat $value | egrep -v "A|O" | xargs echo | sed -e "s/ /,/g"`;
		    chomp($matrix);
		    printf "matrix 16 %s\n", $matrix;
		}
	    }
	}
    }
}
my $foundCount = 0;
foreach my $key (keys %foundSome) {
    ++$foundCount;
}
printf "default values:\n" if ($foundCount != 3);
if (! exists($foundSome{'-minScore'}) ) {
    printf "chainMinScore 1000\n";
}
if (! exists($foundSome{'-scoreScheme'}) ) {
    printf "matrix 16 91,-114,-31,-123,-114,100,-125,-31,-31,-125,100,-114,-123,-31,-114,91\n";
}
if (! exists($foundSome{'-linearGap'}) ) {
    printf "chainLinearGap loose\n";
}
close (FH);


__END__

[hiram@hgwdev /hive/data/genomes/hg19/bed/lastz.panTro2/axtChain/run] grep -i min *.csh
chain.csh:| axtChain -psl -verbose=0 -scoreScheme=/hive/data/genomes/hg19/bed/lastzHg19Haps.2009-03-09/human_chimp.v2.q  -minScore=5000 -linearGap=medium stdin \
[hiram@hgwdev /hive/data/genomes/hg19/bed/lastz.panTro2/axtChain/run] 
