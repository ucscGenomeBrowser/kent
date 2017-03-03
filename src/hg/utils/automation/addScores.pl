#!/usr/bin/env perl

use strict;
use warnings;

my $argc = scalar(@ARGV);

if ($argc != 1) {
  printf STDERR "usage: addScores.pl db > trackDb.chainNet.ra\n";
  printf STDERR "\nwill output stanzas for each chainNet track on this db/assembly\n";
  printf STDERR "with priority override to get them in phylogenetic order\n";
  printf STDERR "and chain/net/matrix score parameters\n";
  printf STDERR "add: 'include trackDb.chainNet.ra' to the trackDb.ra file\n";
  printf STDERR "to get these included\n";
  exit 255;
}

my $db = "";
my $dbTarget = shift;
open (FH, "chainNet.pl $dbTarget 2> /dev/null|") or die "can not run chainNet.pl $dbTarget";
while (my $line = <FH>) {
  chomp $line;
  printf "%s\n", $line;
  if ($line =~ m/^track chainNet/) {
     my $Db = $line;
     $Db =~ s/.*chainNet//;
     $Db =~ s/ override//;
     $db = lcfirst($Db);
#     printf "%s\t%s\t%s\n", $dbTarget, $db, $Db;
  } elsif ($line =~ m/^priority/) {
     my $isActive = `hgsql -h genome-centdb -e 'select * from dbDb where name="$db" AND active=1;' hgcentral | wc -l`;
     chomp $isActive;
     if ($isActive < 2) {
        printf "release alpha\n";
     }
     open (SC, "findScores.pl $dbTarget $db 2> /dev/null|sort|") or die "can not run findScores.pl $dbTarget $db";
     while (my $scores = <SC>) {
       chomp $scores;
       if ($scores =~ m/^matrix/) {
         printf "%s\n", $scores;
       } elsif ($scores =~ m/^chain/) {
         printf "%s\n", $scores;
       }
     }
     close (SC);
  }
}
close (FH);
