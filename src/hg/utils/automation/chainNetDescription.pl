#!/usr/bin/env perl

# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/chainNetDescription.pl instead.

# chainNetDescription.pl - construct a trackDb description .html page for
#   the chain/net alignments of a target assembly to one or more query
#   assemblies, finding both lastz (doBlastzChainNet.pl/pairLastz.sh) and
#   minimap2 (doMiniMap2.pl/pairMiniMap2.sh) builds, primary or -swap.
#   The same idea as the GenArk asmHubChainNetComposite.pl and
#   asmHubMiniMap2ChainNetComposite.pl pages, for UCSC database targets.

use strict;
use warnings;
use File::Basename;
use Getopt::Long;
use FindBin qw($Bin);
use lib "$Bin";
use AsmHub;
use HgAutomate;
use ChainNetInfo qw(htmlEscape accessionOrDb findAlignments readCladeFile);

my $methodOpt = "all";
my $cladeFile = "";
my $optOk = GetOptions('method=s' => \$methodOpt,
                       'cladeFile=s' => \$cladeFile);
my $argc = scalar(@ARGV);

if (! $optOk || $argc < 1 || (length($cladeFile) < 1 && $argc < 2)
      || (length($cladeFile) && $argc > 1)
      || $methodOpt !~ m/^(all|lastz|miniMap2)$/) {
  printf STDERR "usage: chainNetDescription.pl [-method=lastz|miniMap2] <target> <query> [<query> ...] > description.html\n";
  printf STDERR "   or: chainNetDescription.pl [-method=lastz|miniMap2] -cladeFile=<file> <target> > description.html\n";
  printf STDERR "options:\n";
  printf STDERR "  -method=lastz|miniMap2 - only describe the alignments made with\n";
  printf STDERR "      this method, for a page that goes with one method's tracks,\n";
  printf STDERR "      default: all alignments found\n";
  printf STDERR "  -cladeFile=<file> - the queries, their clades and order, from\n";
  printf STDERR "      chainNetCladeOrder.pl, in place of the query list, the same\n";
  printf STDERR "      file given to chainNetCompositeTrackDb.pl\n";
  printf STDERR "where target is a UCSC database (e.g. hg38) or GenArk assembly,\n";
  printf STDERR "and   query is one or more UCSC database or GenArk assemblies\n";
  printf STDERR "      (e.g.: hs1 GCF_002880755.1)\n";
  printf STDERR "looks for the build directories, in /hive/data/genomes/<target>/bed/\n";
  printf STDERR "or in the GenArk <target>/trackData/ directory:\n";
  printf STDERR "  lastz.<query>  blastz.<query>.swap  miniMap2.<query>  miniMap2.<query>.swap\n";
  printf STDERR "and writes the description page to stdout\n";
  exit 255;
}

# specific to UCSC environment
my $dbHost = "hgwdev";
my $hgDownload = "https://hgdownload.soe.ucsc.edu";

my $target = shift;
my @queryList = @ARGV;

my %queryClade;	# key is query id, value is clade name from -cladeFile
if (length($cladeFile)) {
  foreach my $entry (&readCladeFile($cladeFile)) {
    push @queryList, $entry->{'query'};
    $queryClade{$entry->{'query'}} = $entry->{'clade'};
  }
}

my $tId = &accessionOrDb($target);
my $tAsmId = $tId;
my $tRoot = "/hive/data/genomes/$tId/bed";
if ($tId =~ m/^GC/) {
  my $tBuild = &AsmHub::asmBuildDir($tId);
  die "can not find GenArk build directory for target '$target'\n" if (! $tBuild);
  $tAsmId = basename($tBuild);
  $tRoot = "$tBuild/trackData";
}
die "can not find target build directory '$tRoot'\n" if (! -d "$tRoot");

my ($tGenome, $tDate, $tSource) = &HgAutomate::getAssemblyInfo($dbHost, $tAsmId);
$tGenome = $tId if (! defined($tGenome) || length($tGenome) < 1);
$tDate = "" if (! defined($tDate));
$tSource = "" if (! defined($tSource));

