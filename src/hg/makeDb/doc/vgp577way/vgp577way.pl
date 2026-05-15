#!/usr/bin/env perl

use strict;
use warnings;

my $argc = scalar(@ARGV);
if ($argc != 3) {
  printf STDERR "usage: vgp577way.pl <referenceAccession> <577.acc.sciName.comName.clade.tsv> <order.list>\n";
  printf STDERR "  referenceAccession - accession of the reference species (speciesCodonDefault)\n";
  printf STDERR "  the .tsv file has four columns: accession, sciName, comName, clade\n";
  printf STDERR "  the order.list has two columns: featureBitsMeasure, accession\n";
  printf STDERR "output to stdout is a trackDb.txt composite track definition\n";
  exit 255;
}

my $refAcc = shift;
my $tsvFile = shift;
my $orderFile = shift;

# mapping from order.list names to TSV accessions for special cases
my %aliasToTsv = (
  "hs1"  => "GCA_009914755.4",
  "hg38" => "GCA_000001405.15",
  "mm39" => "GCA_000001635.9",
);

# read the TSV: accession -> (sciName, comName, clade)
my %sciName;    # key is accession (TSV column 1), value is scientific name
my %comName;    # key is accession, value is common name
my %clade;      # key is accession, value is clade string

open (my $fh, "<", $tsvFile) or die "can not read $tsvFile";
while (my $line = <$fh>) {
  chomp $line;
  my ($acc, $sci, $com, $cl) = split('\t', $line);
  $sciName{$acc} = $sci;
  $comName{$acc} = $com;
  $clade{$acc} = $cl;
}
close ($fh);

# read order.list: ordered list of accessions (these are the MAF names)
my @orderAcc;   # accessions in coverage order (highest first)

open ($fh, "<", $orderFile) or die "can not read $orderFile";
while (my $line = <$fh>) {
  chomp $line;
  next if ($line =~ m/^\s*$/);
  my ($count, $acc) = split('\s+', $line);
  push (@orderAcc, $acc);
}
close ($fh);

# helper: given a MAF name (from order.list), return the TSV accession for lookup
sub tsvKey {
  my ($mafName) = @_;
  return $aliasToTsv{$mafName} if (defined($aliasToTsv{$mafName}));
  return $mafName;
}

# verify reference accession is in the TSV
my $refTsvKey = tsvKey($refAcc);
if (!defined($clade{$refTsvKey})) {
  printf STDERR "ERROR: reference accession '%s' not found in %s\n", $refAcc, $tsvFile;
  exit 255;
}

# verify all order.list entries are in the TSV
foreach my $acc (@orderAcc) {
  my $key = tsvKey($acc);
  if (!defined($clade{$key})) {
    printf STDERR "ERROR: accession '%s' (tsv key '%s') from %s not found in %s\n",
      $acc, $key, $orderFile, $tsvFile;
    exit 255;
  }
}

# collect unique clades in order of first appearance in order.list
my @cladeOrder;
my %cladeSeen;
foreach my $acc (@orderAcc) {
  my $cl = $clade{tsvKey($acc)};
  if (!defined($cladeSeen{$cl})) {
    push (@cladeOrder, $cl);
    $cladeSeen{$cl} = 1;
  }
}
# also include the reference clade if not already present
my $refClade = $clade{$refTsvKey};
if (!defined($cladeSeen{$refClade})) {
  push (@cladeOrder, $refClade);
  $cladeSeen{$refClade} = 1;
}

# convert clade string to a trackDb-safe identifier
# e.g. "mammals-euarchontoglires" -> "Mammals_Euarchontoglires"
# e.g. "fishes-ray-finned" -> "Fishes_Ray_Finned"
sub cladeToId {
  my ($cl) = @_;
  my @parts = split(/[-]/, $cl);
  foreach my $p (@parts) {
    $p = ucfirst($p);
    # handle sub-parts joined by underscore (from split on -)
    # e.g. "ray-finned" -> "Ray_Finned"
  }
  return join("_", @parts);
}

# build sGroup lines: for each clade, list accessions in order.list order
# these use the MAF names (accessions from order.list)
my %sGroupMembers;  # key is clade string, value is arrayref of MAF names
foreach my $acc (@orderAcc) {
  my $cl = $clade{tsvKey($acc)};
  push (@{$sGroupMembers{$cl}}, $acc);
}

# the total species count
my $totalSpecies = scalar(@orderAcc) + 1;  # +1 for reference

