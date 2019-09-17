#!/usr/bin/env perl

# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/parseLrgXml.pl instead.

use warnings;
use strict;

use Encode;  # Of the UTF-8 sort, not the ENCODE project. :)
use XML::LibXML;
use Data::Dumper; #print Data::Dumper->Dump([$dom], [qw(dom)]);

my $assemblyPrefix = shift @ARGV;

sub usage {
  my ($status) = @_;
  my $base = $0;
  $base =~ s/^(.*\/)?//;
  print STDERR "
usage: $base assemblyPrefix
    Parses LRG_*.xml files in current directory into BED (LRG regions)
    and genePred+Cdna+Pep (transcripts in LRG coordinates.)
    assemblyPrefix is something like \"GRCh37\" to match GRCh37.p5 etc.

";
  exit $status;
} # usage

my %gbAccToHg19Alt = ( gl000250 => 'chr6_apd_hap1',
                       gl000251 => 'chr6_cox_hap2',
                       gl000252 => 'chr6_dbb_hap3',
                       gl000253 => 'chr6_mann_hap4',
                       gl000254 => 'chr6_mcf_hap5',
                       gl000255 => 'chr6_qbl_hap6',
                       gl000256 => 'chr6_ssto_hap7',
                       gl000257 => 'chr4_ctg9_hap1',
                       gl000258 => 'chr17_ctg5_hap1',
                     );

sub findAssemblyMappings {
  # Return the dom node(s) of the <mapping> for the desired assembly (disregarding GRC patch suffix)
  my ($dom, $assemblyPrefix) = @_;
  my @mappings = ();
  my @annotationSets = $dom->findnodes("/lrg/updatable_annotation/annotation_set");
  # There are usually several annotation sets; the one with source/name "LRG" has
  # the reference assembly mapping that we're looking for;
  my $lrgMapping;
  foreach my $s (@annotationSets) {
    my $name = $s->findvalue("source/name");
    if ($name eq "LRG") {
      my @mappingNodes = $s->findnodes("mapping");
      foreach my $m (@mappingNodes) {
	# Check the assembly name
	my $assembly = $m->findvalue('@coord_system');
	push @mappings, $m if ($assembly =~ /^$assemblyPrefix/);
      }
    }
  }
  return @mappings;
} # findAssemblyMappings