# query assembly description: (asmId, common name, date, submitter)
sub queryInfo($) {
  my ($qId) = @_;
  my $qAsmId = $qId;
  my ($qDate, $qAsmName) = &HgAutomate::hubDateName($qId);
  $qAsmId = "${qId}${qAsmName}" if ($qId =~ m/^GC/);
  my ($qCommonName, $qDbDate, $qSubmitter) =
     eval { &HgAutomate::getAssemblyInfo($dbHost, $qAsmId); };
  $qCommonName = $qId if (! defined($qCommonName) || length($qCommonName) < 1);
  $qDate = $qDbDate if ($qId !~ m/^GC/ && defined($qDbDate));
  $qDate = "" if (! defined($qDate) || $qDate eq "some date");
  $qSubmitter = "" if (! defined($qSubmitter));
  return ($qAsmId, $qCommonName, $qDate, $qSubmitter);
}

# for each query, the build directories found by ChainNetInfo's
# findAlignments(), shared with chainNetCompositeTrackDb.pl so the
# description page and the trackDb composite describe the same set
my @rows;	# one per alignment found, hash references
my %queryAsmId;	# key is query id, value is asmId
my %queryName;	# key is query id, value is common name
my %queryDate;	# key is query id, value is assembly date
my %querySubmitter;	# key is query id, value is assembly submitter
my @queryIds;	# in the order given, only those with an alignment

foreach my $query (@queryList) {
  my $qId = &accessionOrDb($query);
  my @found = &findAlignments($tRoot, $tId, $query, $methodOpt);
  push @rows, @found;
  if (! scalar(@found)) {
    printf STDERR "# warning: no %s alignment found for '%s' in %s\n",
       ($methodOpt eq "all") ? "lastz or miniMap2" : $methodOpt, $query, $tRoot;
    next;
  }
  my ($qAsmId, $qCommonName, $qDate, $qSubmitter) = &queryInfo($qId);
  $queryAsmId{$qId} = $qAsmId;
  $queryName{$qId} = $qCommonName;
  $queryDate{$qId} = $qDate;
  $querySubmitter{$qId} = $qSubmitter;
  push @queryIds, $qId;
}

die "no alignments found for any of the queries in $tRoot\n" if (scalar(@rows) < 1);

my $haveLastz = scalar(grep { $_->{'method'} eq "lastz" } @rows);
my $haveMiniMap2 = scalar(grep { $_->{'method'} eq "miniMap2" } @rows);
my $haveSyntenic = scalar(grep { length($_->{'syntenic'}) } @rows);
my $haveRBest = scalar(grep { length($_->{'rBest'}) } @rows);
my $haveQuickLift = scalar(grep { length($_->{'quickLift'}) } @rows);

sub methodLabel($) {
  my ($row) = @_;
  my $label = ($row->{'method'} eq "miniMap2") ? "minimap2" : "lastz";
  $label .= " (swap)" if ($row->{'isSwap'});
  return $label;
}

sub queryLink($) {
  my ($qId) = @_;
  my $qAsmId = $queryAsmId{$qId};
  if ($qId =~ m/^GC/) {
    # the /h/ link takes the accession only
    return sprintf("<a href='https://genome.ucsc.edu/h/%s' target=_blank>%s</a>", $qId, $qAsmId);
  }
  return sprintf("<a href='https://genome.ucsc.edu/cgi-bin/hgTracks?db=%s' target=_blank>%s</a>", $qId, $qId);
}

sub numCell($) {
  my ($value) = @_;
  $value = "&nbsp;" if (! length($value));
  return "<td style='text-align:right;'>$value</td>";
}

my $alignedWith = "lastz";
if ($haveLastz && $haveMiniMap2) {
  $alignedWith = "<em>lastz</em> or <em>minimap2</em>";
} elsif ($haveMiniMap2) {
  $alignedWith = "<em>minimap2</em>";
} else {
  $alignedWith = "<em>lastz</em>";
}

##############################################################################
# Description
print <<_EOF_
<h2>Description</h2>
<p>
These tracks show regions of this <em>target</em> genome
($tGenome - $tDate - $tSource) that have alignments to other
<em>query</em> genome assemblies, aligned with $alignedWith.
The alignable parts are shown with thick blocks that look like exons.
Non-alignable parts between these are shown like introns.
</p>

<p>
<em>Query</em> genome assemblies aligned to this <em>target</em> genome assembly:
<ul>
_EOF_
    ;

