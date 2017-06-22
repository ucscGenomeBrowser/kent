#!/usr/bin/env perl

use strict;
use warnings;
use File::Basename;

# this needs to be fixed up with arguments

my $wrkDir = `pwd`;
chomp $wrkDir;
my $baseDir = basename($wrkDir);
die "expecting to be working in .../chromAlias/ directory" if ($baseDir !~ m/chromAlias/);
my $goodToGo = 0;
$goodToGo = 1 if (-s "ucsc.refseq.tab");
$goodToGo = 1 if (-s "ucsc.genbank.tab");
$goodToGo = 1 if (-s "ucsc.refseq.tab");
die "can not find any files ucsc.*.tab to work with\n" if (0 == $goodToGo);
my %refseqNames;      # key is refseq name, value is ucsc name
my %genbankNames;      # key is genbank name, value is ucsc name
my %ensemblNames;      # key is ensembl name, value is ucsc name
my %overallNames;	# key is external name, value is ucsc name
my %srcReference;	# key is external name, value is csv list of sources
chomp $wrkDir;
my $ucscDb = $wrkDir;
$ucscDb =~ s#/hive/data/genomes/##;
$ucscDb =~ s#/bed/.*##;
my %chromSizes;  # key is UCSC chrom name, value is size
chdir "$wrkDir";
printf STDERR "# working in $wrkDir\n";
printf STDERR "# %s\n", `pwd`;
# read 'master' list of everything in this DB
open (CS, "<../../chrom.sizes") or die "can not read ../../chrom.sizes";
while (my $cs = <CS>) {
chomp $cs;
my ($chr, $size) = split('\s', $cs);
$chromSizes{$chr} = $size;
}
close (CS);
my %refseqIds;  # key is UCSC chrom.name, value is refseq name
if (-s "ucsc.refseq.tab") {
 open (FH, "<ucsc.refseq.tab") or die "can not read ucsc.refseq.tab";
 while (my $line = <FH>) {
   chomp $line;
   my ($chr, $refseq) = split('\t+', $line);
   $refseqIds{$chr} = $refseq;
   if (exists($refseqNames{$refseq})) {
       printf STDERR "# ERROR: $ucscDb refseq dup name $refseq for $chr\n";
   } else {
       $refseqNames{$refseq} = $chr;
       $overallNames{$refseq} = $chr;
       $srcReference{$refseq} = "refseq";
   }
 }
 close (FH);
}
my %genbankIds;  # key is UCSC chrom.name, value is genbank name
if (-s "ucsc.genbank.tab") {
 open (FH, "<ucsc.genbank.tab") or die "can not read ucsc.genbank.tab";
 while (my $line = <FH>) {
   chomp $line;
   my ($chr, $genbank) = split('\t+', $line);
   $genbankIds{$chr} = $genbank;
   if (exists($genbankNames{$genbank})) {
      printf STDERR "# ERROR: $ucscDb genbank dup name $genbank for $chr\n";
   } else {
       $genbankNames{$genbank} = $chr;
       if (exists($overallNames{$genbank})) {
          if ($genbankNames{$genbank} ne $overallNames{$genbank}) {
            printf STDERR "# ERROR: $ucscDb dup genbank refseq key $genbank NE chr $genbankNames{$genbank} != $overallNames{$genbank}\n";
          } else {
            if (exists($srcReference{$genbank})) {
              $srcReference{$genbank} .= ",genbank";
            } else {
              $srcReference{$genbank} = "genbank";
            }
          }
       } else {
         $overallNames{$genbank} = $chr;
         if (exists($srcReference{$genbank})) {
           $srcReference{$genbank} .= ",genbank";
         } else {
           $srcReference{$genbank} = "genbank";
         }
       }
   }
 }
 close (FH);
}
my %ensemblIds;  # key is UCSC chrom.name, value is ensembl name
if (-s "ucsc.ensembl.tab") {
 open (FH, "<ucsc.ensembl.tab") or die "can not read ucsc.ensembl.tab";
 while (my $line = <FH>) {
   chomp $line;
   my ($chr, $ensembl) = split('\t+', $line);
   $ensemblIds{$chr} = $ensembl;
   if (exists($ensemblNames{$ensembl})) {
      printf STDERR "# ERROR: $ucscDb ensembl dup name $ensembl for $chr\n";
   } else {
       $ensemblNames{$ensembl} = $chr;
       if (exists($overallNames{$ensembl})) {
          if ($ensemblNames{$ensembl} ne $overallNames{$ensembl}) {
            printf STDERR "# ERROR: $ucscDb dup ensembl genbank refseq key $ensembl NE chr $ensemblNames{$ensembl} != $overallNames{$ensembl}\n";
          } else {
            if (exists($srcReference{$ensembl})) {
              $srcReference{$ensembl} .= ",ensembl";
            } else {
              $srcReference{$ensembl} = "ensembl";
            }
          }
       } else {
         $overallNames{$ensembl} = $chr;
         if (exists($srcReference{$ensembl})) {
           $srcReference{$ensembl} .= ",ensembl";
         } else {
           $srcReference{$ensembl} = "ensembl";
         }
       }
   }
 }
 close (FH);
}

open (FH, ">$ucscDb.chromAlias.tab") or die "can not write to $ucscDb.Alias.tab\n";

foreach my $chr (sort keys %chromSizes) {
 my $idDone = "";
 my $idDone2 = "";
 if (exists($refseqIds{$chr})) {
    printf FH "%s\t%s\t%s\n", $refseqIds{$chr}, $chr, $srcReference{$refseqIds{$chr}};
    $idDone = $refseqIds{$chr};
 }
 if (exists($genbankIds{$chr})) {
    if ($idDone ne $genbankIds{$chr}) {
      printf FH "%s\t%s\t%s\n", $genbankIds{$chr}, $chr, $srcReference{$genbankIds{$chr}};
      $idDone2 = $genbankIds{$chr};
    }
 }
 if (exists($ensemblIds{$chr})) {
    if ($ensemblIds{$chr} ne $idDone && $ensemblIds{$chr} ne $idDone2) {
      printf FH "%s\t%s\t%s\n", $ensemblIds{$chr}, $chr, $srcReference{$ensemblIds{$chr}};
    }
 }
}
close (FH);
