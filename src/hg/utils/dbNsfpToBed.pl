#!/usr/bin/env perl

# This is under source control -- make sure you are editing
# kent/src/hg/utils/dbNsfpToBed.pl

# The database of non-synonymous functional predictions (dbNSFP)
# contains precomputed scores from a wide variety of tools on all
# non-synon variants of all genomic positions in the CDS of Gencode
# transcripts.  Pick out some interesting subsets of its 52 columns
# and translate into bigBed and bigWig files that can be joined with
# users' variants by the Variant Annotation Integrator (#6152).

use warnings;
use strict;

# As of dbNSFP 2.0 (Feb. 2013), the files have 52 tab-separated columns
# described by a #header line with a label for each column.
# Expect to find these input header column labels, in this order;
# also define corresponding symbolic names for internal use:
my @columnNames = ( [ "chr",			"chrom" ],
		    [ "pos(1-coor)",		"hg19Start" ],
		    [ "ref",			"refAl" ],
		    [ "alt",			"altAl" ],
		    [ "aaref",			"refAa" ],
		    [ "aaalt",			"altAa" ],
		    [ "hg18_pos(1-coor)",	"hg18Start" ],
		    [ "genename",		"geneName" ],
		    [ "Uniprot_acc",		"uniProtAcc" ],
		    [ "Uniprot_id",		"uniProtId" ],
		    [ "Uniprot_aapos",		"uniProtAaPos" ],
		    [ "Interpro_domain",	"interProDomain" ],
		    [ "cds_strand",		"cdsStrand" ],
		    [ "refcodon",		"refCodon" ],
		    [ "SLR_test_statistic ",	"slrTestStat" ],
		    [ "codonpos",		"codonPos" ],
		    [ "fold-degenerate",	"foldDegen" ],
		    [ "Ancestral_allele",	"ancestralAl" ],
		    [ "Ensembl_geneid",		"ensGeneId" ],
		    [ "Ensembl_transcriptid",	"ensTxId" ],
		    [ "aapos",			"aaPos" ],
		    [ "SIFT_score",		"siftScore" ],
		    [ "Polyphen2_HDIV_score",	"pp2HDivScore" ],
		    [ "Polyphen2_HDIV_pred",	"pp2HDivPred" ],
		    [ "Polyphen2_HVAR_score",	"pp2HVarScore" ],
		    [ "Polyphen2_HVAR_pred",	"pp2HVarPred" ],
		    [ "LRT_score",		"lrtScore" ],
		    [ "LRT_pred",		"lrtPred" ],
		    [ "MutationTaster_score",	"mtScore" ],
		    [ "MutationTaster_pred",	"mtPred" ],
		    [ "MutationAssessor_score",	"maScore" ],
		    [ "MutationAssessor_pred",	"maPred" ],
		    [ "FATHMM_score",		"fathmmScore" ],
		    [ "GERP++_NR",		"gerpNr" ],
		    [ "GERP++_RS",		"gerpRs" ],
		    [ "phyloP",			"phyloP" ],
		    [ "29way_pi",		"siPhy29wayPi" ],
		    [ "29way_logOdds",		"siPhy29wayLogOdds" ],
		    [ "LRT_Omega",		"lrtOmega" ],
		    [ "UniSNP_ids",		"uniSnpIds" ],
		    [ "1000Gp1_AC",		"tgpAc" ],
		    [ "1000Gp1_AF",		"tgpAf" ],
		    [ "1000Gp1_AFR_AC",		"tgpAfrAc" ],
		    [ "1000Gp1_AFR_AF",		"tgpAfrAf" ],
		    [ "1000Gp1_EUR_AC",		"tgpEurAc" ],
		    [ "1000Gp1_EUR_AF",		"tgpEurAf" ],
		    [ "1000Gp1_AMR_AC",		"tgpAmrAc" ],
		    [ "1000Gp1_AMR_AF",		"tgpAmrAf" ],
		    [ "1000Gp1_ASN_AC",		"tgpAsnAc" ],
		    [ "1000Gp1_ASN_AF",		"tgpAsnAf" ],
		    [ "ESP6500_AA_AF",		"esp6500AaAf" ],
		    [ "ESP6500_EA_AF",		"esp6500EaAf" ],
		  );

# Define subs to use in lieu of string constants for symbolic names:
# ('use constant' doesn't work with 'use strict';
#  see http://stackoverflow.com/questions/3057027/using-constants-in-perl)
map { my $symbol = $_->[1]; eval "sub $symbol () { $symbol; }" } @columnNames;

