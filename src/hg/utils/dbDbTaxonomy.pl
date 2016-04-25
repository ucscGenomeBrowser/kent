#!/usr/bin/env perl

# This is under source control -- make sure you are editing
# kent/src/hg/utils/dbDbTaxonomy.pl

# This script uses the NCBI Taxonomy database to make a phylogenetic tree that spans
# all genome browser species.  It relies on hgcentral.dbDb.taxId to be correct and
# up-to-date.  If one value of dbDb.genome is associated with multiple values of dbDb.taxId,
# then the tree will contain duplicated labels -- consider editing the dbDb-derived input file
# to avoid that if necessary.
# This prints out javascript (not JSON) for use by the new gateway page (hgChooseDb).

use warnings;
use strict;

# Global because it's used in a sort sub:
use vars qw/$refTree/;

sub usage($) {
  my ($exitCode) = @_;
  print STDERR "
usage: $0 taxIdGenome.txt nodes.dmp names.dmp

Extracts taxonomic tree for species in dbDb from NCBI Taxonomy database,
with a node ordering that favors human, mouse and other model organisms.
Prints out Javascript representation (var dbDbTree = ...) to include using
a <src> tag in HTML.

";
  if (defined $exitCode) {
    exit $exitCode;
  }
}

# Process command line args
if (scalar(@ARGV) != 3) {
  usage(1);
}
my ($taxIdFile, $nodeFile, $nameFile) = @ARGV;

# Util functions
my $verboseLevel = 1;
sub verbose($$) {
  my ($level, $text) = @_;
  if ($level <= $verboseLevel) {
    print STDERR $text;
  }
}

sub listRefContains($$) {
  my ($listRef, $target) = @_;
  # Return TRUE (1) if list contains target string.
  # It's possible, but butt-ugly, to do this with scalar(grep ...)
  foreach my $item (@{$listRef}) {
    if ($item eq $target) {
      return 1;
    }
  }
  return 0;
}

sub trimSpaces($) {
  # Trim whitespace at beginning and end.
  my ($text) = @_;
  $text =~ s/^\s*//;
  $text =~ s/\s*$//;
  return $text;
}

# Read taxIdGenome.txt
my @dbDbTaxIds;
my @dbDbGenomes;
my $dbDbCount = 0;
open(my $T, $taxIdFile) || die "Couldn't open '$taxIdFile': $!";
while (<$T>) {
  /^(\d+)\s+(\w.*)/ || die "Expected two columns, taxId and name, got this:\n$_\n\t";
  push @dbDbTaxIds, $1;
  push @dbDbGenomes, $2;
  $dbDbCount++;
}
close($T);

# Read nodes.dmp
verbose(1, "Reading $nodeFile...\n");
my @taxIdParents;
open(my $NODES, $nodeFile) || die "Couldn't open '$nodeFile': $!";
while (<$NODES>) {
  /^(\d+)\s*\|\s*(\d+)\s*\|\s*([\w ]+)\s*\|/
    || die "Expected |-separated taxId, parentTaxId, rank, got this:\n$_\n\t";
  my ($id, $parentId, $rank) = ($1, $2, $3);
  $taxIdParents[$id] = ($parentId + 0);
}
close ($NODES);

# Tree of nodes like {taxId, parentNodeId, childNodeIds[] }
# (other fields e.g. name and priority may be added to nodes after construction)
sub treeNew() {
  my %tree = ( nodes => [],
               nodeCount => 0,
               taxIdToNode => {},
             );
  return \%tree;
}

sub treeNewNode($$) {
  # Add a new node to the tree; parent and children are not yet hooked up.
  my ($refTree, $taxId) = @_;
  my $node = { taxId => $taxId,
               childIds => []
             };
  my $id = $refTree->{nodeCount}++;
  $refTree->{nodes}->[$id] = $node;
  $refTree->{taxIdToNode}->{$taxId} = $id;
  return $id;
}

sub treeIdFromTaxId($$) {
  my ($refTree, $taxId) = @_;
  return $refTree->{taxIdToNode}->{$taxId};
}

sub treeHasTaxId($$) {
  my ($refTree, $taxId) = @_;
  return (defined treeIdFromTaxId($refTree, $taxId));
}