# output the composite track
printf "track cons577way\n";
printf "compositeTrack on\n";
printf "shortLabel VGP %d-way\n", $totalSpecies;
printf "longLabel VGP %d species Cactus multiple alignment\n", $totalSpecies;
if ( -s "../../contrib/vgp577way/${refAcc}.vgp577wayPhyloP.bw" ) {
  printf "subGroup1 view Views align=Multiz_Alignments phyloP=Basewise_Conservation_(phyloP)\n";
} else {
  printf "subGroup1 view Views align=Multiz_Alignments\n";
}
printf "dragAndDrop subTracks\n";
printf "visibility hide\n";
printf "type bed 4\n";
printf "group compGeno\n";
printf "html contrib/vgp577way/vgp577way\n";
printf "\n";

# the alignment view
printf "    track cons577wayViewalign\n";
printf "    shortLabel Cactus %d-way\n", $totalSpecies;
printf "    view align\n";
printf "    visibility pack\n";
printf "    viewUi on\n";
printf "    subTrack cons577way\n";
printf "\n";

# the bigMaf track
printf "        track vgp577way\n";
printf "        subTrack cons577wayViewalign\n";
printf "        shortLabel VGP %d-way\n", $totalSpecies;
printf "        longLabel VGP Cactus alignment on %d species\n", $totalSpecies;
printf "        subGroups view=align\n";
printf "        noInherit on\n";
printf "        type bigMaf\n";
printf "        viewUi on\n";
printf "        itemFirstCharCase noChange\n";
printf "        group compGeno\n";
printf "        bigDataUrl contrib/vgp577way/%s.vgp577way.bb\n", $refAcc;
printf "        summary contrib/vgp577way/%s.vgp577waySummary.bb\n", $refAcc;
printf "        irows on\n";
printf "        color 0, 10, 100\n";
printf "        altColor 0,90,10\n";

# speciesCodonDefault
printf "        speciesCodonDefault %s\n", $refAcc;

# speciesGroups - list of clade identifiers
printf "        speciesGroups";
foreach my $cl (@cladeOrder) {
  printf " %s", cladeToId($cl);
}
printf "\n";

# sGroup_ lines - accessions in order.list order within each clade
foreach my $cl (@cladeOrder) {
  printf "        sGroup_%s", cladeToId($cl);
  foreach my $acc (@{$sGroupMembers{$cl}}) {
    printf " %s", $acc;
  }
  printf "\n";
}

# speciesLabels - map accession to common name
printf "        speciesLabels";
foreach my $acc (@orderAcc) {
  my $key = tsvKey($acc);
  printf " %s=\"%s\"", $acc, $comName{$key};
}
printf "\n";

# speciesDefaultOff - all species from order.list
printf "        speciesDefaultOff";
foreach my $acc (@orderAcc) {
  printf " %s", $acc;
}
printf "\n";

if ( -s "../../contrib/vgp577way/${refAcc}.vgp577wayPhyloP.bw" ) {

my $bwInfo = `bigWigInfo ../../contrib/vgp577way/${refAcc}.vgp577wayPhyloP.bw | egrep "min:|max:" | awk '{printf "%.2f\\n", \$NF}'| xargs echo | tr ' ' ':'`;
chomp $bwInfo;
my ($bwMin, $bwMax) = split(':', $bwInfo);
my $viewLimitMin = $bwMin / 2.0;
my $viewLimitMax = $bwMax / 2.0;
my $bigWigInfo = $bwInfo;
$bigWigInfo =~ s/:/ /;

printf "
    # PhyloP conservation
    track cons577wayViewphyloP
    shortLabel Basewise Conservation (phyloP)
    view phyloP
    visibility full
    subTrack cons577way
    viewLimits %.1f:%.1f
", $viewLimitMin, $viewLimitMax;

printf "
        track phyloP577wayREV
        subTrack cons577wayViewphyloP
        subGroups view=phyloP
        shortLabel 577 phyloP
        longLabel VGP %d species Basewise Conservation by PhyloP phyloFit
        configurable on
        noInherit on
        type bigWig %s
        bigDataUrl contrib/vgp577way/%s.vgp577wayPhyloP.bw
        maxHeightPixels 100:50:11
        viewLimits %.1f:%.1f
        autoScale off
        spanList 1
        windowingFunction mean
        color 60,60,140
        altColor 140,60,60
        priority 1
        logo on
", $totalSpecies, $bigWigInfo, $refAcc, $viewLimitMin, $viewLimitMax;

}
