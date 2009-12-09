#!/usr/bin/env perl
# chainNet.pl - output chainNet.ra definitions in phylogentic order

#	$Id: chainNet.pl,v 1.2 2009/12/09 18:28:01 hiram Exp $

use strict;
use warnings;
use FindBin qw($Bin);
use lib "$Bin";

my $reroot = "$Bin/rerootTree.pl";
my $hgsql = "hgsql";
my $home = $ENV{'HOME'};
my $dissectTree = "$home/kent/src/hg/utils/phyloTrees/46way.dissect.txt";

my $argc = scalar(@ARGV);

if ($argc != 1) {
    printf STDERR "chainNet.pl - output chainNet.ra definitions in phylogenetic order\n";
    printf STDERR "usage:\n    chainNet.pl <db>\n";
    printf STDERR "<db> - an existing UCSC database\n";
    printf STDERR "will be using commands: rerootTree.pl and hgsql\n";
    printf STDERR "and expecting to find \$HOME/kent/src/hg/utils/phyloTrees/46way.dissect.txt\n";
    printf STDERR "using:\n";
    printf STDERR "$reroot\n";
    printf STDERR "$dissectTree\n";
    exit 255;
}

my $db = shift;
my @orderedList;
my $dbCount = 0;
my %priorities;
my $priority = 200.3;

open (FH, "$reroot $db $dissectTree 2> /dev/null|") or die "can not run rerootTree.pl";
while (my $line = <FH>) {
    chomp $line;
    $orderedList[$dbCount++] = $line;
    $line =~ s/[0-9]+$//;
    if (!exists($priorities{$line})) {
	$priorities{$line} = $priority;
	$priority += 10;
    }
}
close (FH);

for (my $i = 0; $i < $dbCount; ++$i) {
    my $stripDb = $orderedList[$i];
    $stripDb =~ s/[0-9]+$//;
#    printf "%02d: %03d %s\n", $i, $priorities{$stripDb}, $orderedList[$i];
}

my @chainTbls;
my $chainCount = 0;
my %orderChainNet;

open (FH, "hgsql -e 'show tables;' $db | grep -v -i 'self' | grep 'chain.*Link'|") or
	die "can not run hgsql 'show tables' on $db";
while (my $tbl = <FH>) {
    chomp $tbl;
    $tbl =~ s/^chain//;
    $tbl =~ s/Link$//;
    $chainTbls[$chainCount++] = $tbl;
    my $track = "$tbl";
    my $stripDb = $tbl;
    $stripDb =~ s/[0-9]+$//;
    $orderChainNet{$track} = $priorities{lcfirst($stripDb)};
}
close (FH);

foreach my $track (sort { $orderChainNet{$a} cmp $orderChainNet{$b} }
	keys %orderChainNet) {
    my $priority = $orderChainNet{$track};
    printf "track chainNet%s override\n", $track;
    printf "priority %s\n\n", $priority;
}
