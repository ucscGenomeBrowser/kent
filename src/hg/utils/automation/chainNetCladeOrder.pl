#!/usr/bin/env perl

# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/chainNetCladeOrder.pl instead.

# chainNetCladeOrder.pl - make the clade/order file for the chain/net
#   alignments of a target assembly to its query assemblies, read by:
#     chainNetCompositeTrackDb.pl -cladeFile=<file>
#     chainNetDescription.pl -cladeFile=<file>
#   so the trackDb composite and its description page list the queries
#   in the same groups and the same order.
#
# Nothing here is maintained by hand:
#   clade - from the NCBI taxonomy lineages of the target and the query,
#      the query's lineage node just below the deepest node it shares with
#      the target, e.g. for hg38: chimp -> Pan, rhesus -> Cercopithecoidea,
#      marmoset -> Platyrrhini, mouse lemur -> Strepsirrhini,
#      mouse -> Glires, another human assembly -> Homo sapiens
#   order - clades from the closest to the most distant (the deepest
#      shared lineage node first), and in each clade by the mash distance
#      to the target, then by the featureBits chain coverage
#   chainOn netOn - the closest query has its chain and net on, the
#      closest query of each clade has its net on, everything else off
#   copies of one assembly (accession versions, identical GenBank/RefSeq
#      pairs, a UCSC database and its GenArk accession) are collapsed to
#      one, unless -keepAll, and -maxPerClade=N keeps the N closest of
#      each clade
#
# The mash sketches are read from their caches, nothing is sketched here:
#   /hive/data/genomes/<db>/mashSketch/<db>.msh
#   <GenArk build directory>/mashSketch/<accession>.msh
# (the same caches mashDistance.pl/AssemblyDivergence.pm make)

use strict;
use warnings;
use File::Basename;
use Getopt::Long;
use FindBin qw($Bin);
use lib "$Bin";
use AsmHub;
use ChainNetInfo qw(accessionOrDb findAlignments);

my $genbankTaxonomy = "/hive/data/outside/ncbi/genomes/reports/genbank/allAssemblies.taxonomy.tsv";
my $refseqTaxonomy = "/hive/data/outside/ncbi/genomes/reports/refseq/allAssemblies.taxonomy.tsv";

my $methodOpt = "all";
my $within = "";
my $exclude = "";
my $keepAll = 0;
my $maxPerClade = 0;
my $optOk = GetOptions('method=s' => \$methodOpt,
                       'within=s' => \$within,
                       'exclude=s' => \$exclude,
                       'keepAll' => \$keepAll,
                       'maxPerClade=i' => \$maxPerClade);
my $argc = scalar(@ARGV);

if (! $optOk || $argc < 1 || $methodOpt !~ m/^(all|lastz|miniMap2)$/) {
  printf STDERR "usage: chainNetCladeOrder.pl [options] <target> [<query> ...] > target.clade.tab\n";
  printf STDERR "where target is a UCSC database (e.g. hg38) or GenArk assembly,\n";
  printf STDERR "and   query is zero or more UCSC database or GenArk assemblies,\n";
  printf STDERR "      with no queries, all the queries with a build directory in\n";
  printf STDERR "      the target's bed/ (or GenArk trackData/) directory are used:\n";
  printf STDERR "        lastz.<query>  blastz.<query>.swap  miniMap2.<query>  miniMap2.<query>.swap\n";
  printf STDERR "options:\n";
  printf STDERR "  -method=lastz|miniMap2 - only the alignments made with this method,\n";
  printf STDERR "      default: all alignments found\n";
  printf STDERR "  -within=<taxon>[,<taxon>...] - only the queries with one of these\n";
  printf STDERR "      names in their NCBI taxonomy lineage, e.g.: -within=Primates\n";
  printf STDERR "  -exclude=<taxon>[,<taxon>...] - leave out the queries with one of\n";
  printf STDERR "      these names in their lineage, e.g.: -within=Mammalia -exclude=Primates\n";
  printf STDERR "  -maxPerClade=N - only the N closest queries of each clade\n";
  printf STDERR "  -keepAll - do not collapse the copies of one assembly, by default\n";
  printf STDERR "      only one is kept of: the versions of an accession, a GenBank and\n";
  printf STDERR "      RefSeq pair that NCBI says are identical, a UCSC database and its\n";
  printf STDERR "      GenArk accession (hgFixed.asmEquivalent), preferring the one with\n";
  printf STDERR "      its files in /gbdb, then a UCSC database, then RefSeq, then the\n";
  printf STDERR "      newest version, then the highest chain coverage\n";
  printf STDERR "writes the clade/order file to stdout, for the -cladeFile= option of:\n";
  printf STDERR "  chainNetCompositeTrackDb.pl and chainNetDescription.pl\n";
  printf STDERR "columns: query clade chainOn netOn, and for information only:\n";
  printf STDERR "  sharedDepth mashDistance chainCoverage scientificName\n";
  exit 255;
}

