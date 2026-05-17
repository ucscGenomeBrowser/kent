#!/usr/bin/env perl

use strict;
use warnings;

my $argc = scalar(@ARGV);
if ($argc != 3) {
  printf STDERR "usage: mkHtml <referenceAccession> <577.acc.sciName.comName.clade.tsv> <order.list>\n";
  printf STDERR "  referenceAccession - accession of the reference species\n";
  printf STDERR "  the .tsv file has four columns: accession, sciName, comName, clade\n";
  printf STDERR "  the order.list has two columns: coverage, accession\n";
  printf STDERR "output to stdout is vgp577way.html page\n";
  exit 255;
}

my $refAcc = shift;
my $tsvFile = shift;
my $orderFile = shift;

# mapping from order.list names to TSV accessions for special cases
my %aliasToTsv = (
  "hs1"  => "GCA_009914755.4",
  "hg38" => "GCA_000001405.15",
  "mm39" => "GCA_000001635.9",
);

# read the TSV: accession -> (sciName, comName, clade)
my %sciName;    # key is accession, value is scientific name
my %comName;    # key is accession, value is common name
my %clade;      # key is accession, value is clade string

open (my $fh, "<", $tsvFile) or die "can not read $tsvFile";
while (my $line = <$fh>) {
  chomp $line;
  my ($acc, $sci, $com, $cl) = split('\t', $line);
  $sciName{$acc} = $sci;
  $comName{$acc} = $com;
  $clade{$acc} = $cl;
}
close ($fh);

# read order.list: ordered list of accessions with coverage measures
my @orderData;   # array of hashrefs with keys: coverage, acc, tsvKey

open ($fh, "<", $orderFile) or die "can not read $orderFile";
while (my $line = <$fh>) {
  chomp $line;
  next if ($line =~ m/^\s*$/);
  my @fields = split('\s+', $line);

  # check we have exactly 2 fields
  if (scalar(@fields) != 2) {
    warn "ORDER: Skipping malformed line: $line\n";
    next;
  }

  my ($coverage, $acc) = @fields;

  # check that accession is not empty
  if (!defined($acc) || $acc eq '') {
    warn "ORDER: Skipping line with empty accession: $line\n";
    next;
  }

  # determine TSV key for lookup
  my $tsvKey = defined($aliasToTsv{$acc}) ? $aliasToTsv{$acc} : $acc;

  push (@orderData, {
    coverage => $coverage,
    acc => $acc,
    tsvKey => $tsvKey
  });
}
close ($fh);

# helper functions
sub formatNumber {
  my ($num) = @_;
  # add commas to large numbers
  $num =~ s/(\d)(?=(\d{3})+$)/$1,/g;
  return $num;
}

sub getDbLink {
  my ($acc) = @_;

  # known UCSC database mappings
  my %ucscDbs = (
    "hg38" => "hg38",
    "hs1" => "hs1",
    "mm39" => "mm39",
    "GCA_000001405.15" => "hg38",
    "GCA_009914755.4" => "hs1",
    "GCA_000001635.9" => "mm39"
  );

  if (defined($ucscDbs{$acc})) {
    return "/cgi-bin/hgTracks?db=" . $ucscDbs{$acc};
  } elsif ($acc =~ /^GC[AF]_/) {
    return "/h/$acc";
  }
  return undef;
}

sub getTaxonId {
  my ($sciName) = @_;
  # This would ideally query NCBI, but for now return placeholder
  # In practice, you'd want a lookup table or API call
  return "0000";
}

sub cladeDisplayName {
  my ($cl) = @_;
  # Convert clade names to display format
  $cl =~ s/-/ /g;
  $cl =~ s/\b(\w)/\U$1/g;  # capitalize first letter of each word
  return $cl;
}

# get total species count
my $totalSpecies = scalar(@orderData);

# collect unique clades in order of appearance
my @cladeOrder;
my %cladeSeen;
my %cladeSpecies;  # clade -> array of species for stats

foreach my $item (@orderData) {
  my $cl = $clade{$item->{tsvKey}};
  if (!defined($cladeSeen{$cl})) {
    push (@cladeOrder, $cl);
    $cladeSeen{$cl} = 1;
  }
  push (@{$cladeSpecies{$cl}}, $item->{acc});
}