sub utf8ToHtml {
  my ($word) = @_;
  $word = decode_utf8($word, Encode::FB_CROAK);
  my @chars = split(//, $word);
  $word = "";
  foreach my $c (@chars) {
    if (ord($c) > 127) {
      $c = sprintf "&#%d;", ord($c);
    }
    $word .= $c;
  }
  return $word;
} # utf8ToHtml


sub parseOneLrg {
  my ($xmlIn, $bedF, $gpF, $cdnaF, $pepF) = @_;
  my $dom = XML::LibXML->load_xml( location => $xmlIn );

  my @refMappings = findAssemblyMappings($dom, $assemblyPrefix);
  if (@refMappings == 0) {
    die "LRG mapping for $assemblyPrefix not found in $xmlIn.";
  }

  my $lrgName = $dom->findvalue('/lrg/fixed_annotation/id');
  my $lrgSeq = $dom->findvalue('/lrg/fixed_annotation/sequence');
  my $lrgSize = length($lrgSeq);
  # HGNC id & symbol
  my $hgncId = $dom->findvalue('/lrg/fixed_annotation/hgnc_id');
  my $hgncSymbol = $dom->findvalue('/lrg/updatable_annotation/annotation_set/lrg_locus[@source="HGNC"]');
  # GenBank reference sequence accession
  my $lrgNcbiAcc = $dom->findvalue('/lrg/fixed_annotation/sequence_source');
  # LRG sequence source(s); for now, keeping only the first listed source because hgc.c
  # doesn't yet anticipate lists.
  my @lrgSources = $dom->findnodes('/lrg/fixed_annotation/source');
  my ($lrgSource, $lrgSourceUrl) = ('', '');
  while ((defined $lrgSources[0]) && (! $lrgSources[0]->findvalue('name'))) {
    shift @lrgSources;
  }
  if (defined $lrgSources[0]) {
    $lrgSource = utf8ToHtml($lrgSources[0]->findvalue('name'));
    $lrgSourceUrl = $lrgSources[0]->findvalue('url');
  }
  # watch out for stray tab chars:
  $lrgSource =~ s/^\s*(.*?)\s*$/$1/;
  $lrgSourceUrl =~ s/^\s*(.*?)\s*$/$1/;
  my $creationDate = $dom->findvalue('/lrg/fixed_annotation/creation_date');

  foreach my $refMapping (@refMappings) {
    # Find BED 12+ fields.
    my $mapType = $refMapping->findvalue('@type');
    my $seq = $refMapping->findvalue('@other_name');
    if ($seq eq 'unlocalized') {
      $seq = "Un";
    }
    if ($mapType eq 'haplotype' || $mapType eq 'patch') {
      my $gbAcc = $refMapping->findvalue('@other_id_syn');
      $gbAcc =~ m/^[A-Z]+\d+\.\d+$/ || die "$xmlIn: $assemblyPrefix has $mapType mapping with " .
        "other_id_syn='$gbAcc', expecting versioned GenBank acc (e.g. 'KI270850.1').";
      if ($assemblyPrefix eq 'GRCh37') {
        $gbAcc =~ s/\..*//;
        $gbAcc = lc $gbAcc;
      } else {
        $gbAcc =~ s/\./v/;
      }
      # Trim chromosome band stuff if present
      $seq =~ s/[pq].*//;
      if ($assemblyPrefix eq 'GRCh37' && exists $gbAccToHg19Alt{$gbAcc}) {
        $seq = $gbAccToHg19Alt{$gbAcc};
      } elsif ($seq eq 'Un') {
        $seq .= "_$gbAcc";
      } else {
        # NOTE: as of 5/30/18, there are no mappings to hg19 or hg38 seqs with the suffix _random,
        # so I'm not sure what those would look like in the XML.  This could cause us to lose
        # mappings to the _random sequences, *if* any are added in the future.
        my $suffix = ($mapType eq 'haplotype' ? 'alt' : 'fix');
        $seq .= "_${gbAcc}_$suffix";
      }
    }
    $seq = 'chr' . $seq unless ($seq =~ /^chr/);
    my $start = $refMapping->findvalue('@other_start') - 1;
    my $end = $refMapping->findvalue('@other_end');
    my @mappingSpans = $refMapping->findnodes('mapping_span');
    die 'Unusual number of mapping_spans' if (@mappingSpans != 1);
    my $span = $mappingSpans[0];
    my $lrgStart = $span->findvalue('@lrg_start') - 1;
    my $lrgEnd = $span->findvalue('@lrg_end');
    if ($lrgSize < $lrgEnd) {
      die "$xmlIn: length of sequence is $lrgSize but $assemblyPrefix lrg_end is $lrgEnd";
    }
    my $name = $lrgName;
    if ($lrgStart != 0 || $lrgEnd != $lrgSize) {
      $name .= ":". ($lrgStart+1) . "-$lrgEnd";
    }
    my $strand = $span->findvalue('@strand');
    $strand = ($strand == 1 ? '+' : '-');

    # Sort out indels and mismatches when building block coords:
    my @blockSizes = ();
    my @blockStarts = (0);
    my @mismatches = ();
    my @indels = ();
    my @diffs = $refMapping->findnodes('mapping_span/diff');
    # Sort @diffs by ascending assembly start coords, so the loop below has sorted inputs:
    @diffs = sort { $a->findvalue('@other_start') <=> $b->findvalue('@other_start') } @diffs;
    foreach my $d (@diffs) {
      my $lrgStart = $d->findvalue('@lrg_start') - 1;
      my $lrgEnd = $d->findvalue('@lrg_end');
      my $blkStart = $d->findvalue('@other_start') - 1 - $start;
      my $blkEnd = $d->findvalue('@other_end') - $start;
      my $lrgSeq = $d->findvalue('@lrg_sequence');
      my $refSeq = $d->findvalue('@other_sequence');
      my $type = $d->findvalue('@type');
      my $lastBlkStart = $blockStarts[$#blockStarts];
      if ($type eq 'mismatch') {
        push @mismatches, join(':', $blkStart, $lrgStart, $refSeq, $lrgSeq);
      } elsif ($type eq 'lrg_ins') {
        # LRG has sequence where assembly has none; adjust assembly coords to start=end
        $blkStart++;  $blkEnd--;  die "weird lrg_ins other_ coords" if ($blkStart != $blkEnd);
        push @blockSizes, ($blkStart - $lastBlkStart);
        push @blockStarts, $blkStart;
        push @indels, join(':', $blkStart, $lrgStart, $refSeq, $lrgSeq);
      } elsif ($type eq 'other_ins') {
        # assembly has sequence where LRG has none; adjust LRG coords to be start=end
        $lrgStart++;  $lrgEnd--;  die "weird other_ins lrg_ coords" if ($lrgStart != $lrgEnd);
        push @blockSizes, ($blkStart - $lastBlkStart);
        push @blockStarts, $blkEnd;
        push @indels, join(':', $blkStart, $lrgStart, $refSeq, $lrgSeq);
      } else {
        die "Unrecognized diff type '$type' in $xmlIn";
      }
    }
    push @blockSizes, ($end - $start - $blockStarts[$#blockStarts]);
    my $blockCount = @blockStarts;
    die unless ($blockCount == @blockSizes);
    my $blockSizesStr = join(',', @blockSizes) . ',';
    my $blockStartsStr = join(',', @blockStarts) . ',';
    my $mismatchesStr = join(',', @mismatches);
    my $indelsStr = join(',', @indels);

    print $bedF join("\t", $seq, $start, $end, $name, 0, $strand, $start, $end, 0,
                     $blockCount, $blockSizesStr, $blockStartsStr,
                     $mismatchesStr, $indelsStr, $lrgSize,
                     $hgncId, $hgncSymbol, $lrgNcbiAcc,
                     $lrgSource, $lrgSourceUrl, $creationDate) . "\n";
  } # each refMapping

  # Now convert fixed transcript annotation to genePredExt and fasta for mrna and protein.
  my @fixedTs = $dom->findnodes('/lrg/fixed_annotation/transcript');
  foreach my $t (@fixedTs) {
    my $txStart = $t->findvalue('coordinates/@start') - 1;
    my $txEnd = $t->findvalue('coordinates/@end');
    my $txStrand = ($t->findvalue('coordinates/@strand') < 0) ? '-' : '+';
    my $tN = $t->findvalue('@name');
    my $txName = $lrgName . $tN;
    # There may be multiple coding regions for different transcripts (or no coding regions
    # for non-coding gene loci).  LRG_321.xml has an interesting assortment of coding regions,
    # exons and proteins.  We don't really have a way to represent one transcript with multiple
    # coding regions, so if there's a p# that matches the t#, use that; otherwise just use the
    # first given.
    my ($cdsStart, $cdsEnd);
    my $pN = $tN;  $pN =~ s/^t/p/ || die;
    my @codingRegions = $t->findnodes('coding_region[translation/@name="' . $pN . '"]');
    if (@codingRegions == 0) {
      @codingRegions = $t->findnodes('coding_region');
    }
    if (@codingRegions == 0) {
      # non-coding
      $cdsStart = $cdsEnd = $txStart;
    } else {
      $cdsStart = $codingRegions[0]->findvalue('coordinates/@start') - 1;
      $cdsEnd = $codingRegions[0]->findvalue('coordinates/@end');
    }
    my (@exonStarts, @exonEnds, @exonFrames);
    # In LRG xml, phase is assigned to introns and applies to the following exon.
    # There are no intron nodes between exon nodes outside the coding region.
    # So we can't process exons and introns separately; we have to go through
    # all in order, and remember the phase when it's given in an intron node.
    my $phase = -1;
    my @trons = $t->findnodes('exon | intron');
    foreach my $ei (@trons) {
      if ($ei->nodeName eq 'exon') {
	my @lrgCoords = $ei->findnodes('coordinates[@coord_system="' . $lrgName . '"]');
	my $eStart = $lrgCoords[0]->findvalue('@start') - 1;
	my $eEnd = $lrgCoords[0]->findvalue('@end');
	push @exonStarts, $eStart;
	push @exonEnds, $eEnd;
	if ($phase == -1 &&
	    $cdsStart != $cdsEnd &&
	    $eStart <= $cdsStart && $eEnd > $cdsStart) {
	  # First coding exon's phase is 0.
	  $phase = 0;
	}
	push @exonFrames, $phase;
	# In case this is the last coding exon (no subsequent intron node):
	$phase = -1;
      } elsif ($ei->nodeName eq 'intron') {
	# Now we know the phase of the following exon.
	$phase = $ei->findvalue('@phase');
      }
    }
    my $cmpl = ($cdsStart != $cdsEnd) ? "cmpl" : "none";
    print $gpF join("\t", $txName, $lrgName, $txStrand, $txStart, $txEnd, $cdsStart, $cdsEnd,
		    scalar(@exonStarts), join(",", @exonStarts).",", join(",", @exonEnds).",",
		    0, $hgncSymbol, $cmpl, $cmpl, join(",", @exonFrames).",") . "\n";

    print $cdnaF "$txName\t" . $t->findvalue('cdna/sequence') . "\n";

    if (@codingRegions) {
      my @ps = $codingRegions[0]->findnodes('translation');
      # I expect one for protein-coding genes, none for non-coding:
      print $pepF "$txName\t" . $ps[0]->findvalue('sequence') . "\n";
    }
  } # each transcript
} # parseOneLrg

################################ MAIN #################################

if (! $assemblyPrefix) {
  usage(1);
}

my @xmlFiles = <LRG_*.xml>;
if (@xmlFiles == 0) {
  warn "No LRG_*.xml files found in current directory -- need to cd somewhere?\n";
}

open(my $bedF, "| sort -k1,1 -k2n,2n >lrg.bed")
  || die "Can't open lrg.bed for (sorting and) writing): $!\n";
open(my $gpF, "| sort -k2,2 -k4n,4n >lrgTranscriptsUnmapped.gp")
  || die "Can't open lrgTranscriptsUnmapped.gp for (sorting and) writing: $!\n";
open(my $cdnaF, ">lrgCdna.tab") || die "Can't open lrgCdna.tab for writing: $!\n";
open(my $pepF, ">lrgPep.tab") || die "Can't open lrgPep.tab for writing: $!\n";

foreach my $file (@xmlFiles) {
  parseOneLrg($file, $bedF, $gpF, $cdnaF, $pepF);
}

close($bedF);
close($gpF);
close($cdnaF);
close($pepF);
