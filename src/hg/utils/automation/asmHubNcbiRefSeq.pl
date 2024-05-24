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

my $asmId = shift;
my @parts = split('_', $asmId, 3);
my $accession = "$parts[0]_$parts[1]";
my $namesFile = shift;
my $trackDataDir = shift;
my $ncbiRefSeqBbi = "$trackDataDir/ncbiRefSeq/$asmId.ncbiRefSeq.bb";
my $srcGff =  `ls $trackDataDir/ncbiRefSeq/download/*_genomic.gff.gz | head -1`;
chomp $srcGff;
my $srcAsmId = $asmId;
my $gcfToGcaLiftedText = "";
if (length($srcGff) > 10) {
  $srcAsmId = basename($srcGff);
  $srcAsmId =~ s/_genomic.gff.gz//;
  if ($srcAsmId ne $asmId) {
    $gcfToGcaLiftedText = "RefSeq annotations from <b>$srcAsmId</b> were lifted to this <b>$asmId</b> assembly to provide these gene annotations on this corresponding assembly."
  }
}
my $asmIdPath = &AsmHub::asmIdToPath($asmId);
my $downloadGtf = "https://hgdownload.soe.ucsc.edu/hubs/$asmIdPath/$accession/genes/$asmId.ncbiRefSeq.gtf.gz";

if ( ! -s $ncbiRefSeqBbi ) {
  printf STDERR "ERROR: can not find $asmId.ncbiRefSeq.bb file\n";
  exit 255;
}

my @partNames = split('_', $srcAsmId);
my $ftpDirPath = sprintf("%s/%s/%s/%s/%s", $partNames[0],
   substr($partNames[1],0,3), substr($partNames[1],3,3),
   substr($partNames[1],6,3), $srcAsmId);

my $totalBases = `ave -col=2 $trackDataDir/../${asmId}.chrom.sizes | grep "^total" | awk '{printf "%d", \$2}'`;
chomp $totalBases;
my $geneStats = `cat $trackDataDir/ncbiRefSeq/${asmId}.ncbiRefSeq.stats.txt | awk '{printf "%d\\n", \$2}' | xargs echo`;
chomp $geneStats;
my ($itemCount, $basesCovered) = split('\s+', $geneStats);
my $percentCoverage = sprintf("%.3f", 100.0 * $basesCovered / $totalBases);
$itemCount = &AsmHub::commify($itemCount);
$basesCovered = &AsmHub::commify($basesCovered);
my $totalBasesCmfy = &AsmHub::commify($totalBases);

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
The NCBI RefSeq Genes composite track shows
$assemblyDate $em${organism}$noEm/$asmId
protein-coding and non-protein-coding genes taken from the NCBI RNA reference
sequences collection (RefSeq). All subtracks use coordinates provided by RefSeq.
See the <a href="#methods">Methods</a> section for more details about how
the different tracks were created.
</p>
<p>
Please visit NCBI's
<a href="https://www.ncbi.nlm.nih.gov/projects/RefSeq/update.cgi"
target="_blank"> Feedback for Gene and Reference Sequences (RefSeq)</a>
page to make suggestions, submit additions and corrections, or ask for
help concerning RefSeq records.
</p>

<p>
For more information on the different gene tracks, see our <a target=_blank 
href="/FAQ/FAQgenes.html">Genes FAQ</a>.
</p>

<h2>Data Access</h2>
<p>
Download <a href='$downloadGtf' target=_blank> $asmId.ncbiRefSeq.gtf.gz </a> GTF file.
</p>

<h2>Display Conventions and Configuration</h2>
<p>
To show only a selected set of subtracks, uncheck the boxes next to the
tracks that you wish to hide.
</p>

The tracks available here can include (not all may be present):
<dl>
  <dt><em><strong>RefSeq annotations and alignments</strong></em></dt>
  <ul>
    <li><em>RefSeq All</em> &ndash; all curated and predicted annotations
     provided by RefSeq.</li>
    <li><em>RefSeq Curated</em> &ndash; subset of <em>RefSeq All</em> that
     includes only those annotations whose accessions begin with NM, NR,
     NP or YP. <small>(NP and YP are used only for protein-coding genes on
     the mitochondrion; YP is used for human only.)</small></li>
    <li><em>RefSeq Predicted</em> &ndash; subset of RefSeq All that includes
     those annotations whose accessions begin with XM or XR.</li>
    <li><em>RefSeq Other</em> &ndash; all other annotations produced by the
     RefSeq group that do not fit the requirements for inclusion in
     the <em>RefSeq Curated</em> or the <em>RefSeq Predicted</em> tracks.</li>
    <li> <em>RefSeq Alignments</em> &ndash; alignments of RefSeq RNAs to
     the $assemblyDate $em${organism}$noEm/$asmId
     genome provided by the RefSeq group.</li>
  </ul>
