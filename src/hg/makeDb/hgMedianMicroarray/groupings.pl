#!/usr/bin/env perl

use strict;
use warnings;

my $argc = scalar(@ARGV);

if ($argc != 1) {
    print STDERR "usage: ./groupings.pl chipSet.ra\n";
    print STDERR "e.g.:  ./groupings.pl gnfMOE430v2.ra\n";
    exit 1;
}

# my $table = shift;
my $raFile = shift;

my %expIds;	#	key is name, value is string of expIds

open (FH,"<$raFile") or die "can not read $raFile";
while (my $line = <FH>) {
    chomp $line;
    my ($tissue, $organ, $ids) = split('\t', $line);
    if (exists($expIds{$organ})) {
	$ids =~ s/ /,/g;
	$expIds{$organ} = sprintf("%s,%s", $expIds{$organ}, $ids);
    } else {
	$ids =~ s/ /,/g;
	$expIds{$organ} = $ids;
    }
}
close (FH);

my %groupSizes;
my $groupName = `basename $raFile`;
chomp $groupName;
$groupName =~ s/.ra/TissueMedians/;
printf "\n";
printf "name %s\n", $groupName;
printf "type combine median\n";
printf "description Tissue Type Medians\n";
printf "expIds ";
foreach my $key (sort keys %expIds) {
    printf "%s,", $expIds{$key};
    my $groupCount = $expIds{$key};
    $groupCount =~ s/[0-9]//g;
    $groupSizes{$key} = 1 + length($groupCount);
}
printf "\n";
printf "groupSizes ";
foreach my $key (sort keys %expIds) {
    printf "%d,", $groupSizes{$key};
}
printf "\n";
printf "names ";
foreach my $key (sort keys %expIds) {
    printf "%s,", $key;
}
printf "\n";

exit 0

__END__
'3T3-L1'	fat	0 1
'B-cells marginal zone'	immune	12 13
'Baf3'	immune	10 11
my $expIds = "";
my $names;
my $groupSizes;

open (FH,"hgsql -N -e \"select id,name from $table order by id;\" hgFixed|") or
	die "can not select from hgFixed.$table";
while (my $line = <FH>) {
    chomp $line;
    my ($id, $name) = split('\t', $line);
#    print STDERR "$id\t$name\n";
    if (length($expIds) > 0) {
	$expIds = sprintf("%s%s,", $expIds, $id);
	$groupSizes = sprintf("%s1,", $groupSizes);
	$names = sprintf("%s%s,", $names, $name);
    } else {
	$expIds = sprintf("%s,", $id);
	$groupSizes = "1,";
	$names = sprintf("%s,", $name);
    }
}
close (FH);

my $groupName = $table;
$groupName =~ s/MedianExps/All/;
printf "name %s\n", $groupName;
printf "type all\n";
printf "description All Tissues\n";
printf "expIds %s\n", $expIds;
printf "groupSizes %s\n", $groupSizes;
printf "names %s\n", $names;

my %expIds;	#	key is name, value is string of expIds

open (FH,"<$raFile") or die "can not read $raFile";
while (my $line = <FH>) {
    chomp $line;
    my ($tissue, $organ, $ids) = split('\t', $line);
    if (exists($expIds{$organ})) {
#	$ids =~ s/ /,/g;
	$ids =~ s/ .*//;
	$expIds{$organ} = sprintf("%s,%s", $expIds{$organ}, $ids);
    } else {
#	$ids =~ s/ /,/g;
	$ids =~ s/ .*//;
	$expIds{$organ} = $ids;
    }
}
close (FH);

my %groupSizes;
$groupName =~ s/All/TissueMedians/;
printf "\n";
printf "name %s\n", $groupName;
printf "type combine median\n";
printf "description Tissue Type Medians\n";
printf "expIds ";
foreach my $key (sort keys %expIds) {
    printf "%s,", $expIds{$key};
    my $groupCount = $expIds{$key};
    $groupCount =~ s/[0-9]//g;
    $groupSizes{$key} = 1 + length($groupCount);
}
printf "\n";
printf "groupSizes ";
foreach my $key (sort keys %expIds) {
    printf "%d,", $groupSizes{$key};
}
printf "\n";
printf "names ";
foreach my $key (sort keys %expIds) {
    printf "%s,", $key;
}
printf "\n";

__END__
'3T3-L1'	fat	0 1
'B-cells marginal zone'	immune	12 13
'Baf3'	immune	10 11
