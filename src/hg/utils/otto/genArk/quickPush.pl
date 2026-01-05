#!/usr/bin/env perl

#############################################################################
###  The 'stat' file listings used here: dev.todayList.gz, hgw1.todayList.gz
###    and hgwbeta.todayList.gz are made by cron jobs previous to this
###    script running.  They are a listing of files in /gbdb/genark/GC[AF]/
###    with the 'mtime' - last modified time.  This time will be compared
###    to decide if anything needs to go out.
###
###  This source is from the source tree:
###     ~/kent/src/hg/utils/otto/genArk/quickPush.pl
###  do *not* edit this in the otto directory /hive/data/inside/GenArk/pushRR/
###  where this is used.
#############################################################################

use strict;
use warnings;
use File::Basename;

my @machList = qw( hgw0 );

my $lf;		# going to become the output handle for the logfile
my $expectName = "hgwdev";
my $hostName = `hostname -s`;
chomp $hostName;

if ($hostName ne $expectName) {
  printf STDERR "ERROR: must run this on %s !  This is: %s\n", ${expectName}, ${hostName};
  exit 255;
}

##############################################################################
### pass in a machine name and a contrib/directory to rsync out
sub rsyncContrib($$) {
  my ($dest, $contribDir) = @_;
  my $contribPath = "/gbdb/genark/" . $contribDir;
  my $cmd = qq(rsync --mkpath --stats -a -L --itemize-changes "$contribPath/" "qateam\@$dest:$contribPath/" 2>&1);
  printf $lf "%s\n", $cmd;
  my $cmdOut = `$cmd`;
  if (length($cmdOut) > 1) {
    ;
#            printf $lf "%s\n", $cmdOut;
  } else {
    printf $lf "# rsync output mysteriously empty ? '%s'\n", $cmd;
  }
}

##############################################################################
### a contrib/ directory has been identified to go out
### '$which' is either 'beta' or 'all'
sub contribOut($$) {
  my ($which, $contribHash) = @_;
  my $dirCount = scalar keys %$contribHash;
  if ($which eq "beta") {
     for my $contribDir (keys %$contribHash) {
       rsyncContrib("hgwbeta", $contribDir);
     }
  } elsif ($which eq "all") {
     for my $destMach (@machList) {
       for my $contribDir (keys %$contribHash) {
         rsyncContrib($destMach, $contribDir);
       }
     }
  } else {
    printf $lf "# ERROR: contribOut given '%s' which is not 'beta' or 'all'\n", $which;
  }
}

##############################################################################
# given a source hub.txt path and a destination hub.txt path
# send out to the given '$dest' machine
sub rsyncHubTxt($$$) {
  my ($dest, $from, $to) = @_;
  my $cmd = qq(rsync --stats -a -L --itemize-changes "$from" "qateam\@${dest}:$to" 2>&1);
  printf $lf "%s\n", $cmd;
  my $cmdOut = `$cmd`;
  if (length($cmdOut) > 1) {
     ;
  #       printf $lf "%s\n", $cmdOut;
  } else {
    printf $lf "# rsync output mysteriously empty ? '%s'\n", $cmd;
  }
}

##############################################################################
# given a source hub.txt path and a destination hub.txt path
#  push it out to all the machines
# '$which' is either 'beta' or 'all' to determine where to go
sub sendHubTxt($$$) {
  my ($which, $from, $to) = @_;
  if ($which eq "beta") {
     rsyncHubTxt("hgwbeta", $from, $to);
  } elsif ($which eq "all") {
    foreach my $mach (@machList) {
      rsyncHubTxt($mach, $from, $to);
    }
  } else {
    printf $lf "# ERROR: sendHubTxt given '%s' which is not 'beta' or 'all'\n", $which;
  }
}
##############################################################################
### begin scrip main()
##############################################################################

