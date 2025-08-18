#!/usr/bin/env perl

use strict;
use warnings;

my $argc = scalar(@ARGV);

if ($argc != 1) {
  printf STDERR "usage: updateLiftOverChain.pl <hasChainNets.txt>\n";
  exit 255;
}

my $hasChainNets = shift;

my %validDb;	# key is db or GCx name, value is 1
my $genArkCount = 0;

my $genArkList = "https://hgdownload.soe.ucsc.edu/hubs/UCSC_GI.assemblyHubList.txt";
open (my $fh, "-|", "curl $genArkList 2> /dev/null") or die "can not curl $genArkList";
while (my $line = <$fh>) {
  next if ($line =~ m/^#/);
  my @a = split('\t', $line);
  $validDb{$a[0]} = 1;
  ++$genArkCount;
}
close ($fh);

my $rrActiveCount = 0;

open ($fh, "-|", "hgsql -h genome-centdb -N -e 'select name from dbDb where active=1' hgcentral") or die "can not select from hgcentral.liftOverChain";
while (my $db = <$fh>) {
  chomp $db;
  $validDb{$db} = 1;
  ++$rrActiveCount;
}
close ($fh);

printf STDERR "## %d GenArk hubs, %d RR active db\n", $genArkCount, $rrActiveCount;

my %liftOverFiles;	# key is "fromDb|toDb" value is path name
my $liftOverCount = 0;
my $invalidCount = 0;

open ($fh, "-|", "zcat /hive/data/inside/GenArk/liftOver/hgwdev.liftOverList.txt.gz") or die "can not zcat hgwdev.liftOverList.txt.gz";
while (my $line = <$fh>) {
  chomp $line;
  my ($md5, $filePath) = split('\t', $line);
  my @a = split('/', $filePath);
  my $loChain = $a[-1];
  $loChain =~ s/.over.chain.gz//;
  my @b = split('To', $loChain);
  if (defined($b[0]) && defined($b[1])) {
    my $fromDb = $b[0];
    my $toDb = $b[1];
    if ($toDb !~ m/^GC/) {
       $toDb = lcfirst($toDb);
    }
    if ( defined($validDb{$toDb}) && defined($validDb{$fromDb}) ) {
      my $key = sprintf("%s|%s", $fromDb, $toDb);
      printf STDERR "# %s\n", $key if ($liftOverCount < 10);
      $liftOverFiles{$key} = $filePath;
      ++$liftOverCount;
    } else {
      printf STDERR "# invalid Db: %s or %s\n", $fromDb, $toDb if ($invalidCount < 10);
      ++$invalidCount;
    }
  } else {
      printf STDERR "# no db name foundin:\n%s\n"< $filePath if ($invalidCount < 10);
      ++$invalidCount;
#   printf STDERR "warning: no db name found in liftOver file name:\n%s\n", $filePath;
  }
}
close ($fh);

open ($fh, "-|", "zgrep 'over.chain.gz' /hive/data/inside/GenArk/liftOver/gbdb.genark.liftOver.file.list") or die "can not read gbdb/genark.liftOver.file.list";
while (my $filePath = <$fh>) {
  chomp $filePath;
  my @a = split('/', $filePath);
  my $loChain = $a[-1];
  $loChain =~ s/.over.chain.gz//;
  my @b = split('To', $loChain);
  if (defined($b[0]) && defined($b[1])) {
    my $fromDb = $b[0];
    my $toDb = $b[1];
    if ($toDb !~ m/^GC/) {
       $toDb = lcfirst($toDb);
    }
    if ( defined($validDb{$toDb}) && defined($validDb{$fromDb}) ) {
      my $key = sprintf("%s|%s", $fromDb, $toDb);
      printf STDERR "# %s\n", $key if ($liftOverCount < 10);
      $liftOverFiles{$key} = $filePath;
      ++$liftOverCount;
    } else {
      printf STDERR "# invalid Db: %s or %s\n", $fromDb, $toDb if ($invalidCount < 10);
      ++$invalidCount;
    }
  } else {
      printf STDERR "# no db name foundin:\n%s\n"< $filePath if ($invalidCount < 10);
      ++$invalidCount;
#   printf STDERR "warning: no db name found in liftOver file name:\n%s\n", $filePath;
  }
}
close ($fh);

printf STDERR "# %d liftOverFiles, %d invalid files\n", $liftOverCount, $invalidCount;

my %fromToToday;	# from the liftOverChain table, key is fromDb|toDb
			# value is the file path
my $todayLiftCount = 0;
my $pathMissing = 0;
my $invalidToFrom = 0;

open ($fh, "-|", "hgsql -N -e 'select fromDb,toDb,path from liftOverChain;' hgcentraltest") or die "can not select from hgcentraltest.liftOverChain";
while (my $line = <$fh>) {
  chomp $line;
  my ($fromDb, $toDb, $path) = split('\t', $line);
  my $key = sprintf("%s|%s", $fromDb, $toDb);
  if ( -s "${path}" ) {
    if ( defined($validDb{$toDb}) && defined($validDb{$fromDb}) ) {
      $fromToToday{$key} = $path;
      ++$todayLiftCount;
    } else {
      ++$invalidToFrom;
    }
  } else {
    ++$pathMissing;
  }
}
close ($fh);

printf STDERR "# liftOverCount table: %d valid, %d path missing, %d invalidToFrom\n", $todayLiftCount, $pathMissing, $invalidToFrom;

my $toAddCount = 0;
my $missingPath = 0;

printf STDERR "# reading: '%s'\n", $hasChainNets;
my $asmId = "";
open ($fh, "<", $hasChainNets) or die "can not read $hasChainNets";
while (my $line = <$fh>) {
  chomp $line;
  if ($line =~ m/^\t/) {
     my $overChain = $line;
     $overChain =~ s/^\t//;
     $overChain =~ s/.over.chain.gz//;
     my @b = split('To', $overChain);
     if (defined($b[0]) && defined($b[1])) {
       my $fromDb = $b[0];
       my $toDb = $b[1];
       if ($toDb !~ m/^GC/) {
          $toDb = lcfirst($toDb);
       }
       if ( defined($validDb{$toDb}) && defined($validDb{$fromDb}) ) {
          my $key = sprintf("%s|%s", $fromDb, $toDb);
          if (defined($liftOverFiles{$key})) {
            printf STDERR "# to add: %s\n", $key if ($toAddCount < 10);
            printf "%s\t%s\t%s\t0.1\t0\t0\tY\t1\tN\n", $fromDb, $toDb, $liftOverFiles{$key};
            ++$toAddCount;
          } else {
            printf STDERR "# missing path: %s\n", $key if ($missingPath < 10);
            ++$missingPath;
          }
       }
     }
  } else {
     $asmId = $line;
  }
}
close ($fh);
printf STDERR "# %d entries to add to table, %d missing path file\n", $toAddCount, $missingPath;

# head hasChainNets.txt  | cat -A
# GCA_028885625.2_NHGRI_mPonPyg2-v2.0_pri^I10$
# ^IGCA_028885625.2ToGCA_028858775.2.over.chain.gz$
# ^IGCA_028885625.2ToGCA_028878055.2.over.chain.gz$
# ^IGCA_028885625.2ToGCA_028885655.2.over.chain.gz$
# ^IGCA_028885625.2ToGCA_029281585.2.over.chain.gz$
# ^IGCA_028885625.2ToGCA_029289425.2.over.chain.gz$
# ^IGCA_028885625.2ToGCF_011100555.1.over.chain.gz$
# ^IGCA_028885625.2ToGCF_020740605.2.over.chain.gz$
# ^IGCA_028885625.2ToGCF_027406575.1.over.chain.gz$
# ^IGCA_028885625.2ToHg38.over.chain.gz$

