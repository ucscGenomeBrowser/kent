#!/usr/bin/env perl

# extract gene name, transcript name and protein name from infoOut.txt
#	file output from the gtfToGenePred command

# $Id: extractGtf.pl,v 1.1 2008/02/26 18:23:38 hiram Exp $

use warnings;
use strict;

my $argc = @ARGV;

if (1 != $argc) {
    print STDERR "usage: extractGtf.pl infoOut.txt > ensGtp.tab\n";
    exit 255
}

my $infoOut = shift;

open (FH, "<$infoOut") or die "can not open $infoOut";
my $line = <FH>;	#	expected first line has column titles
$line =~ s/^#//;	#	ignore the # comment delimiter
my @columnTitles = split('\t',$line);
my $geneIx = 0;
my $transcriptIx = 0;
my $proteinIx = 0;
# find the column heading indexes
for (my $i = 0; $i < scalar(@columnTitles); ++$i) {
    if ($columnTitles[$i] =~ m/^geneId$/) {
	$geneIx = $i;
    } elsif ($columnTitles[$i] =~ m/^transId$/) {
	$transcriptIx = $i;
    } elsif ($columnTitles[$i] =~ m/proteinId/) {
	$proteinIx = $i;
    }
}
die "can not find column headings geneId, transId or proteinId in file $infoOut"
    if (($geneIx + $transcriptIx + $proteinIx) < 3);

while ($line = <FH>) {
    my @fields = split('\t', $line);
    my $proteinId = "";
    if (defined($fields[$proteinIx])) {
	$proteinId = $fields[$proteinIx];
    }
    die "missing gene name for ensGtf from $infoOut line: $.\n'$line'\n"
	if (!defined($fields[$geneIx]));
    die "missing transcript for ensGene: $fields[$geneIx]\n"
	if (!defined($fields[$transcriptIx]));
    printf "%s\t%s\t%s\n",
	$fields[$geneIx], $fields[$transcriptIx], $proteinId;
}
close (FH);

__END__
#transId^IgeneId^Isource^Ichrom^Istart^Iend^Istrand^IproteinId^IgeneName^ItranscriptName$
ENSOANT00000029087^IENSOANG00000019924^Iprotein_coding^IContig99981^I265^I1052^I+^IENSOANP00000025283^I^I$
