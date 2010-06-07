#!/usr/bin/perl 

# addUclaAnnotations.pl - Quick script to add annotations to the
# expRecord file produced by affyUclaMergePslData program. Once Allen
# Day stabalizes on a format this should be added to the program
# itself, but for now things change every release...

if(@ARGV == 0) {
    print STDERR "addUclaAnnotations.pl - Quick script to add annotations to the\n";
    print STDERR "expRecord file produced by affyUclaMergePslData program. Once Allen\n";
    print STDERR "Day stabalizes on a format this should be added to the program\n";
    print STDERR "itself, but for now things change every release.\n";
    print STDERR "usage:\n   ";
    print STDERR "addUclaAnnotations.pl hg16.affyUcla.expRecords normal_tissue_database_annotations2.txt > new.ExpRecord.tab\n";
    exit(1);
}

$epFile = shift(@ARGV);
$annFile = shift(@ARGV);
open(IN, "$epFile") or die "Can't open $epFile to read.\n";

# Read in exp records, hashing by chip id.
while(<IN>) {
    chomp;
    $line = $_;
    @p = split /\t/,$line;
    $h{$p[1]} = $line;
}
close(IN);

open(IN, "$annFile") or die "Can't open $annFile to read.\n";
while(<IN>) {
    chomp;
    if($_ =~ /^\#/) {
	next;
    }
    # p = pieces
    $tmp = $_;
    @p = split /\t/,$_;
    # n = name
    @n = split / /, $p[1];
    # a = array
    @a = split /-/, $n[0];
    $line = $h{$a[0]};
    if($line eq "") {
	print STDERR "Can't find array for $a[0] in line:\n$tmp\n";
    }
    @l = split /\t/,$line;
    $l[1] = $p[2];
    $l[2] = $p[1];
    $l[6] = 4;
    $l[7] = $l[7] . $a[1] . ",";
    $l[7] = $l[7] . $p[2] . ",";
    $l[7] = $l[7] . $p[3] . ",";
    $j = join "\t", @l;
    print "$j\n";
}

    
       