foreach my $qId (@queryIds) {
  my @methods;
  foreach my $row (grep { $_->{'qId'} eq $qId } @rows) {
    push @methods, &methodLabel($row);
  }
  printf "<li>%s %s %s %s - %s</li>\n", &queryLink($qId),
     $queryName{$qId}, $queryDate{$qId}, $querySubmitter{$qId},
     join(", ", @methods);
}
printf "</ul>\n</p>\n";

if (length($cladeFile)) {
print <<_EOF_
<p>
The <em>query</em> assemblies are grouped by clade, from the NCBI taxonomy:
each clade is named for the taxonomic group of the <em>query</em> just
below the most recent group it shares with this <em>target</em> genome.
The clades are listed from the closest to the most distant from the
<em>target</em>, and within each clade, the assemblies are ordered by their
<em>mash</em> distance to the <em>target</em> assembly, then by their chain
coverage of the <em>target</em> genome.
</p>
_EOF_
    ;
}

if ($haveMiniMap2) {
print <<_EOF_
<p>
The <em>minimap2</em> alignments are intended for pairs of very similar
same-species/same-individual assemblies, for example the haplotypes of
a diploid assembly, or closely related strain assemblies, where
minimap2's splice-free asm5/asm10/asm20 presets do a better job with the
larger indels and structural differences seen between such assemblies
than lastz's general-purpose scoring does.
</p>
_EOF_
    ;
}

##############################################################################
# Coverage table
printf "<h3>Alignment coverage</h3>\n";
printf "<table border='1'>\n";
printf "<caption>percent of the <em>target</em> genome covered by each type of chain";
if ($haveQuickLift) {
  printf ",<br>except <b>quickLift</b>: percent of the <em>query</em> genome covered by the quickLift chain";
}
printf "</caption>\n";
printf "<thead style='position:sticky; top:0; background-color: white;'><tr>\n";
printf "<th>common<br>name</th><th>assembly</th><th>method</th><th>chains</th>";
printf "<th>syntenic</th>" if ($haveSyntenic);
printf "<th>reciprocal<br>best</th>" if ($haveRBest);
printf "<th>lift<br>over</th>";
printf "<th>quickLift<br>(%% of query)</th>" if ($haveQuickLift);
printf "\n</tr></thead><tbody>\n";
my $columnCount = 5 +($haveSyntenic ? 1 : 0) + ($haveRBest ? 1 : 0) + ($haveQuickLift ? 1 : 0);
my $prevClade = "";
foreach my $qId (@queryIds) {
  if (length($cladeFile) && $queryClade{$qId} ne $prevClade) {
    $prevClade = $queryClade{$qId};
    printf "<tr><th colspan=%d style='text-align:left;'>%s</th></tr>\n",
       $columnCount, &htmlEscape($prevClade =~ s/_/ /gr);
  }
  foreach my $row (grep { $_->{'qId'} eq $qId } @rows) {
    printf "<tr><td>%s</td><td>%s</td><td>%s</td>", $queryName{$qId},
       &queryLink($qId), &methodLabel($row);
    printf "%s", &numCell($row->{'chain'});
    printf "%s", &numCell($row->{'syntenic'}) if ($haveSyntenic);
    printf "%s", &numCell($row->{'rBest'}) if ($haveRBest);
    printf "%s", &numCell($row->{'liftOver'});
    printf "%s", &numCell($row->{'quickLift'}) if ($haveQuickLift);
    printf "</tr>\n";
  }
}
printf "</tbody></table>\n";

##############################################################################
# Chain track and the types of chains
print <<_EOF_
<h3>Chain Track</h3>
<p>
The chain tracks show alignments of the <em>query</em> genome assemblies to the
<em>target</em> genome using a gap scoring system that allows longer gaps
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
species.  In cases where multiple chains align over a particular region of
the <em>target</em> genome, the chains with single-lined gaps are often
due to processed pseudogenes, while chains with double-lined gaps are more
often due to paralogs and unprocessed pseudogenes.</p>
<p>
In the &quot;pack&quot; and &quot;full&quot; display
modes, the individual feature names indicate the chromosome, strand, and
location (in thousands) of the match for each matching alignment.</p>
<p>
The types of chains measured in the table above:
<ul>
<li><b>Chains</b> - The first level of chain track showing all potential chains.
  The other chain types are derived from this chain data.</li>
