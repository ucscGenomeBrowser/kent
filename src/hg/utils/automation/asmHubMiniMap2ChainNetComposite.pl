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
  printf STDERR "usage: asmHubMiniMap2ChainNetComposite.pl asmId ncbiAsmId asmId.names.tab > asmId.miniMap2ChainNet.html\n";
  printf STDERR "where asmId is the assembly identifier,\n";
  printf STDERR "and   asmId.names.tab is naming file for this assembly,\n";
  printf STDERR "for UCSC database assemblies, use the third argument asmId.names.tab\n";
  printf STDERR "   as the Scientific_name for the organism.\n";
  exit 255;
}

# specific to UCSC environment
my $dbHost = "hgwdev";

my $asmId = shift;
my $ncbiAsmId = shift;
my $namesFile = shift;

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

my $hgDownload = "https://hgdownload.soe.ucsc.edu";
my @accParts = split('_', $asmId);
my $accession = "$accParts[0]_$accParts[1]";
my $asmIdPath = &AsmHub::asmIdToPath($asmId);
my $downloadDir = "$hgDownload/hubs/$asmIdPath/$accession/bbi";

# doMiniMap2.pl names buildDir/trackData/miniMap2.$QDb where $QDb is
# ucfirst() of the actual query db/accession.  Reconstruct the actual
# db name/accession, except for a GenArk accession (already starts with
# the uppercase 'GC', ucfirst is a no-op there).
sub queryDbFromQDb($) {
  my ($qDb) = @_;
  return $qDb if ($qDb =~ m/^GC/);
  return lcfirst($qDb);
}

my %queryDates;	# key is asmId, value is assembly date
my %queryCommonName;	# key is asmId, value is assembly common name
my %querySubmitter;	# key is asmId, value is assembly submitter
my %fbStats;	# key is asmId, value is featureBits measure for chains
my %loStats;	# key is asmId, value is featureBits measure for lift over
my %fileOtherDb;	# key is asmId, value is the $QDb used in the chain*.bb file names

open (TD, "ls -d ${targetBuildDir}/trackData/miniMap2.*|") or die "can not ls -d ${targetBuildDir}/trackData/miniMap2.*";
while (my $mm2Dir = <TD>) {
  chomp $mm2Dir;
  # a -swap run's default swapDir is itself named 'miniMap2.<otherDb>.swap',
  # which also matches this glob alongside its own dateless symlink
  # 'miniMap2.<otherDb>' -- skip that real *.swap work directory so this
  # query genome is only processed once, via its dateless symlink.
  next if ($mm2Dir =~ m/\.swap$/);
  my $Qdb = basename($mm2Dir);
  # the hubDateName will translate the accessionId into an asmId
  # side effect it also returns the date
  my $accession = &queryDbFromQDb($Qdb);
  my ($qDate, $qAsmName) = &HgAutomate::hubDateName($accession);
  my $qAsmId = "${accession}";
  if ($accession =~ m/^GC/) {
     $qAsmId = "${accession}${qAsmName}";
  }
  $queryDates{$qAsmId} = $qDate;
  $fileOtherDb{$qAsmId} = $Qdb;
  my ($qCommonName, undef, $qSubmitter) = &HgAutomate::getAssemblyInfo($dbHost, $qAsmId);
  $queryCommonName{$qAsmId} = $qCommonName;
  $querySubmitter{$qAsmId} = $qSubmitter;
  my $fbTxt = `ls ${targetBuildDir}/trackData/miniMap2.${Qdb}/fb.*chain${Qdb}Link.txt 2> /dev/null`;
  chomp $fbTxt;
  if ( -s "${fbTxt}" ) {
    my $fBits = `cut -d' ' -f5 $fbTxt | tr -d '()%'`;
    chomp $fBits;
    $fbStats{$qAsmId} = $fBits;
  } else {
    $fbStats{$qAsmId} = "n/a";
  }
  $fbTxt = `ls ${targetBuildDir}/trackData/miniMap2.${Qdb}/fb.*chainLiftOver${Qdb}.txt 2> /dev/null`;
  chomp $fbTxt;
  if ( -s "${fbTxt}" ) {
    my $fBits = `cut -d' ' -f5 $fbTxt | tr -d '()%'`;
    chomp $fBits;
    $loStats{$qAsmId} = $fBits;
  } else {
    $loStats{$qAsmId} = "n/a";
  }
}
close (TD);

