#!/usr/bin/env perl

use strict;
use warnings;
use File::Temp qw(tempfile);

sub usage() {
  printf STDERR "usage: ./compositeFasta.pl [ucsc|ncbi] <pathTo>/*_assembly_structure/Primary_Assembly <pathTo/*.ncbi.2bit>\n";
  printf STDERR "output FASTA is to stdout, redirect to file to save > chr.fa\n";
  printf STDERR "expecting to find assembled_chromosomes/chr2acc file at Primary_Assembly/\n";
  printf STDERR "the ucsc|ncbi selects the naming scheme\n";
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
# #Chromosome     Accession.version
# 1       CM002885.1
# 2       CM002886.1
# 3       CM002887.1

open (FH, "<$primary/assembled_chromosomes/chr2acc") or
        die "can not read Primary_Assembly/assembled_chromosomes/chr2acc";
while (my $line = <FH>) {
    next if ($line =~ m/^#/);
    chomp $line;
    my ($chrN, $acc) = split('\t+', $line);
    $chrN =~ s/ /_/g;
    if ($ncbiUcsc eq "ncbi") {
       $accToChr{$acc} = $acc;
    } else {
       $accToChr{$acc} = $chrN;
    }
}
close (FH);

my ($fh, $tmpFile) = tempfile("seqList_XXXX", DIR=>'/dev/shm', SUFFIX=>'.txt', UNLINK=>1);
printf STDERR "# tmpFile: %s\n", $tmpFile;
foreach my $acc (sort keys %accToChr) {
   printf $fh "%s\n", $acc;
}
# perhaps need $fh->flush; instead, close later
close $fh;

open (FH, "twoBitToFa -seqList=$tmpFile $twoBitFile stdout|") or die "can not twoBitToFa $twoBitFile";
while (my $line = <FH>) {
   if ($line =~ m/^>/) {
      chomp $line;
      $line =~ s/^>//;
      $line =~ s/ .*//;
      die "ERROR: twoBitToFa $twoBitFile returns unknown acc $line" if (! exists($accToChr{$line}));
      if ($ncbiUcsc eq "ncbi") {
         printf ">%s\n", $accToChr{$line};
      } else {
         if ( $accToChr{$line} eq "MT" ) {
           printf ">chrM\n";
         } else {
           printf ">chr%s\n", $accToChr{$line};
         }
      }
   } else {
      print $line;
   }
}
close(FH);