my $target = shift;
my @queryList = @ARGV;
my %withinName;	# key is lower case taxon name from -within
foreach my $name (split(',', $within)) {
  $name =~ s/^\s+//;
  $name =~ s/\s+$//;
  $withinName{lc($name)} = 1 if (length($name));
}
my %excludeName;	# key is lower case taxon name from -exclude
foreach my $name (split(',', $exclude)) {
  $name =~ s/^\s+//;
  $name =~ s/\s+$//;
  $excludeName{lc($name)} = 1 if (length($name));
}

my $tId = &accessionOrDb($target);
die "'$target' does not look like an assembly name\n" if (! &validName($tId));
my $tRoot = "/hive/data/genomes/$tId/bed";
if ($tId =~ m/^GC/) {
  my $tBuild = &AsmHub::asmBuildDir($tId);
  die "can not find GenArk build directory for target '$target'\n" if (! $tBuild);
  $tRoot = "$tBuild/trackData";
}
die "can not find target build directory '$tRoot'\n" if (! -d "$tRoot");

# single row hgsql query, list form open so nothing goes through a shell
sub hgsqlRow($$) {
  my ($sql, $database) = @_;
  open (my $fh, "-|", "hgsql", "-N", "-e", $sql, $database) or return ();
  my $line = <$fh>;
  close ($fh);
  return () if (! defined($line));
  chomp $line;
  return split('\t', $line);
}

# all rows of an hgsql query, first column only
sub hgsqlColumn($$) {
  my ($sql, $database) = @_;
  open (my $fh, "-|", "hgsql", "-N", "-e", $sql, $database) or return ();
  my @values;
  while (my $line = <$fh>) {
    chomp $line;
    my ($value) = split('\t', $line);
    push @values, $value if (defined($value));
  }
  close ($fh);
  return @values;
}

# a query name from a build directory name is only trusted when it looks
# like an assembly name, e.g. not lastz.panTro6.old
sub validName($) {
  my ($name) = @_;
  return 1 if ($name =~ m/^GC[AF]_[0-9]{9}\.[0-9]+$/);
  return 1 if ($name =~ m/^[A-Za-z][A-Za-z0-9]*$/);
  return 0;
}

# the queries from the build directory names in $tRoot, only this one
# directory is listed
sub discoverQueries() {
  my %seen;
  my @found;
  my @paths = (glob("$tRoot/lastz.*"), glob("$tRoot/blastz.*.swap"),
               glob("$tRoot/miniMap2.*"));
  foreach my $path (sort @paths) {
    next if (! -d "$path");
    my $name = basename($path);
    my $method = ($name =~ m/^miniMap2\./) ? "miniMap2" : "lastz";
    next if ($methodOpt ne "all" && $method ne $methodOpt);
    $name =~ s/^(lastz|blastz|miniMap2)\.//;
    $name =~ s/\.swap$//;
    # an older miniMap2 dateless link has the ucfirst() query name
    $name = lcfirst($name) if ($name !~ m/^GC[AF]_/);
    next if (! &validName($name));
    # lastz.self is the target's self alignment
    next if ($name eq "self" || $name eq $tId || defined($seen{$name}));
    $seen{$name} = 1;
    push @found, $name;
  }
  return @found;
}

if (! scalar(@queryList)) {
  @queryList = &discoverQueries();
  die "no query build directories found in $tRoot\n" if (! scalar(@queryList));
  printf STDERR "# %d queries found in %s\n", scalar(@queryList), $tRoot;
}

