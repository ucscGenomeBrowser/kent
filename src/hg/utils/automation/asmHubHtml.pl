#!/usr/bin/env perl

use strict;
use warnings;
use FindBin qw($Bin);
use lib "$Bin";
use AsmHub;
use File::Basename;

my $argc = scalar(@ARGV);

if ($argc != 2) {
  printf STDERR "usage: hubHtml.pl <hubLink> <pathTo>/<base> >> hubIndex.html\n";
  printf STDERR "will look for files <pathTo>/<base>/html/<base>.names.tab and\n";
  printf STDERR "<pathTo>/html/<base>.build.stats.txt\n";
  exit 255;
}

my $hubLink = shift;
my $baseName = shift;
my $asmId = basename($baseName);
my $ftpName = dirname($baseName);
$ftpName =~ s#/hive/data/inside/ncbi/##;
my $namesFile = "$baseName/html/$asmId.names.tab";
my $statsFile = "$baseName/html/$asmId.build.stats.txt";
my $faCountFile = "$baseName/$asmId.faCount.signature.txt";
my $geneStatsFile = "$baseName.ncbiGene.ncbi.stats.txt";
my $contigCount = 0;
my $genomeSize = 0;
my $n50 = 0;
my $totalNucleotides = 0;
my $adenine = 0;
my $cytosine = 0;
my $guanine = 0;
my $thymine = 0;
my $gapsN = 0;
my $CpG = 0;
my $gcContent = 0;
my $NperCent = 0;

# single line file, three numbers: contigCount genomeSize N50
open (FH, "<$statsFile") or die "can not read $statsFile";
while (my $line = <FH>) {
  chomp $line;
  ($contigCount, $genomeSize, $n50) = split('\s+', $line);
}
close (FH);

# single line file, the last line, the 'total' line output of faCount:
# #seq    len     A       C       G       T       N       cpg
# total   16569   5124    5181    2169    4094    1       435

my $geneCount = 0;
my $genePercentCoverage = 0;
my $geneBasesCovered = 0;

if ( -s $geneStatsFile ) {
  my $geneStats=`cat $geneStatsFile | awk '{printf "%d\\n", \$2}' | xargs echo`;
  chomp $geneStats;
  ($geneCount, $geneBasesCovered) = split('\s+', $geneStats);
  $genePercentCoverage = 0;
  if ($genomeSize > 0) {
  $genePercentCoverage = sprintf("%.3f", 100.0 * $geneBasesCovered/$genomeSize);
  }
}

open (FH, "grep '^total' $faCountFile 2> /dev/null|tail -1|") or die "can not read $faCountFile";
while (my $line = <FH>) {
  chomp $line;
  (undef, $totalNucleotides, $adenine, $cytosine, $guanine, $thymine, $gapsN, $CpG) = split('\s+', $line);
  $gcContent = 100.0*($cytosine+$guanine)/$totalNucleotides if ($totalNucleotides > 0);
  $NperCent = 100.0*$gapsN/$totalNucleotides if($totalNucleotides > 0);
}
close (FH);


my $hubText = "hub.txt";
# first line is a comment, second line is the set of names data
open (FH, "grep -v '^#' $namesFile|") or die "can not read $namesFile";
while (my $line = <FH>) {
  chomp $line;
  my ($taxId, $commonName, $submitter, $asmName, $sciName, $bioSample, $asmType, $asmLevel, $asmDate, $asmAccession) = split('\t', $line);
  $asmDate =~ s/ /&nbsp;/g;
  $sciName =~ s/ *\(.*//;
  $sciName =~ s/ /&nbsp;/g;
  printf "<tr align=\"right\"><td><a href=\"https://www.ncbi.nlm.nih.gov/Taxonomy/Browser/wwwtax.cgi?id=%d\" target=_\"_blank\"> %d</a>", $taxId, $taxId;
  printf "<td>%s</td>", $asmDate;
  printf "<td><a href='' class='hgGateway' hubTxt='%s/%s'  asmId='%s_%s' target='_blank'>%s</a></td>", $hubLink, $hubText, $asmAccession, $asmName, $commonName;
  printf "<td>%s</td>", $sciName;
  if ($bioSample ne "(n/a)") {
    printf "<td><a href=\"https://www.ncbi.nlm.nih.gov/biosample/?term=%s\" target=\"_blank\"> %s</a></td>", $bioSample, $bioSample;
  } else {
    printf "<td>(n/a)</td>";
  }
  printf "<td align=\"right\">%s</td>", &AsmHub::commify($contigCount);
  printf "<td align=\"right\">%s</td>", &AsmHub::commify($genomeSize);
  printf "<td align=\"right\">%s</td>", &AsmHub::commify($n50);
  printf "<td align=\"right\">%%&nbsp;%.2f</td>", $gcContent;
  printf "<td align=\"right\">%s<br>%%&nbsp;%.2f</td>", &AsmHub::commify($gapsN), $NperCent;
  printf "<td align=\"right\">%s<br>%s<br>%%&nbsp;%.2f</td>", &AsmHub::commify($geneCount), &AsmHub::commify($geneBasesCovered), $genePercentCoverage;
  printf "<td><a href=\"https://www.ncbi.nlm.nih.gov/assembly/%s\" target=\"_blank\">%s</a></td>", $asmAccession, $asmAccession;
  printf "<td><a href=\"ftp://ftp.ncbi.nlm.nih.gov/%s\" target=\"_blank\">%s</a></td>", $ftpName, $asmName;
  printf "<td>%s</td>", $asmType;
  printf "<td>%s</td>", $asmLevel;
  printf "<td>%s</td></tr>\n", $submitter;
}
close (FH);
