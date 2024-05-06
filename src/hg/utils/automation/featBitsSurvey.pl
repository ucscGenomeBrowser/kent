#!/usr/bin/env perl

use strict;
use warnings;

my $argc = scalar(@ARGV);
if ($argc < 1) {
  printf STDERR "usage: featBitsSurvey.pl clade asmId [... asmId ...] > result.html\n";
  exit 255;
}

printf '<!DOCTYPE HTML>
<!--#set var="TITLE" value="Primates genomes assembly hubs" -->
<!--#set var="ROOT" value="../.." -->

<!--#include virtual="$ROOT/inc/gbPageStartHardcoded.html" -->
';

sub checkOne($) {
  my ($fbTxt) = @_;
  my $stat = "&nbsp;";
  if ( -s "${fbTxt}" ) {
    my $fBits = `cut -d' ' -f5 $fbTxt | tr -d '()%'`;
    chomp $fBits;
    $stat = $fBits;
  } else {
     printf STDERR "not found: '%s'\n", $fbTxt;
  }
  return $stat;
}

printf "<table class='sortable' border='1' style='background-color:powderblue;'>\n";
printf "<caption>showing percent identity, how much of the target is matched by the query</caption>\n";
printf "<thead style='position:sticky; top:0; background-color: white;'><tr>\n";
printf "<th>count</th><th>chains</th><th>syntenic</th><th>reciprocal<br>best</th><th>lift<br>over</th><th>target</th><th>query</th><th>group</th>\n";
printf "</tr></thead><tbody>\n";

my $N = 0;
my $clade = shift;
while (my $asmId = shift) {
  my @a = split('_', $asmId);
  my $target = sprintf("%s_%s", $a[0], $a[1]);
  my $gcX = substr($asmId, 0, 3);
  my $d0 = substr($asmId, 4, 3);
  my $d1 = substr($asmId, 7, 3);
  my $d2 = substr($asmId, 10, 3);
  my $tDir = "/hive/data/genomes/asmHubs/allBuild/$gcX/$d0/$d1/$d2/$asmId";
  my $buildDir = `realpath "${tDir}"`;
  chomp $buildDir;
  if ( -d "${buildDir}" ) {
    open ( my $td, "-|", "ls -d ${buildDir}/trackData/lastz.*") or die "can not ls -d ${buildDir}/trackData/lastz.*";
    while (my $query = <$td>) {
      chomp $query;
      $query =~ s#.*/trackData/lastz.##;
      my $Query = ucfirst($query);
      my $fbTxt = "${buildDir}/trackData/lastz.${query}/fb.${target}.chain${Query}Link.txt";
      my $fBits = checkOne($fbTxt);
      $fbTxt = "${buildDir}/trackData/lastz.${query}/fb.${target}.chainSyn${Query}Link.txt";
      my $synBits = checkOne($fbTxt);
      $fbTxt = "${buildDir}/trackData/lastz.${query}/fb.${target}.chainRBest.${Query}.txt";
      my $rbBits = checkOne($fbTxt);
      $fbTxt = "${buildDir}/trackData/lastz.${query}/fb.${target}.chainLiftOver${Query}Link.txt";
      my $loBits = checkOne($fbTxt);
#      printf STDERR "%s\t%s\t%s\t%s\t%s\t%s\n", $target, $query, $fBits, $synBits, $rbBits, $loBits;
      printf "<tr><td style='text-align:right;'>%4d</td>", ++$N;
      printf "<td style='text-align:right;'>%s</td>", $fBits;;
      printf "<td style='text-align:right;'>%s</td>", $synBits;
      printf "<td style='text-align:right;'>%s</td>", $rbBits;
      printf "<td style='text-align:right;'>%s</td>", $loBits;
      printf "<td>%s</td><td>%s</td><td>%s</td></tr>\n", $target, $query, $clade;
    }
    close ($td);
  } else {
    printf STDERR "# %s - buildDir not found\n", $asmId;
  }
}
printf "</tbody></table>\n";

printf '</div><!-- closing gbsPage from gbPageStartHardcoded.html -->
</div><!-- closing container-fluid from gbPageStartHardcoded.html -->
<!--#include virtual="$ROOT/inc/gbFooterHardcoded.html"-->
<script src="<!--#echo var="ROOT" -->/js/sorttable.js"></script>
</body></html>
';
