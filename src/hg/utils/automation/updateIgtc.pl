#!/usr/bin/env perl

# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/updateIgtc instead.

# $Id: updateIgtc.pl,v 1.3 2007/08/13 20:45:19 angie Exp $

use Getopt::Long;
use warnings;
use strict;
use FindBin qw($Bin);
use lib "$Bin";
use HgAutomate;
use HgRemoteScript;
use HgStepManager;

# Option variable names, both common and peculiar to this script:
use vars @HgAutomate::commonOptionVars;
use vars @HgStepManager::optionVars;
use vars qw/
    $opt_buildDir
    /;

# Specify the steps supported with -continue / -stop:
my $stepper = new HgStepManager(
    [ { name => 'fetch',   func => \&doFetch },
      { name => 'load',    func => \&doLoad },
    ]
				);

# Option defaults:
my $dbHost = 'hgwdev';

my $base = $0;
$base =~ s/^(.*\/)?//;

sub usage {
  # Usage / help / self-documentation:
  my ($status, $detailed) = @_;
  # Basic help (for incorrect usage):
  print STDERR "
usage: $base db
options:
";
  print STDERR $stepper->getOptionHelp();
  print STDERR <<_EOF_
    -buildDir dir         Use dir instead of default
                          $HgAutomate::clusterData/\$db/$HgAutomate::trackBuild/igtc.\$date
                          (necessary when continuing at a later date).
_EOF_
  ;
  print STDERR &HgAutomate::getCommonOptionHelp('dbHost' => $dbHost);
  print STDERR "
Automates the update of IGTC genetrap track (igtc) for db.  Steps:
    fetch: Get files from genetrap.org.
    load: (Re)load the igtc track into the specified database.
All operations are performed in the build directory which is
$HgAutomate::clusterData/\$db/$HgAutomate::trackBuild/igtc.\$date unless -buildDir is given.
";
  # Detailed help (-help):
  print STDERR "
Assumptions:
1. IGTC / genetrap.org (contact: Doug Stryke) makes files available as follows:
   - http://www.genetrap.org/blattrack/genetrap.fasta is the probe sequence,
     same for all databases
   - for each database mm#, the PSL custom track file is
     http://www.genetrap.org/blattrack/genetrap_mm#.psl
" if ($detailed);
  print "\n";
  exit $status;
}


# Globals:
# Command line args: db
my ($db);
# Other:
my ($buildDir, $date);

sub checkOptions {
  # Make sure command line options are valid/supported.
  my $ok = GetOptions(@HgStepManager::optionSpec,
		      'buildDir=s',
		      @HgAutomate::commonOptionSpec,
		      );
  &usage(1) if (!$ok);
  &usage(0, 1) if ($opt_help);
  &HgAutomate::processCommonOptions();
  my $err = $stepper->processOptions();
  usage(1) if ($err);
  $dbHost = $opt_dbHost if ($opt_dbHost);
}


#########################################################################
# * step: fetch [fileServer]
sub doFetch {
  my $runDir = "$buildDir";
  &HgAutomate::mustMkdir($runDir);

  my $whatItDoes = "It fetches track files from genetrap.org.";
  my $fileServer = &HgAutomate::chooseFileServer($runDir);
  my $bossScript = new HgRemoteScript("$runDir/doFetch.csh", $fileServer,
				      $runDir, $whatItDoes);
  $bossScript->add(<<_EOF_
wget -q -N http://www.genetrap.org/blattrack/genetrap_$db.psl
wget -q -N -O genetrap.$date.fasta http://www.genetrap.org/blattrack/genetrap.fasta
_EOF_
  );
  $bossScript->execute();
} # doFetch


#########################################################################
# * step: load [dbHost]
sub doLoad {
  my $runDir = "$buildDir";
  &HgAutomate::mustMkdir($runDir);

  my $whatItDoes = "It loads the igtc track into $db.";
  my $bossScript = new HgRemoteScript("$runDir/doLoad.csh", $dbHost,
				      $runDir, $whatItDoes);

  &HgAutomate::checkExistsUnlessDebug('fetch', 'load',
				      "$buildDir/genetrap.$date.fasta",
				      "$buildDir/genetrap_$db.psl");

  $bossScript->add(<<_EOF_
mkdir -p $HgAutomate::gbdb/$db/igtc
rm -f $HgAutomate::gbdb/$db/igtc/genetrap.$date.fasta
ln -s $buildDir/genetrap.$date.fasta $HgAutomate::gbdb/$db/igtc/
hgLoadSeq -replace $db $HgAutomate::gbdb/$db/igtc/genetrap.$date.fasta
grep -v ^track genetrap_$db.psl \\
| hgLoadPsl $db -table=igtc stdin
rm -f seq.tab
_EOF_
  );
  $bossScript->execute();
} # doLoad


#########################################################################
# main

# Prevent "Suspended (tty input)" hanging:
&HgAutomate::closeStdin();

# Make sure we have valid options and exactly 1 argument:
&checkOptions();
&usage(1) if (scalar(@ARGV) != 1);
($db) = @ARGV;

# Establish what directory we will work in.
$date = `date +%Y-%m-%d`;
chomp $date;
$buildDir = $opt_buildDir ? $opt_buildDir :
  "$HgAutomate::clusterData/$db/$HgAutomate::trackBuild/igtc.$date";

# Do everything.
$stepper->execute();

# Tell the user anything they should know.
my $stopStep = $stepper->getStopStep();
my $upThrough = ($stopStep eq 'load') ? "" :
  "  (through the '$stopStep' step)";

&HgAutomate::verbose(1,
	"\n *** All done!$upThrough\n");
&HgAutomate::verbose(1,
	" *** Steps were performed in $buildDir\n");

my $oldFiles = `ls -1 $HgAutomate::gbdb/$db/igtc/genetrap*.fasta 2>/dev/null | grep -v genetrap.$date.fasta`;
if ($oldFiles && $stopStep eq 'load') {
  chomp($oldFiles);
  &HgAutomate::verbose(1,
	" *** In the pushQ entry, ask for removal of old file(s) $oldFiles\n" .
	" *** from /gbdb and from extFile *after* the rest of the push\n" .
	" *** is completed.");
}

&HgAutomate::verbose(1, "\n");

