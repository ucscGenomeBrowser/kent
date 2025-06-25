#!/usr/bin/env perl

use strict;
use warnings;
use File::Basename;

my $argc = scalar(@ARGV);
if ($argc != 1) {
  printf STDERR "mkSymLinks.pl [two column name list]\n";
  printf STDERR "e.g.: mkSymLinks.pl vgp.primary.assemblies.tsv\n";
  printf STDERR "the name list is found in \$HOME/kent/src/hg/makeDb/doc/asmHubs/\n";
  printf STDERR "\nthe two columns are 1: asmId (accessionId_assemblyName)\n";
  printf STDERR "column 2: common name for species, columns separated by tab\n";
  printf STDERR "the result will create symLinks from the build direcory\n";
  printf STDERR "into the appropriate /asmHubs/GC[AF]/.../ release directory\n";
  printf STDERR "hierarchy.  The output to stderr is merely a progress report.\n";
  exit 255;
}

my $home = $ENV{'HOME'};
my $toolsDir = "$home/kent/src/hg/makeDb/doc/asmHubs";

my $inputList = shift;
my $orderList = $inputList;
if ( ! -s "$orderList" ) {
  $orderList = $toolsDir/$inputList;
}

my @stageHub = qw( alpha beta public );

my %commonName;	# key is asmId, value is common name
my @orderList;	# asmId of the assemblies in order from the *.list files
# the order to read the different .list files:
my $assemblyCount = 0;