##############################################################################
# the alignments, their chain coverage, and the taxId to find for each one
my @entries;	# hash references, one per query with an alignment
my %wantAcc;	# key is accession without version, value is 1
my %wantTaxId;	# key is taxId, value is 1
my %seenQuery;

sub accBase($) {
  my ($acc) = @_;
  $acc =~ s/\.[0-9]+$//;
  return $acc;
}

my ($tTaxId) = ($tId =~ m/^GC/) ? () :
   &hgsqlRow("select taxId from dbDb where name=\"$tId\"", "hgcentraltest");
if ($tId =~ m/^GC/) {
  $wantAcc{&accBase($tId)} = 1;
} elsif (defined($tTaxId) && $tTaxId =~ m/^[1-9][0-9]*$/) {
  $wantTaxId{$tTaxId} = 1;
} else {
  die "can not find the taxId of target '$tId' in hgcentraltest.dbDb\n";
}

foreach my $query (@queryList) {
  my $qId = &accessionOrDb($query);
  next if ($qId eq $tId || defined($seenQuery{$qId}));
  $seenQuery{$qId} = 1;
  if (! &validName($qId)) {
    printf STDERR "# warning: '%s' does not look like an assembly name, skipped\n", $query;
    next;
  }
  my @rows = &findAlignments($tRoot, $tId, $qId, $methodOpt);
  if (! scalar(@rows)) {
    printf STDERR "# warning: no %s alignment found for '%s' in %s\n",
       ($methodOpt eq "all") ? "lastz or miniMap2" : $methodOpt, $query, $tRoot;
    next;
  }
  my $coverage = "";
  my $mm2Mash = "";
  foreach my $row (@rows) {
    $coverage = $row->{'chain'} if (length($row->{'chain'})
          && (! length($coverage) || $row->{'chain'} > $coverage));
    if ($row->{'method'} eq "miniMap2" && defined($row->{'params'})
          && length($row->{'params'}->{'mashDistance'})) {
      $mm2Mash = $row->{'params'}->{'mashDistance'};
    }
  }
  my %entry = ( 'qId' => $qId, 'coverage' => $coverage,
                'mm2Mash' => $mm2Mash, 'taxId' => "" );
  if ($qId =~ m/^GC/) {
    $wantAcc{&accBase($qId)} = 1;
  } else {
    my ($taxId) = &hgsqlRow("select taxId from dbDb where name=\"$qId\"", "hgcentraltest");
    if (defined($taxId) && $taxId =~ m/^[1-9][0-9]*$/) {
      $entry{'taxId'} = $taxId;
      $wantTaxId{$taxId} = 1;
    } else {
      printf STDERR "# warning: can not find the taxId of '%s' in hgcentraltest.dbDb\n", $qId;
    }
  }
  push @entries, \%entry;
}

die "no alignments found for any of the queries in $tRoot\n" if (! scalar(@entries));

##############################################################################
# GenArk accession to taxId, from NCBI's assembly_summary in the genark
# database tables assemblySummary{Genbank,Refseq}, and their ...Historical
# tables for a replaced or suppressed assembly.  The taxonomy files made
# by taxonomyNames.pl have one row per taxId, with the first assembly
# found for it, most accessions are not in them.  The species taxId is
# kept too, taxonomyNames.pl fetches the lineage of both.
my %accTaxId;	# key is accession without version, value is taxId
my %accSpeciesTaxId;	# key is accession without version, value is species taxId
my %accIdentical;	# key is accession, value is the GenBank/RefSeq paired
			# accession when NCBI says the pair is identical

# the exact accession first, then any version of it, the newest one
# returns (taxId, speciesTaxId, identical paired accession or "")
sub asmSummaryTaxIds($) {
  my ($acc) = @_;
  my $table = ($acc =~ m/^GCA_/) ? "assemblySummaryGenbank" : "assemblySummaryRefseq";
  my $base = &accBase($acc);
  foreach my $where ("assemblyAccession=\"$acc\"",
     "assemblyAccession like \"$base.%\" order by assemblyAccession desc limit 1") {
    foreach my $t ($table, "${table}Historical") {
      my ($taxId, $speciesTaxId, $paired, $pairedComp) =
         &hgsqlRow("select taxId,speciesTaxid,gbrsPairedAsm,pairedAsmComp from $t where $where", "genark");
      next if (! defined($speciesTaxId));
      my $identical = "";
      if ($where =~ m/^assemblyAccession=/ && defined($pairedComp)
            && $pairedComp eq "identical" && defined($paired)
            && $paired =~ m/^GC[AF]_[0-9]{9}\.[0-9]+$/) {
        $identical = $paired;
      }
      return ($taxId, $speciesTaxId, $identical);
    }
  }
  return ();
}

