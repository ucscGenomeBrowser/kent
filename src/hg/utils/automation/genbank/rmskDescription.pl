#!/usr/bin/env perl

use strict;
use warnings;
use File::Basename;

my $argc = scalar(@ARGV);

if ($argc != 1) {
  printf STDERR "usage: rmskDescription.pl <buildDirectory>\n";
  printf STDERR "where <buildDirectory> is where the assembly hub is under construction.\n";
  exit 255;
}

my $wrkDir = shift;

if ( ! -d $wrkDir ) {
  printf STDERR "ERROR: can not find work directory:\n\t'%s'\n", $wrkDir;
  exit 255;
}

chdir $wrkDir;
# print STDERR `ls`;
my $accAsm = basename($wrkDir);

my $bbiCount = `ls bbi/*rmsk*.bb 2> /dev/null | wc -l`;
chomp $bbiCount;
exit 0 if ( $bbiCount < 1);

my $groupName = $wrkDir;
my $asmType = "genbank";
$asmType = "refseq" if ( $groupName =~ m#/refseq/#);
$groupName =~ s#.*/$asmType/##;
$groupName =~ s#/.*##;
my $selfUrl = "${groupName}/${accAsm}";

# printf STDERR "%s\n", $accAsm;
my $htmlFile="${accAsm}.repeatMasker.html";
open (FH, ">$htmlFile") or die "can not write to $htmlFile";

print FH <<_EOF_
<h2>Description</h2>

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

<h2>Display Conventions and Configuration</h2>

<p>
In full display mode, this track displays up to ten different classes of repeats:
<ul>
<li>Short interspersed nuclear elements (SINE), which include ALUs</li>
<li>Long interspersed nuclear elements (LINE)</li>
<li>Long terminal repeat elements (LTR), which include retroposons</li>
<li>DNA repeat elements (DNA)</li>
<li>Simple repeats (micro-satellites)</li>
<li>Low complexity repeats</li>
<li>Satellite repeats</li>
<li>RNA repeats (including RNA, tRNA, rRNA, snRNA, scRNA, srpRNA)</li>
<li>Other repeats, which includes class RC (Rolling Circle)</li>
<li>Unknown</li>
</ul>
</p>

<p>
The level of color shading in the graphical display reflects the amount of
base mismatch, base deletion, and base insertion associated with a repeat
element. The higher the combined number of these, the lighter the shading.
</p>

<p>
A &quot;?&quot; at the end of the &quot;Family&quot; or &quot;Class&quot; (for example, DNA?) signifies that
the curator was unsure of the classification. At some point in the future,
either the &quot;?&quot; will be removed or the classification will be changed.</p>

<h2>Methods</h2>

<p>
UCSC has used the most current versions of the RepeatMasker software
and repeat libraries available to generate these data. Note that these
versions may be newer than those that are publicly available on the Internet.
</p>
<p>
Data are generated using the RepeatMasker <em>-s</em> flag. Additional flags
may be used for certain organisms.  Repeats are soft-masked. Alignments may
extend through repeats, but are not permitted to initiate in them.
See the <a href="/FAQ/FAQdownloads#download16">FAQ</a> for more information.
</p>

<h2>Credits</h2>

<p>
Thanks to Arian Smit, Robert Hubley and GIRI for providing the tools and
repeat libraries used to generate this track.
</p>

<h2>References</h2>

<p>
Smit AFA, Hubley R, Green P. <em>RepeatMasker Open-3.0</em>.
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
<em>Trends Genet</em>. 2000 Sep;16(9):418-420.
PMID: <a href="https://www.ncbi.nlm.nih.gov/pubmed/10973072" target="_blank">10973072</a>
</p>

<p>
For a discussion of repeats in mammalian genomes, see:
</p>

<p>
Smit AF.
<a href="http://www.sciencedirect.com/science/article/pii/S0959437X99000313" target="_blank">
Interspersed repeats and other mementos of transposable elements in mammalian genomes</a>.
<em>Curr Opin Genet Dev</em>. 1999 Dec;9(6):657-63.
PMID: <a href="https://www.ncbi.nlm.nih.gov/pubmed/10607616" target="_blank">10607616</a>
</p>

<p>
Smit AF.
<a href="http://www.sciencedirect.com/science/article/pii/S0959437X9680030X" target="_blank">
The origin of interspersed repeats in the human genome</a>.
<em>Curr Opin Genet Dev</em>. 1996 Dec;6(6):743-8.
PMID: <a href="https://www.ncbi.nlm.nih.gov/pubmed/8994846" target="_blank">8994846</a>
</p>
_EOF_
   ;

close (FH);
