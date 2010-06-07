#!/usr/bin/env perl
# Dig up orthologous alleles and swizzle columns so the glommed name that
# includes human position info etc. is first.  It will be used as a key for
# joining up multiple other-species' ortho data.  Also swizzle columns so
# that the remaining columns are in order of appearance in the final result,
# snp129OrthoPanTro2RheMac2.  Upcase ortho alleles for consistency w/dbSNP.
use warnings;
use strict;

my $twoBitFName = shift @ARGV
  || die "usage: getOrthoSeq.pl orthoDb.2bit [file(s)]\n";

sub getOChrSeq($$) {
  # Slurp in fasta sequence using twoBitToFa.
  my ($twoBitFName, $oChr) = @_;
  open(P, "twoBitToFa -noMask $twoBitFName -seq=$oChr stdout |")
    || die "Can't open pipe from twoBitToFa $twoBitFName -seq=$oChr: $!\n";
  <P> =~ /^>\w+/
    || die "Doesn't look like we got fasta -- first line is this:\n$_";
  # From man perlfaq5: trick to slurp entire contents:
  my $c = 0;
  my $seq = do { local $/; my $data = <P>; $c = ($data =~ s/\n//g); $data; };
  close(P);
  return $seq;
}

my %rc = ( "a" => "t", "c" => "g", "g" => "c", "t" => "a",
           "A" => "T", "C" => "G", "G" => "C", "T" => "A", );
sub revComp($) {
  # Reverse-complement fasta input.  (Pass through non-agtc chars.)
  my ($seq) = @_;
  my $rcSeq = reverse $seq;
  for (my $i = 0;  $i < length($rcSeq);  $i++) {
    my $base = substr($rcSeq, $i, 1);
    my $cBase = $rc{$base} || $base;
    substr($rcSeq, $i, 1, $cBase);
  }
  return $rcSeq;
}

my $prevOChr;
my ($oChrSeq, $oChrSize);
while (<>) {
  chomp;
  my ($oChr, $oStart, $oEnd, $nameGlom, undef, $oStrand) = split;
  if (! defined $prevOChr || $oChr ne $prevOChr) {
    $oChrSeq = &getOChrSeq($twoBitFName, $oChr);
    $oChrSize = length($oChrSeq);
  }
  die "Coords out of range, input line $.: $oEnd > $oChr size $oChrSize\n\t"
    if ($oEnd > $oChrSize);
  my $oAllele = substr($oChrSeq, $oStart, $oEnd - $oStart);
  $oAllele = &revComp($oAllele) if ($oStrand eq "-");
  print join("\t", $nameGlom, $oChr, $oStart, $oEnd, $oAllele, $oStrand) .
        "\n";
  $prevOChr = $oChr;
}
