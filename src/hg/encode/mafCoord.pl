#!/usr/local/bin/perl

# Strip empty line after comment (rejected by maf_project)
# and map ENCODE region coordinates to assembly coords

$#ARGV == 3 or die "usage: mafCoord <org> <db.chr> <chromStart> <chromSize>\n";

my $org = shift @ARGV;
my $dbChr = shift @ARGV;
my $start = shift @ARGV;
my $size = shift @ARGV;
my $comment = 0;

while (<>) {
    if (/^#/) {
        print;
        $comment = 1;
    } elsif (/^$/ and $comment) {
        next;
    } elsif (/^s\s+$org\s+([0-9]+)(\s+[0-9]+\s+[+-])\s+[0-9]+(.*)/) {
        printf ("s%11s%11s%11s%11s%11s%s\n", $dbChr, $1 + $start, $2, $size, $3);
        $comment = 0;
    } else {
        print;
        $comment = 0;
    }
}
