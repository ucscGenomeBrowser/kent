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

sub findAssemblyMapping {
  # Return the dom node of the <mapping> for the desired assembly (disregarding GRC patch suffix)
  my ($dom, $assemblyPrefix) = @_;
  my @annotationSets = $dom->findnodes("/lrg/updatable_annotation/annotation_set");
  # There are usually several annotation sets; the one with source/name "LRG" has
  # the reference assembly mapping that we're looking for;
  my $lrgMapping;
  foreach my $s (@annotationSets) {
    my $name = $s->findvalue("source/name");
    if ($name eq "LRG") {
      my @mappingNodes = $s->findnodes("mapping");
      foreach my $m (@mappingNodes) {
	# Check the assembly name and make sure this is for a chrom, not patch etc.
	my $assembly = $m->findvalue('@coord_system');
	my $otherId = $m->findvalue('@other_id');
	return $m if ($assembly =~ /^$assemblyPrefix/ && $otherId =~ /^NC_/);
      }
    }
  }
  return undef;
} # findAssemblyMapping

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

  my $refMapping = findAssemblyMapping($dom, $assemblyPrefix);
  if (! defined $refMapping) {
    die "LRG mapping for $assemblyPrefix not found in $xmlIn.";
  }

  # Find BED 12+ fields.
  my $seq = $refMapping->findvalue('@other_name');
  $seq = 'chr' . $seq unless ($seq =~ /^chr/);
  my $start = $refMapping->findvalue('@other_start') - 1;
  my $end = $refMapping->findvalue('@other_end');
  my $lrgName = $dom->findvalue('/lrg/fixed_annotation/id');
  my @mappingSpans = $refMapping->findnodes('mapping_span');
  die 'Unusual number of mapping_spans' if (@mappingSpans != 1);
  my $span = $mappingSpans[0];
  my $lrgStart = $span->findvalue('@lrg_start') - 1;
  die "$xmlIn: Unexpected $assemblyPrefix mapping_span lrgStart $lrgStart" if ($lrgStart != 0);
  my $lrgEnd = $span->findvalue('@lrg_end');
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

  # HGNC id & symbol
  my $hgncId = $dom->findvalue('/lrg/fixed_annotation/hgnc_id');
  my $hgncSymbol = $dom->findvalue('/lrg/updatable_annotation/annotation_set/lrg_locus[@source="HGNC"]');
  # GenBank reference sequence accession
  my $lrgNcbiAcc = $dom->findvalue('/lrg/fixed_annotation/sequence_source');
  # LRG sequence source
  my $lrgSource = $dom->findvalue('/lrg/fixed_annotation/source/name');
  $lrgSource = utf8ToHtml($lrgSource);
  my $lrgSourceUrl = $dom->findvalue('/lrg/fixed_annotation/source/url');
  my $creationDate = $dom->findvalue('/lrg/fixed_annotation/creation_date');

  # watch out for stray tab chars:
  $lrgSource =~ s/^\s*(.*?)\s*$/$1/;
  $lrgSourceUrl =~ s/^\s*(.*?)\s*$/$1/;

  print $bedF join("\t", $seq, $start, $end, $lrgName, 0, $strand, $start, $end, 0,
		   $blockCount, $blockSizesStr, $blockStartsStr,
		   $mismatchesStr, $indelsStr, $lrgEnd,
		   $hgncId, $hgncSymbol, $lrgNcbiAcc,
		   $lrgSource, $lrgSourceUrl, $creationDate) . "\n";

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
