#!/usr/bin/env perl

use strict;
use warnings;
use File::Basename;

my @machList = qw( hgw0 );

my $expectName = "hgwdev";
my $hostName = `hostname -s`;
chomp $hostName;

if ($hostName ne $expectName) {
  printf STDERR "ERROR: must run this on %s !  This is: %s\n", ${expectName}, ${hostName};
  exit 255;
}

my $DS = `date "+%F"`; chomp $DS;
my $TS = `date "+%T"`; chomp $TS;
my $Y = `date "+%Y"`; chomp $Y;
my $M = `date "+%m"`; chomp $M;
my $logDir = "/hive/data/inside/GenArk/pushRR/logs/${Y}/${M}";
`mkdir -p "${logDir}"`;
my $logFile="${logDir}/betaPublic.${DS}.gz";
open (my $lf, "|gzip -c > '$logFile'") or die "can not write to logFile";
printf STDERR "### log file in: %s\n", $logFile;
printf $lf "### starting alphaBetaPush.pl %s %s\n", $DS, $TS;

my %betaContrib;	# key is name of contrib track, the directory under
                        #  <buildDir>/contrib/<contribName>
my $betaTrackCount = 0;
my %publicContrib;	# key is name of contrib track, the directory under
                        #  <buildDir>/contrib/<contribName>
my $publicTrackCount = 0;
my $home = $ENV{'HOME'};

