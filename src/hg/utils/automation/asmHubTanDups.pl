#!/usr/bin/env perl

use strict;
use warnings;
use FindBin qw($Bin);
use lib "$Bin";
use AsmHub;
use File::Basename;

my $argc = scalar(@ARGV);

if ($argc != 3) {
  printf STDERR "usage: asmHubTanDups.pl asmId asmId.names.tab .../trackData/\n";
  printf STDERR "where asmId is the assembly identifier,\n";
  printf STDERR "and .../trackData/ is the path to the /trackData/ directory.\n";
  exit 255;
}

my $asmId = shift;
my $namesFile = shift;
my $trackDataDir = shift;
my $gapOverlapBbi = "$trackDataDir/gapOverlap/$asmId.gapOverlap.bb";
my $tandemDupsBbi = "$trackDataDir/tandemDups/$asmId.tandemDups.bb";

if ( ! (-s $gapOverlapBbi || -s $tandemDupsBbi) ) {
  printf STDERR "ERROR: can not find gapOverlap or tandemDups bbi files:\n\t'%s'\n\t%s\n", $gapOverlapBbi, $tandemDupsBbi;
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

my $gapOverlapItemCount = "<no items in this track>";
my $tandemDupsItemCount = "<no items in this track>";

if ( -s $gapOverlapBbi ) {
  $gapOverlapItemCount = `bigBedInfo $gapOverlapBbi | egrep "itemCount:|basesCovered:" | xargs echo | sed -e 's/itemCount/Item count/; s/ basesCovered/; Bases covered/;'`;
  chomp $gapOverlapItemCount;
}

if ( -s $tandemDupsBbi ) {
  $tandemDupsItemCount = `bigBedInfo $tandemDupsBbi | egrep "itemCount:|basesCovered:" | xargs echo | sed -e 's/itemCount/Item count/; s/ basesCovered/; Bases covered/;'`;
  chomp $tandemDupsItemCount;
}

print <<_EOF_
<h2>Description</h2>
<p>
This track indicates any pair of exactly identical sequence
for the $assemblyDate $em${organism}$noEm/$asmId genome assembly.
</p>

<p>
There may be two tracks in this composite collection:
<ul> 
<li> Gap Overlaps - Paired exactly identical sequence on each side of a gap</li>
<li> Tandem Dups - Paired exactly identical sequence survey over entire genome assembly
</ul>
The <em>Gap Overlaps</em> is thus a subset of the full <em>Tandem Dups</em> track.
</p>
<p>
This investigation began when an unusual number of paired sequences
around gaps was noticed during the mouse strain sequencing project.
This naturally raised the question, how common is this feature, and what
type of assemblies can it be found in.
</p>

<p>
The <em>Gap Overlaps</em> track indicates any pair of exactly identical sequence
on each side of gaps.  Where a gap is any run of N's, including
a single N.  The end of an upstream sequence before the gap
is duplicated exactly at the beginning of the downstream sequence
following the gap in the assembly.<br>Data in track: $gapOverlapItemCount.
</p>
<p> 
The <em>Tandem Dups</em> track is a similar survey over the entire genome
assembly.  The separation <em>gap</em> between these paired sequences
can range from 1 base up to 20,000 bases.<br>Data in track: $tandemDupsItemCount.
</p>

<h2>Methods</h2>
<p>
The <em>Gap Overlap</em> duplicate sequences were found by
extracting 1,000 bases before and after each gap and aligned to
each other with the blat command:
<pre>
  blat -q=dna -minIdentity=95 -repMatch=10 upstreamContig.fa downstreamContig.fa
</pre>
Filtering the PSL output for a perfect match, no mis-matches,
and therefore of equal size matching sequence,
where the alignment ends exactly at the end of the upstream sequence,
and begins exactly at the start of the downstream sequence.
</p>
<p>
The <em>Tandem Dups</em> paired sequences were found with the following procedure:
<ul>
<li>Generate 29 base kmers for the entire genome, allow only kmers
    with bases: <b>A C T G</b>, no <b>N's</b> allowed.</li>
<li>Pair up identical kmers with at least one base separation and
    up to 20,000 bases separation.</li>
<li>Collapse overlapping kmer pairs when they are the same size of sequence
    and the same spacing between the pairs.  This procedure preserves the
    definition of <em>duplicated identical</em> pairs.</li>
<li>The resulting pairs can now be longer sequences with smaller separation
    then the constituent pairs</li>
<li>Final result selects sizes of 30 bases or more for the size of the
    paired sequence, and at least one base remaining as a separation gap.</li>
<li>Collapsed pairs that close the gap are discarded.  They appear to
    indicate simple repeat sequences when this happens.  It would be
    interesting to have this result available, but that is not available
    at this time.</li>
</ul>
</p>
<p>
The reason for starting with 29 base sized pairs and then selecting
results of at least 30 base sized pairs results in a reasonable
number of 30 base pairs.  If the procedure starts with 30 base
sized pairs, it produces way too many 30 base kmer pairs for
a reasonable count.
</p>
<p>
<h2>See Also</h2>
Interactive tables of all results:
<ul>
<li><a href="http://genomewiki.ucsc.edu/index.php/GapOverlap"
    target=_blank>Gap Overlaps</a>
<li><a href="http://genomewiki.ucsc.edu/index.php/TandemDups"
    target=_blank>Tandem Dups</a>
</ul>
</p>
<h2>Credits</h2>
<p>
Thank you to Joel Armstrong and Benedict Paten of the
<a href="https://cgl.genomics.ucsc.edu/" target=_blank>
Computational Genomics Lab</a>
at the
<a href="https://ucscgenomics.soe.ucsc.edu/" target=_blank>
U.C. Santa Cruz Genomics Institute</a>
for identifying this characteristic of genome assemblies.
<p>

<p> The data and presentation of this track were prepared by
<a href="mailto:&#104;&#105;&#114;a&#109;&#64;&#115;&#111;&#101;
.&#117;&#99;&#115;&#99;.&#101;&#100;u">Hiram Clawson</a>,
<a href="https://ucscgenomics.soe.ucsc.edu/" target=_blank>
U.C. Santa Cruz Genomics Institute</a>
</p>
_EOF_
   ;
