#!/usr/bin/env perl

use strict;
use warnings;
use FindBin qw($Bin);
use lib "$Bin";
use AsmHub;
use File::Basename;

my $argc = scalar(@ARGV);

if ($argc != 3) {
  printf STDERR "usage: asmHubSimpleRepeat.pl asmId asmId.names.tab buildDir\n";
  printf STDERR "where asmId is the assembly identifier,\n";
  printf STDERR "and   asmId.names.tab is naming file for this assembly,\n";
  printf STDERR "and buildDir is the directory with bbi/asmId.simpleRepeats.bb\n";
  printf STDERR "    and the asmId.chrom.sizes\n";
  exit 255;
}

my $asmId = shift;
my $namesFile = shift;
my $buildDir = shift;
my $trfBbi = "$buildDir/bbi/$asmId.simpleRepeat.bb";
my $chromSizes = "$buildDir/$asmId.chrom.sizes";

if ( ! -s $trfBbi ) {
  printf STDERR "ERROR: can not find CpG masked file:\n\t'%s'\n", $trfBbi;
  exit 255;
}

my $em = "<em>";
my $noEm = "</em>";
my $assemblyDate = `grep -v "^#" $namesFile | cut -f9`;
chomp $assemblyDate;
my $ncbiAssemblyId = `grep -v "^#" $namesFile | cut -f10`;
chomp $ncbiAssemblyId;
my $organism = `grep -v "^#" $namesFile | cut -f5`;
chomp $organism;
my $itemCount = `bigBedInfo $trfBbi | egrep "itemCount:" | sed -e 's/itemCount: //;'`;
my $basesCovered = `bigBedInfo $trfBbi | egrep "basesCovered:" | sed -e 's/basesCovered: //;'`;
chomp $itemCount;
chomp $basesCovered;
my $bases = $basesCovered;
$bases =~ s/,//g;
my $asmSize = &AsmHub::asmSize($chromSizes);
my $percentCoverage = sprintf("%.2f", (100.0 * $bases) / $asmSize);
$asmSize = &AsmHub::commify($asmSize);

print <<_EOF_
<h2>Description</h2>
<p>
This track displays simple tandem repeats (possibly imperfect repeats) on
the $assemblyDate $em${organism}$noEm/$asmId/$ncbiAssemblyId genome assembly,
located by <a href="http://tandem.bu.edu/trf/trf.submit.options.html" 
target=_blank>Tandem Repeats
Finder</a> (TRF) which is specialized for this purpose. These repeats can
occur within coding regions of genes and may be quite
polymorphic. Repeat expansions are sometimes associated with specific
diseases.</p>

There are $itemCount items in the track covering $basesCovered bases,
assembly size $asmSize bases, percent coverage % $percentCoverage.

<h2>Methods</h2>
<p>
For more information about the TRF program, see Benson (1999).
</p>

<h2>Credits</h2>
<p>
TRF was written by
<a href="http://tandem.bu.edu/benson.html" target=_blank>Gary Benson</a>.</p>

<h2>References</h2>

<p>
Benson G.
<a href="http://nar.oxfordjournals.org/content/27/2/573.abstract" target="_blank">
Tandem repeats finder: a program to analyze DNA sequences</a>.
<em>Nucleic Acids Res</em>. 1999 Jan 15;27(2):573-80.
PMID: <a href="https://www.ncbi.nlm.nih.gov/pubmed/9862982" target="_blank">9862982</a>; PMC: <a
href="https://www.ncbi.nlm.nih.gov/pmc/articles/PMC148217/" target="_blank">PMC148217</a>
</p>
<h2>Credits</h2>

<p>This track was generated using a modification of a program developed by G. Miklem and L. Hillier 
(unpublished).</p>

<h2>References</h2>

<p>
Gardiner-Garden M, Frommer M.
<a href="http://www.sciencedirect.com/science/article/pii/0022283687906899" target="_blank">
CpG islands in vertebrate genomes</a>.
<em>J Mol Biol</em>. 1987 Jul 20;196(2):261-82.
PMID: <a href="https://www.ncbi.nlm.nih.gov/pubmed/3656447" target="_blank">3656447</a>
</p>
_EOF_
   ;
