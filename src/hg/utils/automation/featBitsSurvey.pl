#!/usr/bin/env perl

use strict;
use warnings;

my $argc = scalar(@ARGV);
if ($argc < 1) {
 printf STDERR "usage: featBitsSurvey.pl asmId [... asmId ...] > result.html\n";
  exit 255;
}

my %commonName;	# key is queryDb, value is common name

open (my $CN, "-|", "hgsql -N -e 'select gcAccession,commonName from genark;' hgcentraltest") or die "can not hgsql -N -e 'select gcAccession,commonName from genark;'";
while (my $line = <$CN>) {
  chomp $line;
  my ($gcX, $comName) = split('\t', $line);
  $comName =~ s/\s\(.*//;
  $commonName{$gcX} = $comName;
}
close ($CN);

open ($CN, "-|", "hgsql -N -e 'select name,organism from dbDb;' hgcentraltest") or die "can not hgsql -N -e 'select name,organism from dbDb;'";
while (my $line = <$CN>) {
  chomp $line;
  my ($gcX, $comName) = split('\t', $line);
  $comName =~ s/\s\(.*//;
  $commonName{$gcX} = "$comName/${gcX}";
}
close ($CN);


printf '<!DOCTYPE HTML>
<!--#set var="TITLE" value="featureBits lastz/chain/net" -->
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
printf "<caption style='font-size: 20px; background-color: powderblue;'>'featureBits' - showing percent identity, how much of the target is matched by the query</caption>\n";
printf "<thead style='position:sticky; top:0; background-color: white;'><tr>\n";
printf "<th>count</th><th>chains</th><th>syntenic</th><th>reciprocal<br>best</th><th>lift<br>over</th><th>target</th><th>query</th><th>target</th><th>query</th>\n";
printf "</tr></thead><tbody>\n";

my $N = 0;
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
      my $targetName = "&nbsp;";
      my $queryName = "&nbsp;";
      $targetName = $commonName{$target} if (defined($commonName{$target}));
      $queryName = $commonName{$query} if (defined($commonName{$query}));
      printf "<td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>\n", $target, $query, $targetName, $queryName;
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