_EOF_
    ;
if ($haveSyntenic) {
print <<_EOF_
<li><b>Syntenic</b> - Filtered first level chain showing the corresponding
  regions between the two genomes in the alignment that have the same
  order of blocks and direction in the alignment.</li>
_EOF_
    ;
}
if ($haveRBest) {
print <<_EOF_
<li><b>Reciprocal best</b> - Filtered first level chain showing the
  corresponding regions where the best target to query alignment,
  and the best query to target alignment identify the same regions.</li>
_EOF_
    ;
}
print <<_EOF_
<li><b>Lift over</b> - Filtered first level chain selecting out the
  best/longest syntenic regions used to translate coordinates from the
  target genome to the query genome.</li>
_EOF_
    ;
if ($haveQuickLift) {
print <<_EOF_
<li><b>quickLift</b> - The lift over chain swapped to the
  <em>query</em> genome coordinates, used to display annotations
  from the <em>target</em> genome on the <em>query</em> genome.
  Its coverage is a percent of the <em>query</em> genome.</li>
_EOF_
    ;
}
printf "</ul>\n</p>\n";
printf "<p>\nA <b>(swap)</b> alignment was made with the <em>query</em> as the\n";
printf "target genome, and then swapped to this <em>target</em> genome coordinates.\n</p>\n";

if ($haveLastz) {
print <<_EOF_
<h3>Net Track</h3>
<p>
The <em>lastz</em> alignments also have a net track, which shows the best
<em>query</em> chain for every part of the <em>target</em> genome.
</p>
_EOF_
    ;
}

##############################################################################
# Display Conventions
print <<_EOF_
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
_EOF_
    ;
if ($haveLastz) {
print <<_EOF_
<h3>Net Track</h3>
<p>
In full display mode, the top-level (level 1)
chains are the largest, highest-scoring chains that
span this region.  In many cases gaps exist in the
top-level chain.  When possible, these are filled in by
other chains that are displayed at level 2.  The gaps in
level 2 chains may be filled by level 3 chains and so
forth.</p>
<p>
Individual items in the display are categorized as one of four types
(other than gap):</p>
<p><ul>
<li><b>Top</b> - the best, longest match. Displayed on level 1.</li>
<li><b>Syn</b> - line-ups on the same chromosome as the gap in the level above
it.</li>
<li><b>Inv</b> - a line-up on the same chromosome as the gap above it, but in
the opposite orientation.</li>
<li><b>NonSyn</b> - a match to a chromosome different from the gap in the
level above.</li>
</ul></p>
_EOF_
    ;
}

##############################################################################
# Methods
printf "<h2>Methods</h2>\n";
if ($haveLastz) {
print <<_EOF_
<h3>lastz alignments</h3>
<p>
The <em>query</em> genome was aligned to the <em>target</em> genome with
<em>lastz</em>.  The resulting alignments were converted into PSL format and
fed into <em>axtChain</em>, which organizes all
alignments between a single <em>query</em> chromosome and a single
<em>target</em> chromosome into a group and creates a kd-tree out
of the gapless subsections (blocks) of the alignments. A dynamic program
was then run over the kd-trees to find the maximally scoring chains of these
blocks.
</p>
<p>
The chains were sorted with the highest-scoring chains in the genome ranked
first. The program <em>chainNet</em> was then used to place the chains one at
a time, trimming them as necessary to fit into sections not already covered by
a higher-scoring chain.  During this process, a natural hierarchy emerged in
which a chain that filled a gap in a higher-scoring chain was placed underneath
that chain. The program <em>netSyntenic</em> was used to fill in information
about the relationship between higher- and lower-level chains, such as whether
a lower-level chain was syntenic or inverted relative to the higher-level chain.
The program <em>netClass</em> was then used to fill in how much of the gaps and
chains contained <em>N</em>s (sequencing gaps) in one or both species and how
much was filled with transposons inserted before and after the two organisms
diverged.  The syntenic and lift over chains were extracted from the net with
<em>netChainSubset</em>.
</p>
_EOF_
    ;
  my @lastzRows = grep { $_->{'method'} eq "lastz" && defined($_->{'params'}) } @rows;
  if (scalar(@lastzRows)) {
    printf "<table border='1'>\n";
    printf "<caption>lastz and axtChain parameters, lastz settings not listed here are the lastz defaults</caption>\n";
    printf "<thead><tr><th>assembly</th><th>method</th><th>lastz<br>version</th>";
    printf "<th>lastz settings</th><th>axtChain<br>minScore</th><th>axtChain<br>linearGap</th></tr></thead><tbody>\n";
    foreach my $row (@lastzRows) {
      my $params = $row->{'params'};
      my $version = length($params->{'lastzVersion'}) ? $params->{'lastzVersion'} : $params->{'lastz'};
      my @settings;
      foreach my $keyVal (@{$params->{'scoring'}}) {
        my ($key, $val) = @$keyVal;
        $val = basename($val) if ($key eq "Q");
        push @settings, &htmlEscape("$key=$val");
      }
      printf "<tr><td>%s</td><td>%s</td><td>%s</td><td><code>%s</code></td>%s<td>%s</td></tr>\n",
         $queryAsmId{$row->{'qId'}}, &methodLabel($row), &htmlEscape($version),
         scalar(@settings) ? join(" ", @settings) : "default",
         &numCell(&htmlEscape($params->{'minScore'})), &htmlEscape($params->{'linearGap'});
    }
    printf "</tbody></table>\n";
  }
}

