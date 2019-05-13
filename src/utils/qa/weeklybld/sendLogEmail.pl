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

my $buildMeisterEmail = $ENV{'BUILDMEISTEREMAIL'} . ',azweig@ucsc.edu,brianlee@soe.ucsc.edu';
my $returnEmail = ' brianlee@soe.ucsc.edu';

my @victims;
my %victimEmail;

open (FH, "git log v${lastNN}_base..v${branchNN}_base --name-status | grep Author | sort | uniq|") or die "can not git log v${lastNN}_base..v${branchNN}_base --name-status";
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
open (FH, "|mail -r $returnEmail -s 'Code summaries for v$branchNN are expected from....' $buildMeisterEmail") or die "can not run mail command";
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
       open (SH, "| /usr/sbin/sendmail -t -oi") or die "can not run sendmail";
       printf SH "To: %s\n", $toAddr;
       printf SH "From: \"Brian Lee\" <brianlee\@soe.ucsc.edu>\n";
       printf SH "Subject: Code summaries are due for %s\n", $victim;
       printf SH "Cc: \"Brian Lee\" <brianlee\@soe.ucsc.edu>\n";
       printf SH "\n";
       print SH `./summaryEmail.sh $victim`;
  }
}