</dl>

<p>
The <em>RefSeq All</em>, <em>RefSeq Curated</em> and <em>RefSeq Predicted</em>,
tracks follow the display conventions for
<a href="../goldenPath/help/hgTracksHelp.html#GeneDisplay"
target="_blank">gene prediction tracks</a>.
The color shading indicates the level of review the RefSeq record has undergone:
predicted (light), provisional (medium), or reviewed (dark), as defined by
<a target=_blank href="https://www.ncbi.nlm.nih.gov/books/NBK21091/table/ch18.T.refseq_status_codes/?report=objectonly">RefSeq</a>. </p>

<p>
<table>
  <thead>
  <tr>
    <th style="border-bottom: 2px solid #6678B1;">Color</th>
    <th style="border-bottom: 2px solid #6678B1;">Level of review</th>
  </tr>
  </thead>
  <tr>
    <th bgcolor="#0C0C78"></th>
    <th align="left">Reviewed: the RefSeq record has been reviewed by NCBI
      staff or by a collaborator. The NCBI review process includes assessing
      available sequence data and the literature. Some RefSeq records may
      incorporate expanded sequence and annotation information.</th>
  </tr>
  <tr>
    <th bgcolor="#5050A0"></th>
    <th align="left">Provisional: the RefSeq record has not yet been subject
      to individual review. The initial sequence-to-gene association has been
      established by outside collaborators or NCBI staff.</th>
  </tr>
  <tr>
    <th bgcolor="#8282D2"></th>
    <th align="left">Predicted: the RefSeq record has not yet been subject to
      individual review, and some aspect of the RefSeq record is predicted.</th>
  </tr>
</table>
</p>

The <em>RefSeq Alignments</em> track follows the display conventions for
<a href="../goldenPath/help/hgTracksHelp.html#PSLDisplay" target="_blank">PSL tracks</a>.</p>
<p>
The item labels and codon display properties for features within this track
can be configured through the controls at the top of the track description
page.  To adjust the settings for an individual subtrack, click the wrench
icon next to the track name in the subtrack list.
</p>
<ul>   
  <li><strong>Label:</strong> By default, items are labeled by gene name. Click
   the appropriate Label option to display the accession name or OMIM
   identifier instead of the gene name, show all or a subset of these labels
   including the gene name, OMIM identifier and accession names, or turn off 
   the label completely.</li>
  <li><strong>Codon coloring:</strong> This track has an optional codon
   coloring feature that allows users to quickly validate and compare gene
   predictions. To display codon colors, select the <em>genomic codons</em>
   option from the <em>Color track by codons</em> pull-down menu. For more 
   information about this feature, go to the <a href="../goldenPath/help/hgCodonColoring.html" 
  target="_blank">Coloring Gene Predictions and Annotations by Codon</a> page.</li>
</ul>

<a name="methods"></a>
<h2>Methods</h2>
<p>
The RefSeq annotation and RefSeq RNA alignment tracks
were created at UCSC using data from the NCBI RefSeq project. GFF format
data files were downloaded from the file <b>${srcAsmId}_genomic.gff.gz</b>
delivered with the NCBI RefSeq genome assemblies at the FTP location:<br>
<a href='https://ftp.ncbi.nlm.nih.gov/genomes/all/$ftpDirPath/' target='_blank'>https://ftp.ncbi.nlm.nih.gov/genomes/all/$ftpDirPath/</a>

$gcfToGcaLiftedText

The GFF file was converted to the
genePred and PSL table formats for display in the Genome Browser.
Information about the NCBI annotation pipeline can be found 
<a href="https://www.ncbi.nlm.nih.gov/genome/annotation_euk/process/"
target="_blank">here</a>.
</p>

<h2>Track statistics summary</h2>
<p>
<b>Total genome size: </b>$totalBasesCmfy <b>bases</b><br><br>
<b>Curated and Predicted Gene count: </b>$itemCount<br>
<b>Bases in these genes: </b>$basesCovered<br>
<b>Percent genome coverage: </b>% $percentCoverage<br>
</p>

_EOF_
   ;

if ( -s "$trackDataDir/ncbiRefSeq/${asmId}.ncbiRefSeqCurated.stats.txt" ) {
  $geneStats = `cat $trackDataDir/ncbiRefSeq/${asmId}.ncbiRefSeqCurated.stats.txt | awk '{printf "%d\\n", \$2}' | xargs echo`;
  chomp $geneStats;
  ($itemCount, $basesCovered) = split('\s+', $geneStats);
  $percentCoverage = sprintf("%.3f", 100.0 * $basesCovered / $totalBases);
  $itemCount = &AsmHub::commify($itemCount);
  $basesCovered = &AsmHub::commify($basesCovered);
  printf <<_EOF_
<p>
<b>Curated gene count: </b>$itemCount<br>
<b>Bases in curated genes: </b>$basesCovered<br>
<b>Percent genome coverage: </b>%% $percentCoverage<br>
</p>
_EOF_
} else {
  printf <<_EOF_
<p>
<b>There are no curated gene annotations.</b>
</p>
_EOF_
}

