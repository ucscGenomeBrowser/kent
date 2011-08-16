#!/usr/bin/env perl

use strict;
use warnings;
use Getopt::Long;

my $field1 = "transId";
my $field2 = "geneName";

my $ok = GetOptions( 'field1=s' => \$field1, 'field2=s' => \$field2);

my $argc = scalar(@ARGV);

if (! $ok || ($argc != 1)) {
    printf STDERR "usage: ensemblInfo.pl [options] infoOut.txt > ensemblToGeneName.tab\n";
    printf STDERR "scans the infoOut.txt file for the columns transId and geneName\n";
    printf STDERR "output is two column tsv: transId geneName\n";
    printf STDERR "to be loaded into the table ensemblToGeneName\n";
    printf STDERR "options:\n";
    printf STDERR "-field1=<firstField> - default \"transId\"\n";
    printf STDERR "-field2=<secondField> - default \"geneName\"\n";
    printf STDERR "e.g.: ensemblInfo.pl -field2=source infoOut.txt > ensemblSource.tab\n";
    exit 255;
}

my $file = shift;

open (FH, "<$file") or die "can not read $file";
# expecting the first line to be the header
my $header = <FH>;
chomp $header;
$header =~ s/^#//;
my @a = split('\s+', $header);
my $field1Ix = -1;
my $field2Ix = -1;
for (my $i = 0; $i < scalar(@a); ++$i) {
    if ($a[$i] =~ m/^${field1}$/) {
	$field1Ix = $i;
    } elsif ($a[$i] =~ m/^${field2}$/) {
	$field2Ix = $i;
    }
}
if ($field1Ix < 0) {
    die "ERROR: can not find $field1 column in header line";
}

if ($field2Ix < 0) {
    die "ERROR: can not find $field2 column in header line";
}
my $maxCol = $field1Ix > $field2Ix ? $field1Ix : $field2Ix;

# printf STDERR "${field1} at: $field1Ix, ${field2} at: $field2Ix\n";

while (my $line = <FH>) {
    chomp $line;
    @a = split('\t', $line);
    my $colCount = scalar(@a);
    if ($colCount > $maxCol && length($a[$field1Ix]) > 0 && length($a[$field2Ix]) > 0) {
       printf "%s\t%s\n", $a[$field1Ix], $a[$field2Ix]; 
    }
}
close(FH);