my $ncbiAssemblyId = $ncbiAsmId;

if ( -s "${namesFile}" ) {
  $ncbiAssemblyId = `grep -v "^#" $namesFile | cut -f10`;
  chomp $ncbiAssemblyId;
}

my $sciName = ${namesFile};

if ( -s "${namesFile}" ) {
  my $sciName = `grep -v "^#" $namesFile | cut -f5`;
  chomp $sciName;
}

my ($tGenome, $tDate, $tSource) = &HgAutomate::getAssemblyInfo($dbHost, $asmId);


print <<_EOF_
<h2>Description</h2>
<p>
This track shows regions of this <em>target</em> genome ($tGenome - $tDate - $tSource) that has alignment
to other very similar <em>query</em> genomes (&quot;chain&quot; subtracks), and the
subset of each chain used to lift over coordinates to that query
(&quot;lift over&quot; subtracks).
The alignable parts are shown with thick blocks that look like exons.
Non-alignable parts between these are shown like introns.
</p>

<p>
These alignments were made with <em>minimap2</em> rather than
<em>lastz</em>.  They are intended for pairs of very similar
same-species/same-individual assemblies -- for example the haplotypes of
a trio-binned or hifiasm/verkko diploid assembly, or closely related
strain assemblies -- where minimap2's splice-free asm5/asm10/asm20
presets do a better job with the larger indels and structural
differences seen between such assemblies than lastz's general-purpose
scoring does.
</p>

<p>
Other <em>query</em> genome assemblies aligning to this <em>target</em> genome assembly:
<ul>
_EOF_
    ;

my @orderedByFBits;
foreach my $qDb ( sort {$fbStats{$b} <=> $fbStats{$a}} keys %fbStats) {
  push @orderedByFBits, $qDb;
}

foreach my $oAsmId (@orderedByFBits) {
  my $fbPercent = "";
  $fbPercent = "% $fbStats{$oAsmId} " if (defined($fbStats{$oAsmId}));
  if ($oAsmId =~ m/^GC/) {
    printf "<li>%s<a href='https://genome.ucsc.edu/h/%s' target=_blank>%s</a> %s %s %s</li>\n", $fbPercent, $oAsmId, $oAsmId, $queryCommonName{$oAsmId}, $queryDates{$oAsmId}, $querySubmitter{$oAsmId};
  } else {
    printf "<li>%s<a href='https://genome.ucsc.edu/cgi-bin/hgTracks?db=%s' target=_blank>%s</a> %s %s %s</li>\n", $fbPercent, $oAsmId, $oAsmId, $queryCommonName{$oAsmId}, $queryDates{$oAsmId}, $querySubmitter{$oAsmId};
  }
}

printf "</ul>
</p>\n";

printf "<h3>Alignments identity</h3>\n";
printf "<table border='1'>\n";
printf "<caption>showing percent identity, how much of the target is matched by the query</caption>\n";
printf "<thead style='position:sticky; top:0; background-color: white;'><tr>\n";
printf "<th>chains</th><th>lift<br>over</th><th>common<br>name</th><th>assembly</th>\n";
printf "</tr></thead><tbody>\n";
foreach my $oAsmId (@orderedByFBits) {
 printf "<tr>";
 if (defined($fbStats{$oAsmId})) {
   printf "<td style='text-align:right;'>%s</td>", $fbStats{$oAsmId};
 } else {
   printf "<td style='text-align:right;'>&nbsp;</td>";
 }
 if (defined($loStats{$oAsmId})) {
   printf "<td style='text-align:right;'>%s</td>", $loStats{$oAsmId};
 } else {
   printf "<td style='text-align:right;'>&nbsp;</td>";
 }
 printf "<td>%s</td><td>%s</td></tr>\n", $queryCommonName{$oAsmId}, $oAsmId;
}
printf "</tbody></table>\n";

