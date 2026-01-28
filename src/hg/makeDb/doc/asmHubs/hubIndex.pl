#!/usr/bin/env perl

use strict;
use warnings;
use File::Basename;

my $thisMachine = `hostname -s`;
chomp $thisMachine;

if ($thisMachine ne "hgdownload") {
  if ($thisMachine ne "hgdownload1" && $thisMachine ne "hgdownload2" && $thisMachine ne "hgdownload3") {
     printf STDERR "# NOTE: This script is only used on hgdownload[123]\n";
     exit 255;
  }
}

#############################################################################
sub startHtml() {
printf '<!DOCTYPE HTML 4.01 Transitional>
<!--#set var="TITLE" value="GenArk: UCSC Genome Repository" -->
<!--#set var="ROOT" value=".." -->

<!--#include virtual="$ROOT/inc/gbPageStartHardcoded.html" -->

<h1>GenArk: UCSC Genome Repository</h1>

'
}

#############################################################################
sub endHtml() {
printf '
</div><!-- closing gbsPage from gbPageStartHardcoded.html -->
</div><!-- closing container-fluid from gbPageStartHardcoded.html -->
<!--#include virtual="$ROOT/inc/gbFooterHardcoded.html"-->
<script src="<!--#echo var="ROOT" -->/js/analytics.js"></script>
</body></html>
'
}

#############################################################################
startHtml;

my %expectedList = (
 "VGP" => 1,
 "CCGP" => 1,
 "HPRC" => 1,
 "BRC" => 1,
 "birds" => 1,
 "fish" => 1,
 "globalReference" => 1,
 "mammals" => 1,
 "primates" => 1,
 "vertebrate" => 1,
 "invertebrate" => 1,
 "fungi" => 1,
 "legacy" => 1,
 "plants" => 1,
 "viral" => 1,
 "archaea" => 1,
 "bacteria" => 1,
);

my %titles = (
 "VGP" => "Vertebrate Genomes Project collection",
 "CCGP" => "The California Conservation Genomics Project",
 "HPRC" => "Human Pangenome Reference Consortium",
 "BRC" => "BRC Analytics - Bioinformatics Research Center",
 "birds" => "NCBI bird genomes",
 "fish" => "NCBI fish genomes",
 "globalReference" => "Global Human Reference genomes, January 2020",
 "mammals" => "NCBI mammal genomes",
 "primates" => "NCBI primate genomes",
 "vertebrate" => "NCBI other vertebrate genomes",
 "invertebrate" => "NCBI invertebrate genomes",
 "fungi" => "NCBI fungi genomes",
 "legacy" => "NCBI genomes legacy/superseded by newer versions",
 "plants" => "NCBI plant genomes",
 "viral" => "NCBI virus genomes",
 "archaea" => "NCBI archaea genomes",
 "bacteria" => "NCBI bacteria genomes",
 "gtexAnalysis" => "Genotype-Tissue Expression (GTEx) Project analysis results track hub, V6 October 2015",
 "gtex" => "Genotype-Tissue Expression (GTEx) RNA-seq signal track hub, V6 October 2015",
 "mouseStrains" => "16 mouse strain assembly and track hub, May 2017",
 "neuroDiffCrispr" => "Structurally conserved primate cerebral cortex lincRNAs track hub, December 2018",
);

my %otherTopLevels = (
  "GCA" => 1,
  "GCF" => 1,
  "gtex" => 1,
  "gtexAnalysis" => 1,
  "mouseStrains" => 1,
  "neuroDiffCrispr" => 1,
  "UCSC_GI.assemblyHubList.txt" => 1,
  "index.html" => 1,
);

my @orderOutHubs = (
 "primates",
 "mammals",
 "birds",
 "fish",
 "vertebrate",
 "invertebrate",
 "fungi",
 "plants",
 "viral",
 "archaea",
 "bacteria",
 "VGP",
 "CCGP",
 "HPRC",
 "BRC",
 "globalReference",
 "mouseStrains",
 "legacy",
);

my @orderOutTracks = (
 "gtexAnalysis",
 "gtex",
 "neuroDiffCrispr",
);

my %indexPage = (
 "primates" => "index.html",
 "mammals" => "index.html",
 "birds" => "index.html",
 "fish" => "index.html",
 "vertebrate" => "index.html",
 "invertebrate" => "index.html",
 "fungi" => "index.html",
 "legacy" => "index.html",
 "plants" => "index.html",
 "viral" => "index.html",
 "archaea" => "index.html",
 "bacteria" => "index.html",
 "VGP" => "index.html",
 "CCGP" => "index.html",
 "HPRC" => "index.html",
 "BRC" => "index.html",
 "mouseStrains" => "hubIndex.html",
 "globalReference" => "index.html",
 "gtexAnalysis" => "index.html",
 "gtex" => "index.html",
 "neuroDiffCrispr" => "index.html",
);

