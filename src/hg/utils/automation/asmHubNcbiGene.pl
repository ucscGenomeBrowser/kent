#!/usr/bin/env perl

use strict;
use warnings;
use FindBin qw($Bin);
use lib "$Bin";
use AsmHub;
use File::Basename;

my $argc = scalar(@ARGV);

if ($argc != 4) {
  printf STDERR "usage: asmHubNcbiGene.pl asmId ncbiAsmId asmId.names.tab .../trackData/\n";
  printf STDERR "where asmId is the assembly identifier,\n";
  printf STDERR "and .../trackData/ is the path to the /trackData/ directory.\n";
  printf STDERR "asmId may be equal to ncbiAsmId if it is a GenArk build\n";
  printf STDERR "or asmId might be a default dbName if it is a UCSC style\n";
  printf STDERR "browser build.\n";
  exit 255;
}

my $asmId = shift;
my $ncbiAsmId = shift;
my $namesFile = shift;
my $trackDataDir = shift;
my $ncbiGeneBbi = "$trackDataDir/ncbiGene/$asmId.ncbiGene.bb";
my $statsFile = "$trackDataDir/ncbiGene/${asmId}.ncbiGene.stats.txt";
my $chromSizes = "$trackDataDir/../${asmId}.chrom.sizes";

print AsmHub::ncbiGeneDescription($ncbiGeneBbi, $statsFile, $chromSizes, $namesFile, $ncbiAsmId);

