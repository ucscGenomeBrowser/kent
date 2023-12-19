#!/usr/bin/env perl

use strict;
use warnings;

my $argc = scalar(@ARGV);

if ($argc != 1) {
  printf STDERR "usage: availableUcscChrNames.pl asmId\n";
  printf STDERR "will examine the staging directory for the given asmId\n";
  printf STDERR "and return the string:\n";
  printf STDERR "chromAuthority ucsc\n";
  printf STDERR "if there are 'ucsc' names and they begin with 'chr'\n";
  exit 255
}

# head -1 GCA_009914755.4_T2T-CHM13v2.0.chromAlias.txt
# # genbank       refseq  assembly        ncbi    ucsc

my $asmId = shift;
my @p = split('_', $asmId);
my $accession = $p[0] . "_" . $p[1];
my $gcX = substr($asmId, 0, 3);
my $d0 = substr($asmId, 4, 3);
my $d1 = substr($asmId, 7, 3);
my $d2 = substr($asmId, 10, 3);

my $srcDir="/hive/data/outside/ncbi/genomes/$gcX/$d0/$d1/$d2/$asmId";
my $asmRpt="$srcDir/${asmId}_assembly_report.txt";
my $asmLevel = "";
if ( -s "$asmRpt" ) {
   $asmLevel=`egrep -i "assembly +level" $asmRpt | head -1 | tr -d \$'\\r'`;
   chomp $asmLevel;
   $asmLevel =~ s/.*assembly\s+level:\s+//i;
} else {
   printf STDERR "not found: '%s'\n", $asmRpt;
   printf "\n";
   exit 0;
}

if ( ! (($asmLevel =~ m/chromosome/i) || ($asmLevel =~ m/complete/i)) ) {
  printf "\n";
  exit 0;
}

my $gbRs = "genbankBuild";
$gbRs = "refseqBuild" if ($gcX =~ m/GCF/);
my $buildDir="/hive/data/genomes/asmHubs/$gbRs/$gcX/$d0/$d1/$d2/$asmId";
my $chromAlias = "$buildDir/trackData/chromAlias/$asmId.chromAlias.txt";

if (! -s "$chromAlias") {
  printf STDERR "not found: '%s'\n", $chromAlias;
  printf "\n";
  exit 0;
}

my $header = `head -1 $chromAlias | sed -e 's/^# //;'`;
chomp $header;

my @columnNames = split('\t', $header);
my $colId = 1;
my $ucscColumn = -1;
foreach my $name (@columnNames) {
#  printf "# %d\t%s\n", $colId, $name;
  if ($name eq "ucsc") {
    $ucscColumn = $colId;
    last;
  }
  ++$colId;
}

# don't need to do this if it is column 1, that is already the default
if (1 == $ucscColumn) {
  printf "\n";
  exit 0;
}
my $sampleCount = 0;
my $haveChr = 0;
my $colOneCount = 0;
  open (FH, "grep -v '^#' $chromAlias | cut -f $ucscColumn|") or die "can not read $chromAlias";
  while (my $name = <FH>) {
    chomp $name;
    ++$sampleCount;
    if (($name =~ m/^chrUn/) || ($name =~ m/^chrM/)) {
        if (1 == $ucscColumn) { ++$colOneCount; }
         next;
    }
    if ($name =~ m/^chr/) {
      if (1 == $ucscColumn) { ++$colOneCount; } else { ++$haveChr; }
    }
    last if ($haveChr > 5);
  }
  close (FH);
  if ($haveChr) {
    printf STDERR "%s\tYES\t%s\t%d\t%d\t%d\n", $asmId, $asmLevel, $sampleCount, $haveChr, $colOneCount;
    printf "chromAuthority ucsc\n";
  } else {
    printf STDERR "%s\tNOT\t%s\t%d\t%d\t%d\n", $asmId, $asmLevel, $sampleCount, $haveChr, $colOneCount;
    printf "\n";
  }