# verify all known directories and files, alert for any new ones
open (FH, "ls -d /mirrordata/hubs/*|") or die "can not ls -d /mirrordata/hubs/*";
while (my $dirPath = <FH>) {
  chomp $dirPath;
  my $fileDirName = basename($dirPath);
  if (! (defined($expectedList{$fileDirName}) || defined($otherTopLevels{$fileDirName})) ) {
    printf STDERR "# something new: %s\n", $fileDirName;
  }
}

close (FH);

### Determine genome counts:
my %genomeCounts;

my $asmListJson = "/mirrordata/hubs/VGP/assemblyList.json";
my $genomeCount = `cat $asmListJson | python -mjson.tool | grep -c '"asmId":'`;
chomp $genomeCount;
$genomeCounts{"VGP"} = $genomeCount;

my @checkList = ('primates', 'mammals', 'birds', 'fish', 'vertebrate', 'legacy', 'plants', "invertebrate", "fungi", 'viral', 'archaea', 'bacteria', 'CCGP', 'HPRC', 'BRC', 'globalReference');

foreach my $hubSet (@checkList) {
  $asmListJson = "/mirrordata/hubs/$hubSet/assemblyList.json";
  if ( -s "${asmListJson}" ) {
    $genomeCount = `cat $asmListJson | python -mjson.tool | grep -c '"asmId":'`;
    chomp $genomeCount;
    $genomeCounts{$hubSet} = $genomeCount;
  } else {
    printf STDERR "# ERROR: can not find assemblyList.json:\n%s\n", $asmListJson;
  }
}

my $hubCount = 0;

printf "<table border='1'>\n";
printf "<thead><tr>\n";
printf "  <th>hub&nbsp;gateway</th>\n";
printf "  <th>description</th>\n";
printf "</tr></thead><tbody>\n";

# construct table
foreach my $orderUp (@orderOutHubs) {
  printf "<tr>\n";
  ++$hubCount;
  if ($orderUp eq "VGP") {
     printf "    <th style='text-align:center;' colspan=2>collections below are subsets of the assemblies above</th>\n";
     printf "</tr>\n";
     printf "<tr>\n";
  }
  if ($orderUp eq "fish") {
     printf "    <td><a href='%s/%s' target=_blank>fishes</a></td>\n", $orderUp, $indexPage{$orderUp};
  } else {
     printf "    <td><a href='%s/%s' target=_blank>%s</a></td>\n", $orderUp, $indexPage{$orderUp}, $orderUp;
  }
  if (defined($genomeCounts{$orderUp})) {
    printf "    <td>%s (%d assemblies)</td>\n", $titles{$orderUp}, $genomeCounts{$orderUp};
  } else {
    printf "    <td>%s</td>\n", $titles{$orderUp};
  }
  printf "</tr>\n";
}

printf "</tbody></table>\n";

my $totalAsmHubs = `grep -v "^#" /mirrordata/hubs/UCSC_GI.assemblyHubList.txt | wc -l`;
chomp $totalAsmHubs;
printf "<p>\n";
printf "Please note: text file <a href='UCSC_GI.assemblyHubList.txt' target=_blank>listing</a> of %d genome assembly hubs\n", $totalAsmHubs;
printf "</p>\n";

printf "<p>\n";
printf "Please note, the <em>invertebrate</em> category contains more than just <em>invertebrate</em> animals.  Until these clades are sorted out, with extra categories created, you will also find parasites, protozoa, and other single celled eukaryotes in the <em>invertebrate</em> grouping.\n";
printf "</p>\n";

printf "<p>\n";
printf "Please use the <a href='https://genome.ucsc.edu/assemblySearch.html' target=_blank>Assembly Search</a> page to find and request GenBank assemblies that have not yet been included in the collections here.\n";
printf "</p>\n";

printf "<p>\n";
printf "To reference these resources in publications, please credit:<br><br>\n";
printf "Clawson, H., Lee, B.T., Raney, B.J. et al.<br>\n";
printf "<b>GenArk: towards a million UCSC genome browsers</b>.<br><em>Genome Biol</em> 24, 217 (2023).\n";
printf "<a href='https://doi.org/10.1186/s13059-023-03057-x' target=_blank>\n";
printf "https://doi.org/10.1186/s13059-023-03057-x</a>\n";
printf "</p>\n";

endHtml;
