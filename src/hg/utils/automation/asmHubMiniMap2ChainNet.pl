#!/usr/bin/env perl

use strict;
use warnings;
use FindBin qw($Bin);
use lib "$Bin";
use AsmHub;
use HgAutomate;
use File::Basename;

my $argc = scalar(@ARGV);

if ($argc != 4) {
  printf STDERR "usage: asmHubMiniMap2ChainNet.pl asmId ncbiAsmId asmId.names.tab queryId > asmId.miniMap2ChainNet.html\n";
  printf STDERR "where asmId is the assembly identifier,\n";
  printf STDERR "and   asmId.names.tab is naming file for this assembly,\n";
  printf STDERR "and   queryId is the asmId or db of the other organism,\n";
  exit 255;
}

# specific to UCSC environment
my $dbHost = "hgwdev";

my $asmId = shift;
my $ncbiAsmId = shift;
my $namesFile = shift;
my $queryId = shift;

# if assembly hub, need to find the real full assembly ID
if ($queryId =~ m/^GC/) {
  my $gcX = substr($queryId,0,3);
  my $d0 = substr($queryId,4,3);
  my $d1 = substr($queryId,7,3);
  my $d2 = substr($queryId,10,3);
  my $hubBuildDir = "refseqBuild";
  $hubBuildDir = "genbankBuild" if ($gcX eq "GCA");
  $queryId = `ls -d /hive/data/genomes/asmHubs/$hubBuildDir/$gcX/$d0/$d1/$d2/${queryId}*`;
  chomp $queryId;
  $queryId =~ s#.*/##;
}

my $ncbiAssemblyId = `grep -v "^#" $namesFile | cut -f10`;
chomp $ncbiAssemblyId;
my $sciName = `grep -v "^#" $namesFile | cut -f5`;
chomp $sciName;

my ($tGenome, $tDate, $tSource) = &HgAutomate::getAssemblyInfo($dbHost, $asmId);
my ($qGenome, $qDate, $qSource) = &HgAutomate::getAssemblyInfo($dbHost, $queryId);

print <<_EOF_
<h2>Description</h2>
<p>
This track shows regions of the genome that are alignable
to $qGenome (&quot;chain&quot; subtrack), and the subset of that
chain used to lift over coordinates from this genome to $qGenome
(&quot;lift over&quot; subtrack). The alignable parts are shown with
thick blocks that look like exons. Non-alignable parts between these
are shown like introns.
</p>
<p>
This alignment was made with <em>minimap2</em> rather than
<em>lastz</em>.  It is intended for a pair of very similar
same-species/same-individual assemblies -- for example the two
haplotypes of a trio-binned or hifiasm/verkko diploid assembly, or
two closely related strain assemblies -- where minimap2's splice-free
asm5/asm10/asm20 presets do a better job with the larger indels and
structural differences seen between such assemblies than lastz's
general-purpose scoring does.
</p>
<h3>Chain Track</h3>
<p>
The chain track shows alignments of $qGenome/($qDate) to the
$tGenome/$sciName/$ncbiAssemblyId/$tDate genome using a gap scoring system that allows longer gaps
than traditional affine gap scoring systems. It can also tolerate gaps in both
<em>$qGenome</em> and <em>$tGenome</em> simultaneously. These
&quot;double-sided&quot; gaps can be caused by local inversions and
overlapping deletions in both species.
</p>
<p>
The chain track displays boxes joined together by either single or
double lines. The boxes represent aligning regions.
Single lines indicate gaps that are largely due to a deletion in the
<em>$qGenome</em> assembly or an insertion in the <em>$tGenome</em>
assembly.  Double lines represent more complex gaps that involve substantial
sequence in both species. This may result from inversions, overlapping
deletions, an abundance of local mutation, or an unsequenced gap in one
species.</p>
<P>
In the &quot;pack&quot; and &quot;full&quot; display
modes, the individual feature names indicate the chromosome, strand, and
location (in thousands) of the match for each matching alignment.</P>

<h3>Lift Over Chain Track</h3>
<p>
The lift over chain track shows the subset of the chain, extracted
with <em>netChainSubset</em> from the best/longest syntenic regions
of the net, that is used to translate coordinates from the
<em>$tGenome</em> genome to the <em>$qGenome</em> genome.</p>