if ($haveMiniMap2) {
print <<_EOF_
<h3>minimap2 alignments</h3>
<p>
The <em>query</em> genome was aligned to the <em>target</em> genome with
<em>minimap2</em>, one job per <em>target</em> sequence against the whole
<em>query</em> genome.  The minimap2 alignment preset was selected from the
<em>mash</em> distance between the two assemblies.  The resulting PAF
alignments were converted into PSL format using the <em>pafToPsl</em>
program, and fed into <em>axtChain</em> to construct the chains in the same
way as for the lastz alignments.  The chain was placed with <em>chainNet</em>,
and the program <em>netChainSubset</em> was used to extract the best/longest
syntenic part of that net back out of the chain, giving the lift over chain.
</p>
_EOF_
    ;
  my @mm2Rows = grep { $_->{'method'} eq "miniMap2" && defined($_->{'params'}) } @rows;
  if (scalar(@mm2Rows)) {
    printf "<table border='1'>\n";
    printf "<caption>minimap2 version, <em>mash</em> distance between the target and query assemblies, the minimap2 alignment preset selected from that distance, and the minimap2 command used for each target sequence job</caption>\n";
    printf "<thead><tr><th>assembly</th><th>method</th><th>minimap2<br>version</th>";
    printf "<th>mash<br>distance</th><th>preset</th><th>command</th></tr></thead><tbody>\n";
    foreach my $row (@mm2Rows) {
      my $params = $row->{'params'};
      printf "<tr><td>%s</td><td>%s</td><td>%s</td>%s<td>%s</td><td><code>%s</code></td></tr>\n",
         $queryAsmId{$row->{'qId'}}, &methodLabel($row),
         &htmlEscape($params->{'version'}),
         &numCell(&htmlEscape($params->{'mashDistance'})),
         &htmlEscape($params->{'minimapPreset'}),
         &htmlEscape($params->{'command'});
    }
    printf "</tbody></table>\n";
  }
}

##############################################################################
# Data Access
my $liftOverUrl = "$hgDownload/goldenPath/$tId/liftOver/";
if ($tId =~ m/^GC/) {
  $liftOverUrl = "$hgDownload/hubs/" . &AsmHub::asmIdToPath($tId) . "/$tId/liftOver/";
}
print <<_EOF_
<h2>Data Access</h2>
<p>
The lift over chain files are available in the
<a href='$liftOverUrl' target=_blank>liftOver</a> directory of our download
server, for use with the <em>liftOver</em> command line tool, available from the
<a href='https://hgdownload.soe.ucsc.edu/downloads.html#utilities_downloads'
target=_blank>utilities download directory</a>.
_EOF_
    ;
