#!/usr/bin/perl -w
# Digest BGI files into bgiGeneInfo and bgiSnp tables.
# Use SQL INSERT statements for bgiGeneInfo because of longblobs;
# tab-sep file will do for bgiSnp.

use Carp;
use strict;

my $infoOut = 'bgiGeneInfo.sql';
my $snpOut = 'bgiSnp.bed';

my $strainExpr = '(Broiler|Layer|Silkie)';
my $geneSrcExpr = '(GenBank_complete|GenBank_partial|HumanDiseaseGene|IMS-GSF|UMIST\(orf lessthan 300\)|UMIST\(orf morethan 300\))';

# 2 args:  file containing list of SNPlist-chr*.txt[.gz] files, and 
# file containing list of GeneSNPcon*.txt[.gz] files.  
if (scalar(@ARGV) != 2) {
  die "Usage: $0 SNPlists.list GeneSNPcons.list\n";
}
my $SNPlists = shift @ARGV;
my $GeneSNPlists = shift @ARGV;


#######################################################################
# SUBs

# SNP position is either a single 1-based coord or 1-based start,end
sub parsePos {
  my $pos = shift;
  my ($start, $end);
  if ($pos =~ /^(\d+),(\d+)$/) {
    $start = $1 - 1;
    $end = $2;
  } elsif ($pos =~ /^\d+$/) {
    $start = $pos - 1;
    $end = $pos;
  } else {
    confess "Bad pos $pos";
  }
  return($start, $end);
}

# 3-character code for whether each strain has SNP, doesn't, or wasn't covered:
sub parseStrains {
  my $strains = shift;
  my ($inB, $inL, $inS);
  if ($strains =~ /^([B\-X])([L\-X])([S\-X])$/) {
    $inB = &parseStrain($1);
    $inL = &parseStrain($2);
    $inS = &parseStrain($3);
  } else {
    confess "Bad strains $strains";
  }
  return($inB, $inL, $inS);
}
sub parseStrain {
  my $strain = shift;
  if ($strain eq '-') {
    return('no');
  } elsif ($strain eq 'X') {
    return('n/a');
  } else {
    return('yes');
  }
}

# Since we have GO in a db, pare down to just the numeric IDs:
sub parseGo {
  my $line = shift;
  while ($line =~ s/GO:(\d+): [^;]*;/$1,/) {}
  return($line);
}


#######################################################################
# MAIN

#
# Get core info about SNPs from the files listed in $SNPlists.
#
my %snpCore = ();
open(L, "$SNPlists") || die "Can't open $SNPlists: $!\n";
while (<L>) {
  chop;  my $fname = $_;
  if ($fname =~ m@$strainExpr.*SNPlist-(chr\w+)\.txt(.*)$@) {
    my ($strain, $chr, $gz) = ($1, $2, $3);
    $chr =~ s/_[A-Z]$//;
    if ($gz ne "") {
      open(IN, "gunzip -c $fname|") || die "Can't gunzip $fname: $!";
    } else {
      open(IN, "$fname") || die "Can't read $fname: $!";
    }
    while (<IN>) {
      chop;
      my @words = split(/\t/);
      if (scalar(@words) != 15) {
	die "Wrong #words for line $. of $fname:\n$_\n";
      }
      my ($snpId, $snpType, $pos, $rPos, $qual, $rQual, $change,
	  $flankL, $flankR, $rName, $readDir, $strains, $primerL, $primerR,
	  $conf, $extra)
	= @words;
      my ($start, $end) = &parsePos($pos);
      my ($rStart, $rEnd) = &parsePos($rPos);
      my ($inB, $inL, $inS) = &parseStrains($strains);
      $readDir = ($readDir eq 'F') ? '+' : '-';
      if (defined $snpCore{$snpId}) {
	die "Duplicate def for $snpId, line $. of $fname:\n$_\n";
      }
      $snpCore{$snpId} = [$chr, $start, $end, $snpType, $rStart, $rEnd,
			  $qual, $rQual, $change, $flankL, $flankR,
			  $rName, $readDir, $inB, $inL, $inS, $primerL,
			  $primerR, $conf, $extra];
    }
    close(IN);
  } else {
    die "Can't get strain info and/or chrom from filename $fname\n";
  }
}
close(L);