# Finally, the interesting part!  Which columns will we tease out into independent BED files?
my %subsetColumns = ( Sift             => [ refAl(), ensTxId(), altAl(), siftScore() ],

		      # PolyPhen2 scores are not directly related to ensTxIds,
		      # but rather to UniProt seqs.
		      # We can use uniProt db to recover ensTxIds via extDb and extDbRef:
		      # select extAcc1 from extDb,extDbRef
		      #   where val = "Ensembl" and id = extDb and acc = "P51172";
		      PolyPhen2        => [ refAl(), uniProtAaPos(), altAl(),
					    pp2HDivScore(), pp2HDivPred(),
					    pp2HVarScore(), pp2HVarPred() ],

		      SeqChange        => [ refAl(), ensTxId(), ensGeneId(), cdsStrand(),
					    refCodon(), codonPos(), refAa(), aaPos(),
					    altAl(), altAa() ],

		      Lrt              => [ refAl(), ensTxId(), altAl(),
					    lrtScore(), lrtPred(), lrtOmega() ],

		      MutationTaster   => [ refAl(), ensTxId(), altAl(), mtScore(), mtPred() ],

		      MutationAssessor => [ refAl(), ensTxId(), altAl(), maScore(), maPred() ],

		      # These subsets are related to position or transcript, not alleles:
		      GerpNr           => [ gerpNr() ],

		      GerpRs           => [ gerpRs() ],

		      InterPro         => [ interProDomain() ],

		      # Note: these were taken from PolyPhen2! Not directly related to ensTxIds.
		      UniProt          => [ uniProtAcc(), uniProtId() ],

		      # These two sound great but are extremely sparse as of v2.0...
		      # UniSnp           => [ uniSnpIds() ],
		      #
		      # Tgp              => [ tgpAf(), tgpAfrAf(), tgpEurAf(), tgpAmrAf(), tgpAsnAf() ],
		    );



sub checkHeader($) {
# In order to catch any future column swizzles, require that the input header
# columns exactly match our expectation.
  my ($headerLine) = @_;
  my @expectedHeaderColumns = map { my $label = $_->[0]; } @columnNames;
  $headerLine =~ s/^#// || die "Expected header line to begin with #";
  chomp $headerLine;  $headerLine =~ s/\r$//; # why doesn't chomp do this?
  my @headerColumns = split("\t", $headerLine);
  my $columnCount = @headerColumns;
  my $expectedCount = @expectedHeaderColumns;
  if ($columnCount != $expectedCount) {
    die "Different number of columns! Expected $expectedCount columns, "
      . "got $columnCount";
  };
  for (my $i = 0;  $i < $columnCount;  $i++) {
    if ($headerColumns[$i] ne $expectedHeaderColumns[$i]) {
      die "Header column labels don't match expectations -- you must edit this script "
	. "to accomodate input format changes, starting with column[$i] = "
	  . "'$headerColumns[$i]' (expected '$expectedHeaderColumns[$i]')";
    }
  }
}

sub createOutputFile($) {
# Return a file handle to a dbNsfp{Subset}.bed file opened for writing
  my ($base) = (@_);
  my $file = "dbNsfp" . $base . ".bed";
  my $pipe = "| uniq >$file";
  if ($subsetColumns{$base}->[0] ne refAl()) {
    # Subsets that are not related to specific {ref, alt} often span many bases,
    # and dbNSFP skips 4-fold degenerate bases (wobble bases where any nucleotide
    # codes for the same amino acid), so merge ranges within 1bp of each other
    # if all columns after bed3 are the same for each range:
    $pipe = "| mergeSortedBed3Plus.pl -maxGap=1 - >$file";
  }
  open(my $handle, $pipe) || die "Can't open pipe '$pipe' for writing: $!\n";
  $handle;
}

# Make a hash mapping symbolic names to column indices:
my $n = 0;
my %symbolIx = map { my $symbol = $_->[1]; $symbol => $n++; } @columnNames;

sub getIxList($) {
  my ($subset) = @_;
  my @cols = @{$subsetColumns{$subset}};
  map { if (!exists $symbolIx{$_}) { die "Unrecognized column name '$_'"; } } @cols;
  my @ixList = map { $symbolIx{$_} } @cols;
  return \@ixList;
}

my ($refAlIx, $altAlIx) = ($symbolIx{refAl()}, $symbolIx{altAl()});

