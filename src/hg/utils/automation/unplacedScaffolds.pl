#!/usr/bin/env perl

use strict;
use warnings;

sub usage() {
  printf STDERR "usage: unplacedScaffolds.pl <db>\n";
  printf STDERR "expecting /hive/data/genomes/<db>/genbank/ existing\n";
  printf STDERR "and only genbank/Primary_Assembly/unplaced_scaffolds/FASTA/\n";
  printf STDERR "existing with no other FASTA/ directories.\n";
  printf STDERR "will construct /hive/data/genomes/<db>/ucsc/<db>.ucsc.fa.gz and\n";
  printf STDERR " /hive/data/genomes/<db>/ucsc/<db>.agp.fa\n";
  printf STDERR " /hive/data/genomes/<db>/<db>.unmasked.2bit\n";
  printf STDERR "then verifies the 2bit with the AGP file\n";
  printf STDERR "produces log output in /hive/data/genomes/<db>/ucsc/checkAgp.result.txt\n";
  exit 255;
}

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
if ( -d "/hive/data/genomes/$db/ucsc" ) {
  printf STDERR "ERROR: directory already done: /hive/data/genomes/$db/ucsc\n";
  usage;
}

`mkdir -p /hive/data/genomes/$db/ucsc`;
chdir("/hive/data/genomes/$db/ucsc");
my $here=`pwd`;
chomp $here;
if ($here ne "/hive/data/genomes/$db/ucsc") {
  die "ERROR: not successful going to directory /hive/data/genomes/$db/ucsc";
}
print STDERR "pwd: $here\n";

my @fastaDirList;

open (FH, "find /hive/data/genomes/$db/genbank -type d -name FASTA|") or
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
  printf STDERR "# one extension found: %s\n", $extensions[0];
  `zcat $agpFile | sed -e "s/$extensions[0]\t/\t/;" > ${db}.ucsc.agp`;
  printf STDERR "# written ${db}.ucsc.agp\n";
  printf STDERR "# writing ${db}.ucsc.fa.gz\n";
  `zcat $fastaFile | sed -e "s/^>gi.*gb./>/; s/$extensions[0]. .*//;" | gzip -c > ${db}.ucsc.fa.gz`;
  printf STDERR "# done ${db}.ucsc.fa.gz\n";
  die "ERROR: ../${db}.unmasked.2bit already exists ?"
    if ( -s "../${db}.unmasked.2bit" );
  `faToTwoBit ${db}.ucsc.fa.gz ../${db}.unmasked.2bit`;
  `checkAgpAndFa ${db}.ucsc.agp ../${db}.unmasked.2bit 2>&1 | tail -5 > checkAgp.result.txt`;
}

exit 0;

__END__
# verify we don't have any .acc numbers different from .1
    zcat \
    ../genbank/Primary_Assembly/unplaced_scaffolds/AGP/unplaced.scaf.agp.gz \
        | cut -f1 | egrep "^GL|^ADFV" \
        | sed -e 's/^GL[0-9][0-9]*//; s/^ADFV[0-9][0-9]*//' | sort | uniq -c
    #   33196 .1

