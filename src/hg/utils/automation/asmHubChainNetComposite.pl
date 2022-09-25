#!/usr/bin/env perl

use strict;
use warnings;
use FindBin qw($Bin);
use lib "$Bin";
use AsmHub;
use HgAutomate;
use File::Basename;

my $argc = scalar(@ARGV);

if ($argc != 3) {
  printf STDERR "usage: asmHubChainNet.pl asmId ncbiAsmId asmId.names.tab > asmId.chainNet.html\n";
  printf STDERR "where asmId is the assembly identifier,\n";
  printf STDERR "and   asmId.names.tab is naming file for this assembly,\n";
  exit 255;
}

# specific to UCSC environment
my $dbHost = "hgwdev";

my $asmId = shift;
my $ncbiAsmId = shift;
my $namesFile = shift;

my $queryId = "";

my $targetBuildDir = "";
if ($asmId =~ m/^GC/) {
  my $gcX = substr($asmId,0,3);
  my $d0 = substr($asmId,4,3);
  my $d1 = substr($asmId,7,3);
  my $d2 = substr($asmId,10,3);
  my $hubBuildDir = "refseqBuild";
  $hubBuildDir = "genbankBuild" if ($gcX eq "GCA");
  $targetBuildDir = "/hive/data/genomes/asmHubs/$hubBuildDir/$gcX/$d0/$d1/$d2/$asmId";
} else {
  $targetBuildDir = "/hive/data/genomes/$asmId";
}

my %queryDates;	# key is asmId, value is assembly date
my %queryCommonName;	# key is asmId, value is assembly common name
my %querySubmitter;	# key is asmId, value is assembly submitter

open (TD, "ls -d ${targetBuildDir}/trackData/lastz.*|") or die "can not ls -d ${targetBuildDir}/trackData/lastz.*";
while (my $accession = <TD>) {
  chomp $accession;
  $accession =~ s#.*/trackData/lastz.##;
  # the hubDateName will translate the accessionId into an asmId
  # side effect it also returns the date
  my ($qDate, $qAsmName) = &HgAutomate::hubDateName($accession);
  my $qAsmId = "${accession}";
  if ($accession =~ m/^GC/) {
     $qAsmId = "${accession}${qAsmName}";
  }
  $queryDates{$qAsmId} = $qDate;
  my ($qCommonName, undef, $qSubmitter) = &HgAutomate::getAssemblyInfo($dbHost, $qAsmId);
  $queryCommonName{$qAsmId} = $qCommonName;
  $querySubmitter{$qAsmId} = $qSubmitter;
}
close (TD);


my $ncbiAssemblyId = `grep -v "^#" $namesFile | cut -f10`;
chomp $ncbiAssemblyId;

my $sciName = `grep -v "^#" $namesFile | cut -f5`;
chomp $sciName;

my ($tGenome, $tDate, $tSource) = &HgAutomate::getAssemblyInfo($dbHost, $asmId);


print <<_EOF_
<h2>Description</h2>
<p>
This track shows regions of this <em>target</em> genome ($tGenome - $tDate - $tSource) that has alignment
to other <em>query</em> genomes (&quot;chain&quot; subtracks) or in synteny (&quot;net&quot; subtracks).
The alignable parts are shown with thick blocks that look like exons.
Non-alignable parts between these are shown like introns.
</p>

<p>
Other <em>query</em> genome assemblies aligning to this <em>target</em> genome assembly:
<ul>
_EOF_
    ;

foreach my $oAsmId (sort keys %queryDates) {
  if ($oAsmId =~ m/^GC/) {
    printf "<li><a href='https://genome.ucsc.edu/h/%s' target=_blank>%s</a> %s %s %s</li>\n", $oAsmId, $oAsmId, $queryCommonName{$oAsmId}, $queryDates{$oAsmId}, $querySubmitter{$oAsmId};
  } else {
    printf "<li><a href='https://genome.ucsc.edu/cgi-bin/hgTracks?db=%s' target=_blank>%s</a> %s %s %s</li>\n", $oAsmId, $oAsmId, $queryCommonName{$oAsmId}, $queryDates{$oAsmId}, $querySubmitter{$oAsmId};
  }
}

printf "</ul>
</p>\n";

print <<_EOF_
<h3>Chain Track</h3>
<p>
The chain tracks shows alignments of the other genome assemblies to the
$tGenome/$sciName/$ncbiAssemblyId/$tDate genome using a gap scoring system that allows longer gaps
than traditional affine gap scoring systems. It can also tolerate gaps in both
<em>query</em> and <em>target</em> geomes simultaneously. These
&quot;double-sided&quot; gaps can be caused by local inversions and
overlapping deletions in both species.
</p>
<p>
The chain track displays boxes joined together by either single or
double lines. The boxes represent aligning regions.
Single lines indicate gaps that are largely due to a deletion in the
<em>query</em> assembly or an insertion in the <em>target</em>
assembly.  Double lines represent more complex gaps that involve substantial
sequence in both species. This may result from inversions, overlapping
deletions, an abundance of local mutation, or an unsequenced gap in one
species.  In cases where multiple chains align over a particular region of
the <em>target</em> genome, the chains with single-lined gaps are often
due to processed pseudogenes, while chains with double-lined gaps are more
often due to paralogs and unprocessed pseudogenes.</p>
<P>
In the &quot;pack&quot; and &quot;full&quot; display
modes, the individual feature names indicate the chromosome, strand, and
location (in thousands) of the match for each matching alignment.</P>

