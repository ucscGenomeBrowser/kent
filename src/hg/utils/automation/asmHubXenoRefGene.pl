#!/usr/bin/env perl

use strict;
use warnings;
use FindBin qw($Bin);
use lib "$Bin";
use AsmHub;
use File::Basename;

my $argc = scalar(@ARGV);

if ($argc != 3) {
  printf STDERR "usage: asmHubXenoRefGene.pl asmId asmId.names.tab .../trackData/\n";
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
my $xenoRefGeneBbi = "$trackDataDir/xenoRefGene/$asmId.xenoRefGene.bb";
my $asmType = "refseq";

if ( ! -s $xenoRefGeneBbi ) {
  printf STDERR "ERROR: can not find $asmId.xenoRefGene.bb file\n";
  exit 255;
}

my @partNames = split('_', $asmId);
my $ftpDirPath = sprintf("%s/%s/%s/%s/%s", $partNames[0],
   substr($partNames[1],0,3), substr($partNames[1],3,3),
   substr($partNames[1],6,3), $asmId);

$asmType = "genbank" if ($partNames[0] =~ m/GCA/);
my $totalBases = `ave -col=2 $trackDataDir/../${asmId}.chrom.sizes | grep "^total" | awk '{printf "%d", \$2}'`;
chomp $totalBases;
my $geneStats = `cat $trackDataDir/xenoRefGene/${asmId}.xenoRefGene.stats.txt | awk '{printf "%d\\n", \$2}' | xargs echo`;
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
The RefSeq mRNAs gene track for the $assemblyDate $em${organism}$noEm/$asmId
genome assembly displays translated blat alignments of vertebrate and
invertebrate mRNA in
<a href="https://www.ncbi.nlm.nih.gov/genbank/" target="_blank"> GenBank</a>.
</p>

<h2>Track statistics summary</h2>
<p>
<b>Total genome size: </b>$totalBases</br>
<b>Gene count: </b>$itemCount<br>
<b>Bases in genes: </b>$basesCovered</br>
<b>Percent genome coverage: </b>% $percentCoverage</br>
</p>

<h2>Methods</h2>

<p>
The mRNAs were aligned against the $em${organism}$noEm/$asmId genome using
translated blat.  When a single mRNA aligned in multiple places, the alignment
having the highest base identity was found.  Only those alignments having a base
identity level within 1% of the best and at least 25% base identity with the
genomic sequence were kept.
</p>

<p>
Specifically, the translated blat command is:
<pre>
blat -noHead -q=rnax -t=dnax -mask=lower target.fa query.fa target.query.psl

where target.fa is one of the chromosome sequence of the genome assembly,
and the query.fa is the mRNAs from RefSeq
</pre>
The resulting PSL outputs are filtered:
<pre>
pslCDnaFilter -minId=0.35 -minCover=0.25  -globalNearBest=0.0100 -minQSize=20 \
  -ignoreIntrons -repsAsMatch -ignoreNs -bestOverlap \
    all.results.psl $asmId.xenoRefGene.psl
</pre>
The filtered $asmId.xenoRefGene.psl is converted to
<a href='http://genome.ucsc.edu/FAQ/FAQformat.html#format9'
target=_blank>genePred data</a> to display for this track.
</p>

<H2>Credits</h2>

<p>
The mRNA track was produced at UCSC from mRNA sequence data
submitted to the international public sequence databases by
scientists worldwide.
</p>

<h2>References</h2>
<p>
Benson DA, Cavanaugh M, Clark K, Karsch-Mizrachi I, Lipman DJ, Ostell J, Sayers EW.
<a href="https://academic.oup.com/nar/article/41/D1/D36/1068219/GenBank" target="_blank">
GenBank</a>.
<em>Nucleic Acids Res</em>. 2013 Jan;41(Database issue):D36-42.
PMID: <a href="https://www.ncbi.nlm.nih.gov/pubmed/23193287" target="_blank">23193287</a>; PMC: <a
href="https://www.ncbi.nlm.nih.gov/pmc/articles/PMC3531190/" target="_blank">PMC3531190</a>
</p>

<P>
Benson DA, Karsch-Mizrachi I, Lipman DJ, Ostell J, Wheeler DL.
<A HREF="https://academic.oup.com/nar/article/32/suppl_1/D23/2505202/GenBank-update"
TARGET=_blank>GenBank: update</A>.
<em>Nucleic Acids Res</em>. 2004 Jan 1;32(Database issue):D23-6.
PMID: <a href="https://www.ncbi.nlm.nih.gov/pubmed/14681350" target="_blank">14681350</a>; PMC: <a
href="https://www.ncbi.nlm.nih.gov/pmc/articles/PMC308779/" target="_blank">PMC308779</a>
</p>

<P>
Kent WJ.
<A HREF="https://genome.cshlp.org/content/12/4/656.abstract"
TARGET=_blank>BLAT - the BLAST-like alignment tool</A>.
<em>Genome Res</em>. 2002 Apr;12(4):656-64.
PMID: <a href="https://www.ncbi.nlm.nih.gov/pubmed/11932250" target="_blank">11932250</a>; PMC: <a
href="https://www.ncbi.nlm.nih.gov/pmc/articles/PMC187518/" target="_blank">PMC187518</a>
</p>

_EOF_
   ;
