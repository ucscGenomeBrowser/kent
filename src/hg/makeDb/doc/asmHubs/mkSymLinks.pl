#!/usr/bin/env perl

use strict;
use warnings;
use File::Basename;

my $argc = scalar(@ARGV);
if ($argc != 2) {
  printf STDERR "mkSymLinks Name asmName\n";
  printf STDERR "e.g.: mkAsmStats Mammals mammals\n";
  exit 255;
}
my $Name = shift;
my $asmHubName = shift;

my %betterName;	# key is asmId, value is common name
my $srcDocDir = "${asmHubName}AsmHub";

my $home = $ENV{'HOME'};
my $toolsDir = "$home/kent/src/hg/makeDb/doc/asmHubs";
my $commonNameList = "$asmHubName.asmId.commonName.tsv";
my $commonNameOrder = "$asmHubName.commonName.asmId.orderList.tsv";

open (FH, "<$toolsDir/${commonNameList}") or die "can not read $toolsDir/${commonNameList}";
while (my $line = <FH>) {
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
  chomp $line;
  my ($commonName, $asmId) = split('\t', $line);
  push @orderList, $asmId;
  ++$assemblyCount;
}
close (FH);

my $destDir = "/hive/data/genomes/asmHubs";

my $orderKey = 1;
foreach my $asmId (reverse(@orderList)) {
  my $accessionDir = substr($asmId, 0 ,3);
  $accessionDir .= "/" . substr($asmId, 4 ,3);
  $accessionDir .= "/" . substr($asmId, 7 ,3);
  $accessionDir .= "/" . substr($asmId, 10 ,3);
  $accessionDir .= "/" . $asmId;
  $destDir = "/hive/data/genomes/asmHubs/$accessionDir";
  my $buildDir = "/hive/data/genomes/asmHubs/refseqBuild/$accessionDir";
  if ( ! -d "${destDir}" ) {
    `mkdir -p "${destDir}"`;
  }
  printf STDERR "ln -s '${buildDir}' '${destDir}'\n";
  `rm -f "${destDir}/bbi"`;
  `rm -f "${destDir}/ixIxx"`;
  `rm -fr "${destDir}/html"`;
  `mkdir -p "${destDir}/html"`;
  `rm -f "${destDir}/${asmId}.2bit"`;
  `rm -f "${destDir}/${asmId}.agp.gz"`;
  `rm -f "${destDir}/${asmId}.chrom.sizes"`;
  `rm -f "${destDir}/${asmId}_assembly_report.txt"`;
  `rm -f "${destDir}/${asmId}.trackDb.txt"`;
  `rm -f "${destDir}/${asmId}.genomes.txt"`;
  `rm -f "${destDir}/${asmId}.hub.txt"`;
  `rm -f "${destDir}/${asmId}.groups.txt"`;
  `ln -s "${buildDir}/bbi" "${destDir}/bbi"`;
  `ln -s "${buildDir}/ixIxx" "${destDir}/ixIxx"`;
  `ln -s ${buildDir}/html/*.html "${destDir}/html/"`;
   my $jpgFiles =`ls ${buildDir}/html/*.jpg 2> /dev/null | wc -l`;
   chomp $jpgFiles;
   if ($jpgFiles > 0) {
    `rm -f ${destDir}/html/*.jpg`;
    `ln -s ${buildDir}/html/*.jpg "${destDir}/html/"`;
   }
#  `ln -s ${buildDir}/html/*.png "${destDir}/genomes/${asmId}/html/"`;
  `ln -s "${buildDir}/${asmId}.2bit" "${destDir}/"`;
  `ln -s "${buildDir}/${asmId}.agp.gz" "${destDir}/"`;
  `ln -s "${buildDir}/${asmId}.chrom.sizes" "${destDir}/"`;
  `ln -s "${buildDir}/download/${asmId}_assembly_report.txt" "${destDir}/"`;
  `ln -s "${buildDir}/${asmId}.trackDb.txt" "${destDir}/"`;
  `ln -s "${buildDir}/${asmId}.genomes.txt" "${destDir}/"`;
  `ln -s "${buildDir}/${asmId}.hub.txt" "${destDir}/"`;
  `ln -s "${buildDir}/${asmId}.groups.txt" "${destDir}/"`;
}
