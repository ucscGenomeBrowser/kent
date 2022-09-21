#!/usr/bin/env perl

use strict;
use warnings;
use FindBin qw($Bin);
use lib "$Bin";
use AsmHub;
use File::Basename;

my $argc = scalar(@ARGV);

if ($argc != 3) {
  printf STDERR "usage: asmHubTOGA.pl asmId asmId.names.tab .../trackData/\n";
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
my $TOGABbi = `ls $trackDataDir/TOGAv*/HLTOGAannotVs*.bb`;
chomp $TOGABbi;
my $track = "TOGAannotation";

if ( ! -s $TOGABbi ) {
  printf STDERR "ERROR: can not find trackData/TOGAv*/HLTOGAannotVs*.bb file\n";
  exit 255;
}

my $gcX = substr($asmId,0,3);
my $d0 = substr($asmId,4,3);
my $d1 = substr($asmId,7,3);
my $d2 = substr($asmId,10,3);
my (undef, $acc, undef) = split('_', $asmId, 3);
my $accession = "${gcX}_${acc}";

my $togaDir = dirname($TOGABbi);
$togaDir =~ s#.*/trackData/##;

my $bbFile = basename($TOGABbi);

my $totalBases = `ave -col=2 $trackDataDir/../${asmId}.chrom.sizes | grep "^total" | awk '{printf "%d", \$2}'`;
chomp $totalBases;
my $itemsBases = `bigBedInfo $TOGABbi | egrep "itemCount|basesCovered" | awk '{print \$NF}' | sed -e 's/,//g;' | xargs echo`;
my ($itemCount, $basesCovered) = split('\s+', $itemsBases);

my $percentCoverage = sprintf("%.2f", 100.0 * $basesCovered / $totalBases);
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

my $downloadUrl="https://hgdownload.soe.ucsc.edu/hubs/$gcX/$d0/$d1/$d2/$accession/bbi/$bbFile";
# there is a bug in the original files, should have been GalGal6
# the symlinks in bbi have been fixed
$downloadUrl =~ s/galGal6/GalGal6/;

print <<_EOF_

<h2>Description</h2>
<p>
<b>TOGA</b>
(<b>T</b>ool to infer <b>O</b>rthologs from <b>G</b>enome <b>A</b>lignments)
is a homology-based method that integrates gene annotation, inferring
orthologs and classifying genes as intact or lost.
</p>

<p>
This track has $itemCount items in the track, covering $basesCovered bases
in the sequence which is % $percentCoverage of the total sequence of size
$totalBases nucleotides.
</p>

<h2>Methods</h2>
<p>
As input, <b>TOGA</b> uses a gene annotation of a reference species
(human/hg38 for mammals, chicken/galGal6 for birds) and
a whole genome alignment between the reference and query genome.
</p>
<p>
<b>TOGA</b> implements a novel paradigm that relies on alignments of intronic
and intergenic regions and uses machine learning to accurately distinguish
orthologs from paralogs or processed pseudogenes.
</p>
<p>
To annotate genes,
<a href='https://academic.oup.com/bioinformatics/article/33/24/3985/4095639'
target=blank>CESAR 2.0</a>
is used to determine the positions and boundaries of coding exons of a
reference transcript in the orthologous genomic locus in the query species.
</p>

<h2>Display Conventions and Configuration</h2>
<p>
Each annotated transcript is shown in a color-coded classification as
<ul>
<li><span style='display:inline-block; width:40px; height:15px; background-color:blue;'>&nbsp;</span>
    <span style='color:blue'>"intact"</span>: middle 80% of the CDS
    (coding sequence) is present and exhibits no gene-inactivating mutation.
    These transcripts likely encode functional proteins.</li>
<li><span style='display:inline-block; width:40px; height:15px; background-color:lightblue;'>&nbsp;</span>
    <span style='color:#7193a0'>"partially intact"</span>: 50% of the CDS
     is present in the query and the middle 80% of the CDS exhibits no
     inactivating mutation. These transcripts may also encode functional
     proteins, but the evidence is weaker as parts of the CDS are missing,
     often due to assembly gaps.</li>
<li><span style='display:inline-block; width:40px; height:15px; background-color:grey;'>&nbsp;</span>
    <span style='color:grey'>"missing"</span>: &lt;50% of the CDS is present
     in the query and the middle 80% of the CDS exhibits no inactivating
     mutation.</li>
<li><span style='display:inline-block; width:40px; height:15px; background-color:orange;'>&nbsp;</span>
    <span style='color:orange'>"uncertain loss"</span>: there is 1
     inactivating mutation in the middle 80% of the CDS, but evidence is not
     strong enough to classify the transcript as lost. These transcripts may
     or may not encode a functional protein.</li>
<li><span style='display:inline-block; width:40px; height:15px; background-color:red;'>&nbsp;</span>
    <span style='color:red'>"lost"</span>: typically several inactivating
     mutations are present, thus there is strong evidence that the transcript
     is unlikely to encode a functional protein.</li>
</ul>
</p>
<p>
Clicking on a transcript provides additional information about the orthology
classification, inactivating mutations, the protein sequence and protein/exon
alignments.
</p>

<H2>Data Access</H2>
<p>
The data for this track is available from the <em>bigBed</em> file format
with the command line access tool <em>bigBedToBed</em> available from
the utilities download directory
<a href='http://hgdownload.soe.ucsc.edu/admin/exe/linux_x86_64/'
target=_blank>hgdownload.soe.ucsc.edu/admin/exe/linux_x86_64</a>.
</p>

<p>
To extract from the <em>bigBed</em> file:
<pre>
  bigBedToBed "$downloadUrl" togaData.bed
</pre>
with the result in the <em>togaData.bed</em> file.
</p>

<h2>Credits</h2>
<p>
This data was prepared by the <a href='https://tbg.senckenberg.de/hillerlab/'
target=_blank>Michael Hiller Lab</a>
</p>

<h2>References</h2>
<p>
The <b>TOGA</b> software is available from
<a href='https://github.com/hillerlab/TOGA'
target=_blank>github.com/hillerlab/TOGA</a>
</p>

<p>
Kirilenko BM, Munegowda C, Osipova E, Jebb D, Sharma V, Blumer M, Morales A,
Ahmed AW, Kontopoulos DG, Hilgers L, Zoonomia Consortium, Hiller M.
<a href='https://www.biorxiv.org/content/10.1101/2022.09.08.507143v1'
target=_blank>TOGA integrates gene annotation with orthology inference
at scale</a>. <em>bioRxiv preprint September 2022</em>
</p>

_EOF_
   ;
