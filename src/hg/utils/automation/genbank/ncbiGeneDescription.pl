#!/usr/bin/env perl

use strict;
use warnings;
use File::Basename;

my $argc = scalar(@ARGV);

if ($argc != 1) {
  printf STDERR "usage: ncbiGeneDescription.pl <buildDirectory>\n";
  printf STDERR "where <buildDirectory> is where the assembly hub is under construction.\n";
  exit 255;
}

# from Perl Cookbook Recipe 2.17, print out large numbers with comma
# delimiters:
sub commify($) {
    my $text = reverse $_[0];
    $text =~ s/(\d\d\d)(?=\d)(?!\d*\.)/$1,/g;
    return scalar reverse $text
}

my $wrkDir = shift;

if ( ! -d $wrkDir ) {
  printf STDERR "ERROR: can not find work directory:\n\t'%s'\n", $wrkDir;
  exit 255;
}

chdir $wrkDir;
# print STDERR `ls`;
my $accAsm = basename($wrkDir);

exit 0 if ( ! -s "bbi/${accAsm}.ncbiGene.ncbi.bb" );

my $ftpName = $wrkDir;
$ftpName =~ s#/hive/data/outside/ncbi/##;
$ftpName =~ s#/hive/data/inside/ncbi/##;
my $urlDirectory = `basename $ftpName`;
chomp $urlDirectory;
my $urlPath = $ftpName;

my $groupName = $wrkDir;
my $asmType = "genbank";
$asmType = "refseq" if ( $groupName =~ m#/refseq/#);
$groupName =~ s#.*/$asmType/##;
$groupName =~ s#/.*##;
my $selfUrl = "${groupName}/${accAsm}";
my $totalBases = `ave -col=2 ${accAsm}.ncbi.chrom.sizes | grep "^total" | awk '{printf "%d", \$2}'`;
chomp $totalBases;
my $geneStats = `cat ${accAsm}.ncbiGene.ncbi.stats.txt | awk '{printf "%d\\n", \$2}' | xargs echo`;
chomp $geneStats;
my ($itemCount, $basesCovered) = split('\s+', $geneStats);
my $percentCoverage = sprintf("%.3f", 100.0 * $basesCovered / $totalBases);
$itemCount = commify($itemCount);
$basesCovered = commify($basesCovered);
$totalBases = commify($totalBases);

my $htmlFile="${accAsm}.ncbiGene.html";
open (FH, ">$htmlFile") or die "can not write to $htmlFile";

printf FH "<h2>Description</h2>
<p>
The NCBI Gene track is constructed from the gff file <b>%s_genomic.gff.gz</b>
delivered with the NCBI RefSeq genome assemblies at the FTP location:<br>
<a href=\"ftp://ftp.ncbi.nlm.nih.gov/genomes/all/%s\" target=\"_blank\">ftp://ftp.ncbi.nlm.nih.gov/genomes/all/%s</a>
</p>\n", $accAsm, $urlDirectory, $accAsm;

print FH <<_EOF_
<h2>Track statistics summary</h2>
<p>
<b>Total genome size: </b>$totalBases</br>
<b>Gene count: </b>$itemCount<br>
<b>Bases in genes: </b>$basesCovered</br>
<b>Percent genome coverage: </b>% $percentCoverage</br>
</p>

<h2>Downloads</h2>
<p>
The data for this track can be found in the files
in this <a href="/gbdb/hubs/$asmType/$selfUrl/" target=_blank>build directory</a>, specifically:<br>
<b>bigBed file format: </b><a href="/gbdb/hubs/$asmType/$selfUrl/bbi/$accAsm.ncbiGene.ncbi.bb" target=_blank>$accAsm.ncbiGene.ncbi.bb</a><br>
<b>genePred file format: </b><a href="/gbdb/hubs/$asmType/$selfUrl/$accAsm.ncbiGene.ncbi.genePred.gz" target=_blank>$accAsm.ncbiGene.ncbi.genePred.gz</a>
</p>
<p>
Return to UCSC naming <a href="/gbdb/hubs/$asmType/$groupName/${groupName}.html">$groupName</a> assembly hub index.<br>
Return to NCBI naming <a href="/gbdb/hubs/$asmType/$groupName/${groupName}.ncbi.html">$groupName</a> assembly hub index.
</p>
_EOF_
   ;

close (FH);
