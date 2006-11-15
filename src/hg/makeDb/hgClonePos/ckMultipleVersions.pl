#!/usr/bin/env perl

#	"$Id: ckMultipleVersions.pl,v 1.2 2006/11/04 00:30:01 hiram Exp $";

use warnings;
use strict;
sub usage() {
    print "usage: ./ckMultipleVersions.pl allClonesConsidered.list\n";
    exit 255;
}
my $argc = scalar(@ARGV);
if ($argc != 1) { usage; }
my $fileName = shift;
open (FH,"<$fileName") or die "Can not open $fileName";
my %cloneAcc;	#	key is clone accession major number, value is version
while (my $clone = <FH>) {
    chomp $clone;
    my ($major, $version) = split('\.', $clone);
    $version =~ s/_.*//;
    if (exists($cloneAcc{$major})) {
	my $previousVersion = $cloneAcc{$major};
	if ($previousVersion > $version) {
	    printf STDERR "$major.$version -> $major.$previousVersion\n";
	} elsif ($previousVersion < $version) {
	    printf STDERR "$major.$previousVersion -> $major.$version\n";
	    $cloneAcc{$major} = $version;
	}
    } else {
	$cloneAcc{$major} = $version;
    }
}
close (FH);
foreach my $major (sort keys %cloneAcc) {
    printf "$major.$cloneAcc{$major}\n";
}
