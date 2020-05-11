#!/usr/bin/env perl

use strict;
use warnings;
use File::Basename;

my $argc = scalar(@ARGV);
if ($argc != 2) {
  printf STDERR "mkGenomes.pl Name asmName\n";
  printf STDERR "e.g.: mkGenomes.pl Mammals mammals\n";
  exit 255;
}
my $Name = shift;
my $asmHubName = shift;

my %betterName;	# key is asmId, value is common name
my $srcDocDir = "${asmHubName}AsmHub";
my $destDir = "/hive/data/genomes/asmHubs/$asmHubName";

my $home = $ENV{'HOME'};
my $toolsDir = "$home/kent/src/hg/makeDb/doc/asmHubs";
my $commonNameList = "$asmHubName.asmId.commonName.tsv";
my $commonNameOrder = "$asmHubName.commonName.asmId.orderList.tsv";

open (FH, "<$toolsDir/${commonNameList}") or die "can not read $toolsDir/${commonNameList}";
while (my $line = <FH>) {
  next if ($line =~ m/^#/);
  chomp $line;
  my ($asmId, $name) = split('\t', $line);
  $betterName{$asmId} = $name;
}
close (FH);

my @orderList;	# asmId of the assemblies in order from the *.list files
# the order to read the different .list files:
my $assemblyCount = 0;

open (FH, "<$toolsDir/${commonNameOrder}") or die "can not read ${commonNameOrder}";
while (my $line = <FH>) {
  next if ($line =~ m/^#/);
  chomp $line;
  my ($commonName, $asmId) = split('\t', $line);
  push @orderList, $asmId;
  ++$assemblyCount;
}
close (FH);

my $buildDone = 0;
my $orderKey = 0;
foreach my $asmId (reverse(@orderList)) {
  ++$orderKey;
  my ($gcPrefix, $accession, undef) = split('_', $asmId);
  my $accessionId = sprintf("%s_%s", $gcPrefix, $accession);
  my $accessionDir = substr($asmId, 0 ,3);
  $accessionDir .= "/" . substr($asmId, 4 ,3);
  $accessionDir .= "/" . substr($asmId, 7 ,3);
  $accessionDir .= "/" . substr($asmId, 10 ,3);
  my $buildDir = "/hive/data/genomes/asmHubs/refseqBuild/$accessionDir/$asmId";
  if ($gcPrefix eq "GCA") {
     $buildDir = "/hive/data/genomes/asmHubs/genbankBuild/$accessionDir/$asmId";
  }
  my $asmReport="$buildDir/download/${asmId}_assembly_report.txt";
  my $trackDb = "$buildDir/$asmId.trackDb.txt";
  if ( ! -s "${trackDb}" ) {
    printf STDERR "# %03d not built yet: %s\n", $orderKey, $asmId;
    next;
  }
  if ( ! -s "${asmReport}" ) {
    printf STDERR "# %03d missing assembly_report: %s\n", $orderKey, $asmId;
    next;
  }
  ++$buildDone;
printf STDERR "# %03d genomes.txt %s/%s\n", $buildDone, $accessionDir, $accessionId;
  my $descr=`grep -i "organism name:" $asmReport | head -1 | sed -e 's#.*organism name: *##i; s# (.*\$##;'`;
  chomp $descr;
  my $orgName=`grep -i "organism name:" $asmReport | head -1 | sed -e 's#.* name: .* (##; s#).*##;'`;
  chomp $orgName;
  $orgName = $betterName{$asmId} if (exists($betterName{$asmId}));

  printf "genome %s\n", $accessionId;
  printf "trackDb ../%s/%s/trackDb.txt\n", $accessionDir, $accessionId;
  printf "groups groups.txt\n";
  printf "description %s\n", $orgName;
  printf "twoBitPath ../%s/%s/%s.2bit\n", $accessionDir, $accessionId, $accessionId;
  printf "organism %s\n", $descr;
  my $chrName=`head -1 $buildDir/$asmId.chrom.sizes | awk '{print \$1}'`;
  chomp $chrName;
  my $bigChrom=`head -1 $buildDir/$asmId.chrom.sizes | awk '{print \$NF}'`;
  chomp $bigChrom;
  my $oneThird = int($bigChrom/3);
  my $tenK = $oneThird + 10000;
  $tenK = $bigChrom if ($tenK > $bigChrom);
  my $defPos="${chrName}:${oneThird}-${tenK}";
  if ( -s "$asmId/defaultPos.txt" ) {
    $defPos=`cat "$asmId/defaultPos.txt"`;
    chomp $defPos;
  }
  printf "defaultPos %s\n", $defPos;
  printf "orderKey %d\n", $buildDone;
  printf "scientificName %s\n", $descr;
  printf "htmlPath ../%s/%s/html/%s.description.html\n", $accessionDir, $accessionId, $asmId;
  printf "\n";
  my $localGenomesFile = "$buildDir/${asmId}.genomes.txt";
  my $localOrderKey;
  open (GF, ">$localGenomesFile") or die "can not write to $localGenomesFile";
  printf GF "genome %s\n", $accessionId;
  printf GF "trackDb trackDb.txt\n";
  printf GF "groups groups.txt\n";
  printf GF "description %s\n", $orgName;
  printf GF "twoBitPath %s.2bit\n", $accessionId;
  printf GF "organism %s\n", $descr;
  printf GF "defaultPos %s\n", $defPos;
  printf GF "orderKey %d\n", $localOrderKey++;
  printf GF "scientificName %s\n", $descr;
  printf GF "htmlPath html/%s.description.html\n", $asmId;
  close (GF);
  my $localHubTxt = "$buildDir/${asmId}.hub.txt";
  open (HT, ">$localHubTxt") or die "can not write to $localHubTxt";
  printf HT "hub %s genome assembly\n", $accessionId;
  printf HT "shortLabel %s\n", $orgName;
  printf HT "longLabel %s/%s/%s genome assembly\n", $orgName, $descr, $asmId;
  printf HT "genomesFile genomes.txt\n";
  printf HT "email hclawson\@ucsc.edu\n";
  printf HT "descriptionUrl html/%s.description.html\n", $asmId;
  close (HT);

  my $localGroups = "$buildDir/${asmId}.groups.txt";
  open (GR, ">$localGroups") or die "can not write to $localGroups";
  print GR <<_EOF_
name user
label Custom
priority 1
defaultIsClosed 1

name map
label Mapping
priority 2
defaultIsClosed 0

name genes
label Genes
priority 3
defaultIsClosed 0

name rna
label mRNA
priority 4
defaultIsClosed 0

name regulation
label Regulation
priority 5
defaultIsClosed 0

name compGeno
label Comparative
priority 6
defaultIsClosed 0

name varRep
label Variation
priority 7
defaultIsClosed 0

name x
label Experimental
priority 10
defaultIsClosed 1
_EOF_
   ;
   close (GR);
}

__END__

description Mastacembelus armatus
twoBitPath GCA_900324485.2_fMasArm1.2/trackData/addMask/GCA_900324485.2_fMasArm1.2.masked.2bit
organism Zig-Zag eel
defaultPos LR535842.1:14552035-14572034
orderKey 1
scientificName Mastacembelus armatus
htmlPath GCA_900324485.2_fMasArm1.2/html/GCA_900324485.2_fMasArm1.2.description.html

# head -25 GCA_002180035.3_HG00514_prelim_3.0_assembly_report.txt

# Assembly name:  HG00514_prelim_3.0
# Organism name:  Homo sapiens (human)
# Isolate:  HG00514
# Sex:  female
# Taxid:          9606
# BioSample:      SAMN04229552
# BioProject:     PRJNA300843
# Submitter:      The Genome Institute at Washington University School of Medicine
# Date:           2018-05-22
# Assembly type:  haploid
# Release type:   major
# Assembly level: Chromosome
# Genome representation: full
# WGS project:    NIOH01
# Assembly method: Falcon v. November 2016
# Expected final version: no
# Genome coverage: 80.0x
# Sequencing technology: PacBio RSII
# GenBank assembly accession: GCA_002180035.3
#
## Assembly-Units:
## GenBank Unit Accession       RefSeq Unit Accession   Assembly-Unit name
## GCA_002180045.3              Primary Assembly