# start generating HTML
print <<'__EOF__';
<H2>Description</H2>
<P>
This track shows a multiple alignment of 577 vertebrate genomes made with Cactus.
This alignment represents the largest and most taxonomically diverse vertebrate
alignment to date, spanning mammals, birds, fishes, reptiles, and amphibians from
the <A target=_blank HREF="https://vertebrategenomesproject.org/">Vertebrate Genomes Project (VGP)</A>
and related sequencing initiatives.
</P>

<P>
All alignments were performed using the <A target=_blank HREF="https://github.com/ComparativeGenomicsToolkit/cactus"
TARGET=_blank>Cactus toolkit</A>, which uses a reference-free approach to construct
genome alignments and ancestral reconstructions. Unlike traditional progressive
alignment methods, Cactus simultaneously considers all input genomes to minimize
reference bias and improve alignment accuracy across diverse evolutionary distances.
</P>

<P>
The alignment includes:
<UL>
__EOF__

# print clade statistics
foreach my $cl (@cladeOrder) {
  my $count = scalar(@{$cladeSpecies{$cl}});
  my $displayName = cladeDisplayName($cl);
  print "<li><b>$displayName</b>: $count species\n";
}

print <<'__EOF__';
</UL>
</P>

<P>
This broad phylogenetic sampling enables comparative genomics studies across
the vertebrate tree of life, supporting research in evolution, conservation,
and functional genomics.
</P>
__EOF__

# <h2>Data Access</h2>
# <p>
# Downloads for data in this track are available from the directory:
# <ul>
# <li>
# <a href="https://hgdownload.soe.ucsc.edu/goldenPath/hg38/vgp577way/">VGP 577-way alignments</a> (MAF format)
# </ul>
# </p>

print <<'__EOF__';
<h2>Display Conventions and Configuration</h2>
<p>
Pairwise alignments of each species to the reference genome are
displayed as a grayscale density plot (in pack mode) or as a wiggle (in full mode)
that indicates alignment quality. In dense display mode, alignments are shown in
grayscale using darker values to indicate higher levels of coverage.
</p>
<p>
Checkboxes on the track configuration page allow selection of the
species to include in the pairwise display.
Note that excluding species from the pairwise display does not alter the
underlying alignment data.
</p>
<p>
To view detailed information about the alignments at a specific
position, zoom the display in to 30,000 or fewer bases, then click on
the alignment.
</p>

<h3>Gap Annotation</h3>
<p>
The <em>Display chains between alignments</em> configuration option
enables display of gaps between alignment blocks in the pairwise alignments in
a manner similar to the Chain track display.  Missing sequence in any
assembly is highlighted in the track display by regions of yellow when zoomed
out and by Ns when displayed at base level.  The following conventions are used:
<ul>
<li><b>Single line:</b> No bases in the aligned species. Possibly due to a
lineage-specific insertion between the aligned blocks in the reference genome
or a lineage-specific deletion between the aligned blocks in the aligning
species.
<li><b>Double line:</b> Aligning species has one or more unalignable bases in
the gap region. Possibly due to excessive evolutionary distance between
species or independent indels in the region between the aligned blocks in both
species.
<li><b>Pale yellow coloring:</b> Aligning species has Ns in the gap region.
Reflects uncertainty in the relationship between the DNA of both species, due
to lack of sequence in relevant portions of the aligning species.
</ul></p>

<h3>Genomic Breaks</h3>
<p>
Discontinuities in the genomic context (chromosome, scaffold or region) of the
aligned DNA in the aligning species are shown as follows:
<ul>
<li>
<b>Vertical blue bar:</b> Represents a discontinuity that persists indefinitely
on either side, <em>e.g.</em> a large region of DNA on either side of the bar
comes from a different chromosome in the aligned species due to a large scale
rearrangement.
<li>
<b>Green square brackets:</b> Enclose shorter alignments consisting of DNA from
one genomic context in the aligned species nested inside a larger chain of
alignments from a different genomic context. The alignment within the
brackets may represent a short misalignment, a lineage-specific insertion of a
transposon in the reference genome that aligns to a paralogous copy somewhere
else in the aligned species, or other similar occurrence.
</ul></p>

<h3>Base Level</h3>
<p>
When zoomed-in to the base-level display, the track shows the base
composition of each alignment. The numbers and symbols on the Gaps
line indicate the lengths of gaps in the reference sequence at those
alignment positions relative to the longest non-reference sequence.
If there is sufficient space in the display, the size of the gap is shown.
If the space is insufficient and the gap size is a multiple of 3, a
&quot;*&quot; is displayed; other gap sizes are indicated by &quot;+&quot;.</p>

