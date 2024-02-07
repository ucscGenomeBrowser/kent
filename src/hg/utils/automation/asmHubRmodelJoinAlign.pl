#!/usr/bin/env perl

use strict;
use warnings;
use FindBin qw($Bin);
use lib "$Bin";
use AsmHub;
use File::Basename;

my $argc = scalar(@ARGV);

if ($argc != 2) {
  printf STDERR "usage: asmHubRmodelJoinAlign.pl asmId buildDir > asmId.repeatModeler.html\n";
  printf STDERR "where asmId is the assembly identifier,\n";
  printf STDERR "expecting to find buildDir/html/asmId.names.tab naming file for this assembly,\n";
  printf STDERR "and buildDir/trackData/repeatModeler/asmId.rmsk.class.profile counts of rmsk categories.\n";
  exit 255;
}

my $asmId = shift;
my $buildDir = shift;
my $namesFile = "$buildDir/html/$asmId.names.tab";
my $rModelClassProfile = "$buildDir/trackData/repeatMasker/$asmId.rmsk.class.profile.txt";
my $faSizeFile = "$buildDir/trackData/repeatModeler/faSize.rmsk.txt";
my $rmskVersion = "$buildDir/trackData/repeatModeler/versionInfo.txt";
my $rModelVersion = "$buildDir/trackData/repeatModeler/modelerVersion.txt";

my $maskingPercent = "";
if ( -s "${faSizeFile}" ) {
  $maskingPercent=`grep -w masked "${faSizeFile}" | cut -d' ' -f1`;
  chomp $maskingPercent;
}

my $errOut = 0;
if ( ! -s $rModelClassProfile ) {
  printf STDERR "ERROR: can not find rmsk class profile file:\n\t'%s'\n", $rModelClassProfile;
  $errOut = 255;
}

if ( ! -s $namesFile ) {
  printf STDERR "ERROR: can not find rmsk class profile file:\n\t'%s'\n", $rModelClassProfile;
  $errOut = 255;
}

if ($errOut) {
  exit $errOut;
}

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
This track shows the Repeat Masker annotations on the $assemblyDate $em${organism}$noEm/$asmId genome assembly as calculated from a custom library produced by RepeatModeler.
</p>

<p>
This track was created by using Arian Smit's
<a href="http://www.repeatmasker.org/" target="_blank">RepeatMasker</a>
program, which screens DNA sequences
for interspersed repeats and low complexity DNA sequences. The program
outputs a detailed annotation of the repeats that are present in the
query sequence (represented by this track), as well as a modified version
of the query sequence in which all the annotated repeats have been masked
(generally available on the
<a href="http://hgdownload.soe.ucsc.edu/downloads.html"
target=_blank>Downloads</a> page). RepeatMasker uses the
<a href="http://www.girinst.org/repbase/update/index.html"
target=_blank>Repbase Update</a> library of repeats from the
<a href="http://www.girinst.org/" target=_blank>Genetic 
Information Research Institute</a> (GIRI).
Repbase Update is described in Jurka (2000) in the References section below.</p>
_EOF_
;

if ( length($maskingPercent) ) {
  printf "<h2>Percent masking of sequence: %s</h2>\n", $maskingPercent;
  my $asmSize=`grep -w bases "${faSizeFile}" | cut -d' ' -f1`;
  chomp $asmSize;
  my $maskedBases=`grep -w bases "${faSizeFile}" | cut -d' ' -f9`;
  chomp $maskedBases;
  printf "<p><b>Assembly size:</b> %s bases<br>\n", &AsmHub::commify($asmSize);
  printf "<b>Sequence masked:</b> %s bases\n", &AsmHub::commify($maskedBases);
  printf "</p>\n";
}

if ( -s "$rModelVersion" ) {
  my $modelerVersion = `cat $rModelVersion`;
  chomp $modelerVersion;
  $modelerVersion =~ s/version/version:/;
  printf "<h2>%s</h2>\n", $modelerVersion;
}

if ( -s "$rmskVersion" ) {

print <<_EOF_
<h2>RepeatMasker and libraries version</h2>
<p>
<pre>
_EOF_
;
print `cat $rmskVersion`;
print <<_EOF_
</pre>
</p>
_EOF_
;

}