sub skipToAlt($) {
# When a subset list starts with refAl and altAl, which are never placeholders,
# we want to look for non-placeholder values *after* those.
  my ($ixList) = @_;
  if ($ixList->[0] != $refAlIx) {
    return $ixList;
  } else {
    my $i = 1;
    for (; $i < @$ixList;  $i++) {
      last if ($ixList->[$i] == $altAlIx);
    }
    my @ixListFromAlt = ();
    for (; $i < @$ixList;  $i++) {
      push @ixListFromAlt, $ixList->[$i];
    }
    return \@ixListFromAlt;
  }
}

sub getVals($$) {
# Given a reference to array of words, and ref to list of indices,
# return a list of corresponding elements of $words.
  my ($words, $ixList) = @_;
  my @vals = map { $words->[$_] } @$ixList;
  return @vals;
}

sub haveNonPlaceholder($$) {
  # Look for anything starting at $vals->[$iStart] that is not the placeholder ".".
  my ($iStart, $vals) = @_;
  my $i = 0;
  return grep { $i++ >= $iStart && $_ ne "." } @$vals;
}

sub useCommasForLists($) {
  # dbNsfp includes some text values that contain commas, so it uses ';' as its
  # list separator.  Html-encode commas, then replace ; with , to separate lists.
  my ($listRef) = @_;
  for (my $i = 0;  $i < @$listRef;  $i++) {
    $listRef->[$i] =~ s/,/%2C/g;
    $listRef->[$i] =~ s/;/,/g;
  }
}

my ($ensTxIdIx, $aaPosIx) = ($symbolIx{ensTxId()}, $symbolIx{aaPos()});

sub collateEnsTx($$$) {
  # dbNSFP rows are *almost* unique by {chr, start, altAl} -- but actually
  # it's by {chr, start, altAl, ensTxId}, and the ensTxIds appear on
  # alternating lines.  So, collate rows by ensTxId before collapsing them.
  my ($chr, $start, $rows) = @_;
  my %ensTxRows = ();
  foreach my $r (@$rows) {
    my $ensTxId = $r->[$ensTxIdIx];
    push @{$ensTxRows{$ensTxId}}, $r;
  }
  foreach my $ensTxId (sort keys %ensTxRows) {
    my @rowsForTx = @{$ensTxRows{$ensTxId}};
    my $rowCount = @rowsForTx;
    if ($rowCount > 3) {
      # There are some duplicate rows which have identical content from
      # 'cut -f 1-4,7-13,15,18-20,23-', so differ in 'cut -f 5,6,14,16,17,21,22'
      # (aaRef, aaAlt, refCodon, codonPos, foldDegen, aaPos, siftScore).
      # These seem to be explained by less informative rows with "-1" for aaPos
      # that precede rows that have nonnegative aaPos and other coding info.
      print "FYI: >3 rows ($rowCount) for {$chr, $start, $ensTxId}; "
	. "removing less-informative duplicates.\n";
      my @newRows = ();
      for (my $i = 0; $i < $rowCount;  $i++) {
	if ($rowsForTx[$i]->[$aaPosIx] =~ /^-1(;-1)*$/ && $i < $rowCount-1 &&
	    $rowsForTx[$i+1]->[$aaPosIx] !~ /^-1(;-1)*$/) {
	  next;
	}
	push @newRows, $rowsForTx[$i];
      }
      $rowCount = @newRows;
      if ($rowCount > 3) {
	die "Failed to resolve rows to 3 {$chr, $start, altAl, $ensTxId}.";
      }
      $ensTxRows{$ensTxId} = \@newRows;
    }
  }
  return %ensTxRows;
}


# Parallel arrays of our subset names, associated file handles and column indices
my @subsets = (keys %subsetColumns);
my $subsetCount = @subsets;
my @handles = map { createOutputFile($_) } @subsets;
my @ixLists = map { getIxList($_) } @subsets;
my @ixListsFromAlt = map { skipToAlt($_) } @ixLists;
my @hasAaPos = map { 0 + grep { $_ == $aaPosIx } @{$_} } @ixLists;
my @iAfterAlt = ();
for (my $i = 0;  $i < $subsetCount;  $i++) {
  $iAfterAlt[$i] = 1 + @{$ixLists[$i]} - @{$ixListsFromAlt[$i]};
}