if ( -s "$home/kent/src/hg/makeDb/trackDb/betaGenArk.txt" ) {
  open (my $fh, "<", "$home/kent/src/hg/makeDb/trackDb/betaGenArk.txt") or die "can not read ~/kent/src/hg/makeDb/trackDb/betaGenArk.txt";
  while (my $line = <$fh>) {
    next if ($line =~ m/^#/);
    chomp $line;
    $betaContrib{$line} = 1;
    $betaTrackCount += 1;
    printf $lf "# beta track %s\n", $line;
  }
  close ($fh);
}
if ( -s "$home/kent/src/hg/makeDb/trackDb/publicGenArk.txt" ) {
  open (my $fh, "<", "$home/kent/src/hg/makeDb/trackDb/publicGenArk.txt") or die "can not read ~/kent/src/hg/makeDb/trackDb/publicGenArk.txt";
  while (my $line = <$fh>) {
    next if ($line =~ m/^#/);
    chomp $line;
    $publicContrib{$line} = 1;
    $publicTrackCount += 1;
    printf $lf "# public track %s\n", $line;
  }
  close ($fh);
}

# no tracks defined, nothing to do
if ( ($betaTrackCount < 1) && ($publicTrackCount < 1) ) {
   exit 0;
}
printf $lf "# %d beta contrib tracks defined\n", $betaTrackCount;
printf $lf "# %d public contrib tracks defined\n", $publicTrackCount;

my @stageList = qw( alpha beta public );
my %betaList;	# key will be GCx accession and the value will be the
                # /gbdb/genark/GCx/... directory name where
                # beta.hub.txt files have been found
my $betaCount = 0;
my %publicList;	# key will be GCx accession and the value will be the
                # /gbdb/genark/GCx/... directory name where
                # public.hub.txt files have been found
my $publicCount = 0;

### appears to be about the same performance for this with egrep
### or with the names specified in the find command.  Supposed to be better
### to specify the names in the find command.

# open (my $fh, "-|", 'find /gbdb/genark/GCA /gbdb/genark/GCF ! -type d | egrep "beta.hub.txt|public.hub.txt"') or die "can not run find on /gbdb/genark";

open (my $fh, "-|", 'find /gbdb/genark/GCA /gbdb/genark/GCF ! -type d \\( -name "beta.hub.txt" -o -name "public.hub.txt" \\)') or die "can not run find on /gbdb/genark";

while (my $hubTxt = <$fh>) {
    chomp $hubTxt;
    if ($hubTxt =~ m/beta.hub.txt/) {
      my $dirName = dirname($hubTxt);
      my $accession = basename($dirName);
      $betaList{$accession} = $dirName;
      ++$betaCount;
    } elsif ($hubTxt =~ m/public.hub.txt/) {
      my $dirName = dirname($hubTxt);
      my $accession = basename($dirName);
      $publicList{$accession} = $dirName;
      ++$publicCount;
    } else {
      printf $lf "ERROR: unrecognized hub.txt: '%s'\n", $hubTxt;
      exit 255;
    }
}
close ($fh);
printf $lf "# found %d beta hub.txt files\n", $betaCount;
printf $lf "# found %d public hub.txt files\n", $publicCount;

my $cmd = "";
my $cmdOut = "";

if ($betaCount > 0) {
  printf $lf "# found %d beta hub.txt files\n", $betaCount;
  foreach my $accession (keys %betaList) {
     my $pathDir = $betaList{$accession};
     printf $lf "# %s %s\n", $accession, $pathDir;
     foreach my $betaTrack (keys %betaContrib) {
        if ( -d "$pathDir/contrib/$betaTrack" ) {
          $cmd = qq(ssh qateam\@hgwbeta mkdir -p "$pathDir/contrib/$betaTrack" 2>&1);
          printf $lf "%s\n", $cmd;
          $cmdOut = `$cmd`;
          if (length($cmdOut) > 1) {
             printf $lf "%s\n", $cmdOut;
          } else {
             printf $lf "# ssh mkdir $pathDir/contrib/$betaTrack successfull\n";
          }
          $cmd = qq(rsync --stats -a -L --itemize-changes "$pathDir/contrib/$betaTrack/" "qateam\@hgwbeta:$pathDir/contrib/$betaTrack/" 2>&1);
          printf $lf "%s\n", $cmd;
          $cmdOut = `$cmd`;
          if (length($cmdOut) > 1) {
             printf $lf "%s\n", $cmdOut;
          } else {
             printf $lf "# rsync output mysteriously empty ?\n";
          }
        }	#	if ( -d "$pathDir/contrib/$betaTrack" )
     }		#	foreach my $betaTrack (keys %betaContrib)
     $cmd = qq(rsync --stats -a -L --itemize-changes "$pathDir/beta.hub.txt" "qateam\@hgwbeta:$pathDir/hub.txt" 2>&1);
     printf $lf "%s\n", $cmd;
     $cmdOut = `$cmd`;
     if (length($cmdOut) > 1) {
        printf $lf "%s\n", $cmdOut;
     } else {
        printf $lf "# rsync output mysteriously empty ? '%s'\n", $cmdOut;
     }
     printf $lf "https://hgwbeta.soe.ucsc.edu/cgi-bin/hgTracks?genome=%s&hubUrl=%s/hub.txt\n", $accession, $pathDir;
  }	#	foreach my $accession (keys %betaList)
}	#	if ($betaCount > 0)

if ($publicCount > 0) {
  printf $lf "# found %d public hub.txt files\n", $publicCount;
  foreach my $accession (keys %publicList) {
     printf $lf "# %s %s\n", $accession, $publicList{$accession};
     foreach my $machName ( @machList ) {
       printf $lf "push %s public to %s\n", $accession, $machName;
       my $pathDir = $publicList{$accession};
       foreach my $publicTrack (keys %publicContrib) {
          if ( -d "$pathDir/contrib/$publicTrack" ) {
            $cmd = qq(ssh qateam\@$machName mkdir -p "$pathDir/contrib/$publicTrack" 2>&1);
            printf $lf "%s\n", $cmd;
            $cmdOut = `$cmd`;
            if (length($cmdOut) > 1) {
               printf $lf "%s\n", $cmdOut;
            } else {
               printf $lf "# ssh mkdir $machName $pathDir/contrib/$publicTrack successfull\n";
            }
            $cmd = qq(rsync --stats -a -L --itemize-changes "$pathDir/contrib/$publicTrack/" "qateam\@$machName:$pathDir/contrib/$publicTrack/" 2>&1);
            printf $lf "%s\n", $cmd;
            $cmdOut = `$cmd`;
            if (length($cmdOut) > 1) {
               printf $lf "%s\n", $cmdOut;
            } else {
               printf $lf "# rsync output mysteriously empty ?\n";
            }
          }	#	if ( -d "$pathDir/contrib/$publicTrack" )
       }	#	foreach my $publicTrack (keys %publicContrib)
       $cmd = qq(rsync --stats -a -L --itemize-changes "$pathDir/public.hub.txt" "qateam\@$machName:$pathDir/hub.txt" 2>&1);
       printf $lf "%s\n", $cmd;
       $cmdOut = `$cmd`;
       if (length($cmdOut) > 1) {
          printf $lf "%s\n", $cmdOut;
       } else {
          printf $lf "# rsync output mysteriously empty ? '%s'\n", $cmdOut;
       }
       if ($machName =~ m/^hgw/) {
         printf $lf "https://$machName.soe.ucsc.edu/cgi-bin/hgTracks?genome=%s&hubUrl=%s/hub.txt\n", $accession, $pathDir;
       } else {
         printf $lf "https://genome-euro.ucsc.edu/cgi-bin/hgTracks?genome=%s&hubUrl=%s/hub.txt\n", $accession, $pathDir;
       }
     }	#	foreach my $machName ( @machList )
  }	#	foreach my $accession (keys %publicList)
}	#	if ($publicCount > 0)

close ($lf);