sub treeGetNode($$) {
  my ($refTree, $id) = @_;
  return $refTree->{nodes}->[$id];
}

sub treeSetParent($$$) {
  my ($refTree, $id, $parentId) = @_;
  my $node = treeGetNode($refTree, $id);
  if (defined $node->{parentId}) {
    die "treeSetParent: node$id already has parent " . $node->{parentId} .
      ", now trying to set it to $parentId";
  }
  $node->{parentId} = $parentId;
}

sub treeAddChild($$$) {
  my ($refTree, $id, $childId,) = @_;
  my $node = treeGetNode($refTree, $id);
  push @{$node->{childIds}}, $childId;
}

sub treeAddNode($$$) {
  # Add nodes for taxId and parentTaxId to tree if they're not already there and
  # hook up taxId's node as a child of parentTaxId's node.
  my ($refTree, $taxId, $parentTaxId) = @_;
  my $id = treeIdFromTaxId($refTree, $taxId);
  if (! defined $id) {
    $id = treeNewNode($refTree, $taxId);
  }
  my $parentId = treeIdFromTaxId($refTree, $parentTaxId);
  # Create parent if not already in the tree so we can hook up parent and child
  if (! defined $parentId) {
    $parentId = treeNewNode($refTree, $parentTaxId);
  }
  treeSetParent($refTree, $id, $parentId);
  treeAddChild($refTree, $parentId, $id);
}

sub treeNodeHasChildByTaxId($$$) {
  my ($refTree, $taxId, $childTaxId) = @_;
  my $id = treeIdFromTaxId($refTree, $taxId);
  return 0 if (! defined $id);
  my $childId = treeIdFromTaxId($refTree, $childTaxId);
  return 0 if (! defined $childId);
  my $node = treeGetNode($refTree, $id);
  return listRefContains($node->{childIds}, $childId);
}

# Recursive function to construct tree by tracing ancestors of dbDb taxIds
sub traceback($$$);
sub traceback($$$) {
  my ($taxId, $refTaxIdParents, $refTree) = @_;
  my $parentTaxId = $refTaxIdParents->[$taxId];
  if ($parentTaxId && $parentTaxId != $taxId) {
    my $parentTraced = treeHasTaxId($refTree, $parentTaxId);
    my $selfTraced = $parentTraced && treeNodeHasChildByTaxId($refTree, $parentTaxId, $taxId);
    if (! $selfTraced) {
      treeAddNode($refTree, $taxId, $parentTaxId);
      if (! $parentTraced) {
        traceback($parentTaxId, $refTaxIdParents, $refTree);
      }
    }
  }
}

# Extract the tree for only dbDb genomes (and a list of Ancestors and other hgwdev stuff...)
verbose(1, "Tracing back from $taxIdFile...\n");
my $refTree = treeNew();
my @others;
my $lastTaxId = -1;
for (my $i = 0;  $i < $dbDbCount;  $i++) {
  my $taxId = $dbDbTaxIds[$i];
  if ($taxId > 1 && $taxId != $lastTaxId) {
    traceback($taxId, \@taxIdParents, $refTree);
  } else {
    push @others, $taxId;
  }
  $lastTaxId = $taxId;
}

# Add genome labels to tree for nodes with dbDb taxIds.
$lastTaxId = -1;
for (my $i = 0;  $i < $dbDbCount;  $i++) {
  my $taxId = $dbDbTaxIds[$i];
  my $genome = $dbDbGenomes[$i];
  if ($taxId > 1 && $taxId != $lastTaxId) {
    my $id = treeIdFromTaxId($refTree, $taxId);
    my $node = treeGetNode($refTree, $id);
    $node->{name} = $genome;
  }
  $lastTaxId = $taxId;
}

# Read names.dmp, adding scientific names to nodes.  If node already has a name, add {sciName}.
verbose(1, "Reading $nameFile...\n");
open(my $NAMES, $nameFile) || die "Couldn't open '$nameFile': $!";
while (<$NAMES>) {
  next unless (index($_, 'scientific name') > 0);
  my ($taxId, $name, undef, $type) = split(/\s*\|\s*/);
  if ($type eq 'scientific name') {
    my $id = treeIdFromTaxId($refTree, $taxId);
    if (defined $id) {
      my $node = treeGetNode($refTree, $id);
      if (! $node->{name}) {
        $node->{name} = $name;
      } else {
        $node->{sciName} = $name;
      }
    }
  }
}
close($NAMES);