<h2>Display Conventions and Configuration</h2>
<p>By default, the chains to chromosome-based assemblies are colored
based on which chromosome they map to in the aligning organism. To turn
off the coloring, check the &quot;off&quot; button next to: Color
track based on chromosome.</p>
<p>
To display only the chains of one chromosome in the aligning
organism, enter the name of that chromosome (e.g. chr4) in box next to:
Filter by chromosome.</p>

<h2>Methods</h2>
<h3>Chain track</h3>
<p>
The <em>$qGenome</em> genome was aligned to <em>$tGenome</em> genome with
<em>minimap2</em>, one job per target sequence against the whole query
genome. The resulting PAF alignments were converted into PSL format using
the <em>pafToPsl</em> program. The PSL alignments were fed into
<em>axtChain</em>, which organizes all alignments between a single
<em>$qGenome</em> chromosome and a single <em>$tGenome</em> chromosome
into a group and creates a kd-tree out of the gapless subsections (blocks)
of the alignments. A dynamic program was then run over the kd-trees to
find the maximally scoring chains of these blocks.
</p>

<h3>Lift over chain track</h3>
<p>
The chain was placed with <em>chainNet</em>, trimming it as necessary to
fit into sections not already covered by a higher-scoring part of the
chain, then classified with <em>netSyntenic</em> and <em>netClass</em>.
The program <em>netChainSubset</em> was then used to extract the
best/longest syntenic part of that net back out of the chain, giving the
chain used to lift over coordinates between the two assemblies.
</p>

<h2>Credits</h2>
<p>
<em>minimap2</em> was developed by Heng Li.
</p>
<p>
The <em>mash</em> program, used to estimate the divergence between the
target and query assemblies and automatically select the minimap2
alignment preset, was developed by Brian Ondov, Todd Treangen, and
colleagues at the National Biodefense Analysis and Countermeasures
Center and the University of Maryland.
</p>
<p>
The <em>axtChain</em> program was developed at the University of California at
Santa Cruz by Jim Kent with advice from Webb Miller and David Haussler.</p>
<p>
The <em>chainNet</em>, <em>netSyntenic</em>, and <em>netClass</em> programs
were developed at the University of California
Santa Cruz by Jim Kent.</p>

<h2>References</h2>
<p>
Li H.
<a href="https://doi.org/10.1093/bioinformatics/bty191"
target=_blank>Minimap2: pairwise alignment for nucleotide sequences</a>.
<em>Bioinformatics</em>. 2018 Sep 15;34(18):3094-3100.
PMID: <a href="https://www.ncbi.nlm.nih.gov/pubmed/29750242" target="_blank">29750242</a>; PMC: <a
href="https://www.ncbi.nlm.nih.gov/pmc/articles/PMC6137996/" target="_blank">PMC6137996</a>
</p>

<p>
Ondov BD, Treangen TJ, Melsted P, Mallonee AB, Bergman NH, Koren S, Phillippy AM.
<a href="https://doi.org/10.1186/s13059-016-0997-x"
target=_blank>Mash: fast genome and metagenome distance estimation using MinHash</a>.
<em>Genome Biol</em>. 2016 Jun 20;17(1):132.
PMID: <a href="https://www.ncbi.nlm.nih.gov/pubmed/27323842" target="_blank">27323842</a>; PMC: <a
href="https://www.ncbi.nlm.nih.gov/pmc/articles/PMC4915045/" target="_blank">PMC4915045</a>
</p>

<p>
Kent WJ, Baertsch R, Hinrichs A, Miller W, Haussler D.
<A HREF="https://www.pnas.org/content/100/20/11484"
TARGET=_blank>Evolution's cauldron:
duplication, deletion, and rearrangement in the mouse and human genomes</A>.
<em>Proc Natl Acad Sci U S A</em>. 2003 Sep 30;100(20):11484-9.
PMID: <a href="https://www.ncbi.nlm.nih.gov/pubmed/14500911" target="_blank">14500911</a>; PMC: <a
href="https://www.ncbi.nlm.nih.gov/pmc/articles/PMC208784/" target="_blank">PMC208784</a>
</p>

_EOF_
    ;