sub printIfNonEmpty($$$) {
# If any of the special columns (after refAl and altAl) is not placeholder ".", print one line.
  my ($chr, $start, $rows) = @_;
  my @bed3Vals = ($chr, $start, $start+1);
  my %ensTxRows = collateEnsTx($chr, $start, $rows);
  my $ensTxIdCount = scalar(keys %ensTxRows);
  foreach my $ensTxId (sort keys %ensTxRows) {
    my @rowsForTx = @{$ensTxRows{$ensTxId}};
    # Most locations have only one Ensembl transcript set; to cut file size,
    # abbreviate ensTxId to '.' when there's only one.
    if ($ensTxIdCount == 1) {
      $rowsForTx[0]->[$ensTxIdIx] = '.';
    }
    # Select columns and print output for each subset:
    for (my $subset = 0;  $subset < $subsetCount;  $subset++) {
      my $fOut = $handles[$subset];
      my $ixList = $ixLists[$subset];
      if ($ixList->[0] ne $refAlIx) {
	# If this subset is unrelated to refAl and altAl, just print out subset columns
	# (if nonempty) as independent lines and let mergeSortedBed3Plus.pl compress them:
	foreach my $r (@rowsForTx) {
	  my @specialVals = getVals($r, $ixList);
	  if (haveNonPlaceholder(0, \@specialVals)) {
	    useCommasForLists(\@specialVals);
	    print $fOut join("\t", @bed3Vals, @specialVals) . "\n";
	  }
	}
      } else {
	# Since we generally have different content per alt allele, print one line with
	# combined content from all rows' alt alleles and their associated values.
	# Print all subset columns starting with refAl (ixList) for first row only.
	my @specialVals = getVals($rowsForTx[0], $ixList);
	# refAl is *almost* always uppercase: force the few stragglers to uppercase:
	$specialVals[0] = uc $specialVals[0];
	# Watch out for missing ref-associated vals in the first row that are defined in
	# subsequent rows -- find and replace:
	if (defined $rowsForTx[1] && $hasAaPos[$subset]) {
	  my $nextRow = $rowsForTx[1];
	  my @extraSpecialVals = getVals($nextRow, $ixList);
	  for (my $i = 0;  $i < @$nextRow;  $i++) {
	    last if ($ixList->[$i] == $altAlIx);
	    if (($specialVals[$i] eq '.' || $specialVals[$i] eq '-1') &&
		$extraSpecialVals[$i] ne '.' && $extraSpecialVals[$i] ne '-1') {
	      $specialVals[$i] = $extraSpecialVals[$i];
	    }
	  }
	}
	my $nonEmpty = 0;
	my $ixListFromAlt = $ixListsFromAlt[$subset];
	# Skip everything up through altAl when looking for non-placeholders:
	$nonEmpty = 1 if (haveNonPlaceholder($iAfterAlt[$subset], \@specialVals));
	# Print only altAl and beyond for subsequent rows. If there are fewer than 3
	# altAl's / rows, fill in with placeholders to get correct column count.
	for (my $i = 1;  $i < 3;  $i++) {
	  if (defined $rowsForTx[$i]) {
	    my @subsetVals = getVals($rowsForTx[$i], $ixListFromAlt);
	    push @specialVals, @subsetVals;
	    $nonEmpty = 1 if (haveNonPlaceholder(1, \@subsetVals));
	  } else {
	    push @specialVals, map { "." } @$ixListFromAlt;
	  }
	}
	if ($nonEmpty){
	  useCommasForLists(\@specialVals);
	  print $fOut join("\t", @bed3Vals, @specialVals) . "\n";
	}
      }
    }
  }
}

##################################  MAIN  #################################

my $expectedCols = @columnNames;

my ($chromIx, $hg19StartIx) = ($symbolIx{chrom()}, $symbolIx{hg19Start()});
my ($prevChr, $prevStart);
my @prevRows = ();

while (<>) {
  if (/^#/) {
    checkHeader($_);
    next;
  }
  chomp; s/\r$//;
  my @w = split("\t");
  if (@w != $expectedCols) {
    die "Wrong number of columns (expected $expectedCols, got " . scalar(@w);
  }

  # Positions are all given as {chr, start}; end is always start+1.
  my ($chr, $start) = getVals(\@w, [ $chromIx, $hg19StartIx ]);
  $chr = "chr" . $chr unless ($chr =~ /^chr/);
  $start--;

  if (defined $prevChr && ($chr ne $prevChr || $start != $prevStart)) {
    if ($chr eq $prevChr && $start < $prevStart) {
      die "Input is not sorted: $chr, $start < $prevStart";
    }
    printIfNonEmpty($prevChr, $prevStart, \@prevRows);
    @prevRows = ();
  }
  ($prevChr, $prevStart) = ($chr, $start);
  push @prevRows, \@w;
}

if (defined $prevChr) {
  printIfNonEmpty($prevChr, $prevStart, \@prevRows);
}
