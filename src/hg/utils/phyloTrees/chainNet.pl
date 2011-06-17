#!/usr/bin/env perl
# chainNet.pl - output chainNet.ra definitions in phylogentic order

#	$Id: chainNet.pl,v 1.5 2010/04/01 17:04:12 hiram Exp $

use strict;
use warnings;
use FindBin qw($Bin);
use lib "$Bin";

# new genomes since the 46-way construction.  This will keep
#	the priority numbers the same for the previous 46 and place
#	these new ones in between the previous ones.
my $newAssemblies = "melGal1 ailMel susScr oviAri";
my $newAssemblyOffset = 5;
my $offsetNway = 10;
my $reroot = "$Bin/rerootTree.pl";
my $hgsql = "hgsql";
my $home = $ENV{'HOME'};
my $dissectTree = "$home/kent/src/hg/utils/phyloTrees/50way.dissect.txt";

my $argc = scalar(@ARGV);

if ($argc != 1) {
    printf STDERR "chainNet.pl - output chainNet.ra definitions in phylogenetic order\n";
    printf STDERR "usage:\n    chainNet.pl <db>\n";
    printf STDERR "<db> - an existing UCSC database\n";
    printf STDERR "will be using commands: rerootTree.pl and hgsql\n";
    printf STDERR "and expecting to find \$HOME/kent/src/hg/utils/phyloTrees/50way.dissect.txt\n";
    printf STDERR "using:\n";
    printf STDERR "$reroot\n";
    printf STDERR "$dissectTree\n";
    exit 255;
}

my $initialPrio = 200.3;
my $db = shift;
my $dbCount = 0;
my %priorities;
my $priority = $initialPrio;
my $stripName = $db;
$stripName =~ s/[0-9]+$//;

my $actualTreeName = "notFound";

# given an arbitrary database name, find corresponding database name
# that is actually in the phylo tree
open (FH, "grep name $dissectTree | sort -u | sed -e \"s/.*name = '//; s/'.*//;\"|") or die "can not read $dissectTree";
while (my $name = <FH>) {
    chomp $name;
    my $treeName = $name;
    $name =~ s/[0-9]+$//;
    if ($name eq $stripName) {
	$actualTreeName = $treeName;
	last;
    }
}

# verify we found one
if ($actualTreeName eq "notFound") {
    printf "ERROR: specified db '$db' is not like one of the databases in the phylo tree\n";
    printf `grep name $dissectTree | sort -u | sed -e "s/.*name = '//; s/'.*//;" | xargs echo`, "\n";
    exit 255;
}

# reroot the tree to that database name
open (FH, "$reroot $actualTreeName $dissectTree 2> /dev/null|") or die "can not run rerootTree.pl";
while (my $line = <FH>) {
    chomp $line;
    $line =~ s/[0-9]+$//;
    if (!exists($priorities{lcfirst($line)})) {
	if ($newAssemblies =~ m/$line/) {
	    $priority -= $newAssemblyOffset;
	    $priorities{lcfirst($line)} = $priority;
	    $priority += $newAssemblyOffset;
	} else {
	    $priorities{lcfirst($line)} = $priority;
	    $priority += $offsetNway;
        }
    }
}
close (FH);

my $chainCount = 0;
my %orderChainNet;

# fetch all chain tables from the given database and for ones that are
# in the phylo tree, output their priority
open (FH, "hgsql -e 'show tables;' $db | grep 'chain.*Link' | egrep -i -v 'self|chainNet' | sed -e 's/^.*_chain/chain/' | sort -u|") or
	die "can not run hgsql 'show tables' on $db";
while (my $tbl = <FH>) {
    chomp $tbl;
    $tbl =~ s/^chain//;
    $tbl =~ s/Link$//;
    my $track = $tbl;
    my $stripDb = $tbl;
    $stripDb =~ s/[0-9]+$//;
    $stripDb =~ s/V17e$//;
    $stripDb =~ s/Poodle$//;
    if (defined($stripDb) && length($stripDb) > 0) {
	if (exists($priorities{lcfirst($stripDb)})) {
	    my $priority = $priorities{lcfirst($stripDb)};
	    $orderChainNet{$track} = $priority;
	} else {
	    printf STDERR "warning: not in phylo tree: $tbl ($stripDb)\n";
	}
    }
}
close (FH);

# print out the priorities in order by priority, lowest first
foreach my $track (sort { $orderChainNet{$a} <=> $orderChainNet{$b} }
	keys %orderChainNet) {
    my $priority = $orderChainNet{$track};
    printf "track chainNet%s override\n", $track;
    printf "priority %s\n\n", $priority;
}
