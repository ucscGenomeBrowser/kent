#!/usr/bin/env perl

use strict;
use warnings;
use FindBin qw($Bin);
use lib "$Bin";
use AsmHub;
use File::Basename;

my $argc = scalar(@ARGV);

if ($argc != 3) {
  printf STDERR "usage: asmHubAugustusGene.pl asmId asmId.names.tab bbi/asmId\n";
  printf STDERR "where asmId is the assembly identifier,\n";
  printf STDERR "and   asmId.names.tab is naming file for this assembly,\n";
  printf STDERR "and bbi/asmId is the path prefix to .augustus.bb.\n";
  exit 255;
}

my $asmId = shift;
my $namesFile = shift;
my $bbiPrefix = shift;
my $augustusBbi = "$bbiPrefix.augustus.bb";

if ( ! -s $augustusBbi ) {
  printf STDERR "ERROR: can not find augustus bbi file:\n\t'%s'\n", $augustusBbi;
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

my $geneCount = `bigBedInfo $augustusBbi | egrep "itemCount:|basesCovered:" | xargs echo | sed -e 's/itemCount/Gene count/; s/ basesCovered/; Bases covered/;'`;
chomp $geneCount;

print <<_EOF_
<h2>Description</h2>
<p>
This track shows <i>ab initio</i> predictions from the program
  <a href="http://bioinf.uni-greifswald.de/augustus/"
     target="_blank">AUGUSTUS</a> (version 3.1).
for the $assemblyDate $em${organism}$noEm/$asmId genome assembly.<br>
<br>
The predictions are based on the genome sequence alone.<br>
<br>
$geneCount
</p>

<h2>Methods</h2>

<p>
Statistical signal models were built for splice sites, branch-point
patterns, translation start sites, and the poly-A signal.
Furthermore, models were built for the sequence content of
protein-coding and non-coding regions as well as for the length distributions
of different exon and intron types. Detailed descriptions of most of these different models
can be found in Mario Stanke's
<a href="http://ediss.uni-goettingen.de/handle/11858/00-1735-0000-0006-B3F8-4" target="_blank">dissertation</a>.
This track shows the most likely gene structure according to a
Semi-Markov Conditional Random Field model.
Alternative splicing transcripts were obtained with
a sampling algorithm (<tt>--alternatives-from-sampling=true --sample=100 --minexonintronprob=0.2
--minmeanexonintronprob=0.5 --maxtracks=3 --temperature=2</tt>).
</p>

<p>
The different models used by Augustus were trained on a number of different species-specific
gene sets, which included 1000-2000 training gene structures. The <tt>--species</tt> option allows
one to choose the species used for training the models. Different training species were used
for the <tt>--species</tt> option when generating these predictions for different groups of
assemblies.
<table class="stdTbl">
        <tr>
                <td align=center><b>Assembly Group</b></td>
                <td align=center><b>Training Species</b></td>
        </tr>
        <tr>
                <td align=center>Fish</td>
                <td align=center><tt>zebrafish</tt>
        </tr>
        <tr>
                <td align=center>Birds</td>
                <td align=center><tt>chicken</tt>
        </tr>
        <tr>
                <td align=center>Human and all other vertebrates</td>
                <td align=center><tt>human</tt>
        </tr>
        <tr>
                <td align=center>Nematodes</td>
                <td align=center><tt>caenorhabditis</tt></td>
        </tr>
        <tr>
                <td align=center>Drosophila</td>
                <td align=center><tt>fly</tt></td>
        </tr>
        <tr>
                <td align=center><em>A. mellifera</em></td>
                <td align=center><tt>honeybee1</tt></td>
        </tr>
        <tr>
                <td align=center><em>A. gambiae</em></td>
                <td align=center><tt>culex</tt></td>
        </tr>
        <tr>
                <td align=center><em>S. cerevisiae</em></td>
                <td align=center><tt>saccharomyces</tt></td>
        </tr>
</table>
<p>
This table describes which training species was used for a particular group of assemblies.
When available, the closest related training species was used.
</p>

<h2>Credits</h2>

Thanks to the
<a href="https://math-inf.uni-greifswald.de/en/department/about-us/employees/prof-dr-mario-stanke-english/"
target="_blank">Stanke lab</a>
for providing the AUGUSTUS program.  The training for the <tt>chicken</tt> version was
done by Stefanie K&ouml;nig and the training for the
<tt>human</tt> and <tt>zebrafish</tt> versions was done by Mario Stanke.

<h2>References</h2>

<p>
Stanke M, Diekhans M, Baertsch R, Haussler D.
<a href="http://bioinformatics.oxfordjournals.org/content/24/5/637.long"
target="_blank">
Using native and syntenically mapped cDNA alignments to improve de novo gene finding</a>.
<em>Bioinformatics</em>. 2008 Mar 1;24(5):637-44.
PMID: <a href="https://www.ncbi.nlm.nih.gov/pubmed/18218656" target="_blank">18218656</a>
</p>

<p>
Stanke M, Waack S.
<a href="http://bioinformatics.oxfordjournals.org/content/19/suppl_2/ii215.long"
target="_blank">
Gene prediction with a hidden Markov model and a new intron submodel</a>.
<em>Bioinformatics</em>. 2003 Oct;19 Suppl 2:ii215-25.
PMID: <a href="https://www.ncbi.nlm.nih.gov/pubmed/14534192" target="_blank">14534192</a>
</p>
_EOF_
   ;
