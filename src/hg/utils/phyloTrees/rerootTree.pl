#!/usr/bin/env perl
# rerootTree.pl - rearrange a phylogenetic tree in order from a given node

#	$Id: rerootTree.pl,v 1.1 2009/12/09 17:05:37 hiram Exp $

use strict;
use warnings;
no warnings 'recursion';

my $argc = scalar(@ARGV);

if ($argc != 2) {
    printf STDERR "rerootTree.pl - rearrange a phylogenetic tree in order from a given node\n";
    printf STDERR "usage:\n    rerootTree.pl <nodeName> <dissect_output>\n";
    printf STDERR "obtain dissect_output from tree-doctor with --dissect option\n";
    printf STDERR "e.g. tree_doctor --dissect --name-ancestors 46way.nh \\\n";
    printf STDERR "        | rerootTree.pl monDom5 stdin\n";
    exit 255;
}

my @nodes;		# index is node number, value is name
my @parent;		# index is node number, value is parent node number
my %nodeNames;		# key is node name, value is node number
my @lchild;		# index is node number, value is node number of lchild
my @rchild;		# index is node number, value is node number of rchild
my %nodesDone;		# during tree printout, to mark nodes done

# recursive routine, given a node number, show its tree
sub outputTree($) {
    my ($id) = @_;
    return if (exists($nodesDone{$id}));
    if ($rchild[$id] > -1) {
	&outputTree($rchild[$id]);
    }
    if ($lchild[$id] > -1) {
	&outputTree($lchild[$id]);
    }
    printf "%s\n", $nodes[$id] if ($id ne $nodes[$id]);
    $nodesDone{$id} = 1;
    &outputTree($parent[$id]) if ($parent[$id] > -1);
    return;
}

my $findName = shift;
my $file = shift;

if ($file =~ m/stdin/) {
    open (FH, "</dev/stdin") or die "can not read stdin";
} else {
    open (FH, "<$file") or die "can not read $file";
}


my $curNode = -1;

while (my $line = <FH>) {
#    print $line;
    chomp $line;
    if ($line =~ m/^Node /) {
	(my $dummy, $curNode) = split('\s+', $line);
	$curNode =~ s/://;
    } elsif ($line =~ m/\s+parent = /) {
	$line =~ s/^\s+//;
	my ($dummy, $equal, $id) = split('\s+', $line);
	$parent[$curNode] = $id;
    } elsif ($line =~ m/\s+lchild = /) {
	$line =~ s/^\s+//;
	my ($dummy, $equal, $id) = split('\s+', $line);
	$lchild[$curNode] = $id;
    } elsif ($line =~ m/\s+rchild = /) {
	$line =~ s/^\s+//;
	my ($dummy, $equal, $id) = split('\s+', $line);
	$rchild[$curNode] = $id;
    } elsif ($line =~ m/\s+name = /) {
	$line =~ s/^\s+//;
	my ($dummy, $equal, $name) = split('\s+', $line);
	$name =~ s/'//g;
	$name = $curNode if (length($name) < 1);
	$nodes[$curNode] = $name;
	$nodeNames{$name} = $curNode;
    }
}
close (FH);

if (exists($nodeNames{$findName})) {
	$curNode = $nodeNames{$findName};
	my $parentNode = $parent[$curNode];
	my $parentName = $nodes[$parentNode];
	printf STDERR "tree from: Node %d, name: %s, parent: %d, parent: %s\n", $curNode, $findName, $parentNode, $parentName;
	&outputTree($curNode);
} else {
    printf STDERR "ERROR: can not find specified name: $findName\n";
    exit 255;
}

__END__

Node 89:
        parent = 81
        lchild = -1
        rchild = -1
        name = 'danRer6'
        dparent = 0.731166