if ( -s "$trackDataDir/ncbiRefSeq/${asmId}.ncbiRefSeqPredicted.stats.txt" ) {
  $geneStats = `cat $trackDataDir/ncbiRefSeq/${asmId}.ncbiRefSeqPredicted.stats.txt | awk '{printf "%d\\n", \$2}' | xargs echo`;
  chomp $geneStats;
  ($itemCount, $basesCovered) = split('\s+', $geneStats);
  $percentCoverage = sprintf("%.3f", 100.0 * $basesCovered / $totalBases);
  $itemCount = &AsmHub::commify($itemCount);
  $basesCovered = &AsmHub::commify($basesCovered);
  printf <<_EOF_
<p>
<b>Predicted gene count: </b>$itemCount<br>
<b>Bases in genes: </b>$basesCovered<br>
<b>Percent genome coverage: </b>%% $percentCoverage<br>
</p>
_EOF_
} else {
  printf <<_EOF_
<p>
<b>there are no predicted gene annotations</b>
</p>
_EOF_
}

if ( -s "$trackDataDir/ncbiRefSeq/${asmId}.ncbiRefSeqOther.stats.txt" ) {
  $geneStats = `cat $trackDataDir/ncbiRefSeq/${asmId}.ncbiRefSeqOther.stats.txt | awk '{printf "%d\\n", \$2}' | xargs echo`;
  chomp $geneStats;
  ($itemCount, $basesCovered) = split('\s+', $geneStats);
  $percentCoverage = sprintf("%.3f", 100.0 * $basesCovered / $totalBases);
  $itemCount = &AsmHub::commify($itemCount);
  $basesCovered = &AsmHub::commify($basesCovered);
  printf <<_EOF_
<p>
<b>Other annotation count: </b>$itemCount<br>
<b>Bases in other annotations: </b>$basesCovered<br>
<b>Percent genome coverage: </b>%% $percentCoverage<br>
</p>
_EOF_
}

printf <<_EOF_
<h2>Credits</h2>
<p>
This track was produced at UCSC from data generated by scientists worldwide
and curated by the NCBI RefSeq project.
</p>

<h2>References</h2>
<p>
Kent WJ.  <a href="https://genome.cshlp.org/content/12/4/656.full"
target="_blank">BLAT - the BLAST-like alignment tool</a>.
<em>Genome Res.</em> 2002 Apr;12(4):656-64.  PMID:
<a href="https://www.ncbi.nlm.nih.gov/pubmed/11932250"
target="_blank">11932250</a>; PMC:
<a href="https://www.ncbi.nlm.nih.gov/pmc/articles/PMC187518/"
target="_blank">PMC187518</a>
</p>
<p>
Pruitt KD, Brown GR, Hiatt SM, Thibaud-Nissen F, Astashyn A,
Ermolaeva O, Farrell CM, Hart J, Landrum MJ, McGarvey KM <em>et al</em>.
<a href="https://academic.oup.com/nar/article/42/D1/D756/1051112/RefSeq-an-update-on-mammalian-reference-sequences"
target="_blank">RefSeq: an update on mammalian reference sequences</a>.
<em>Nucleic Acids Res</em>. 2014 Jan;42(Database issue):D756-63.  PMID:
<a href="https://www.ncbi.nlm.nih.gov/pubmed/24259432"
target="_blank">24259432</a>; PMC:
<a href="https://www.ncbi.nlm.nih.gov/pmc/articles/PMC3965018/"
target="_blank">PMC3965018</a>
</p>
<p>
Pruitt KD, Tatusova T, Maglott DR.
<a href="https://academic.oup.com/nar/article/33/suppl_1/D501/2505241/NCBI-Reference-Sequence-RefSeq-a-curated-non" target="_blank">
NCBI Reference Sequence (RefSeq): a curated non-redundant sequence database
of genomes, transcripts and proteins</a>.
<em>Nucleic Acids Res.</em> 2005 Jan 1;33(Database issue):D501-4.  PMID:
<a href="https://www.ncbi.nlm.nih.gov/pubmed/15608248"
target="_blank">15608248</a>; PMC: <a
href="https://www.ncbi.nlm.nih.gov/pmc/articles/PMC539979/"
target="_blank">PMC539979</a>
</p>
_EOF_