foreach my $acc (($tId =~ m/^GC/) ? $tId : (), grep { m/^GC/ } map { $_->{'qId'} } @entries) {
  my ($taxId, $speciesTaxId, $identical) = &asmSummaryTaxIds($acc);
  next if (! defined($speciesTaxId));
  my $base = &accBase($acc);
  $accTaxId{$base} = $taxId;
  $accSpeciesTaxId{$base} = $speciesTaxId;
  $accIdentical{$acc} = $identical if (length($identical));
  $wantTaxId{$taxId} = 1;
  $wantTaxId{$speciesTaxId} = 1;
}

##############################################################################
# collapse the copies of one assembly, unless -keepAll:
#   the versions of one accession: GCA_018852615.1 .2 .3
#   a GenBank/RefSeq pair NCBI says is identical: GCA_028858775.2 GCF_028858775.2
#   a UCSC database and its GenArk accession: hs1 GCA_009914755.4
# one of each set is kept: the one with its chains in /gbdb/<db>/chainNet/
# or as a database table (only a UCSC database target), then a UCSC
# database, then RefSeq, then the newest version, then the highest chain
# coverage.  The copies are grouped with a union-find.
if (! $keepAll && scalar(@entries) > 1) {
  my %gbdbFile;	# key is a file name in /gbdb/<tId>/chainNet/, value is 1
  if ($tId !~ m/^GC/ && opendir(my $dh, "/gbdb/$tId/chainNet")) {
    %gbdbFile = map { $_ => 1 } readdir($dh);
    closedir($dh);
  }
  # 1 when the query's chains can be shown on a UCSC database target
  my $installed = sub {
    my ($qId) = @_;
    return 1 if ($tId =~ m/^GC/);
    my $Q = ucfirst($qId);
    foreach my $name ("chain$Q", "chain$qId", "chainMiniMap2$Q") {
      foreach my $swap ("", "Swap") {
        return 1 if (defined($gbdbFile{"$tId.$name$swap.bb"}));
      }
    }
    if ($qId !~ m/^GC/) {
      my ($table) = &hgsqlRow("show tables like \"chain$Q\"", $tId);
      return 1 if (defined($table));
    }
    return 0;
  };

  my %parent;	# union-find, key is qId, value is its parent qId
  my $findRoot = sub {
    my ($x) = @_;
    while ($parent{$x} ne $x) {
      $parent{$x} = $parent{$parent{$x}};
      $x = $parent{$x};
    }
    return $x;
  };
  my $joinSets = sub {
    my ($x, $y) = @_;
    my ($rx, $ry) = ($findRoot->($x), $findRoot->($y));
    $parent{$rx} = $ry if ($rx ne $ry);
  };
  my %byBase;	# key is accession without version, value is a qId with it
  foreach my $entry (@entries) {
    my $qId = $entry->{'qId'};
    $parent{$qId} = $qId;
    next if ($qId !~ m/^GC/);
    my $base = &accBase($qId);
    if (defined($byBase{$base})) {
      $joinSets->($qId, $byBase{$base});
    } else {
      $byBase{$base} = $qId;
    }
  }
  foreach my $entry (@entries) {
    my $qId = $entry->{'qId'};
    if ($qId =~ m/^GC/) {
      my $paired = $accIdentical{$qId};
      if (defined($paired) && defined($byBase{&accBase($paired)})) {
        $joinSets->($qId, $byBase{&accBase($paired)});
      }
    } else {
      # this database's assembly, every sequence matching exactly
      foreach my $dest (&hgsqlColumn("select destination from asmEquivalent where source=\"$qId\" and sourceAuthority=\"ucsc\" and destinationAuthority in (\"genbank\",\"refseq\") and matchCount=least(sourceCount,destinationCount)", "hgFixed")) {
        next if ($dest !~ m/^(GC[AF]_[0-9]{9})\.[0-9]+/);
        $joinSets->($qId, $byBase{$1}) if (defined($byBase{$1}));
      }
    }
  }

  my %sets;	# key is root qId, value is list of entries
  foreach my $entry (@entries) {
    push @{$sets{$findRoot->($entry->{'qId'})}}, $entry;
  }
  my %keep;	# key is qId, value is 1
  foreach my $root (sort keys %sets) {
    my @set = @{$sets{$root}};
    if (scalar(@set) == 1) {
      $keep{$set[0]->{'qId'}} = 1;
      next;
    }
    foreach my $entry (@set) {
      my $qId = $entry->{'qId'};
      $entry->{'installed'} = $installed->($qId);
      $entry->{'isDb'} = ($qId =~ m/^GC/) ? 0 : 1;
      $entry->{'isRefSeq'} = ($qId =~ m/^GCF_/) ? 1 : 0;
      $entry->{'version'} = ($qId =~ m/\.([0-9]+)$/) ? $1 : 0;
    }
    my @ranked = sort {
        $b->{'installed'} <=> $a->{'installed'}
          || $b->{'isDb'} <=> $a->{'isDb'}
          || $b->{'isRefSeq'} <=> $a->{'isRefSeq'}
          || $b->{'version'} <=> $a->{'version'}
          || (length($b->{'coverage'}) ? $b->{'coverage'} : -1)
                <=> (length($a->{'coverage'}) ? $a->{'coverage'} : -1)
          || $a->{'qId'} cmp $b->{'qId'}
      } @set;
    $keep{$ranked[0]->{'qId'}} = 1;
    printf STDERR "# collapsed: kept %s, left out: %s\n", $ranked[0]->{'qId'},
       join(" ", map { $_->{'qId'} } @ranked[1 .. $#ranked]);
  }
  @entries = grep { defined($keep{$_->{'qId'}}) } @entries;
}

##############################################################################
# taxonomy lineages.  taxonomyNames.pl appends a row when it first sees
# a taxId and never updates it, each lineage is the NCBI taxonomy of the
# day it was fetched (e.g. Holometabola vs. the older Endopterygota),
# even inside one file.  The shared node is found by name, below, not
# by position, so a renamed node elsewhere in a lineage does not matter.
# A lineage is taken from the genbank file, from the refseq file only
# when the genbank file does not have that taxId.
my %genbankLineage;	# key is taxId, value is [ sciName, [ lineage nodes ] ]
my %refseqLineage;	# same, from the refseq file

# one pass through a taxonomy file, keeping only what is wanted, an
# accession still not found in the assembly_summary files (a removed
# assembly) may be in here
sub readTaxonomy($$) {
  my ($file, $lineageStore) = @_;
  open (my $fh, "<", $file) or die "can not read $file\n";
  while (my $line = <$fh>) {
    chomp $line;
    my ($taxId, $acc, $asmName, $sciName, $lineage) = split('\t', $line);
    next if (! defined($lineage));
    my $base = &accBase($acc);
    if (defined($wantAcc{$base}) && ! defined($accTaxId{$base})) {
      $accTaxId{$base} = $taxId;
      $wantTaxId{$taxId} = 1;
    }
    next if (! defined($wantTaxId{$taxId}) || defined($lineageStore->{$taxId}));
    my @nodes = grep { length($_) && $_ ne "cellular organisms" } split(';', $lineage);
    # the lineage stops above the organism itself, the scientific name
    # makes the last node, so two assemblies of the same species share it
    push @nodes, $sciName;
    $lineageStore->{$taxId} = [ $sciName, \@nodes ];
  }
  close ($fh);
}

# refseq first, it is the only place to find the taxId of a GCF accession
&readTaxonomy($refseqTaxonomy, \%refseqLineage);
&readTaxonomy($genbankTaxonomy, \%genbankLineage);

sub lineageOf($) {
  my ($taxId) = @_;
  return undef if (! length($taxId));
  return $genbankLineage{$taxId} if (defined($genbankLineage{$taxId}));
  return $refseqLineage{$taxId};
}

# a GenArk accession: the lineage of its taxId, or of its species taxId
# when taxonomyNames.pl has not fetched that one
sub accLineage($) {
  my ($acc) = @_;
  my $base = &accBase($acc);
  my $lineage = &lineageOf($accTaxId{$base} // "");
  $lineage = &lineageOf($accSpeciesTaxId{$base} // "") if (! defined($lineage));
  return $lineage;
}

$tTaxId = $accTaxId{&accBase($tId)} // "" if ($tId =~ m/^GC/);
my $tLineage = ($tId =~ m/^GC/) ? &accLineage($tId) : &lineageOf($tTaxId);
die "can not find the taxonomy lineage of target '$tId'\n" if (! defined($tLineage));
my @tNodes = @{$tLineage->[1]};
my %tNodeIndex;	# key is a target lineage node name, value is its index
for (my $i = 0; $i < scalar(@tNodes); ++$i) {
  $tNodeIndex{$tNodes[$i]} = $i;
}

##############################################################################
# mash distance from the cached sketches
sub mshFile($) {
  my ($id) = @_;
  if ($id =~ m/^GC[AF]_/) {
    my $dir = &AsmHub::mashSketchDir($id);
    return "" if (! $dir);
    return (-s "$dir/$id.msh") ? "$dir/$id.msh" : "";
  }
  my $msh = "/hive/data/genomes/$id/mashSketch/$id.msh";
  return (-s "$msh") ? $msh : "";
}

my $tMsh = &mshFile($tId);
printf STDERR "# warning: no mash sketch found for target '%s', no mash distances\n", $tId
   if (! length($tMsh));

sub mashDist($) {
  my ($qMsh) = @_;
  return "" if (! length($tMsh) || ! length($qMsh));
  open (my $fh, "-|", "mash", "dist", $tMsh, $qMsh) or return "";
  my $line = <$fh>;
  close ($fh);
  return "" if (! defined($line));
  chomp $line;
  my @fields = split('\t', $line);
  return "" if (! defined($fields[2]) || $fields[2] !~ m/^[0-9.eE+-]+$/);
  return $fields[2];
}

##############################################################################
# clade and sort keys for each query
my @kept;
foreach my $entry (@entries) {
  my $qId = $entry->{'qId'};
  $entry->{'taxId'} = $accTaxId{&accBase($qId)} // "" if ($qId =~ m/^GC/);
  my $lineage = ($qId =~ m/^GC/) ? &accLineage($qId) : &lineageOf($entry->{'taxId'});
  if (defined($lineage)) {
    my @qNodes = @{$lineage->[1]};
    # the deepest query node that is also in the target lineage, found by
    # name, the lineages may come from different taxonomy snapshots
    my $shared = -1;
    for (my $j = scalar(@qNodes) - 1; $j >= 0; --$j) {
      if (defined($tNodeIndex{$qNodes[$j]})) {
        $shared = $j;
        last;
      }
    }
    $entry->{'depth'} = ($shared >= 0) ? $tNodeIndex{$qNodes[$shared]} + 1 : 0;
    $entry->{'sciName'} = $lineage->[0];
    # the query's own node just below the shared node names its clade,
    # the same species has nothing below, the shared node is its name
    if ($shared + 1 < scalar(@qNodes)) {
      $entry->{'clade'} = $qNodes[$shared + 1];
    } else {
      $entry->{'clade'} = $qNodes[-1];
    }
    if (scalar(keys %withinName)) {
      next if (! scalar(grep { defined($withinName{lc($_)}) } @qNodes));
    }
    next if (scalar(grep { defined($excludeName{lc($_)}) } @qNodes));
  } else {
    printf STDERR "# warning: no taxonomy lineage found for '%s'%s\n", $qId,
       length($entry->{'taxId'}) ? " taxId $entry->{'taxId'}" : "";
    next if (scalar(keys %withinName));
    $entry->{'depth'} = -1;
    $entry->{'sciName'} = "";
    $entry->{'clade'} = "unknown";
  }
  $entry->{'clade'} =~ s/[^A-Za-z0-9'_-]+/_/g;
  $entry->{'clade'} =~ s/_+/_/g;
  $entry->{'clade'} =~ s/^_//;
  $entry->{'clade'} =~ s/_$//;
  my $mash = &mashDist(&mshFile($qId));
  $mash = $entry->{'mm2Mash'} if (! length($mash));
  $entry->{'mash'} = $mash;
  # mash distance stops measuring out at around 25% divergence, a
  # saturated or missing distance sorts after the measured ones, by the
  # chain coverage among themselves
  $entry->{'mashKey'} = (length($mash) && $mash < 1) ? $mash : 1;
  $entry->{'covKey'} = length($entry->{'coverage'}) ? $entry->{'coverage'} : -1;
  push @kept, $entry;
}

die "none of the queries are within: $within\n" if (! scalar(@kept));

# in a clade: the smallest mash distance, then the highest chain coverage
sub compareMembers($$) {
  my ($x, $y) = @_;
  return $x->{'mashKey'} <=> $y->{'mashKey'}
      || $y->{'covKey'} <=> $x->{'covKey'}
      || $x->{'qId'} cmp $y->{'qId'};
}

# clades: the deepest shared node first, among clades at the same depth
# the one with the closest member first
my %cladeMembers;	# key is "depth\tclade", value is list of entries
foreach my $entry (@kept) {
  push @{$cladeMembers{"$entry->{'depth'}\t$entry->{'clade'}"}}, $entry;
}
foreach my $key (keys %cladeMembers) {
  @{$cladeMembers{$key}} = sort { &compareMembers($a, $b) } @{$cladeMembers{$key}};
  my $count = scalar(@{$cladeMembers{$key}});
  if ($maxPerClade > 0 && $count > $maxPerClade) {
    my @dropped = splice(@{$cladeMembers{$key}}, $maxPerClade);
    printf STDERR "# -maxPerClade=%d: %s, left out %d: %s\n", $maxPerClade,
       $dropped[0]->{'clade'}, scalar(@dropped),
       join(" ", map { $_->{'qId'} } @dropped);
  }
}
my @cladeOrder = sort {
    my $aBest = $cladeMembers{$a}->[0];
    my $bBest = $cladeMembers{$b}->[0];
    $bBest->{'depth'} <=> $aBest->{'depth'}
      || &compareMembers($aBest, $bBest)
      || $aBest->{'clade'} cmp $bBest->{'clade'};
  } keys %cladeMembers;

##############################################################################
# output
printf "# made by: chainNetCladeOrder.pl%s%s%s%s%s %s%s\n",
   ($methodOpt ne "all") ? " -method=$methodOpt" : "",
   length($within) ? " -within=$within" : "",
   length($exclude) ? " -exclude=$exclude" : "",
   $keepAll ? " -keepAll" : "",
   ($maxPerClade > 0) ? " -maxPerClade=$maxPerClade" : "",
   $tId, scalar(@ARGV) ? " " . join(" ", @ARGV) : "";
printf "# target: %s taxId %s %s\n", $tId, $tTaxId, $tLineage->[0];
printf "# for: chainNetCompositeTrackDb.pl -cladeFile=<this file> and\n";
printf "#      chainNetDescription.pl -cladeFile=<this file>\n";
printf "# they read the first four columns, the others are for information.\n";
printf "# clade: the query's taxonomy node just below the deepest node shared\n";
printf "#   with the target, sharedDepth: the number of shared lineage nodes\n";
printf "#query\tclade\tchainOn\tnetOn\tsharedDepth\tmashDistance\tchainCoverage\tscientificName\n";
my $first = 1;
foreach my $key (@cladeOrder) {
  my $cladeFirst = 1;
  foreach my $entry (@{$cladeMembers{$key}}) {
    printf "%s\t%s\t%s\t%s\t%d\t%s\t%s\t%s\n", $entry->{'qId'}, $entry->{'clade'},
       $first ? "on" : "off", ($first || $cladeFirst) ? "on" : "off",
       $entry->{'depth'},
       length($entry->{'mash'}) ? $entry->{'mash'} : "-",
       length($entry->{'coverage'}) ? $entry->{'coverage'} : "-",
       $entry->{'sciName'};
    $first = 0;
    $cladeFirst = 0;
  }
}
