#!/usr/bin/env perl

# This is under source control -- make sure you are editing
# kent/src/hg/utils/dbNsfpToBed.pl

# The database of non-synonymous functional predictions (dbNSFP)
# contains precomputed scores from a wide variety of tools on all
# non-synon variants of all genomic positions in the CDS of Gencode
# transcripts.  Pick out some interesting subsets of its many columns
# and translate into bigBed and bigWig files that can be joined with
# users' variants by the Variant Annotation Integrator (#6152).

use warnings;
use strict;

# As of dbNSFP 3.1a (Nov. 2015), the files had 143 tab-separated columns
# described by a #header line with a label for each column.
# (Back in dbNSFP 2.0 (Feb. 2013), the files had a mere 52 columns.)
# Expect to find these input header column labels, in this order;
# also define corresponding symbolic names for internal use:
my @columnNames = ( [ "chr",			"chrom" ],
		    [ "pos(1-based)",		"hg38Pos" ],
		    [ "ref",			"refAl" ],
		    [ "alt",			"altAl" ],
		    [ "aaref",			"refAa" ],
		    [ "aaalt",			"altAa" ],
		    [ "rs_dbSNP144",		"" ],
		    [ "hg19_chr",		"hg19Chrom" ],
		    [ "hg19_pos(1-based)",	"hg19Pos" ],
		    [ "hg18_chr",		"hg18Chrom" ],
		    [ "hg18_pos(1-based)",	"hg18Pos" ],
		    [ "genename",		"geneName" ],
		    [ "cds_strand",		"cdsStrand" ],
		    [ "refcodon",		"refCodon" ],
		    [ "codonpos",		"codonPos" ],
		    [ "codon_degeneracy",	"" ],
		    [ "Ancestral_allele",	"" ],
		    [ "AltaiNeandertal",	"" ],
		    [ "Denisova",		"" ],
		    [ "Ensembl_geneid",		"ensGeneId" ],
		    [ "Ensembl_transcriptid",	"ensTxId" ],
		    [ "Ensembl_proteinid",	"" ],
		    [ "aapos",			"aaPos" ],
		    [ "SIFT_score",			"siftScore" ],
		    [ "SIFT_converted_rankscore",	"" ],
		    [ "SIFT_pred",			"" ],
		    [ "Uniprot_acc_Polyphen2",		"uniProtAcc" ],
		    [ "Uniprot_id_Polyphen2",		"uniProtId" ],
		    [ "Uniprot_aapos_Polyphen2",	"uniProtAaPos" ],
		    [ "Polyphen2_HDIV_score",		"pp2HDivScore" ],
		    [ "Polyphen2_HDIV_rankscore",	"" ],
		    [ "Polyphen2_HDIV_pred",		"pp2HDivPred" ],
		    [ "Polyphen2_HVAR_score",		"pp2HVarScore" ],
		    [ "Polyphen2_HVAR_rankscore",	"" ],
		    [ "Polyphen2_HVAR_pred",		"pp2HVarPred" ],
		    [ "LRT_score",			"lrtScore" ],
		    [ "LRT_converted_rankscore",	"" ],
		    [ "LRT_pred",			"lrtPred" ],
		    [ "LRT_Omega",			"lrtOmega" ],
		    [ "MutationTaster_score",			"mtScore" ],
		    [ "MutationTaster_converted_rankscore",	"" ],
		    [ "MutationTaster_pred",			"mtPred" ],
		    [ "MutationTaster_model",			"" ],
		    [ "MutationTaster_AAE",			"" ],
		    [ "Uniprot_id_MutationAssessor",		"" ],
		    [ "Uniprot_variant_MutationAssessor",	"" ],
		    [ "MutationAssessor_score",		"maScore" ],
		    [ "MutationAssessor_rankscore",	"" ],
		    [ "MutationAssessor_pred",		"maPred" ],
		    [ "FATHMM_score",			"" ],
		    [ "FATHMM_converted_rankscore",	"" ],
		    [ "FATHMM_pred",			"" ],
		    [ "PROVEAN_score",			"" ],
		    [ "PROVEAN_converted_rankscore",	"" ],
		    [ "PROVEAN_pred",			"" ],
		    [ "Transcript_id_VEST3",		"vestNm" ],
		    [ "Transcript_var_VEST3",		"vestVar" ],
		    [ "VEST3_score",			"vestScore" ],
		    [ "VEST3_rankscore",		"" ],
		    [ "CADD_raw",			"" ],
		    [ "CADD_raw_rankscore",		"" ],
		    [ "CADD_phred",			"" ],
		    [ "DANN_score",			"" ],
		    [ "DANN_rankscore",			"" ],
		    [ "fathmm-MKL_coding_score",	"" ],
		    [ "fathmm-MKL_coding_rankscore",	"" ],
		    [ "fathmm-MKL_coding_pred",		"" ],
		    [ "fathmm-MKL_coding_group",	"" ],
		    [ "MetaSVM_score",			"" ],
		    [ "MetaSVM_rankscore",		"" ],
		    [ "MetaSVM_pred",			"" ],
		    [ "MetaLR_score",			"" ],
		    [ "MetaLR_rankscore",		"" ],
		    [ "MetaLR_pred",			"" ],
		    [ "Reliability_index",		"" ],
		    [ "integrated_fitCons_score",	"" ],
		    [ "integrated_fitCons_rankscore",	"" ],
		    [ "integrated_confidence_value",	"" ],
		    [ "GM12878_fitCons_score",		"" ],
		    [ "GM12878_fitCons_rankscore",	"" ],
		    [ "GM12878_confidence_value",	"" ],
		    [ "H1-hESC_fitCons_score",		"" ],
		    [ "H1-hESC_fitCons_rankscore",	"" ],
		    [ "H1-hESC_confidence_value",	"" ],
		    [ "HUVEC_fitCons_score",		"" ],
		    [ "HUVEC_fitCons_rankscore",	"" ],
		    [ "HUVEC_confidence_value",		"" ],
		    [ "GERP++_NR",			"gerpNr" ],
		    [ "GERP++_RS",			"gerpRs" ],
		    [ "GERP++_RS_rankscore",		"" ],
		    [ "phyloP7way_vertebrate",			"" ],
		    [ "phyloP7way_vertebrate_rankscore",	"" ],
		    [ "phyloP20way_mammalian",			"" ],
		    [ "phyloP20way_mammalian_rankscore",	"" ],
		    [ "phastCons7way_vertebrate",		"" ],
		    [ "phastCons7way_vertebrate_rankscore",	"" ],
		    [ "phastCons20way_mammalian",		"" ],
		    [ "phastCons20way_mammalian_rankscore",	"" ],
		    [ "SiPhy_29way_pi",			"" ],
		    [ "SiPhy_29way_logOdds",		"" ],
		    [ "SiPhy_29way_logOdds_rankscore",	"" ],
		    [ "1000Gp3_AC",			"" ],
		    [ "1000Gp3_AF",			"" ],
		    [ "1000Gp3_AFR_AC",			"" ],
		    [ "1000Gp3_AFR_AF",			"" ],
		    [ "1000Gp3_EUR_AC",			"" ],
		    [ "1000Gp3_EUR_AF",			"" ],
		    [ "1000Gp3_AMR_AC",			"" ],
		    [ "1000Gp3_AMR_AF",			"" ],
		    [ "1000Gp3_EAS_AC",			"" ],
		    [ "1000Gp3_EAS_AF",			"" ],
		    [ "1000Gp3_SAS_AC",			"" ],
		    [ "1000Gp3_SAS_AF",			"" ],
		    [ "TWINSUK_AC",			"" ],
		    [ "TWINSUK_AF",			"" ],
		    [ "ALSPAC_AC",			"" ],
		    [ "ALSPAC_AF",			"" ],
		    [ "ESP6500_AA_AC",			"" ],
		    [ "ESP6500_AA_AF",			"" ],
		    [ "ESP6500_EA_AC",			"" ],
		    [ "ESP6500_EA_AF",			"" ],
		    [ "ExAC_AC",			"" ],
		    [ "ExAC_AF",			"" ],
		    [ "ExAC_Adj_AC",			"" ],
		    [ "ExAC_Adj_AF",			"" ],
		    [ "ExAC_AFR_AC",			"" ],
		    [ "ExAC_AFR_AF",			"" ],
		    [ "ExAC_AMR_AC",			"" ],
		    [ "ExAC_AMR_AF",			"" ],
		    [ "ExAC_EAS_AC",			"" ],
		    [ "ExAC_EAS_AF",			"" ],
		    [ "ExAC_FIN_AC",			"" ],
		    [ "ExAC_FIN_AF",			"" ],
		    [ "ExAC_NFE_AC",			"" ],
		    [ "ExAC_NFE_AF",			"" ],
		    [ "ExAC_SAS_AC",			"" ],
		    [ "ExAC_SAS_AF",			"" ],
		    [ "clinvar_rs",			"" ],
		    [ "clinvar_clnsig",			"" ],
		    [ "clinvar_trait",			"" ],
		    [ "Interpro_domain",		"interProDomain" ],
		    [ "GTEx_V6_gene",			"" ],
		    [ "GTEx_V6_tissue",			"" ],
		  );

