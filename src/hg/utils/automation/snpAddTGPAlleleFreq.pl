#!/usr/bin/perl -w

# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/snpAddTGPAlleleFreq.pl instead.

use Getopt::Long;
use strict;

use vars qw/
    $opt_help
    /;

sub usage {
  my ($status) = @_;
  my $base = $0;
  $base =~ s/^(.*\/)?//;
  print STDERR "
usage: $base dbSNPdb > ucscAlleleFreq.txt
    Combines allele counts from dbSNP database tables SNPAlleleFreq and
    SNPAlleleFreq_TGP which was added in human build 134.
    Assumes that the tables are sorted by snp_id (first column).
    Prints to stdout combined allele frequencies in the format used in
    our snpNNN table (where NNN >= 132).
options:
    -help         Print this message
\n";
  exit $status;
}

# Storage for lines we're not ready to process yet.
my %lineStash;

sub getOneSnp($) {
  # Read lines from pipe until we have all lines for the current id. Return combined data for id.
  my ($pipe) = @_;
  my ($id, $al, $count, $freq);
  if (exists $lineStash{$pipe}) {
    ($id, $al, $count, $freq) = @{$lineStash{$pipe}};
    delete $lineStash{$pipe};
  } else {
    $_ = <$pipe>;
    return undef if (! $_);
    chomp;  ($id, $al, $count, $freq) = split("\t");
  }
  return undef if (! $id);
  my %snp = (id => $id, als => [[$al, $count, $freq]]);
  while (<$pipe>) {
    chomp;  my ($nextId, $nextAl, $nextCount, $nextFreq) = split("\t");
    if ($nextId ne $id) {
      $lineStash{$pipe} = [$nextId, $nextAl, $nextCount, $nextFreq];
      last;
    }
    push @{$snp{als}}, [$nextAl, $nextCount, $nextFreq];
  }
  for (my $i = 0;  $i <= $#{$snp{als}};  $i++) {
    my ($al, $cnt, $freq) = @{$snp{als}->[$i]};
    if ($i < $#{$snp{als}}) {
      my $nextAl = $snp{als}->[$i+1][0];
      die "allele $al appears twice for $snp{id}" if ($al eq $nextAl);
    }
#    print STDERR "id=$snp{id}\tal=$al\tcount=$cnt\tfreq=$freq\n";
  }
  @{$snp{als}} = sort {$a->[0] cmp $b->[0]} @{$snp{als}};
  return \%snp;
} # getOneSnp

sub getNextSnp($$$) {
  # Get next snp from pipe, and make sure id's are in ascending order.
  my ($pipe, $prevSnp, $table) = @_;
  my $prevId = $prevSnp->{id};
  my $newSnp = &getOneSnp($pipe);
  return undef if (!defined $newSnp);
  my $newId = $newSnp->{id};
  if ($newId <= $prevId) {
    die "$table data are not sorted on snp_id ($newId follows $prevId)";
  }
  return $newSnp;
} # getNextSnp

sub printSnp($) {
  # Glom together per-allele counts and freqs and print one line of our ucscAlleleFreq table.
  my ($snp) = @_;
  my ($numAls, $alBlob, $cntBlob, $freqBlob) = (0, "", "", "");
  foreach my $alInfo (@{$snp->{als}}) {
    my ($al, $cnt, $freq) = @{$alInfo};
    $numAls++;
    $alBlob .= "$al,";
    $cntBlob .= "$cnt,";
    $freqBlob .= "$freq,";
  }
  print "$snp->{id}\t$numAls\t$alBlob\t$cntBlob\t$freqBlob\n";
} # printSnp

my %rc = ( 'A' => 'T',
	   'C' => 'G',
	   'G' => 'C',
	   'T' => 'A',
	   'N' => 'N',
	   '/' => '/',
	 );

sub revComp($) {
  # Reverse-complement an allele, possibly with /-delims, unless it is symbolic.
  my ($al) = @_;
  die "How do I revComp '$al'?" unless ($al =~ /^[ACGTN\/]+$/);
  my $rcAl = "";
  while ($al =~ s/(.)$//) {
    $rcAl .= $rc{$1};
  }
  return $rcAl;
} # revComp

sub revCompSnp($) {
  my ($snp) = @_;
  my %rcSnp = ( id => $snp->{id}, als => [] );
  foreach my $alInfo (@{$snp->{als}}) {
    $alInfo->[0] = &revComp($alInfo->[0]);
    unshift @{$rcSnp{als}}, $alInfo;
  }
  return \%rcSnp;
} # revCompSnp

