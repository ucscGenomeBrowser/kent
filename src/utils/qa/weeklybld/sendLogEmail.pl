#!/usr/bin/env perl

use strict;
use warnings;

my $argc = scalar(@ARGV);
if ($argc != 2) {
  printf STDERR "usage: sendLogEmail.pl \$LASTNN \$BRANCHNN\n";
  exit 255;
}

my $lastNN = shift;
my $branchNN = shift;

my $buildMeisterOnly = $ENV{'BUILDMEISTEREMAIL'};
my $buildMeisterEtc =  $buildMeisterOnly . ' clayfischer@ucsc.edu lrnassar@ucsc.edu';
# the bounceEmail address needs to be in the ucsc.edu domain to work correctly
my $bounceEmail = $buildMeisterOnly;
my $returnEmail = ' lrnassar@ucsc.edu';

my @victims;
my %victimEmail;

my $authorFilter = `getent group kentcommit | cut -d':' -f4 | tr ',' '|'`;
chomp $authorFilter;

open (FH, "git log v${lastNN}_base..v${branchNN}_base --name-status | grep Author | egrep -i \"${authorFilter}\" | sort -u |") or die "can not git log v${lastNN}_base..v${branchNN}_base --name-status";
while (my $line = <FH>) {
  chomp $line;
  if ($line =~ m/^Author:/) {
    $line =~ s/Author: //;
    my @b = split('\s+', $line);
    my $sizeB = scalar(@b);
    my $email = $b[$sizeB-1];
    $email =~ s/<//;
    $email =~ s/@.*//;
    if (!exists($victimEmail{$email})) {
      push @victims, $email;
      $victimEmail{$email} = $line;
    }
  }
}
close (FH);

my $victimList = join(' ', sort @victims);
open (FH, "|mail -r $returnEmail -s 'Code summaries for v$branchNN are expected from....' $buildMeisterEtc") or die "can not run mail command";
printf FH "%s\n", $victimList;
close (FH);
foreach my $victim (sort keys %victimEmail) {
  my $logData = "";
  open (FH, "git log --author=${victim} v${lastNN}_base..v${branchNN}_base --pretty=oneline|") or die "can not git log --author=$victim";
  while (my $line = <FH>) {
     chomp $line;
     $logData .= $line;
  }
  close (FH);
  if (length($logData) > 1) {
       my $toAddr = $victimEmail{$victim};
       printf STDERR "# sending email to $toAddr\n";
       open (SH, "| /usr/sbin/sendmail -f $bounceEmail -t -oi") or die "can not run sendmail";
       printf SH "To: %s\n", $toAddr;
       printf SH "From: \"Lou Nassar\" <lrnassar\@ucsc.edu>\n";
       printf SH "Subject: Code summaries are due for %s\n", $victim;
       printf SH "Cc: \"Lou Nassar\" <lrnassar\@ucsc.edu>\n";
       printf SH "\n";
       print SH `./summaryEmail.sh $victim`;
  }
}
