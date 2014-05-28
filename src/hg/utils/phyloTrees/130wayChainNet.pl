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
# This mechanism remains in place, but new assemblies are now automatically
#  interpolated.
my $newAssemblies = "nomLeu hetGla susScr oviAri ailMel sarHar melGal ailMis croPor latCha oreNil chrPic gadMor saiBol";
my $newAssemblyOffset = 5;
my $offsetNway = 10;
my $reroot = "$Bin/rerootTree.pl";
my $hgsql = "hgsql";
my $home = $ENV{'HOME'};
my $dissectTree = "$home/kent/src/hg/utils/phyloTrees/130way.dissect.txt";


# existingDbs - this is a record of databases that were taken care of in the
# previous schemes.  With the many new assemblies rolling in, it wasn't feasable
# to fit them in with the 'newAssemblies' mechanism above.  What happens
# in this new scheme will keep the previous handled assemblies in the
# positions they were in before, and any new assemblies will be interpolated
# in between these existing ones.  This is not a perfect scheme as newly
# arriving assemblies will push around these other new assemblies, but at least
# it will leave the existing benchmarks in place with shuffling happening
# only between benchmarks.

my %existingDbs = (
'hg' => 1,
'panTro' => 1,
'gorGor' => 1,
'ponAbe' => 1,
'nomLeu' => 1,
'papHam' => 1,
'rheMac' => 1,
'saiBol' => 1,
'calJac' => 1,
'tarSyr' => 1,
'otoGar' => 1,
'micMur' => 1,
'tupBel' => 1,
'ochPri' => 1,
'oryCun' => 1,
'cavPor' => 1,
'speTri' => 1,
'dipOrd' => 1,
'rn' => 1,
'mm' => 1,
'sorAra' => 1,
'eriEur' => 1,
'pteVam' => 1,
'myoLuc' => 1,
'ailMel' => 1,
'canFam' => 1,
'felCat' => 1,
'equCab' => 1,
'bosTau' => 1,
'oviAri' => 1,
'turTru' => 1,
'vicPac' => 1,
'susScr' => 1,
'choHof' => 1,
'dasNov' => 1,
'echTel' => 1,
'proCap' => 1,
'loxAfr' => 1,
'macEug' => 1,
'monDom' => 1,
'ornAna' => 1,
'anoCar' => 1,
'taeGut' => 1,
'galGal' => 1,
'chrPic' => 1,
'melGal' => 1,
'xenTro' => 1,
'danRer' => 1,
'oryLat' => 1,
'gasAcu' => 1,
'fr' => 1,
'tetNig' => 1,
'petMar' => 1,
);

my $argc = scalar(@ARGV);

if ($argc != 1) {
    printf STDERR "chainNet.pl - output chainNet.ra definitions in phylogenetic order\n";
    printf STDERR "usage:\n    chainNet.pl <db>\n";
    printf STDERR "<db> - an existing UCSC database\n";
    printf STDERR "will be using commands: rerootTree.pl and hgsql\n";
    printf STDERR "therefore expecting to find:\n";
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

my @needsPriority;   # value is db name waiting for priority assignment
my $prevExistingPriority = 0;

# reroot the tree to that database name
# This business redesigned with the advent of the 130way phylo tree to
# accomodate the lot of new assemblies
open (FH, "$reroot $actualTreeName $dissectTree 2> /dev/null|") or die "can not run rerootTree.pl";
while (my $line = <FH>) {
    chomp $line;
    $line =~ s/[0-9]+$//;
    my $checkDb = lcfirst($line);
    if (exists($existingDbs{$checkDb})) {
        if (! exists($priorities{$checkDb})) {
            my $prioAssigned = $priority;
            if ($newAssemblies =~ m/$line/) {
                $priority -= $newAssemblyOffset;
                $priorities{$checkDb} = $priority;
                $prioAssigned = $priority;
                $priority += $newAssemblyOffset;
            } else {
                $priorities{$checkDb} = $priority;
                $priority += $offsetNway;
            }
            if (scalar(@needsPriority) > 0) {
                my $gapSize = $prioAssigned - $prevExistingPriority;
                my $interleave = $gapSize / (1 + scalar(@needsPriority));
                my $newPrio = sprintf("%.2f",$prevExistingPriority+$interleave);
                while (my $db = shift(@needsPriority)) {
                    $priorities{$db} = $newPrio;
                    $newPrio = sprintf("%.2f", $newPrio + $interleave);
                }
            }
            if (scalar(@needsPriority) == 0) {
               $prevExistingPriority = $prioAssigned;
            }
        }
    } else {
        push(@needsPriority, $checkDb);
        if ($prevExistingPriority < 1) {
            $prevExistingPriority = $priority;
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