sub combineSnps($$) {
  # Tally up per-allele counts from two sources.  Destroys inputs.
  my ($aSnp, $bSnp) = @_;
  die if ($aSnp->{id} ne $bSnp->{id});
  my %snp;
  $snp{id} = $aSnp->{id};
  $snp{als} = ();
  # Warn if the different sources give us different sets of alleles:
  my ($aStr, $bStr) = ("", "");
  foreach my $alInfo (@{$aSnp->{als}}) {
    $aStr .= "$alInfo->[0]/";
  }
  foreach my $alInfo (@{$bSnp->{als}}) {
    $bStr .= "$alInfo->[0]/";
  }
  if ($aStr ne $bStr) {
    $aStr =~ s#N/##;  $aStr =~ s#/$##;
    $bStr =~ s#N/##;  $bStr =~ s#/$##;
    if ($aStr =~ /$bStr/ || $bStr =~ /$aStr/) {
#      print STDERR "id=$snp{id}: allele subset relationship $aStr vs. $bStr\n";
    } else {
      my $bRcStr = &revComp($bStr);
      if ($bRcStr eq $aStr) {
	$bSnp = &revCompSnp($bSnp);
# 	 print STDERR "id=$snp{id}: revComp(b) matches a ($aStr); rc'ing B's alleles.\n";
      } else {
# TODO: it would be awesome to use the alleles in b (1000 Genomes) to fix strand goofs
# in a -- strand goofs would explain what I see from this:
#	print STDERR "id=$snp{id}: different allele sets $aStr vs. $bStr\n";
      }
    }
  }
  # This is where we could detect the AlleleFreqSumNot1 exception....
  # Merge alleles and counts:
  my $totalCount = 0;
  my $aInfo = shift @{$aSnp->{als}};
  my $bInfo = shift @{$bSnp->{als}};
  while ($aInfo || $bInfo) {
    if (!defined $bInfo || (defined $aInfo && ord($aInfo->[0]) < ord($bInfo->[0]))) {
      push @{$snp{als}}, $aInfo;
      $totalCount += $aInfo->[1];
      $aInfo = shift @{$aSnp->{als}};
    } elsif (!defined $aInfo || (defined $bInfo && ord($bInfo->[0]) < ord($aInfo->[0]))) {
      push @{$snp{als}}, $bInfo;
      $totalCount += $bInfo->[1];
      $bInfo = shift @{$bSnp->{als}};
    } else {
      my $al = $aInfo->[0];
      my $count = $aInfo->[1] + $bInfo->[1];
      push @{$snp{als}}, [$al, $count, -1];
      $totalCount += $count;
      $aInfo = shift @{$aSnp->{als}};
      $bInfo = shift @{$bSnp->{als}};
    }
  }
  # Fix up frequencies now that we have new totalCount:
  foreach my $info (@{$snp{als}}) {
    $info->[2] = $info->[1] / $totalCount;
  }
  return \%snp;
} # combineSnps


#########################################################################
#
# -- main --

my $ok = GetOptions("help");
&usage(1) if (!$ok);
&usage(0) if ($opt_help);
&usage(1) if (scalar(@ARGV) != 1);
my ($dbSNPdb) = @ARGV;

open(my $oldAlF,
     "hgsql $dbSNPdb -NBe 'select snp_id,allele,chr_cnt,freq " .
     "from SNPAlleleFreq, Allele " .
     "where SNPAlleleFreq.allele_id = Allele.allele_id;' |") || die "$!";
open(my $tgpAlF,
     "hgsql $dbSNPdb -NBe 'select snp_id,allele,count,freq " .
     "from SNPAlleleFreq_TGP, Allele " .
     "where SNPAlleleFreq_TGP.allele_id = Allele.allele_id;' |") || die "$!";

my $aSnp = &getOneSnp($oldAlF);
my $bSnp = &getOneSnp($tgpAlF);
while (defined $aSnp || defined $bSnp) {
  if (!defined $bSnp || (defined $aSnp && $aSnp->{id} < $bSnp->{id})) {
    &printSnp($aSnp);
    $aSnp = &getNextSnp($oldAlF, $aSnp, 'SNPAlleleFreq');
  } elsif (!defined $aSnp || (defined $bSnp && $bSnp->{id} < $aSnp->{id})) {
    &printSnp($bSnp);
    $bSnp = &getNextSnp($tgpAlF, $bSnp, 'SNPAlleleFreq_TGP');
  } else {
    # Same ID -- combine per-allele counts.
    my $cSnp = &combineSnps($aSnp, $bSnp);
    &printSnp($cSnp);
    $aSnp = &getNextSnp($oldAlF, $aSnp, 'SNPAlleleFreq');
    $bSnp = &getNextSnp($tgpAlF, $bSnp, 'SNPAlleleFreq_TGP');
  }
}
