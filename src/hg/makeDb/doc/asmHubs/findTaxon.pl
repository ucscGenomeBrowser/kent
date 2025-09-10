#!/usr/bin/env perl

use strict;
use warnings;


my $argc = scalar(@ARGV);

if ($argc != 1) {
   printf STDERR "usage: findTaxon.pl Scientific_name\n";
   exit 255;
}

my $RMSK="/hive/data/outside/RepeatMasker/RepeatMasker-4.2.1/RepeatMasker";

sub testOne($) {
  my $return = 1;
  my ($s) = @_;
  my $rmSpecies = $s;
  $rmSpecies =~ s/_/ /g;
  printf STDERR "# checking '%s'\n", $rmSpecies;
  open (YN, "${RMSK} -engine rmblast -pa 1 -s -species \"${rmSpecies}\" /dev/null 2>&1 | egrep \"families in|may not be any|No results were found for that\" | tr '\\n' ' '|") or die "can not RepeatMasker\n";
  my $line = <YN>;
  chomp $line;
  if ($line =~ m/may not be any|for that name/) {
    printf STDERR "%s - is not known\n", $rmSpecies;
    $return = 0;
  } else {
    printf STDERR "%s - %s\n", ${rmSpecies}, ${line};
  }
  close (YN);
  return $return;
}

my $TOP="/hive/data/genomes/asmHubs/allBuild/rmCheck";
# my $path = "/cluster/software/bin:$ENV{'PATH'}";
# $ENV{'PATH'} = $path;
# print `env`;

my $findMe = shift;
my $thisProcess = $$;
my $tmpDir = "/dev/shm/findRm$thisProcess";
# printf STDERR "# tmpDir: %s\n", $tmpDir;
mkdir("$tmpDir");
chdir("$tmpDir");

my $yesNo = testOne($findMe);
if (1 == $yesNo) {
  chdir($TOP);
  rmdir("$tmpDir");
  printf "%s\n", $findMe;
  exit 0;
}

`rm -fr $tmpDir/RM_*`;
printf STDERR "# checking for other names %s\n", $findMe;

open (FH, "cut -f4,5 /hive/data/outside/ncbi/genomes/reports/refseq/allAssemblies.taxonomy.tsv /hive/data/outside/ncbi/genomes/reports/genbank/allAssemblies.taxonomy.tsv | tr ' ' '_' | sort -k1,1 -u|") or die "can not read /hive/data/outside/ncbi/genomes/reports/refseq/allAssemblies.taxonomy.tsv";
while (my $line = <FH>) {
  chomp $line;
  my ($sciName, $taxonomy) = split('\t', $line);
  if ($sciName eq $findMe) {
     $taxonomy =~ s/;$//;
     printf STDERR "# %s - %s\n", $sciName, $taxonomy;
     my @a = split(';', $taxonomy);
     my $termCount = scalar(@a);
     for (my $i = $termCount-1; $i > 0; --$i) {
       my $yesNo = testOne($a[$i]);
       if (1 == $yesNo) {
         chdir($TOP);
         rmdir("$tmpDir");
         printf "%s\n", $a[$i];
         last;
       } else {
         `rm -fr $tmpDir/RM_*`;
       }
     }
  }
}
close (FH);
chdir($TOP);
rmdir("$tmpDir");