my $DS = `date "+%F"`; chomp $DS;
my $TS = `date "+%T"`; chomp $TS;
my $Y = `date "+%Y"`; chomp $Y;
my $M = `date "+%m"`; chomp $M;
my $logDir = "/hive/data/inside/GenArk/pushRR/logs/${Y}/${M}";
`mkdir -p "${logDir}"`;
my $logFile="${logDir}/quickBetaPublic.${DS}.gz";
open ($lf, "|gzip -c > '$logFile'") or die "can not write to logFile";

printf $lf "### starting quickPush.pl at: %s %s\n", $DS, $TS;

my %betaContrib;	# key is name of contrib track, the directory under
                        #  <buildDir>/contrib/<contribName>
my $betaTrackCount = 0;
my %publicContrib;	# key is name of contrib track, the directory under
                        #  <buildDir>/contrib/<contribName>
my $publicTrackCount = 0;
my $home = $ENV{'HOME'};

my $fh;	# file handle used in all open() calls (one at a time of course)

### check if any 'public' or 'beta' contrib tracks are defined for release
###  the betaGenArk.txt and publicGenArk.txt are in the source tree and
###  control the release of 'contrib' tracks

### this is just getting the listing of 'contrib' directories from those
### source tree files.  These two lists: 'publicContrib' and 'betaContrib' will
### be used later to identify files in the /contrib/<trackName>/ directories

