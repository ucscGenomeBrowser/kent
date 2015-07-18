#!/usr/bin/perl -w

# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/snpAddTGPAlleleFreq.pl instead.

use Getopt::Long;
use strict;

use vars qw/
    $opt_help
    $opt_contigLoc
    $opt_deDupTGP
    $opt_deDupTGP2
    /;

sub usage {
  my ($status) = @_;
  my $base = $0;
  $base =~ s/^(.*\/)?//;
  print STDERR "
usage: $base dbSNPdb > ucscAlleleFreq.txt
    Combines allele counts from dbSNP database tables SNPAlleleFreq and
    SNPAlleleFreq_TGP which was added in human build 134.
    Prints to stdout combined allele frequencies in the format used in
    our snpNNN table (where NNN >= 132).
options:
    -contigLoc=t  Use table t as a bNNN_SNPContigLoc table to determine strand
    -deDupTGP     When SNPAlleleFreq's info is identical to SNPAlleleFreq_TGP,
                  ignore SNPAlleleFreq_TGP data because it has been copied into
                  SNPAlleleFreq.  dbSNP started doing this in b138.
    -deDupTGP2    In b142, dbSNP started neglecting to strand-correct TGP
                  data copied into otherwise strand-corrected SNPAlleleFreq.
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
      if ($al eq $nextAl) {
	# 7/3/2012: Problematic merging of adjacent ss's into same rs -- skip it instead of die.
	warn "allele $al appears twice for $snp{id}; skipping snp." ;
	%snp = %{&getOneSnp($pipe)};
      }
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

sub checkStrandIds($$$$) {
  # If $id is the $snpId we're looking for, return its orientation.
  # If $id precedes $snpId, keep looking.  If $id is after $snpId, stop looking but
  # complain and reuse the line from $strandFh.
  my ($snpId, $id, $ori, $strandFh) = @_;
  my ($keepLooking, $strand);
  if ($id < $snpId) {
    $keepLooking = 1;
  }
  elsif ($id == $snpId) {
    # we found it, all done:
    $keepLooking = 0;
    $strand = ($ori == 1) ? '-' : '+';
  } elsif ($id > $snpId) {
    warn "Missing strand info for $snpId\n";
    $lineStash{$strandFh} = [$id, $ori];
    $keepLooking = 0;
    $strand = '+';
  }
  return ($keepLooking, $strand);
} # checkStrandIds

sub getStrand($;$) {
  # If strandFh is given, match up snp_id with strand query and return orientation as + or -.
  # Otherwise just assume strand is +.
  my ($snp, $strandFh) = @_;
  if ($strandFh) {
    my $snpId = $snp->{id};
    my ($id, $ori);
    if (exists $lineStash{$strandFh}) {
      ($id, $ori) = @{$lineStash{$strandFh}};
      delete $lineStash{$strandFh};
      my ($keepLooking, $strand) = &checkStrandIds($snpId, $id, $ori, $strandFh);
      return $strand unless $keepLooking;
    }
    while (my $line = <$strandFh>) {
      $line =~ /^(\d+)\t([01]$)/ || die "weird output from strand query:\n$line\t";
      ($id, $ori) = ($1, $2);
      my ($keepLooking, $strand) = &checkStrandIds($snpId, $id, $ori, $strandFh);
      return $strand unless $keepLooking;
    }
    # End of strand file.
    warn "Missing strand info for $snpId (EOF)\n";
  }
  return '+';
} # getStrand

my %rc = ( 'A' => 'T',
	   'C' => 'G',
	   'G' => 'C',
	   'T' => 'A',
	   'N' => 'N',
	   '/' => '/',
	   '-' => '-',
	 );

sub revComp($) {
  # Reverse-complement an allele, possibly with /-delims, unless it is symbolic.
  my ($al) = @_;
  die "How do I revComp '$al'?" unless ($al =~ /^[ACGTN\/-]+$/);
  my $rcAl = "";
  while ($al =~ s/(.)$//) {
    $rcAl .= $rc{$1};
  }
  return $rcAl;
} # revComp

sub printSnp($;$) {
  # Glom together per-allele counts and freqs and print one line of our ucscAlleleFreq table.
  my ($snp, $strandFh) = @_;
  my $strand = getStrand($snp, $strandFh);;
  my ($numAls, $alBlob, $cntBlob, $freqBlob) = (0, "", "", "");
  foreach my $alInfo (@{$snp->{als}}) {
    my ($al, $cnt, $freq) = @{$alInfo};
    if ($strand eq '-') {
      $al = &revComp($al);
    }
    $numAls++;
    $alBlob .= "$al,";
    $cntBlob .= "$cnt,";
    $freqBlob .= "$freq,";
  }
  print "$snp->{id}\t$numAls\t$alBlob\t$cntBlob\t$freqBlob\n";
} # printSnp

sub revCompSnp($) {
  my ($snp) = @_;
  my %rcSnp = ( id => $snp->{id}, als => [] );
  foreach my $alInfo (@{$snp->{als}}) {
    $alInfo->[0] = &revComp($alInfo->[0]);
    unshift @{$rcSnp{als}}, $alInfo;
  }
  @{$rcSnp{als}} = sort {$a->[0] cmp $b->[0]} @{$rcSnp{als}};
  return \%rcSnp;
} # revCompSnp

sub sameSnps($$) {
  # Return true if the two inputs have identical info.
  my ($aSnp, $bSnp) = @_;
  return 0 if ($aSnp->{id} != $bSnp->{id});
  my $aLen = @{$aSnp->{als}};
  my $bLen = @{$bSnp->{als}};
  return 0 if ($aLen != $bLen);
  for (my $i=0;  $i < $aLen;  $i++) {
    my ($aInfo, $bInfo) = ($aSnp->{als}[$i], $bSnp->{als}[$i]);
    return 0 if ($aInfo->[0] ne $bInfo->[0]);
    return 0 if ($aInfo->[1] != $bInfo->[1]);
    return 0 if ($aInfo->[2] != $bInfo->[2]);
  }
  return 1;
} # sameSnps

sub mergeSnps($$) {
  # Return a snp with alleles and counts combined from the two input snps.  Destroys inputs.
  my ($aSnp, $bSnp) = @_;
  my %snp;
  $snp{id} = $aSnp->{id};
  $snp{als} = ();
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
} # mergeSnps

sub combineSnps($$;$) {
  # Tally up per-allele counts from two sources.  Destroys inputs.  If snp is on - strand,
  # revComp the alleles from 1000 Genomes which always reports on + strand.
  my ($aSnp, $bSnp, $strandFh) = @_;
  die if ($aSnp->{id} ne $bSnp->{id});
  my $strand = &getStrand($bSnp, $strandFh);
  if ($strand eq '-') {
    # SNPAlleleFreq alleles are strand-corrected, but SNPAlleleFreq_TGP's are all + strand.
    # Reverse complement SNPAlleleFreq_TGP's alleles for comparison.
    if ($opt_deDupTGP2 && sameSnps($aSnp, $bSnp)) {
      # In b142 dbSNP started forgetting to strand-correct the TGP data copied into
      # SNPAlleleFreq.  Strand-correct it for them...
      $aSnp = revCompSnp($aSnp);
    }
    $bSnp = &revCompSnp($bSnp);
  }

  # Before July 2015 there was some commented-out code to warn about inconsistent
  # alleles -- those are reported as exceptions anyway.  See git blame for history.
  # We could also detect the AlleleFreqSumNot1 exception here.

  # Merge alleles and counts:
  if ($opt_deDupTGP && &sameSnps($aSnp, $bSnp)) {
    return $aSnp;
  } else {
    return &mergeSnps($aSnp, $bSnp);
  }
} # combineSnps


#########################################################################
#
# -- main --

my $ok = GetOptions("help", "contigLoc=s", "deDupTGP", "deDupTGP2");
&usage(1) if (!$ok);
&usage(0) if ($opt_help);
&usage(1) if (scalar(@ARGV) != 1);
my ($dbSNPdb) = @ARGV;

open(my $oldAlF,
     "hgsql $dbSNPdb -NBe 'select snp_id,allele,chr_cnt,freq " .
     "from SNPAlleleFreq, Allele " .
     "where SNPAlleleFreq.allele_id = Allele.allele_id " .
     "order by snp_id;' |") || die "$!";
open(my $tgpAlF,
     "hgsql $dbSNPdb -NBe 'select snp_id,allele,count,freq " .
     "from SNPAlleleFreq_TGP, Allele " .
     "where SNPAlleleFreq_TGP.allele_id = Allele.allele_id " .
     "order by snp_id;' |") || die "$!";

my $strandFh;
if ($opt_contigLoc) {
  open($strandFh,
       "hgsql $dbSNPdb -NBe 'select snp_id, orientation " .
       "from $opt_contigLoc order by snp_id' |") || die "$!";
}

my $aSnp = &getOneSnp($oldAlF);
my $bSnp = &getOneSnp($tgpAlF);
while (defined $aSnp || defined $bSnp) {
  if (!defined $bSnp || (defined $aSnp && $aSnp->{id} < $bSnp->{id})) {
    &printSnp($aSnp);
    $aSnp = &getNextSnp($oldAlF, $aSnp, 'SNPAlleleFreq');
  } elsif (!defined $aSnp || (defined $bSnp && $bSnp->{id} < $aSnp->{id})) {
    &printSnp($bSnp, $strandFh);
    $bSnp = &getNextSnp($tgpAlF, $bSnp, 'SNPAlleleFreq_TGP');
  } else {
    # Same ID -- combine per-allele counts.
    my $cSnp = &combineSnps($aSnp, $bSnp, $strandFh);
    &printSnp($cSnp);
    $aSnp = &getNextSnp($oldAlF, $aSnp, 'SNPAlleleFreq');
    $bSnp = &getNextSnp($tgpAlF, $bSnp, 'SNPAlleleFreq_TGP');
  }
}