open (FH, "<${orderList}") or die "can not read ${orderList}";
while (my $line = <FH>) {
  next if ($line =~ m/^#/);
  chomp $line;
  my ($asmId, $commonName) = split('\t', $line);
  if (defined($commonName{$asmId})) {
    printf STDERR "ERROR: duplicate asmId: '%s'\n", $asmId;
    printf STDERR "previous name: '%s'\n", $commonName{$asmId};
    printf STDERR "duplicate name: '%s'\n", $commonName;
    exit 255;
  }
  $commonName{$asmId} = $commonName;
  push @orderList, $asmId;
  ++$assemblyCount;
}
close (FH);

# top level global declaration for these three, they will be specifically
#   set later.  The destDir is for hgdownload staging, the dynaDir is
#   for a set of backUp links for the dynamic blat machine,
#   the gbdbDir is for the staging of the assembly hubs into /gbdb/
#   to host the assembly hub in /gbdb/ instead of on hgdownload
my $destDir = "/hive/data/genomes/asmHubs";
my $dynaDir = "/hive/data/inside/dynaBlat/backUpLinks";
my $gbdbDir = "/gbdb/genark";

my $buildDone = 0;
my $orderIndex = 0;
foreach my $asmId (@orderList) {
  ++$orderIndex;
  if ($asmId !~ m/^GC/) {
    printf STDERR "# not an assembly hub: %s\n", $asmId;
    next;
  }
  my ($gcPrefix, $accession, undef) = split('_', $asmId);
  my $accessionId = sprintf("%s_%s", $gcPrefix, $accession);
  my $accessionDir = substr($asmId, 0 ,3);
  $accessionDir .= "/" . substr($asmId, 4 ,3);
  $accessionDir .= "/" . substr($asmId, 7 ,3);
  $accessionDir .= "/" . substr($asmId, 10 ,3);
  $destDir = "/hive/data/genomes/asmHubs/$accessionDir/$accessionId";
  $dynaDir = "/hive/data/inside/dynaBlat/backUpLinks/$accessionDir/$accessionId";
  $gbdbDir = "/gbdb/genark/$accessionDir/$accessionId";
  my $buildDir = "/hive/data/genomes/asmHubs/refseqBuild/$accessionDir/$asmId";
  if ($gcPrefix eq "GCA") {
     $buildDir = "/hive/data/genomes/asmHubs/genbankBuild/$accessionDir/$asmId";
  }
  my $trackDb = "$buildDir/$asmId.trackDb.txt";
  if ( ! -s "${trackDb}" ) {
    printf STDERR "# %03d not built yet: %s\n", $orderIndex, $asmId;
    printf STDERR "# error missing tdb: '%s'\n", $trackDb;
    next;
  }
  ++$buildDone;
  printf STDERR "# %03d symlinks %s %s\n", $buildDone, $asmId, $commonName{$asmId};
#  printf STDERR "%s\n", $buildDir;
#  printf STDERR "%s\n", $destDir;
  if ( ! -d "${destDir}" ) {
    `mkdir -p "${destDir}"`;
  }
  if ( ! -d "${dynaDir}" ) {
    `mkdir -p "${dynaDir}"`;
  }
  if ( ! -d "${gbdbDir}" ) {
    `mkdir -p "${gbdbDir}"`;
  }
  `rm -f "${destDir}/bbi"`;
  `rm -fr "${gbdbDir}/bbi"`;
  `rm -f "${destDir}/contrib"`;
  `rm -f "${gbdbDir}/contrib"`;
  `rm -f "${destDir}/genes"`;
  `rm -f "${destDir}/ixIxx"`;
  `rm -f "${destDir}/genesGtf"`;
  `rm -f "${destDir}/liftOver"`;
  `rm -f "${gbdbDir}/liftOver"`;
  `rm -f "${destDir}/otherAligners"`;
  `rm -fr "${destDir}/html"`;
  `mkdir -p "${destDir}/html"`;
  `rm -fr "${gbdbDir}/html"`;
  `rm -fr "${gbdbDir}/ixIxx"`;
  `rm -f "${destDir}/${accessionId}.2bit"`;
  `rm -f "${dynaDir}/${accessionId}.2bit"`;
  `rm -f "${gbdbDir}/${accessionId}.2bit"`;
  `rm -f "${destDir}/${accessionId}.chrNames.2bit"`;
  `rm -f "${destDir}/${accessionId}.fa.gz"`;
  `rm -f "${destDir}/${accessionId}.chrNames.fa.gz"`;
  `rm -f "${destDir}/${accessionId}.2bit.bpt"`;
  `rm -f "${gbdbDir}/${accessionId}.2bit.bpt"`;
  `rm -f "${destDir}/${accessionId}.untrans.gfidx"`;
  `rm -f "${destDir}/${accessionId}.trans.gfidx"`;
  `rm -f "${dynaDir}/${accessionId}.untrans.gfidx"`;
  `rm -f "${dynaDir}/${accessionId}.trans.gfidx"`;
  `rm -f "${destDir}/${accessionId}.agp.gz"`;
  `rm -f "${destDir}/${accessionId}.chrom.sizes"`;
  `rm -f "${destDir}/${accessionId}.chrom.sizes.txt"`;
  `rm -f "${gbdbDir}/${accessionId}.chrom.sizes.txt"`;
  `rm -f "${destDir}/${accessionId}.chromAlias.txt"`;
  `rm -f "${destDir}/${accessionId}.chromAlias.bb"`;
  `rm -f "${gbdbDir}/${accessionId}.chromAlias.bb"`;
  `rm -f "${destDir}/${accessionId}_assembly_report.txt"`;
  `rm -f "${destDir}/${accessionId}.rmsk.customLib.fa.gz"`;
  `rm -f "${destDir}/${accessionId}.repeatMasker.out.gz"`;
  `rm -f "${destDir}/${accessionId}.repeatMasker.version.txt"`;
  `rm -f "${destDir}/${accessionId}.repeatModeler.version.txt"`;
  `rm -f "${destDir}/${accessionId}.repeatModeler.families.fa.gz"`;
  `rm -f "${destDir}/${accessionId}.repeatModeler.families.stk.gz"`;
  `rm -f "${destDir}/${accessionId}.repeatModeler.out.gz"`;
  `rm -f "${destDir}/${accessionId}.repeatModeler.2bit"`;
  `rm -f "${destDir}/${accessionId}.rmod.log.txt"`;
  `rm -f "${destDir}/${accessionId}.userTrackDb.txt"`;
  `rm -f "${gbdbDir}/${accessionId}.userTrackDb.txt"`;
  `rm -f "${destDir}/trackDb.txt"`;
  `rm -f "${destDir}/genomes.txt"`;
  `rm -f "${destDir}/download.genomes.txt"`;
  `rm -f "${destDir}/hub.txt"`;
  `rm -f "${gbdbDir}/hub.txt"`;
  foreach my $hubTxt (@stageHub) {
    `rm -f "${destDir}/$hubTxt.hub.txt"`;
    `rm -f "${gbdbDir}/$hubTxt.hub.txt"`;
  }
  # it used to be standard practice to have a different hub.txt on genome-test
  #   vs. the hub.txt on hgdownload.  They evolved into being identical, but
  #   there may be a case in the future where this function may be required.
  #   Also, the sendDownload script expects to use this download.hub.txt file.
  `rm -f "${destDir}/download.hub.txt"`;
  `rm -f "${destDir}/groups.txt"`;
  `rm -f "${gbdbDir}/groups.txt"`;
   if (-d "${buildDir}/bbi") {
     `ln -s "${buildDir}/bbi" "${destDir}/bbi"`;
     `ln -s "${buildDir}/bbi" "${gbdbDir}/bbi"`
   }
   if (-d "${buildDir}/contrib") {
    `ln -s "${buildDir}/contrib" "${destDir}/contrib"`;
    `ln -s "${buildDir}/contrib" "${gbdbDir}/contrib"`;
   }
  `ln -s "${buildDir}/genes" "${destDir}/genes"` if (-d "${buildDir}/genes");
   if (-d "${buildDir}/ixIxx") {
     `ln -s "${buildDir}/ixIxx" "${destDir}/ixIxx"`;
     `ln -s "${buildDir}/ixIxx" "${gbdbDir}/ixIxx"`;
   }
  `ln -s "${buildDir}/genesGtf" "${destDir}/genesGtf"` if (-d "${buildDir}/genesGtf");
   if (-d "${buildDir}/liftOver") {
      `ln -s "${buildDir}/liftOver" "${destDir}/liftOver"`;
      `ln -s "${buildDir}/liftOver" "${gbdbDir}/liftOver"`;
   }
  `ln -s "${buildDir}/otherAligners" "${destDir}/otherAligners"` if (-d "${buildDir}/otherAligners");
   if (-d "${buildDir}/html") {
     `ln -s ${buildDir}/html/*.html "${destDir}/html/"`;
     `ln -s ${destDir}/html "${gbdbDir}/html"`;
   }
   my $jpgFiles =`ls ${buildDir}/html/*.jpg 2> /dev/null | wc -l`;
   chomp $jpgFiles;
   if ($jpgFiles > 0) {
    `rm -f ${destDir}/html/*.jpg`;
    `ln -s ${buildDir}/html/*.jpg "${destDir}/html/"`;
   }
#  `ln -s ${buildDir}/html/*.png "${destDir}/genomes/${asmId}/html/"`;
   if (-s "${buildDir}/trackData/addMask/${asmId}.masked.2bit") {
     `ln -s "${buildDir}/trackData/addMask/${asmId}.masked.2bit" "${destDir}/${accessionId}.2bit"`;
     `ln -s "${buildDir}/trackData/addMask/${asmId}.masked.2bit" "${dynaDir}/${accessionId}.2bit"`;
     `ln -s "${buildDir}/trackData/addMask/${asmId}.masked.2bit" "${gbdbDir}/${accessionId}.2bit"`;
   }
   if (-s "${buildDir}/${asmId}.fa.gz") {
      `ln -s "${buildDir}/${asmId}.fa.gz" "${destDir}/${accessionId}.fa.gz"`;
   } else {
      printf STDERR "# error missing ${asmId}.fa.gz\n";
   }
  `ln -s "${buildDir}/${asmId}.chrNames.fa.gz" "${destDir}/${accessionId}.chrNames.fa.gz"` if (-s "${buildDir}/${asmId}.chrNames.fa.gz");
   if (-s "${buildDir}/trackData/addMask/${asmId}.masked.2bit.bpt") {
      `ln -s "${buildDir}/trackData/addMask/${asmId}.masked.2bit.bpt" "${destDir}/${accessionId}.2bit.bpt"`;
      `ln -s "${buildDir}/trackData/addMask/${asmId}.masked.2bit.bpt" "${gbdbDir}/${accessionId}.2bit.bpt"`;
   } else {
      printf STDERR "# error missing ${asmId}.masked.2bit.bpt\n";
   }
  `ln -s "${buildDir}/${asmId}.chrNames.2bit" "${destDir}/${accessionId}.chrNames.2bit"` if (-s "${buildDir}/${asmId}.chrNames.2bit");
#   if (-s "${buildDir}/${accessionId}.untrans.gfidx") {
#      if (-s "${buildDir}/${accessionId}.trans.gfidx") {
#        `ln -s "${buildDir}/${accessionId}.untrans.gfidx" "${destDir}/${accessionId}.untrans.gfidx"`;
#        `ln -s "${buildDir}/${accessionId}.trans.gfidx" "${destDir}/${accessionId}.trans.gfidx"`;
#        `ln -s "${buildDir}/${accessionId}.untrans.gfidx" "${dynaDir}/${accessionId}.untrans.gfidx"`;
#        `ln -s "${buildDir}/${accessionId}.trans.gfidx" "${dynaDir}/${accessionId}.trans.gfidx"`;
#      }
#   }
  `ln -s "${buildDir}/${asmId}.agp.gz" "${destDir}/${accessionId}.agp.gz"` if (-s "${buildDir}/${asmId}.agp.gz");
   if (-s "${buildDir}/${asmId}.chrom.sizes") {
    `ln -s "${buildDir}/${asmId}.chrom.sizes" "${destDir}/${accessionId}.chrom.sizes.txt"`;
    `ln -s "${buildDir}/${asmId}.chrom.sizes" "${gbdbDir}/${accessionId}.chrom.sizes.txt"`;
   }
   if (-s "${buildDir}/${asmId}.chromAlias.txt") {
    `ln -s "${buildDir}/${asmId}.chromAlias.txt" "${destDir}/${accessionId}.chromAlias.txt"`;
   }
   if (-s "${buildDir}/${asmId}.chromAlias.bb") {
     `ln -s "${buildDir}/${asmId}.chromAlias.bb" "${destDir}/${accessionId}.chromAlias.bb"`;
     `ln -s "${buildDir}/${asmId}.chromAlias.bb" "${gbdbDir}/${accessionId}.chromAlias.bb"`;
   }
   `ln -s "${buildDir}/${asmId}.rmsk.customLib.fa.gz" "${destDir}/${accessionId}.rmsk.customLib.fa.gz"` if (-s "${buildDir}/${asmId}.rmsk.customLib.fa.gz");
  `ln -s "${buildDir}/${asmId}.repeatMasker.out.gz" "${destDir}/${accessionId}.repeatMasker.out.gz"` if (-s "${buildDir}/${asmId}.repeatMasker.out.gz");
  `ln -s "${buildDir}/${asmId}.repeatModeler.out.gz" "${destDir}/${accessionId}.repeatModeler.out.gz"` if (-s "${buildDir}/${asmId}.repeatModeler.out.gz");
  `ln -s "${buildDir}/${asmId}.repeatMasker.version.txt" "${destDir}/${accessionId}.repeatMasker.version.txt"` if (-s "${buildDir}/${asmId}.repeatMasker.version.txt");
  `ln -s "${buildDir}/${asmId}.repeatModeler.version.txt" "${destDir}/${accessionId}.repeatModeler.version.txt"` if (-s "${buildDir}/${asmId}.repeatModeler.version.txt");
  `ln -s "${buildDir}/${asmId}.repeatModeler.families.fa.gz" "${destDir}/${accessionId}.repeatModeler.families.fa.gz"` if (-s "${buildDir}/${asmId}.repeatModeler.families.fa.gz");
  `ln -s "${buildDir}/${asmId}.repeatModeler.families.stk.gz" "${destDir}/${accessionId}.repeatModeler.families.stk.gz"` if (-s "${buildDir}/${asmId}.repeatModeler.families.stk.gz");
  `ln -s "${buildDir}/${asmId}.repeatModeler.2bit" "${destDir}/${accessionId}.repeatModeler.2bit"` if (-s "${buildDir}/${asmId}.repeatModeler.2bit");
  `ln -s "${buildDir}/${asmId}.rmod.log.txt" "${destDir}/${accessionId}.rmod.log.txt"` if (-s "${buildDir}/${asmId}.rmod.log.txt");
  `ln -s "${buildDir}/download/${asmId}_assembly_report.txt" "${destDir}/${accessionId}_assembly_report.txt"` if (-s "${buildDir}/download/${asmId}_assembly_report.txt");
  # trackDb.txt still needed for use by top-level genomes.txt file
  `ln -s "${buildDir}/${asmId}.trackDb.txt" "${destDir}/trackDb.txt"` if (-s "${buildDir}/${asmId}.trackDb.txt");
  # genomes.txt obsolete now with single file
#   `ln -s "${buildDir}/${asmId}.genomes.txt" "${destDir}/genomes.txt"` if (-s "${buildDir}/${asmId}.genomes.txt");
  `ln -s "${buildDir}/${asmId}.download.genomes.txt" "${destDir}/download.genomes.txt"` if (-s "${buildDir}/${asmId}.download.genomes.txt");
   foreach my $hubTxt (@stageHub) {
     if (-s "${buildDir}/${hubTxt}.hub.txt") {
       `ln -s "${buildDir}/${hubTxt}.hub.txt" "${destDir}/${hubTxt}.hub.txt"`;
       `ln -s "${buildDir}/${hubTxt}.hub.txt" "${gbdbDir}/${hubTxt}.hub.txt"`;
     }
   }
   if (-s "${buildDir}/${asmId}.singleFile.hub.txt") {
    `ln -s "${buildDir}/${asmId}.singleFile.hub.txt" "${destDir}/hub.txt"`;
    `ln -s "${buildDir}/${asmId}.singleFile.hub.txt" "${gbdbDir}/hub.txt"`;
    `ln -s "${buildDir}/${asmId}.download.hub.txt" "${destDir}/download.hub.txt"` if (-s "${buildDir}/${asmId}.download.hub.txt");
   } else {
    `ln -s "${buildDir}/${asmId}.hub.txt" "${destDir}/hub.txt"` if (-s "${buildDir}/${asmId}.hub.txt");
   }
   if (-s "${buildDir}/${asmId}.groups.txt") {
      `ln -s "${buildDir}/${asmId}.groups.txt" "${destDir}/groups.txt"`;
      `ln -s "${buildDir}/${asmId}.groups.txt" "${gbdbDir}/groups.txt"`;
   }
   if ( -s "${buildDir}/${asmId}.userTrackDb.txt") {
    `ln -s "${buildDir}/${asmId}.userTrackDb.txt" "${destDir}/${accessionId}.userTrackDb.txt"`;
    `ln -s "${buildDir}/${asmId}.userTrackDb.txt" "${gbdbDir}/${accessionId}.userTrackDb.txt"`;
   }
   if (-s "${buildDir}/${asmId}.bigMaf.trackDb.txt") {
     `rm -f "${destDir}/${asmId}.bigMaf.trackDb.txt"`;
     `ln -s "${buildDir}/${asmId}.bigMaf.trackDb.txt" "${destDir}/${asmId}.bigMaf.trackDb.txt"`;
   }
}