#
# Get gene info and SNP-gene relationships from the files listed in 
# GeneSNPlists.  Dump out $infoOut as we go.
#
open(INFO, ">$infoOut") || die "Can't open $infoOut for writing: $!\n";
my %snpGene = ();
open(L, "$GeneSNPlists") || die "Can't open $GeneSNPlists: $!\n";
while (<L>) {
  chop;  my $fname = $_;
  if ($fname =~ m@$geneSrcExpr.*\.txt(.*)$@) {
    my ($geneSrc, $gz) = ($1, $2);
    if ($gz ne "") {
      open(IN, "gunzip -c '$fname'|") || die "Can't gunzip $fname: $!";
    } else {
      open(IN, "$fname") || die "Can't read $fname: $!";
    }
    my ($geneName, $chr, $start, $end, $strand, $pctId, $go, $ipr);
    my $snpIds = "";
    while (<IN>) {
      chop;
      next if (/^\s*$/);
      my @words = split(/\t/);
      if ($words[0] eq "GN") {
	if (defined $geneName) {
	  print INFO "INSERT INTO bgiGeneInfo VALUES ( \"$geneName\", \"$geneSrc\", \"$go\", \"$ipr\", \"$snpIds\");\n";
	  $chr = $start = $end = $strand = $pctId = $go = $ipr = undef;
	  $snpIds = "";
	}
	($geneName, $go, $ipr) = ($words[1], &parseGo($words[2]), $words[3]);
	$go = "" if (! defined $go);
	$ipr = "" if (! defined $ipr);
      } elsif ($words[0] eq "GP") {
	(undef, $chr, $start, $end, $strand, $pctId) = @words;
	$start -= 1;
      } elsif ($words[0] eq "S5") {
	my $snpId = $words[1];
	$snpGene{$snpId} = [$geneName, "5' upstream", "", ""];
	$snpIds .= "$snpId,";
      } elsif ($words[0] eq "S3") {
	my $snpId = $words[1];
	$snpGene{$snpId} = [$geneName, "3' downstream", "", ""];
	$snpIds .= "$snpId,";
      } elsif ($words[0] eq "U5") {
	my $snpId = $words[1];
	$snpGene{$snpId} = [$geneName, "5' UTR", "", ""];
	$snpIds .= "$snpId,";
      } elsif ($words[0] eq "U3") {
	my $snpId = $words[1];
	$snpGene{$snpId} = [$geneName, "3' UTR", "", ""];
	$snpIds .= "$snpId,";
      } elsif ($words[0] eq "CS") {
	my ($snpId, $ccNuc, $ccPept, $phase) = ($words[1], $words[8],
						$words[9], $words[10]);
	my $cc = "$ccNuc ($ccPept)";
	$snpGene{$snpId} = [$geneName, "Coding (Synonymous)", $cc, $phase];
	$snpIds .= "$snpId,";
      } elsif ($words[0] eq "CN") {
	my ($snpId, $ccNuc, $ccPept, $phase) = ($words[1], $words[8],
						$words[9], $words[10]);
	my $cc = "$ccNuc ($ccPept)";
	$snpGene{$snpId} = [$geneName, "Coding (Nonsynonymous)", $cc, $phase];
	$snpIds .= "$snpId,";
      } elsif ($words[0] eq "CF") {
	my $snpId = $words[1];
	$snpGene{$snpId} = [$geneName, "Coding (Frameshift)", "", ""];
	$snpIds .= "$snpId,";
      } elsif ($words[0] eq "IR") {
	my $snpId = $words[1];
	$snpGene{$snpId} = [$geneName, "Intron", "", ""];
	$snpIds .= "$snpId,";
      } elsif ($words[0] eq "SS") {
	my ($snpId, $cc) = ($words[1], $words[8]);
	$cc = "" if ($cc eq "SSIndels");
	$snpGene{$snpId} = [$geneName, "Splice Site", $cc, ""];
	$snpIds .= "$snpId,";
      } else {
	die "Unrecognized first word $words[0] line $. of $fname:\n$_\n";
      }
    }
    if (defined $geneName) {
      print INFO "INSERT INTO bgiGeneInfo VALUES ( \"$geneName\", \"$geneSrc\", \"$go\", \"$ipr\", \"$snpIds\");\n";
    }
    close(IN);
  } else {
    die "Can't get gene source info from filename $fname\n";
  }
}
close(L);
close(INFO);

#
# Combine SNP info and dump out $snpOut.
#
open(SNP, ">$snpOut") || die "Can't open $snpOut for writing: $!\n";
foreach my $snpId (keys %snpCore) {
  my ($chr, $start, $end, $snpType, $rStart, $rEnd,
      $qual, $rQual, $change, $flankL, $flankR,
      $rName, $readDir, $inB, $inL, $inS, $primerL,
      $primerR, $conf, $extra) = @{$snpCore{$snpId}};
  $extra = "" if (! defined $extra);
  my ($geneName, $geneAssoc, $cc, $phase) = ("", "", "", "");
  if (defined $snpGene{$snpId}) {
    ($geneName, $geneAssoc, $cc, $phase) = @{$snpGene{$snpId}};
  }
  print SNP "$chr\t$start\t$end\t$snpId\t$snpType\t$rStart\t$rEnd\t" .
        "$qual\t$rQual\t$change\t" .
        "$rName\t$readDir\t$inB\t$inL\t$inS\t$primerL\t" .
	"$primerR\t$conf\t$extra\t" .
	"$geneName\t$geneAssoc\t$cc\t$phase\n";
}
close(SNP);

