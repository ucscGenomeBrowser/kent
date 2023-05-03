#!/usr/bin/env perl

use strict;
use warnings;
use FindBin qw($Bin);
use lib "$Bin";
use AsmHub;
use File::Basename;

my $argc = scalar(@ARGV);

if ($argc != 4) {
  printf STDERR "usage: asmHubEbiGene.pl asmId asmId.names.tab bbi/asmId ebiVersion\n";
  printf STDERR "where asmId is the assembly identifier,\n";
  printf STDERR "and   asmId.names.tab is naming file for this assembly,\n";
  printf STDERR "and bbi/asmId is the path prefix to .ebiGene.bb.\n";
  printf STDERR "the ebiVersion is from trackData/ebiGene/version.txt\n";
  exit 255;
}

# from Perl Cookbook Recipe 2.17, print out large numbers with comma
# delimiters:
sub commify($) {
    my $text = reverse $_[0];
    $text =~ s/(\d\d\d)(?=\d)(?!\d*\.)/$1,/g;
    return scalar reverse $text
}

# $scriptDir/asmHubEbiGene.pl $asmId $buildDir/html/$asmId.names.tab $buildDir/bbi/$asmId > $buildDir/html/$asmId.ebiGene.html "${ebiVersion}"

my $asmId = shift;
my @parts = split('_', $asmId, 3);
my $accession = "$parts[0]_$parts[1]";
my $gcX = substr($asmId,0,3);
my $d0 = substr($asmId,4,3);
my $d1 = substr($asmId,7,3);
my $d2 = substr($asmId,10,3);
my $namesFile = shift;
my $bbiPrefix = shift;
my $ebiVersion = shift;
my $ebiGeneBbi = "$bbiPrefix.ebiGene.bb";
my $runDir = $bbiPrefix;
$runDir =~ s#/bbi/.*#/trackData/ebiGene#;
my $fbResults = "${runDir}/fb.ebiGene.txt";
my $fbBases = "";
if ( -s "${fbResults}" ) {
  ($fbBases, undef) = split('\s+', `cat $fbResults`);
}

if ( ! -s $ebiGeneBbi ) {
  printf STDERR "ERROR: can not find ebiGene bbi file:\n\t'%s'\n", $ebiGeneBbi;
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
my $itemCount = `grep itemCount ${runDir}/ebiGene.stats.txt | awk '{print \$NF}'`;
chomp $itemCount;
my $bases = `grep basesCovered ${runDir}/ebiGene.stats.txt | awk '{print \$NF}'`;
chomp $bases;
my $geneCount = sprintf("Gene count: %s; Bases covered: %s", commify($itemCount), commify($bases));
if (length($fbBases)) {
  $geneCount .= sprintf(" (%s bases in exons only)", commify($fbBases));
}

print <<_EOF_
<h2>Description</h2>
<p>
Ensembl genes annotations of the HPRC assemblies, version: $ebiVersion
on the $assemblyDate $em${organism}$noEm/$asmId genome assembly.
<br>
$geneCount
</p>

<h2>Methods</h2>
<p>
Ensembl annotation of the human assemblies has been produced via a new
mapping pipeline:<br>
<br>
A subset of the GENCODE 38 genes and transcripts have been annotated on each
of the haploid assemblies. The subset excludes readthrough genes and genes
on patches or haplotypes.  For each gene, anchor sequences built from the
surrounding region were used to locate the most likely corresponding
region(s) in the target genome. A pairwise alignment of the reference and
target regions was then carried out and used to map the exon coordinates and
other features of the gene.  In addition to the primary mapping, potential
recent duplications and collapsed paralogues were identified by aligning
canonical transcripts across the entire genome and searching for new
mappings that did not overlap existing annotations.  For more details on the
annotation process, please refer to the
<a href="https://www.biorxiv.org/content/10.1101/2022.07.09.499321v1" target="_blank">preprint publication</a>
(see "Methods" section: "Ensembl Mapping Pipeline for Assembly Annotation").
</p>

<h2>Data availability</h2>

<p>Ensembl Human Pangenome Reference Consortium:
<a href="https://projects.ensembl.org/hprc/" target="_blank">https://projects.ensembl.org/hprc/</a>
</p>
<p>The bigGenePred file in this assembly hub can be obtained from:
<a href="https://hgdownload.soe.ucsc.edu/hubs/$gcX/$d0/$d1/$d2/$accession/bbi/$asmId.ebiGene.bb" target=_blank>https://hgdownload.soe.ucsc.edu/hubs/$gcX/$d0/$d1/$d2/$accession/bbi/$asmId.ebiGene.bb</a>
</p>

<h2>References</h2>
<p>
A Draft Human Pangenome Reference<br>
Wen-Wei Liao, Mobin Asri, Jana Ebler, Daniel Doerr, Marina Haukness,
Glenn Hickey, Shuangjia Lu, Julian K. Lucas, Jean Monlong, Haley J. Abel,
Silvia Buonaiuto, Xian H. Chang, Haoyu Cheng, Justin Chu, Vincenza Colonna,
Jordan M. Eizenga, Xiaowen Feng, Christian Fischer, Robert S. Fulton,
Shilpa Garg, Cristian Groza, Andrea Guarracino, William T Harvey,
Simon Heumos, Kerstin Howe, Miten Jain, Tsung-Yu Lu, Charles Markello,
Fergal J. Martin, Matthew W. Mitchell, Katherine M. Munson,
Moses Njagi Mwaniki, Adam M. Novak, Hugh E. Olsen, Trevor Pesout,
David Porubsky, Pjotr Prins, Jonas A. Sibbesen, Chad Tomlinson,
Flavia Villani, Mitchell R. Vollger, Human Pangenome Reference Consortium,
Guillaume Bourque, Mark JP Chaisson, Paul Flicek, Adam M. Phillippy,
Justin M. Zook, Evan E. Eichler, David Haussler, Erich D. Jarvis,
Karen H. Miga, Ting Wang, Erik Garrison, Tobias Marschall, Ira Hall,
Heng Li, Benedict Paten<br>
bioRxiv: <a href="https://www.biorxiv.org/content/10.1101/2022.07.09.499321v1" target="_blank">2022.07.09.499321</a>;
doi: <a href="https://doi.org/10.1101/2022.07.09.499321" target="_blank">https://doi.org/10.1101/2022.07.09.499321></p>

<h2>Contact</h2>
For inquiries, please contact:
<p><a href="http://useast.ensembl.org/info/about/contact/index.html" target="_blank">Contact Ensembl</a></p>

<h2>Credits</h2>
<p><a href="https://projects.ensembl.org/hprc/" target="_blank">Ensembl</a></p>
_EOF_
   ;