if ($haveLastz && $tId !~ m/^GC/) {
  my @vsLinks;
  foreach my $qId (@queryIds) {
    next if (! scalar(grep { $_->{'qId'} eq $qId && $_->{'method'} eq "lastz" } @rows));
    my $vsDir = "vs" . ucfirst($qId);
    push @vsLinks, "<a href='$hgDownload/goldenPath/$tId/$vsDir/' target=_blank>$vsDir</a>";
  }
  printf "The complete lastz chain and net files are in the %s download director%s.\n",
     join(", ", @vsLinks), (scalar(@vsLinks) > 1) ? "ies" : "y";
}
printf "</p>\n";

##############################################################################
# Credits and References
printf "\n<h2>Credits</h2>\n";
if ($haveLastz) {
  printf "<p>\n<em>lastz</em> was developed by Robert Harris, Pennsylvania State University.\n</p>\n";
}
if ($haveMiniMap2) {
print <<_EOF_
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
_EOF_
    ;
} elsif (length($cladeFile)) {
print <<_EOF_
<p>
The <em>mash</em> program, used to order the <em>query</em> assemblies by
their distance to the target, was developed by Brian Ondov, Todd Treangen,
and colleagues at the National Biodefense Analysis and Countermeasures
Center and the University of Maryland.
</p>
_EOF_
    ;
}
print <<_EOF_
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
_EOF_
    ;
if ($haveLastz) {
print <<_EOF_
<p>
Harris RS.
<a href="http://www.bx.psu.edu/~rsharris/rsharris_phd_thesis_2007.pdf"
target="_blank">Improved pairwise alignment of genomic DNA</a>.
<em>Ph.D. Thesis</em>. Pennsylvania State University, USA. 2007.
</p>

<p>
Chiaromonte F, Yap VB, Miller W.
<a href="http://psb.stanford.edu/psb-online/proceedings/psb02/chiaromonte.pdf"
target=_blank>Scoring pairwise genomic sequence alignments</a>.
<em>Pac Symp Biocomput</em>. 2002:115-26.
PMID: <a href="https://www.ncbi.nlm.nih.gov/pubmed/11928468" target="_blank">11928468</a>
</p>

<p>
Schwartz S, Kent WJ, Smit A, Zhang Z, Baertsch R, Hardison RC,
Haussler D, Miller W.
<a href="https://genome.cshlp.org/content/13/1/103.abstract"
target=_blank>Human-mouse alignments with BLASTZ</a>.
<em>Genome Res</em>. 2003 Jan;13(1):103-7.
PMID: <a href="https://www.ncbi.nlm.nih.gov/pubmed/12529312" target="_blank">12529312</a>; PMC: <a
href="https://www.ncbi.nlm.nih.gov/pmc/articles/PMC430961/" target="_blank">PMC430961</a>
</p>
_EOF_
    ;
}
if ($haveMiniMap2) {
print <<_EOF_
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
_EOF_
    ;
} elsif (length($cladeFile)) {
print <<_EOF_
<p>
Ondov BD, Treangen TJ, Melsted P, Mallonee AB, Bergman NH, Koren S, Phillippy AM.
<a href="https://doi.org/10.1186/s13059-016-0997-x"
target=_blank>Mash: fast genome and metagenome distance estimation using MinHash</a>.
<em>Genome Biol</em>. 2016 Jun 20;17(1):132.
PMID: <a href="https://www.ncbi.nlm.nih.gov/pubmed/27323842" target="_blank">27323842</a>; PMC: <a
href="https://www.ncbi.nlm.nih.gov/pmc/articles/PMC4915045/" target="_blank">PMC4915045</a>
</p>
_EOF_
    ;
}
print <<_EOF_
<p>
Kent WJ, Baertsch R, Hinrichs A, Miller W, Haussler D.
<a href="https://www.pnas.org/content/100/20/11484"
target=_blank>Evolution's cauldron:
duplication, deletion, and rearrangement in the mouse and human genomes</a>.
<em>Proc Natl Acad Sci U S A</em>. 2003 Sep 30;100(20):11484-9.
PMID: <a href="https://www.ncbi.nlm.nih.gov/pubmed/14500911" target="_blank">14500911</a>; PMC: <a
href="https://www.ncbi.nlm.nih.gov/pmc/articles/PMC208784/" target="_blank">PMC208784</a>
</p>
_EOF_
    ;
