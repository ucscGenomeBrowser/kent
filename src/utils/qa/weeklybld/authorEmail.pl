#!/usr/bin/env perl

use strict;
use warnings;

my $argc = scalar(@ARGV);

if ($argc != 2) {
  printf STDERR "usage: ./authorEmail.pl authorLogname /path/to/git-reports/.../user/authors.html\n";
  printf STDERR "answer printed to STDOUT is: <authorLogname\@correct.email> Full Name\n";
  exit 255;
}

my $logname = shift;
my $authorsFile = shift;

open (FH, "<$authorsFile") or die "can not read $authorsFile";
my $inAuthors = 0;
while (my $line = <FH>) {
  if ($line =~ m/^<ul>/) {
    $inAuthors = 1;
  } elsif ($line =~ m#^</ul>#) {
    $inAuthors = 0;
  } elsif ($inAuthors) {
    chomp $line;
    my ($name, $email) = split(' &lt;', $line);
    $email =~ s/&gt;.*//;
    my ($login, $domain) = split('@', $email);
    if ($login eq $logname) {
      printf "<%s> %s", $email, $name;
      exit 0;
    }
  }
}
close (FH);
printf "<hiram\@soe.ucsc.edu> Hiram Clawson";

