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

my $destDir = "/hive/data/genomes/asmHubs";

my $buildDone = 0;
my $orderIndex = 0;
foreach my $asmId (reverse(@orderList)) {
  ++$orderIndex;
  my ($gcPrefix, $accession, undef) = split('_', $asmId);
  my $accessionId = sprintf("%s_%s", $gcPrefix, $accession);
  my $accessionDir = substr($asmId, 0 ,3);
  $accessionDir .= "/" . substr($asmId, 4 ,3);
  $accessionDir .= "/" . substr($asmId, 7 ,3);
  $accessionDir .= "/" . substr($asmId, 10 ,3);
  $destDir = "/hive/data/genomes/asmHubs/$accessionDir/$accessionId";
  my $buildDir = "/hive/data/genomes/asmHubs/refseqBuild/$accessionDir/$asmId";
  if ($gcPrefix eq "GCA") {
     $buildDir = "/hive/data/genomes/asmHubs/genbankBuild/$accessionDir/$asmId";
  }
  my $trackDb = "$buildDir/$asmId.trackDb.txt";
  if ( ! -s "${trackDb}" ) {
    printf STDERR "# %03d not built yet: %s\n", $orderIndex, $asmId;
    next;
  }
  ++$buildDone;
  printf STDERR "# %03d symlinks %s\n", $buildDone, $accessionId;
#  printf STDERR "%s\n", $destDir;
  if ( ! -d "${destDir}" ) {
    `mkdir -p "${destDir}"`;
  }
  `rm -f "${destDir}/bbi"`;
  `rm -f "${destDir}/ixIxx"`;
  `rm -fr "${destDir}/html"`;
  `mkdir -p "${destDir}/html"`;
  `rm -f "${destDir}/${accessionId}.2bit"`;
  `rm -f "${destDir}/${accessionId}.agp.gz"`;
  `rm -f "${destDir}/${accessionId}.chrom.sizes"`;
  `rm -f "${destDir}/${accessionId}_assembly_report.txt"`;
  `rm -f "${destDir}/trackDb.txt"`;
  `rm -f "${destDir}/genomes.txt"`;
  `rm -f "${destDir}/hub.txt"`;
  `rm -f "${destDir}/groups.txt"`;
  `ln -s "${buildDir}/bbi" "${destDir}/bbi"` if (-d "${buildDir}/bbi");
  `ln -s "${buildDir}/ixIxx" "${destDir}/ixIxx"` if (-d "${buildDir}/ixIxx");
  `ln -s ${buildDir}/html/*.html "${destDir}/html/"` if (-d "${buildDir}/html");
   my $jpgFiles =`ls ${buildDir}/html/*.jpg 2> /dev/null | wc -l`;
   chomp $jpgFiles;
   if ($jpgFiles > 0) {
    `rm -f ${destDir}/html/*.jpg`;
    `ln -s ${buildDir}/html/*.jpg "${destDir}/html/"`;
   }
#  `ln -s ${buildDir}/html/*.png "${destDir}/genomes/${asmId}/html/"`;
  `ln -s "${buildDir}/${asmId}.2bit" "${destDir}/${accessionId}.2bit"` if (-s "${buildDir}/${asmId}.2bit");
  `ln -s "${buildDir}/${asmId}.agp.gz" "${destDir}/${accessionId}.agp.gz"` if (-s "${buildDir}/${asmId}.agp.gz");
  `ln -s "${buildDir}/${asmId}.chrom.sizes" "${destDir}/${accessionId}.chrom.sizes"` if (-s "${buildDir}/${asmId}.chrom.sizes");
  `ln -s "${buildDir}/download/${asmId}_assembly_report.txt" "${destDir}/${accessionId}_assembly_report.txt"` if (-s "${buildDir}/${asmId}_assembly_report.txt");
  `ln -s "${buildDir}/${asmId}.trackDb.txt" "${destDir}/trackDb.txt"` if (-s "${buildDir}/${asmId}.trackDb.txt");
  `ln -s "${buildDir}/${asmId}.genomes.txt" "${destDir}/genomes.txt"` if (-s "${buildDir}/${asmId}.genomes.txt");
  `ln -s "${buildDir}/${asmId}.hub.txt" "${destDir}/hub.txt"` if (-s "${buildDir}/${asmId}.hub.txt");
  `ln -s "${buildDir}/${asmId}.groups.txt" "${destDir}/groups.txt"` if (-s "${buildDir}/${asmId}.groups.txt");
}
