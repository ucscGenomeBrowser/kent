#!/usr/bin/env perl

use strict;
use warnings;
use File::Temp qw(tempfile);

sub usage() {
  printf STDERR "usage: ./unlocalizedFasta.pl [ucsc|ncbi] <pathTo>/*_assembly_structure/Primary_Assembly <pathTo>/*.ncbi.2bit\n";
  printf STDERR "output is to stdout, redirect to a file > unlocalized.fa\n";
  printf STDERR "expecting to find unlocalized_scaffolds/unlocalized.chr2scaf file\n";
  printf STDERR "at Primary_Assembly/\n";
  printf STDERR "the ucsc|ncbi selects the naming scheme.\n";
}

my $argc = scalar(@ARGV);

if ($argc != 3) {
  usage;
  exit 255;
}

my $ncbiUcsc = shift(@ARGV);
my $primary = shift(@ARGV);
my $twoBitFile = shift(@ARGV);

my %accToChr;

open (FH, "<$primary/unlocalized_scaffolds/unlocalized.chr2scaf") or
        die "can not read $primary/unlocalized_scaffolds/unlocalized.chr2scaf";
while (my $line = <FH>) {
    next if ($line =~ m/^#/);
    chomp $line;
    my ($chrN, $acc) = split('\s+', $line);
    if ($ncbiUcsc eq "ncbi") {
      $accToChr{$acc} = $acc;
    } else {
      $accToChr{$acc} = $chrN;
    }
}
close (FH);

my ($fh, $tmpFile) = tempfile("seqList_XXXX", DIR=>'/dev/shm', SUFFIX=>'.txt', UNLINK=>1);
foreach my $acc (sort keys %accToChr) {
   printf $fh "%s\n", $acc;
}
close $fh;

open (FH, "twoBitToFa -seqList=$tmpFile $twoBitFile stdout|") or die "can not twoBitToFa $twoBitFile";
while (my $line = <FH>) {
   if ($line =~ m/^>/) {
      chomp $line;
      $line =~ s/^>//;
      $line =~ s/ .*//;
      die "ERROR: twoBitToFa $twoBitFile returns unknown acc $line" if (! exists($accToChr{$line}));
      my $chrN = $accToChr{$line};
      my $acc = $line;
      $acc =~ s/\./v/ if ($ncbiUcsc ne "ncbi");
      my $ucscName = "chr${chrN}_${acc}_random";
      $ucscName = ${acc} if ($ncbiUcsc eq "ncbi");
      printf ">%s\n", $ucscName;
   } else {
      print $line;
   }
}
close(FH);
