#!/usr/bin/env perl

use Getopt::Long;
use strict;
use warnings;
use FindBin qw($Bin);
use lib "$Bin";
use HgAutomate;

sub usage() {
  printf STDERR "usage: unplacedScaffolds.pl <db> [-mito=NC_xxx]\n";
  printf STDERR "If there is a mitochondrion sequence, use the -mito option.\n";
  printf STDERR "\nExpecting /hive/data/genomes/<db>/genbank/ existing\n";
  printf STDERR "and only genbank/Primary_Assembly/unplaced_scaffolds/FASTA/\n";
  printf STDERR "existing with no other FASTA/ directories.\n";
  printf STDERR "will construct files in: /hive/data/genomes/<db>/ucsc/ including:\n";
  printf STDERR " /hive/data/genomes/<db>/ucsc/<db>.ucsc.fa.gz\n";
  printf STDERR " /hive/data/genomes/<db>/ucsc/<db>.ucsc.agp\n";
  printf STDERR " /hive/data/genomes/<db>/<db>.unmasked.2bit\n";
  printf STDERR " /hive/data/genomes/<db>/<db>.agp\n";
  printf STDERR " /hive/data/genomes/<db>/chrom.sizes\n";
  printf STDERR "then verifies the 2bit with the AGP file\n";
  printf STDERR "produces log output in /hive/data/genomes/<db>/ucsc/checkAgp.result.txt\n";
  exit 255;
}

# Command line option vars:
use vars @HgAutomate::commonOptionVars;
# specific command line options
use vars qw/
    $opt_mito
    /;
my $ok = GetOptions(@HgAutomate::commonOptionSpec,
                   "mito=s"
		   );
usage if (!$ok);
usage if ($opt_help);

my $argc = scalar(@ARGV);

if ($argc != 1) {
   usage;
}

my $db = shift;

if ( ! -d "/hive/data/genomes/$db/genbank" ) {
  printf STDERR "ERROR: can not find directory: /hive/data/genomes/$db/genbank\n";
  usage;
}

# XXX temporary disable exit here for testing
# if ( -d "/hive/data/genomes/$db/ucsc" ) {
#   printf STDERR "ERROR: directory already done: /hive/data/genomes/$db/ucsc\n";
#   usage;
# }

my $sciName = `egrep -E "^$db\t" $ENV{'HOME'}/kent/src/hg/utils/phyloTrees/dbCommonScientificCladeOrderkey.tab | tail -1 | cut -f3`;
chomp $sciName;
die "ERROR: can not find scientific name in $ENV{'HOME'}/kent/src/hg/utils/phyloTrees/dbCommonScientificCladeOrderkey.tab" if (length($sciName) < 4);

print STDERR "# Scientific name: $sciName\n";

`mkdir -p /hive/data/genomes/$db/ucsc`;
chdir("/hive/data/genomes/$db/ucsc");
my $here=`pwd`;
chomp $here;
if ($here ne "/hive/data/genomes/$db/ucsc") {
  die "ERROR: not successful going to directory /hive/data/genomes/$db/ucsc";
}
print STDERR "# working in: $here\n";

if ($opt_mito) {
  print STDERR "# fetch mitochondrion sequence: $opt_mito";
  print STDERR `wget -O $opt_mito.fa "https://www.ncbi.nlm.nih.gov/sviewer/viewer.fcgi?db=nuccore&dopt=fasta&sendto=on&id=$opt_mito"`;
  my @a = split('\s+', $sciName);
  my $seqCount = `grep ">" $opt_mito.fa | wc -l`;
  chomp $seqCount;
  die "ERROR: do not find only one sequence in $opt_mito.fa, instead found: $seqCount" if ($seqCount != 1);
  my $mSize = `faSize $opt_mito.fa | grep bases | awk '{print \$1;}'`;
  chomp $mSize;
  my $maxMitoSize = 25000;
  die "ERROR: odd sequence length in $opt_mito.fa of: $mSize" if ($mSize < 10000 || $mSize > $maxMitoSize);
  my $fastaHead=`head -1 $opt_mito.fa`;
  chomp $fastaHead;
  $fastaHead =~ s/\|/_/g;
  my @headWords = split('\s+', $fastaHead);
  my @b = grep (/mitochondrion/, @headWords);
  die "ERROR: can not find 'mitochondrion' in the mitochondrion sequence $opt_mito.fa\n# ->'$fastaHead'" if (scalar(@b) != 1);
  undef(@b);
  my $foundNames = 0;
  for (my $i = 0; $i < scalar(@a); ++$i) {
    my @b = grep (/$a[$i]/, @headWords);
    ++$foundNames if (scalar(@b) == 1);
    undef(@b);
  }
  die "ERROR: $foundNames can not find $sciName in mitochondron sequence $opt_mito.fa\n-> '$fastaHead'" if ($foundNames < 2);
  open (FH, ">chrM.fa") or die "ERROR: can not write to chrM.fa";
  printf FH ">chrM\n";
  print FH `grep -v "^>" $opt_mito.fa`;
  close (FH);
  my $checkSize = `faSize chrM.fa | grep bases | awk '{print \$1;}'`;
  chomp ($checkSize);
  die "ERROR: incorrect size chrM.fa != $opt_mito.fa $checkSize != $mSize" if ($checkSize != $mSize);
  `gzip -f chrM.fa`;
  `gzip -f $opt_mito.fa`;
  open (FH, ">chrM.agp") or die "ERROR: can not write to chrM.agp";
  printf FH "chrM\t1\t$mSize\t1\tF\t$opt_mito\t1\t$mSize\t+\n";
  close (FH);
}

