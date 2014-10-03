#!/usr/bin/env perl

use strict;
use warnings;

my $argc = scalar(@ARGV);
if ($argc < 1) {
  printf STDERR "usage: gbkToGp.pl <file.gbk> [moreFiles.gbk]\n";
  exit 255;
}

sub recordOut($$$$$$$$$) {
  my ($accId, $name, $txS, $txE, $cdsS, $cdsE, $exonsS, $exonsE, $eCount) = @_;
  # adjust GP name based on total length
  if ($name eq "GP") {
    my $size = int(($cdsE - $cdsS)/3);
    if ($size < 320) {
      $name = "ssGP";
    } elsif ($size < 600) {
      $name = "sGP";
    }
  }
  my @starts = split(',', $exonsS);
  my @ends = split(',', $exonsE);
  $starts[0] = $txS;
  $ends[$#ends] = $txE;
  # fixup overlapping exons
  if (2 == $eCount) {
    if ($starts[1] <= $ends[0]) {
       $ends[0] -= 3;
    }
  }
  $exonsS = join(',', @starts) . ",";
  $exonsE = join(',', @ends) . ",";
  printf "%s\t%s\t+\t%d\t%d\t%d\t%d\t%d\t%s\t%s\n", $name, $accId, $txS, $txE, $cdsS, $cdsE, $eCount, $exonsS, $exonsE;
}

# given nnnn..mmmm return nnnn mmmm
sub coordRange($) {
  my $start = -1;
  my $end = -1;
  my ($line) = @_;
  $line =~ s/^\s+//;
  my @a = split('\s+', $line);
  if (scalar(@a) != 2) {
    printf STDERR "ERROR: do not recognize range line: '%s'\n", $line;
  } else {
    ($start, $end) = split('\.\.', $a[1]);
    if (! defined($start) || ! defined($end)) {
      printf STDERR "ERROR: do not recognize range line: '%s'\n", $line;
      $start = -1;
      $end = -1;
    }
    $start -= 1;
  }
  return($start, $end);
}

# CDS lines can be simple nnnn..mmmm or join(nnnn.mmmm,jjjj.kkkk,etc...)
sub cdsRanges($) {
  my $start = -1;
  my $end = -1;
  my $exonCount = 0;
  my $starts = "";
  my $ends = "";
  my ($line) = @_;
  $line =~ s/^\s+//;
  my @a = split('\s+', $line);
  if (scalar(@a) != 2) {
    printf STDERR "ERROR: do not recognize range line: '%s'\n", $line;
  } else {
    if ($a[1] =~ m/^join/) {
      my $joinCoords = $a[1];
      $joinCoords =~ s/join\(//;
      $joinCoords =~ s/\)//;
      my @b = split(',', $joinCoords);
      $exonCount = scalar(@b);
      my @starts = ();
      my @ends = ();
      for (my $i=0; $i < $exonCount; ++$i) {
        my ($xStart, $xEnd) = coordRange("CDS $b[$i]");
        $start = $xStart if (0 == $i);
        $end = $xEnd if (($exonCount-1) == $i);
        $starts[$i] = $xStart;
        $ends[$i] = $xEnd;
      }
      $starts = join(',',@starts);
      $ends = join(',',@ends);
    } else {
      ($start, $end) = coordRange("CDS $a[1]");
      $starts = "${start},";
      $ends = "${end},";
      $exonCount = 1;
    }
  }
  return($start, $end, $exonCount, $starts, $ends);
}

while (my $file = shift) {
  my $accessionId = "";
  open (FH, "<$file") or die "can not read file $file";
  my $txStart = -1;
  my $txEnd = -1;
  my $cdsStart = -1;
  my $cdsEnd = -1;
  my $geneName = "";
  my $productName = "";
  my $exonStarts = "";
  my $exonEnds = "";
  my $exonCount = 0;
  while (my $line = <FH>) {
    chomp $line;
    if ($line =~ m#^\s+#) {
      if ($line =~ m#^\s+/#) {
        if ($line =~ m#^\s+/product=#) {
           $productName = $line;
           $productName =~ s#^\s+/product=##;
           $productName =~ s#"##g;
           $productName =~ s#nucleoprotein#NP#;
           $productName =~ s#glycoprotein#GP#;
           $productName =~ s#viral protein #VP#;
        }
        if ($line =~ m#^\s+/gene=#) {
           $geneName = $line;
           $geneName =~ s#^\s+/gene=##;
           $geneName =~ s#"##g;
        }
      } elsif ($line =~ m/^\s+gene\s+/) {
        if ($txStart != -1 && $txEnd != 1 ) {
          $geneName = $productName if (1 > length($geneName));
          recordOut($accessionId, $geneName, $txStart, $txEnd, $cdsStart, $cdsEnd, $exonStarts, $exonEnds, $exonCount);
        }
        $cdsStart = -1;
        $cdsEnd = -1;
        $geneName = "";
        $exonStarts = "";
        $exonEnds = "";
        ($txStart, $txEnd) = coordRange("$line");
      } elsif ($line =~ m/^\s+CDS\s+/) {
        if ($cdsStart != -1 && $cdsEnd != -1) {
          my $noTxEnds = 0;
          if ($txStart == -1 || $txEnd == -1) {
            $noTxEnds = 1;
          }
          $txStart = $cdsStart if ($txStart == -1);
          $txEnd = $cdsEnd if ($txEnd == -1);
          $geneName = $productName if (1 > length($geneName));
          recordOut($accessionId, $geneName, $txStart, $txEnd, $cdsStart, $cdsEnd, $exonStarts, $exonEnds, $exonCount);
          if ($noTxEnds) {
             $txStart = -1;
             $txEnd = -1;
          }
        }
        $cdsStart = -1;
        $cdsEnd = -1;
        $geneName = "";
        $exonStarts = "";
        $exonEnds = "";
        ($cdsStart, $cdsEnd, $exonCount, $exonStarts, $exonEnds) = cdsRanges("$line");
      }
    } else {
      if ($line =~ m/^VERSION/) {
        (undef, $accessionId, undef) = split('\s+', $line, 3);
        $accessionId =~ s/\./v/;
      }
    }
  }
  close (FH);
}


# VERSION     NC_002549.1  GI:10313991