printf "<h2>Data Access</h2>\n";
printf "<p>\n";
printf "The underlying data for these tracks are stored as bigChain/bigLink file pairs, one\n";
printf "pair per query assembly (and, where present, a second pair for its lift over chain),\n";
printf "on our <a href='%s' target=_blank>download server</a>:\n", $downloadDir;
printf "</p>\n";
printf "<table border='1'>\n";
printf "<thead><tr><th>assembly</th><th>chain</th><th>lift over chain</th></tr></thead>\n";
printf "<tbody>\n";
foreach my $oAsmId (@orderedByFBits) {
  my $Qdb = $fileOtherDb{$oAsmId};
  printf "<tr><td>%s</td><td><code>%s.chainMiniMap2%s.bb</code></td>", $oAsmId, $asmId, $Qdb;
  if (defined($loStats{$oAsmId}) && $loStats{$oAsmId} ne "n/a") {
    printf "<td><code>%s.chainLiftOverMiniMap2%s.bb</code></td></tr>\n", $asmId, $Qdb;
  } else {
    printf "<td>&nbsp;</td></tr>\n";
  }
}
printf "</tbody></table>\n";
printf "<p>\n";
printf "Regions, or the whole genome, can be extracted from these files with our command\n";
printf "line tool <b>bigChainToChain</b>, available from the\n";
printf "<a href='https://hgdownload.soe.ucsc.edu/downloads.html#utilities_downloads'\n";
printf "target=_blank>utilities download directory</a>.  Each bigChain file has a\n";
printf "companion <b>...Link.bb</b> file that must be given as the second argument, for\n";
printf "example, for %s:\n", $orderedByFBits[0];
printf "<pre>\n";
printf "bigChainToChain %s/%s.chainMiniMap2%s.bb \\\n", $downloadDir, $asmId, $fileOtherDb{$orderedByFBits[0]};
printf "    %s/%s.chainMiniMap2%sLink.bb stdout\n", $downloadDir, $asmId, $fileOtherDb{$orderedByFBits[0]};
printf "</pre>\n";
printf "optionally restricted to a single region with the <b>-chrom=</b>, <b>-start=</b> and\n";
printf "<b>-end=</b> options.\n";
printf "</p>\n";

print <<_EOF_
<h3>Chain Track</h3>
<p>
The chain tracks shows alignments of the other genome assemblies to the
$tGenome/$sciName/$ncbiAssemblyId/$tDate genome using a gap scoring system that allows longer gaps
than traditional affine gap scoring systems. It can also tolerate gaps in both
<em>query</em> and <em>target</em> genomes simultaneously. These
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
species.</p>
<p>
In the &quot;pack&quot; and &quot;full&quot; display
modes, the individual feature names indicate the chromosome, strand, and
location (in thousands) of the match for each matching alignment.</p>
<p>
There are two different types of chain tracks:
<ul>
<li><b>Chains</b> - The first level of chain track showing all potential chains.
  The lift over chain track is derived from this chain data.</li>
<li><b>Lift over</b> - filtered first level chain selecting out the
  best/longest syntenic regions used to translate coordinates from the
  target genome to the query genome.</li>
</ul>
</p>

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
Each <em>query</em> genome was aligned to the <em>target</em> genome with
<em>minimap2</em>, one job per target sequence against the whole query
genome. The resulting PAF alignments were converted into PSL format using
the <em>pafToPsl</em> program. The PSL alignments were fed into
<em>axtChain</em>, which organizes all alignments between a single
<em>query</em> chromosome and a single <em>target</em> chromosome into a
group and creates a kd-tree out of the gapless subsections (blocks) of
the alignments. A dynamic program was then run over the kd-trees to find
the maximally scoring chains of these blocks.
</p>

<h3>Lift over chain track</h3>
<p>
Each chain was placed with <em>chainNet</em>, trimming it as necessary to
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
target and each query assembly and automatically select the minimap2
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