print <<_EOF_
<h2>Display Conventions and Configuration</h2>
<h4>Context Sensitive Zooming</h4>
<p>
This track employs a technique which chooses the appropriate visual representation for the data based on the
zoom scale, and or the number of annotations currently in view.  The track will automatically switch from the
most detailed visualization ('Full' mode) to the denser view ('Pack' mode) when the window size is greater
than 45kb of sequence.  It will further switch to the even denser single line view ('Dense' mode) if more than
500 annotations are present in the current view.
</p>
<h4>Dense Mode Visualization</h4>
<p>
In dense display mode, a single line is displayed denoting the coverage of repeats using a series
of colored boxes.  The boxes are colored based on the classification of the repeat (see below for legend).
<br>
<br>
<!-- t2tRepeatMasker-dense-mode.png -->
<img height="31" width="1070" src="/images/rmskDense.jpg">
</p>
<h4>Pack Mode Visualization</h4>
<p>
In pack mode, repeats are represented as sets of joined features.  These are color coded as above based on the
class of the repeat, and the further details such as orientation (denoted by chevrons) and a family label are provided.
This family label may be optionally turned off in the track configuration.
<br>
<br>
<!-- t2tRepeatMasker-pack-mode.png -->
<img height="94" width="1092" src="/images/rmskPack.jpg">
<br>
<br>
The pack display mode may also be configured to resemble the original UCSC repeat track.  In this visualization
repeat features are grouped by classes (see below), and displayed on seperate track lines.  The repeat ranges are
denoted as grayscale boxes, reflecting both the size of the repeat and
the amount of base mismatch, base deletion, and base insertion associated with a repeat element.
The higher the combined number of these, the lighter the shading.
<br>
<br>
<!-- t2tRepeatMasker-orig-pack-mode.png -->
<img height="116" width="1216" src="/images/rmskClassicPack.jpg">
</p>
<h4>Full Mode Visualization</h4>
<p>
In the most detailed visualization repeats are displayed as chevron boxes, indicating the size and orientation of
the repeat.  The interior grayscale shading represents the divergence of the repeat (see above) while the outline color
represents the class of the repeat. Dotted lines above the repeat and extending left or right
indicate the length of unaligned repeat model sequence and provide context for where a repeat fragment originates in its
consensus or pHMM model.  If the length of the unaligned sequence
is large, an interruption line and bp size is indicated instead of drawing the extension to scale.
<br>
<br>
<!-- t2tRepeatMasker-full-mode.png -->
<img height="90" width="1098" src="/images/rmskFull.jpg">
</p>
<p>
For example, the following repeat is a SINE element in the forward orientation with average
divergence. Only the 5' proximal fragment of the consensus sequence is aligned to the genome.
The 3' unaligned length (384bp) is not drawn to scale and is instead displayed using a set of
interruption lines along with the length of the unaligned sequence.
</p>
<img height="66" width="474" src="/images/rmskSine384.jpg">
<p>
Repeats that have been fragmented by insertions or large internal deletions are now represented
by join lines.  In the example below, a LINE element is found as two fragments.  The solid
connection lines indicate that there are no unaligned consensus bases between the two fragments.
Also note these fragments form the 3' extremity of the repeat, as there is no unaligned consensus
sequence following the last fragment.
</p>
<img height="83" width="464" src="/images/rmskLine2fragments.jpg">
<p>
In cases where there is unaligned consensus sequence between the fragments, the repeat will look like
the following.  The dotted line indicates the length of the unaligned sequence between the two
fragments.  In this case the unaligned consensus is longer than the actual genomic distance between
these two fragments.
</p>
<img height="71" width="463" src="/images/rmskLine2unaligned.jpg">
<p>
If there is consensus overlap between the two fragments, the joining lines will be drawn to indicate
how much of the left fragment is repeated in the right fragment.
</p>
<img height="70" width="462" src="/images/rmskLine2overlap.jpg">
<p>
The following table lists the repeat class colors:
</p>