<h2>Methods</h2>
<p>
The VGP 577-way alignment was created using the Cactus multiple genome alignment tool.
Cactus employs a reference-free progressive alignment strategy that constructs a series
of pairwise alignments organized according to a guide tree, then uses these to build
progressively larger alignments up to the full multiple alignment.
</p>

<p>
Unlike traditional progressive alignment approaches, Cactus performs an additional
"bar" phase that identifies and resolves alignment conflicts by considering all
genomes simultaneously. This helps reduce reference bias and improves accuracy
across diverse evolutionary distances present in vertebrate genomes.
</p>

<p>
The input genomes represent high-quality chromosome-level assemblies primarily
from the Vertebrate Genomes Project, with additional assemblies from NCBI RefSeq
and GenBank that meet quality standards for whole-genome alignment.
</p>

<h2>Sequences</h2>
<p>
<blockquote>
<table border=1 class='stdTbl'>
<tr><th>count</th>
    <th>common<br>name</th>
    <th>clade</th>
    <th>scientific&nbsp;name<br>(link&nbsp;to&nbsp;browser&nbsp;when&nbsp;existing)</th>
</tr>
__EOF__

# print species table
my $rowCount = 1;
foreach my $item (@orderData) {
  my $pos = sprintf("%03d", $rowCount++);
  my $acc = $item->{acc};
  my $tsvKey = $item->{tsvKey};
  my $coverage = formatNumber($item->{coverage});

  my $commonName = $comName{$tsvKey} || "unknown";
  my $sciName = $sciName{$tsvKey} || "unknown";
  my $cladeDisplay = cladeDisplayName($clade{$tsvKey} || "unknown");

  # clean up common name - remove extra info after /
  $commonName =~ s/\/.*$//;

  # create scientific name with link if available
  my $sciNameDisplay = $sciName;
  my $dbLink = getDbLink($acc);
  if (defined($dbLink)) {
    $sciNameDisplay = $sciName . "<br><a href='$dbLink' target=_blank>$acc</a>";
  } elsif ($acc =~ /^GC[AF]_/) {
    $sciNameDisplay = $sciName . "<br><a href='/h/$acc' target=_blank>$acc</a>";
  } else {
    $sciNameDisplay = $sciName . "<br>$acc";
  }

  # make reference species special
  if ($acc eq $refAcc || $tsvKey eq $refAcc) {
    $sciNameDisplay .= "<br><b>reference species</b>";
  }

  print "<tr><td>$pos</td><th>$commonName</th><td>$cladeDisplay</td><td>$sciNameDisplay</td></tr>\n";
}

print <<'__EOF__';
</table><br>
<b>Table 1.</b> <em>Genome assemblies included in the VGP 577-way alignment.</em>
</blockquote></p>

<h2>References</h2>
<p>
Rhie A, McCarthy SA, Fedrigo O, Damas J, Formenti G, Koren S, Uliano-Silva M, Chow W, Fungtammasan A, Kim J
<em>et al</em>.
<a href="https://doi.org/10.1038/s41586-021-03451-0" target="_blank">
Towards complete and error-free genome assemblies of all vertebrate species</a>.
<em>Nature</em>. 2021 Apr 29;592(7856):737-746.
DOI: <a href="https://doi.org/10.1038/s41586-021-03451-0"
target="_blank">10.1038/s41586-021-03451-0</a>; PMID: <a
href="https://www.ncbi.nlm.nih.gov/pubmed/33911273" target="_blank">33911273</a>;
PMC: <a href="https://www.ncbi.nlm.nih.gov/pmc/articles/PMC8081667/" target="_blank">PMC8081667</a>
</p>
<p>
Paten B, Earl D, Nguyen N, Diekhans M, Zerbino D, Haussler D.
<a href="https://doi.org/10.1186/gb-2011-12-6-r63" target="_blank">
Cactus: Algorithms for genome multiple sequence alignment</a>.
<em>Genome Biol</em>. 2011 Jun 24;12(6):R63.
DOI: <a href="https://doi.org/10.1186/gb-2011-12-6-r63"
target="_blank">10.1186/gb-2011-12-6-r63</a>; PMID: <a
href="https://www.ncbi.nlm.nih.gov/pubmed/21703006" target="_blank">21703006</a>;
PMC: <a href="https://www.ncbi.nlm.nih.gov/pmc/articles/PMC3218810/" target="_blank">PMC3218810</a>
</p>
__EOF__
