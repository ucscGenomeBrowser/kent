#!/usr/bin/env perl

use strict;
use warnings;
use FindBin qw($Bin);
use lib "$Bin";
use AsmHub;
use File::Basename;

my $argc = scalar(@ARGV);

if ($argc != 3) {
  printf STDERR "usage: asmHubNcbiGene.pl asmId asmId.names.tab .../trackData/\n";
  printf STDERR "where asmId is the assembly identifier,\n";
  printf STDERR "and .../trackData/ is the path to the /trackData/ directory.\n";
  exit 255;
}

# from Perl Cookbook Recipe 2.17, print out large numbers with comma
# delimiters:
sub commify($) {
    my $text = reverse $_[0];
    $text =~ s/(\d\d\d)(?=\d)(?!\d*\.)/$1,/g;
    return scalar reverse $text
}

my $asmId = shift;
my $namesFile = shift;
my $trackDataDir = shift;
my $ncbiGeneBbi = "$trackDataDir/ncbiGene/$asmId.ncbiGene.bb";
my $asmType = "refseq";

if ( ! -s $ncbiGeneBbi ) {
  printf STDERR "ERROR: can not find $asmId.ncbiGene.bb file\n";
  exit 255;
}

my @partNames = split('_', $asmId);
my $ftpDirPath = sprintf("%s/%s/%s/%s/%s", $partNames[0],
   substr($partNames[1],0,3), substr($partNames[1],3,3),
   substr($partNames[1],6,3), $asmId);

$asmType = "genbank" if ($partNames[0] =~ m/GCA/);
my $totalBases = `ave -col=2 $trackDataDir/../${asmId}.chrom.sizes | grep "^total" | awk '{printf "%d", \$2}'`;
chomp $totalBases;
my $geneStats = `cat $trackDataDir/ncbiGene/${asmId}.ncbiGene.stats.txt | awk '{printf "%d\\n", \$2}' | xargs echo`;
chomp $geneStats;
my ($itemCount, $basesCovered) = split('\s+', $geneStats);
my $percentCoverage = sprintf("%.3f", 100.0 * $basesCovered / $totalBases);
$itemCount = commify($itemCount);
$basesCovered = commify($basesCovered);
$totalBases = commify($totalBases);

my $em = "<em>";
my $noEm = "</em>";
my $assemblyDate = `grep -v "^#" $namesFile | cut -f9`;
chomp $assemblyDate;
my $ncbiAssemblyId = `grep -v "^#" $namesFile | cut -f10`;
chomp $ncbiAssemblyId;
my $organism = `grep -v "^#" $namesFile | cut -f5`;
chomp $organism;

print <<_EOF_
<h2>Description</h2>
<p>
The NCBI Gene track for the $assemblyDate $em${organism}$noEm/$asmId
genome assembly is constructed from the gff file <b>${asmId}_genomic.gff.gz</b>
delivered with the NCBI RefSeq genome assemblies at the FTP location:<br>
<a href='ftp://ftp.ncbi.nlm.nih.gov/genomes/all/$ftpDirPath/' target='_blank'>ftp://ftp.ncbi.nlm.nih.gov/genomes/all/$ftpDirPath/</a>
</p>

<h2>Track statistics summary</h2>
<p>
<b>Total genome size: </b>$totalBases<br>
<b>Gene count: </b>$itemCount<br>
<b>Bases in genes: </b>$basesCovered<br>
<b>Percent genome coverage: </b>% $percentCoverage<br>
</p>

_EOF_
   ;