my @fastaDirList;

open (FH, "find /hive/data/genomes/$db/genbank -type d -name FASTA|grep -v non-nuclear|") or
   die "can not run find on /hive/data/genomes/$db/genbank";
while (my $dir = <FH>) {
   chomp $dir;
   push (@fastaDirList, $dir);
}
close (FH);

if (scalar(@fastaDirList) > 1) {
  printf STDERR "ERROR: more than one FASTA directory found:\n";
  for (my $i = 0; $i < scalar(@fastaDirList); ++$i) {
    printf STDERR "%s\n", $fastaDirList[$i];
  }
  usage;
}

my $fastaDir = $fastaDirList[0];
my $fastaFile = "$fastaDir/unplaced.scaf.fa.gz";
printf STDERR "# fasta file: $fastaFile\n";
if ( ! -s "$fastaFile" ) {
  die "ERROR: can not find FASTS file: $fastaFile";
}

my $agpDir=$fastaDirList[0];
$agpDir =~ s/FASTA/AGP/;
my $agpFile = "$agpDir/unplaced.scaf.agp.gz";
printf STDERR "working: %s\n", $agpFile;
if ( ! -s "$agpFile" ) {
  die "ERROR: can not find AGP file: $agpFile";
}

my @extensions;
open (FH, "zcat $agpFile | cut -f1 | grep -v '^#' | sed -e 's/[A-Z0-9]*//i'| sort -u|") or
    die "can not zcat $agpFile";
while (my $line = <FH>) {
  next if (length($line) < 1);
  chomp $line;
  push (@extensions, $line);
}
close (FH);

if (scalar(@extensions) == 1) {
  die "ERROR: extensions[0] is not == \".1\" $extensions[0]"
    if ($extensions[0] ne ".1");
}
if (scalar(@extensions) > 1) {
  printf STDERR "# more than one extension found:\n";
  for (my $i = 0; $i < scalar(@extensions); ++$i) {
    printf "%s\n", $extensions[$i];
  }
  die "TO BE DONE";
} else {
  my $regex = "\\" . $extensions[0];  # escape the . in .1
  printf STDERR "# one extension found: %s\n", $extensions[0];
  `zcat $agpFile | sed -e "s/$regex\t/\t/;" > ${db}.ucsc.agp`;
  `cat chrM.agp >> ${db}.ucsc.agp` if ($opt_mito);
  printf STDERR "# written ${db}.ucsc.agp\n";
  printf STDERR "# writing ${db}.ucsc.fa.gz\n";
  $fastaFile .= " chrM.fa.gz" if ($opt_mito);
  `zcat $fastaFile | sed -e "s/^>gi.*[bgm][bj][|]/>/; s/$regex. .*//;" | gzip -c > ${db}.ucsc.fa.gz`;
  printf STDERR "# done ${db}.ucsc.fa.gz\n";
  die "ERROR: ../${db}.unmasked.2bit already exists ?"
    if ( -s "../${db}.unmasked.2bit" );
  `faToTwoBit ${db}.ucsc.fa.gz ../${db}.unmasked.2bit`;
  my $twoBitDup=`twoBitDup ../${db}.unmasked.2bit`;
  chomp $twoBitDup;
  die "ERROR: ${db}: twoBitDup indicates duplicates:\n$twoBitDup\n"
    if (length($twoBitDup) > 0);
  `checkAgpAndFa ${db}.ucsc.agp ../${db}.unmasked.2bit 2>&1 | tail -5 > checkAgp.result.txt`;
  `twoBitInfo ../${db}.unmasked.2bit stdout | sort -k2,2nr > ../chrom.sizes`;
}