<h3>Net Track</h3>
<p>
The net track shows the alignment of the best <em>query/target</em> chain for
every part of the <em>target</em> genome. It is useful for
finding syntenic regions, possibly orthologs, and for studying genome
rearrangement.</p>

<h2>Display Conventions and Configuration</h2>
<h3>Chain Track</h3>
<p>By default, the chains to chromosome-based assemblies are colored
based on which chromosome they map to in the aligning organism. To turn
off the coloring, check the &quot;off&quot; button next to: Color
track based on chromosome.</p>
<p>
To display only the chains of one chromosome in the aligning
organism, enter the name of that chromosome (e.g. chr4) in box next to:
Filter by chromosome.</p>

<h2>Net Track</h3>
<p>
At base level in full display mode, this track will show the
sequence of <em>query</em> as it aligned to <em>target</em>.  When the view is
too large to show such detail, a graph of the alignment score will be
shown.
</p>

<h2>Methods</h2>
<h3>Chain track</h3>
<p>
The <em>query</em> genome was aligned to <em>target</em> genome with <em>lastz</em>.
The resulting alignments were converted into axt format using the <em>lavToAxt</em>
program. The axt alignments were fed into <em>axtChain</em>, which organizes all
alignments between a single <em>query</em> chromosome and a single
<em>target</em> chromosome into a group and creates a kd-tree out
of the gapless subsections (blocks) of the alignments. A dynamic program
was then run over the kd-trees to find the maximally scoring chains of these
blocks.
</p>

<h3>Net track</h3>
<p>
Chains were derived from lastz alignments, using the methods
described on the chain tracks description pages, and sorted with the
highest-scoring chains in the genome ranked first. The program
<em>chainNet</em> was then used to place the chains one at a time, trimming them as
necessary to fit into sections not already covered by a higher-scoring chain.
During this process, a natural hierarchy emerged in which a chain that filled
a gap in a higher-scoring chain was placed underneath that chain. The program
<em>netSyntenic</em> was used to fill in information about the relationship between
higher- and lower-level chains, such as whether a lower-level
chain was syntenic or inverted relative to the higher-level chain.
The program <em>netClass</em> was then used to fill in how much of the gaps and chains
contained <em>N</em>s (sequencing gaps) in one or both species and how much
was filled with transposons inserted before and after the two organisms
diverged.
</p>

<p>
The resulting net file was converted to axt format via <em>netToAxt</em>,
then converted to maf format via <em>axtToMaf</em>, then converted to
the bigMaf format with <em>mafToBigMaf</em> and <em>bedToBigBed</em>
</p>

<h2>Credits</h2>
<p>
<em>lastz</em> was developed by Robert Harris, Pennsylvania State University.
</p>
<p>
The <em>axtChain</em> program was developed at the University of California at
Santa Cruz by Jim Kent with advice from Webb Miller and David Haussler.</p>
<p>
The browser display and database storage of the chains and nets were created
by Robert Baertsch and Jim Kent.</p>
<p>
The <em>chainNet</em>, <em>netSyntenic</em>, and <em>netClass</em> programs
were developed at the University of California
Santa Cruz by Jim Kent.</p>

<h2>References</h2>
<p>
Harris, R.S.
<a href="http://www.bx.psu.edu/~rsharris/lastz/"
target=_blank>(2007) Improved pairwise alignment of genomic DNA</a>
Ph.D. Thesis, The Pennsylvania State University
</p>

<p>
Chiaromonte F, Yap VB, Miller W.
<A HREF="http://psb.stanford.edu/psb-online/proceedings/psb02/chiaromonte.pdf"
TARGET=_blank>Scoring pairwise genomic sequence alignments</A>.
<em>Pac Symp Biocomput</em>. 2002:115-26.
PMID: <a href="https://www.ncbi.nlm.nih.gov/pubmed/11928468" target="_blank">11928468</a>
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

<p>
Schwartz S, Kent WJ, Smit A, Zhang Z, Baertsch R, Hardison RC,
Haussler D, Miller W.
<A HREF="https://genome.cshlp.org/content/13/1/103.abstract"
TARGET=_blank>Human-mouse alignments with BLASTZ</A>.
<em>Genome Res</em>. 2003 Jan;13(1):103-7.
PMID: <a href="https://www.ncbi.nlm.nih.gov/pubmed/12529312" target="_blank">12529312</a>; PMC: <a
href="https://www.ncbi.nlm.nih.gov/pmc/articles/PMC430961/" target="_blank">PMC430961</a>
</p>

_EOF_
    ;