# Define subs to use in lieu of string constants for symbolic names:
# ('use constant' doesn't work with 'use strict';
#  see http://stackoverflow.com/questions/3057027/using-constants-in-perl)
eval join(" ",
          map { my $symbol = $_->[1]; $symbol ? "sub $symbol () { $symbol; } " : "" }
              @columnNames);

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

                      # New in v3: VEST, from Rachel Karchin's lab at JHU
                      Vest             => [ refAl(), ensTxId(), vestNm(), altAl(),
                                            vestVar(), vestScore() ],

		      # These subsets are related to position or transcript, not alleles:
		      GerpNr           => [ gerpNr() ],

		      GerpRs           => [ gerpRs() ],

		      InterPro         => [ interProDomain() ],

		      # Note: these were taken from PolyPhen2! Not directly related to ensTxIds.
		      UniProt          => [ uniProtAcc(), uniProtId() ],

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
        if ($rowsForTx[$i]->[$altAlIx] eq $rowsForTx[$i]->[$refAlIx]) {
          # Sometimes there's a row with altAl the same as refAl and mostly empty columns;
          # drop those rows.
          print STDERR "refAl == altAl:\n" . join("\t", @{$rowsForTx[$i]}) . "\n";
        } else {
          push @newRows, $rowsForTx[$i];
        }
      }
      $rowCount = @newRows;
      if ($rowCount > 3) {
	print STDERR "Failed to resolve rows to 3 {$chr, $start, altAl, $ensTxId}.\n";
        foreach my $r (@newRows) {
          print STDERR join("\t", @{$r}) . "\n";
        }
        die;
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
  foreach my $ensTxId (sort keys %ensTxRows) {
    my @rowsForTx = @{$ensTxRows{$ensTxId}};
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

my ($chromIx, $hg38PosIx) = ($symbolIx{chrom()}, $symbolIx{hg38Pos()});
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

  # Positions are all given as {chr, start}; size is always 1 base.
  my ($chr, $start) = getVals(\@w, [ $chromIx, $hg38PosIx ]);
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