# Add priority number to each node of tree based on lineages that we would like to appear
# before others.
my @favoredSpecies = ( 9606,  # Human
                       9598,  # Chimp
                       10090, # Mouse
                       9365,  # Hedgehog
                       9823,  # Pig
                       9913,  # Cow
                       9733,  # Orca
                       9796,  # Horse
                       9615,  # Dog
                       9708,  # Walrus
                       132908,# Bat (Megabat)
                       9785,  # Elephant
                       127582,# Manatee
                       9361,  # Armadillo
                       9315,  # Kangaroo
                       9031,  # Bird (chicken)
                       28377, # Lizard
                       8364,  # Frog (tropicalis)
                       7955,  # Fish (zebrafish)
                       7227,  # D. melanogaster
                       6239,  # C. elegans
                       4932,  # S. cerevisiae
                       186538 # Ebola
                     );

my $firstTime = 1;

sub markPriorities($$$);
sub markPriorities($$$) {
  # If node is one of the favored species, give it that species' priority value.
  # Otherwise, set the node to the minimum of its children's priorities.
  # If the node is not favored and has no children, give it a high number (lowest prio).
  my ($refTree, $refFaves, $id) = @_;
  my $node = treeGetNode($refTree, $id);
  my $faveCount = scalar(@{$refFaves});
  $node->{priority} = $faveCount;
  my @childIds = @{$node->{childIds}};
  foreach my $childId (@childIds) {
    my $childPrio = markPriorities($refTree, $refFaves, $childId);
    if ($childPrio < $node->{priority}) {
      $node->{priority} = $childPrio;
    }
  }
  for (my $i = 0;  $i < $faveCount;  $i++) {
    if ($node->{taxId} == $refFaves->[$i]) {
      $node->{priority} = $i;
      last;
    }
  }
  return $node->{priority};
}

my $rootId = treeIdFromTaxId($refTree, 1);

verbose(1, "Marking favored lineages in tree...\n");
markPriorities($refTree, \@favoredSpecies, $rootId);

sub linAlphCmp() {
  # Compare two ids by lineage favoritism, falling back on alphabetical order.
  # For use by sort, which provides global $a and $b.
  my $nodeA = treeGetNode($refTree, $a);
  my $nodeB = treeGetNode($refTree, $b);
  my $dif = $nodeA->{priority} <=> $nodeB->{priority};
  if ($dif == 0) {
    $dif = $nodeA->{name} cmp $nodeB->{name};
  }
  return $dif;
}

sub indent($) {
  # Return a string with two spaces per level.
  my ($level) = @_;
  my $indentLength = 2 * $level;
  my $indent = '';
  for (my $i = 0;  $i < $indentLength;  $i++) {
    $indent .= ' ';
  }
  return $indent;
}

sub dumpJs($$$);
sub dumpJs($$$) {
  # Recursively descend the tree, printing a Javascript literal encoded as arrays of arrays
  # (could be JSON if newlines are omitted, e.g. $jsIndent = '').
  my ($refTree, $id, $level) = @_;
  my $node = treeGetNode($refTree, $id);
  my @kids = @{$node->{childIds}};
  my $name = $node->{name};
  my $sciName;
  if (exists $node->{sciName}) {
    $sciName = '"' . $node->{sciName} . '"';
  } else {
    $sciName = "null";
  }
  my $jsIndent = "\n" . indent($level);
  print "[\"$name\", $node->{taxId}, $sciName";
  my $isFirst = 1;
  foreach my $kid (sort linAlphCmp @kids) {
    if ($isFirst) {
      print ",$jsIndent\[ ";
      $isFirst = 0;
    } else {
      print ",$jsIndent";
    }
    dumpJs($refTree, $kid, $level+1);
  }
  if (! $isFirst) {
    print " ]";
  }
  print " ]";
}

verbose(1, "Printing out tree as JS...\n");
# Add a config comment for jshint so it doesn't complain about long lines and
# the apparent unused-ness of dbDbTree:
print "/* jshint maxlen: false, unused: false */\n";
print "var dbDbTree = ";
dumpJs($refTree, $rootId, 0);
print ";\n";
