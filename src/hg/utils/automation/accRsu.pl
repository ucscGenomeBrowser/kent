#!/usr/bin/env perl

# process output raFile from gbProcess to link the 'acc' identifiers
# with their corresponding 'rsu' description records, output is two column
# tab seperated

use strict;
use warnings;

my $argc = scalar(@ARGV);

if ($argc != 1) {
  printf STDERR "usage: ./accRsu.pl raFile > acc.rsu.tab\n";
  printf STDERR "where raFile is the output of gbProcess\n";
  exit 255;
}

my $file = shift;

if ($file =~ m/stdin/) {
  open (FH, "</dev/stdin") or die "can not read stdin";
} elsif ($file =~ m/.gz$/) {
  open (FH, "zcat $file|") or die "can not zcat $file";
} else {
  open (FH, "<$file") or die "can not read $file";
}

my $acc = "";
my $rsu = "";
my $def = "";

while (my $line = <FH>) {
  chomp $line;
  if ($line =~ m/^acc /) {
    if (length($rsu)) {
      printf "%s\t%s\n", $acc, $rsu;
    } elsif (length($def)) {
      printf "%s\t%s\n", $acc, $def;
    } elsif (length($acc)) {
      printf STDERR "# no rsu or def for %s\n", $acc
    }
    $def = "";
    $rsu = "";
    $acc = $line;
    $acc =~ s/^acc //;
  } elsif ($line =~ m/^rsu/) {
    $rsu = $line;
    $rsu =~ s/^rsu //;
  } elsif ($line =~ m/^def/) {
    $def = $line;
    $def =~ s/^def //;
  }
}
close (FH);

if (length($rsu)) {
  printf "%s\t%s\n", $acc, $rsu;
} elsif (length($def)) {
  printf "%s\t%s\n", $acc, $def;
} elsif (length($acc)) {
  printf STDERR "# no rsu or def for %s\n", $acc
}