if ( -s "$home/kent/src/hg/makeDb/trackDb/betaGenArk.txt" ) {
  open ($fh, "<", "$home/kent/src/hg/makeDb/trackDb/betaGenArk.txt") or die "can not read ~/kent/src/hg/makeDb/trackDb/betaGenArk.txt";
  while (my $line = <$fh>) {
    next if ($line =~ m/^#/);
    chomp $line;
    $betaContrib{$line} = 1;
    $betaTrackCount += 1;
    $DS = `date "+%F"`; chomp $DS;
    $TS = `date "+%T"`; chomp $TS;
    printf $lf "# beta track: '%s' specified %s %s\n", $line, $DS, $TS;
  }
  close ($fh);
}
if ( -s "$home/kent/src/hg/makeDb/trackDb/publicGenArk.txt" ) {
  open ($fh, "<", "$home/kent/src/hg/makeDb/trackDb/publicGenArk.txt") or die "can not read ~/kent/src/hg/makeDb/trackDb/publicGenArk.txt";
  while (my $line = <$fh>) {
    next if ($line =~ m/^#/);
    chomp $line;
    $publicContrib{$line} = 1;
    $publicTrackCount += 1;
    $DS = `date "+%F"`; chomp $DS;
    $TS = `date "+%T"`; chomp $TS;
    printf $lf "# public track: '%s' specified %s %s\n", $line, $DS, $TS;
  }
  close ($fh);
}

# no tracks defined, nothing to do (never happens since 'tiberius' has existed)
if ( ($betaTrackCount < 1) && ($publicTrackCount < 1) ) {
   exit 0;
}

printf $lf "# %d beta contrib tracks defined\n", $betaTrackCount;
printf $lf "# %d public contrib tracks defined\n", $publicTrackCount;

### the %devAccession list is going to be the set of 'accession' directories
###  on hgwdev to allow verifying it is the same set as exists on hgw1

my %devAccession;	# key is an assembly 'accession' found on hgwdev, value
			# is the 'mtime' of the hub.txt file on hgwdev
my %contribPublic;	# key is file name under /contrib/ value is mtime for public tracks
my %contribBeta;	# key is file name under /contrib/ value is mtime for beta tracks
my %devList;	# key is file name, value is mtime == last modified time
my %hubList;	# same thing for file names hub.txt
my %publicList;	# same thing for file names public.hub.txt, name changed to hub.txt
my %betaHubList;	# same thing for file names beta.hub.txt, name changed to hub.txt

### this is going to establish the listings of 'contrib' and 'hub.txt'
### files that exist on hgwdev.  'public' and 'beta' contrib files will
###  be on separate listings from the 'hub.txt' listings.

open ($fh, "-|", "zegrep '/contrib/|hub.txt' dev.todayList.gz") or die "can not read dev.todayList.gz";
while (my $line = <$fh>) {
  chomp $line;
  my ($mtime, $fileName) = split('\t', $line);
  $devList{$fileName} = $mtime;
  if ($line =~ m#/contrib/#) {
    foreach my $pubContrib (keys %publicContrib) {
       if ($line =~ m#/contrib/$pubContrib/#) {
          $contribPublic{$fileName} = $mtime;
          last;
       }
    }
    foreach my $betaContrib (keys %betaContrib) {
       if ($line =~ m#/contrib/$betaContrib/#) {
          $contribBeta{$fileName} = $mtime;
          last;
       }
    }
  } elsif ($line =~ m#/hub.txt#) {
    $hubList{$fileName} = $mtime;
    my $dirName = dirname($fileName);
    my $accession = basename($dirName);
    $devAccession{$accession} = $mtime;
  } elsif ($line =~ m#/public.hub.txt#) {
    $fileName =~ s#/public.#/#;
    $publicList{$fileName} = $mtime;
  } elsif ($line =~ m#/beta.hub.txt#) {
    $fileName =~ s#/beta.#/#;
    $betaHubList{$fileName} = $mtime;
  }
}
close ($fh);

my $publicContribCount = scalar keys %contribPublic;
my $betaContribCount = scalar keys %contribBeta;

printf $lf "# %d contrib files available for 'public' release\n", $publicContribCount;
printf $lf "# %d contrib files available for 'beta' release\n", $betaContribCount;

### establish the list of hgw1 files with their mtime
### the %hgw1Accession list is going to be the set of 'accession' directories
###  on hgw1 to allow verifying it is the same set as exists on hgwdev

my %hgw1List;	# key is file name, value is mtime == last modified time
my %hgw1Accession;	# key is an assembly 'accession' found on hgw1, value
			# is the 'mtime' of the hub.txt file on hgw1
# this list only has /hub.txt files and /contrib/ files
open ($fh, "-|", "zegrep '/contrib/|hub.txt' hgw1.todayList.gz") or die "can not read hgw1.todayList.gz";
while (my $line = <$fh>) {
  chomp $line;
  my ($mtime, $fileName) = split('\t', $line);
  $hgw1List{$fileName} = $mtime;
  if ($fileName =~ m#/hub.txt#) {
    my $dirName = dirname($fileName);
    my $accession = basename($dirName);
    $hgw1Accession{$accession} = $mtime;
  }
}
close ($fh);

###  check which accessions do not exist on hgw1
my $toPushCount = 0;
foreach my $accession (keys %devAccession) {
  ++$toPushCount if (! defined($hgw1Accession{$accession}));
}

###  check if there are extra accessions on hgw1, should not be
my $extraExisting = 0;
foreach my $accession (keys %hgw1Accession) {
  ++$extraExisting if (! defined($devAccession{$accession}));
}

printf $lf "### %d assemblies to push from hgwdev out\n", $toPushCount;
printf $lf "### %d assemblies on hgw1 not on hgwdev - ERROR should not be present.\n", $extraExisting if ($extraExisting);

### establish the list of hgwbeta files with their mtime

my %betaList;	# key is file name, value is mtime == last modified time
# this list only has /hub.txt files and /contrib/ files
open ($fh, "-|", "zegrep '/contrib/|hub.txt' hgwbeta.todayList.gz") or die "can not read hgwbeta.todayList.gz";
while (my $line = <$fh>) {
  chomp $line;
  my ($mtime, $fileName) = split('\t', $line);
  $betaList{$fileName} = $mtime;
}
close ($fh);

###  check for updated or new 'contig' directory files, public and beta

my %publicContribList;	# key is contig directory name, value is number of files

# check /contrib/... files on the 'public' release list
foreach my $fileName (keys %contribPublic) {
   my $devTime = $contribPublic{$fileName};
   my $yesUpdate = 0;
   if (defined($hgw1List{$fileName})) {
     my $hubTime = $hgw1List{$fileName};
     if ($hubTime ne $devTime) {	# an updated file
       $yesUpdate = 1;
     }
   } else {	# a new file
     $yesUpdate = 1;
   }
   if ($yesUpdate) {
      my $dirName = dirname($fileName);
      $publicContribList{$dirName} += 1;
   }
}

my %betaContribList;	# key is contig directory name, value is number of files

# check /contrib/... files on the 'beta' release list
foreach my $fileName (keys %contribBeta) {
   my $devTime = $contribBeta{$fileName};
   my $yesUpdate = 0;
   if (defined($betaList{$fileName})) {
     my $betaTime = $betaList{$fileName};
     if ($betaTime ne $devTime) {	# an updated file
       $yesUpdate = 1;
     }
   } else {	# a new file
     $yesUpdate = 1;
   }
   if ($yesUpdate) {
      my $dirName = dirname($fileName);
      $betaContribList{$dirName} += 1;
   }
}

### push out any new or modified 'contrib' directories
if ((scalar keys %betaContribList) > 0) {
  $DS = `date "+%F"`; chomp $DS;
  $TS = `date "+%T"`; chomp $TS;
  printf $lf "# pushing %d beta /contrib/ directories %s %s\n", scalar keys %betaContribList, $DS, $TS;
  contribOut("beta", \%betaContribList);
}
if ((scalar keys %publicContribList) > 0) {
  $DS = `date "+%F"`; chomp $DS;
  $TS = `date "+%T"`; chomp $TS;
  printf $lf "# pushing %d public /contrib/ directories %s %s\n", scalar keys %publicContribList, $DS, $TS;
  contribOut("all", \%publicContribList);
}

$DS = `date "+%F"`; chomp $DS;
$TS = `date "+%T"`; chomp $TS;
printf $lf "# checking %d hub.txt files for pushing %s %s\n", scalar keys %publicContribList, $DS, $TS;

### push out any new or modified hub.txt files
### when beta.hub.txt exists it goes to hgwbeta as 'hub.txt'
### and the public.hub.txt goes out to all RR machines as 'hub.txt'
foreach my $fileName (keys %devList) {
   my $devTime = $devList{$fileName};
   if ($fileName =~ m#public.hub.txt#) {
      my $pubHub = $fileName;
      $pubHub =~ s#/public.#/#;
      my $yesUpdate = 0;
      if (defined($hgw1List{$pubHub})) {
        my $hubTime = $hgw1List{$pubHub};
        if ($hubTime ne $devTime) {
          $yesUpdate = 1;
        }
      } else {	# new file
          $yesUpdate = 1;
      }
      if ($yesUpdate) {
          my $dirName = dirname($pubHub);
          my $accession = basename($dirName);
          my $pathDir = sprintf("/gbdb/genark/%s", $dirName);
          my $src = "$pathDir/public.hub.txt";
          my $dest = "$pathDir/hub.txt";
          sendHubTxt("all", $src, $dest);
      }
   } elsif ($fileName =~ m#beta.hub.txt#) {
      my $betaHub = $fileName;
      $betaHub =~ s#/beta.#/#;
      my $yesUpdate = 0;
      if (defined($betaList{$betaHub})) {
        my $hubTime = $betaList{$betaHub};
        if ($hubTime ne $devTime) {
          $yesUpdate = 1;
        }
      } else {	# new file
          $yesUpdate = 1;
      }
      if ($yesUpdate) {
          my $dirName = dirname($betaHub);
          my $accession = basename($dirName);
          my $pathDir = sprintf("/gbdb/genark/%s", $dirName);
          my $src = "$pathDir/beta.hub.txt";
          my $dest = "$pathDir/hub.txt";
          sendHubTxt("beta", $src, $dest);
      }
   }	#	elsif ($fileName =~ m#beta.hub.txt#)
}	#	foreach my $fileName (keys %devList)

$DS = `date "+%F"`; chomp $DS;
$TS = `date "+%T"`; chomp $TS;
printf $lf "### quickPush.pl finished at: %s %s\n", $DS, $TS;