<table>
  <thead>
  <tr>
    <th style="border-bottom: 2px solid #6678B1;">Color</th>
    <th style="border-bottom: 2px solid #6678B1;">Repeat Class</th>
  </tr>
  </thead>
  <tr>
    <th bgcolor="#1F77B4"></th>
    <th align="left">SINE - Short Interspersed Nuclear Element</th>
  </tr>
  <tr>
    <th bgcolor="#FF7F0E"></th>
    <th align="left">LINE - Long Interspersed Nuclear Element</th>
  </tr>
  <tr>
    <th bgcolor="#2CA02C"></th>
    <th align="left">LTR - Long Terminal Repeat</th>
  </tr>
  <tr>
    <th bgcolor="#D62728"></th>
    <th align="left">DNA - DNA Transposon</th>
  </tr>
  <tr>
    <th bgcolor="#9467BD"></th>
    <th align="left">Simple - Single Nucleotide Stretches and Tandem Repeats</th>
  </tr>
  <tr>

  <tr>
    <th bgcolor="#8C564B"></th>
    <th align="left">Low_complexity - Low Complexity DNA</th>
  </tr>
  <tr>
    <th bgcolor="#E377C2"></th>
    <th align="left">Satellite - Satellite Repeats</th>
  </tr>
  <tr>
    <th bgcolor="#7F7F7F"></th>
    <th align="left">RNA - RNA Repeats (including RNA, tRNA, rRNA, snRNA, scRNA, srpRNA)</th>
  </tr>
  <tr>
    <th bgcolor="#BCBD22"></th>
    <th align="left">Other - Other Repeats (including class RC - Rolling Circle)</th>
  </tr>
  <tr>
    <th bgcolor="#17BECF"></th>
    <th align="left">Unknown - Unknown Classification</th>
  </tr>
</table>
</p>

<p>
A &quot;?&quot; at the end of the &quot;Family&quot; or &quot;Class&quot; (for example, DNA?)
signifies that the curator was unsure of the classification. At some point in the future,
either the &quot;?&quot; will be removed or the classification will be changed.</p>

<h2>Methods</h2>

<p>
The RepeatMasker (<a href="www.repeatmasker.org">www.repeatmasker.org</a>) tool was used to generate the datasets found on this track hub.  
</p>

<h2>Class profiles</h2>
<p>
<ul>
_EOF_
   ;

open (FH, "grep classBed $rModelClassProfile | sed -e 's/^  *//; s#$asmId.rmsk.##; s#classBed/##; s#.bed##;'|sort -rn|") or die "can not grep $rModelClassProfile";
while (my $line = <FH>) {
  chomp $line;
  my ($count, $class) = split('\s+', $line);
  printf "<li>%s - %s</li>\n", &AsmHub::commify($count), $class;
}
close (FH);
printf "</ul>\n</p>\n<h2>Detail class profiles</h2>\n<p>\n<ul>\n";
open (FH, "grep rmskClass $rModelClassProfile | sed -e 's/^  *//; s#rmskClass/##; s#.tab##;'|sort -rn|") or die "can not grep $rModelClassProfile";
while (my $line = <FH>) {
  chomp $line;
  my ($count, $class) = split('\s+', $line);
  printf "<li>%s - %s</li>\n", &AsmHub::commify($count), $class;
}
close (FH);

print <<_EOF_
</ul>
</p>
<h2>Credits</h2>

<p>
Thanks to Arian Smit, Robert Hubley and GIRI for providing the tools and
repeat libraries used to generate this track.
</p>

<h2>References</h2>

<p>
Smit AFA, Hubley R, Green P. ${em}RepeatMasker Open-3.0${noEm}.
<a href="http://www.repeatmasker.org" target="_blank">
http://www.repeatmasker.org</a>. 1996-2010.
</p>

<p>
Repbase Update is described in:
</p>

<p>
Jurka J.
<a href="http://www.sciencedirect.com/science/article/pii/S016895250002093X" target="_blank">
Repbase Update: a database and an electronic journal of repetitive elements</a>.
${em}Trends Genet${noEm}. 2000 Sep;16(9):418-420.
PMID: <a href="https://www.ncbi.nlm.nih.gov/pubmed/10973072" target="_blank">10973072</a>
</p>

<p>
For a discussion of repeats in mammalian genomes, see:
</p>

<p>
Smit AF.
<a href="http://www.sciencedirect.com/science/article/pii/S0959437X99000313" target="_blank">
Interspersed repeats and other mementos of transposable elements in mammalian genomes</a>.
${em}Curr Opin Genet Dev${noEm}. 1999 Dec;9(6):657-63.
PMID: <a href="https://www.ncbi.nlm.nih.gov/pubmed/10607616" target="_blank">10607616</a>
</p>

<p>
Smit AF.
<a href="http://www.sciencedirect.com/science/article/pii/S0959437X9680030X" target="_blank">
The origin of interspersed repeats in the human genome</a>.
${em}Curr Opin Genet Dev${noEm}. 1996 Dec;6(6):743-8.
PMID: <a href="https://www.ncbi.nlm.nih.gov/pubmed/8994846" target="_blank">8994846</a>
</p>
_EOF_
   ;

