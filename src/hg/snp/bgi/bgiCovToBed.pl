#!/usr/bin/perl -w

# get chrom and strain from filename; get start, end from file contents;
# dump bed 4.

use strict;

foreach my $fname (@ARGV) {
  if ($fname =~ m@/(Broiler|Layer|Silkie)/.*/ContigCov-(chr\w+).txt(.*)$@) {
    my ($strain, $chr, $gz) = ($1, $2, $3);
    if ($gz ne "") {
      open(IN, "gunzip -c $fname|") || die "Can't gunzip $fname: $!";
    } else {
      open(IN, "$fname") || die "Can't read $fname: $!";
    }
    while (<IN>) {
      if (/^(\d+)\s+(\d+)\s*$/) {
	print "$chr\t" . ($1 - 1) . "\t$2\t$strain.$chr.$1.$2\n";
      }
    }
    close(IN);
  } else {
    die "Can't get strain and chrom from this filename: $fname\n";
  }
}
