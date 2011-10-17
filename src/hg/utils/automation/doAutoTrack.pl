#!/usr/bin/env perl

# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/doAutoTrack.pl instead.

use Getopt::Long;
use warnings;
use strict;
use FindBin qw($Bin);
use lib "$Bin";
use HgAutomate;
use HgRemoteScript;
use HgStepManager;
use File::Basename;

# Option variable names, both common and peculiar to this script:
use vars @HgAutomate::commonOptionVars;
use vars @HgStepManager::optionVars;
# Options coming in on the command line:
use vars qw/
    $opt_buildDir
    /;


# Specify the steps supported with -continue / -stop:
my $stepper = new HgStepManager(
    [ { name => 'download',   func => \&doDownload },
      { name => 'process',   func => \&doProcess },
      { name => 'load',   func => \&doLoad },
      { name => 'verify', func => \&doVerify },
      { name => 'push', func => \&doPush },
      { name => 'cleanup', func => \&doCleanup },
    ]
);

# Option defaults:
my $buildDir = '';

my $base = $0;
$base =~ s/^(.*\/)?//;

sub usage {
  # Usage / help / self-documentation:
  my ($status, $detailed) = @_;
  # Basic help (for incorrect usage):
  print STDERR "
usage: $base [options] /path/to/config.file.ra
required argument:
  /path/to/config.file.ra - tag,value pairs specifying procedures

other options:
";
  print STDERR $stepper->getOptionHelp();
  print STDERR <<_EOF_
    -buildDir dir         Use dir instead of the automatic date stamp directory
                          (necessary if experimenting with builds).
_EOF_
  ;
  print STDERR &HgAutomate::getCommonOptionHelp('dbHost' => "xyz",
						'workhorse' => '',
						'bigClusterHub' => '',
						'smallClusterHub' => '');
  print STDERR "
Automates track building.  Scripts for the steps are specified in the
              config.file.ra:
    download: performs the job of fetching the outside data
    process: processes the outside data into files ready to use
    load: loads the files into tables or place bigWig/bigBed into htdocs
    verify: checks the loaded data or big* files
    push: sets the signals or data locations so the push can take place
    cleanup: Removes or compresses intermediate files

All operations are performed in a date-stamped build directory whose base
path is specified in the config.file.ra.  The additional date stamp to
the specified workDir adds year/month/day: YEAR/MM/DD

This directory can be overridden by the -buildDir option.

To see detailed information about what should appear in the config.file.ra,
run \"$base -help\".
";
  # Detailed help (-help):
  if ($detailed) {
  print STDERR <<_EOF_
-----------------------------------------------------------------------------
Required config.file.ra settings:

workDir /hive/data/path/for/this/track
  - specify your top-level /hive/data/.../ pathname
  in which all work should take place.  A date-stamped
  named directory will be constructed under the workDir in which all scripts
  should do their work.  Each script is given that path as an argument.
  The scripts should go to that directory and construct
  whatever hierarchy they would like.  Any output from the scripts, to stdout
  or stderr, will go into a log file under workDir/log/year/mm/day_hh:mm

Each of the following scripts are specific to your process.

download /path/to/your/download/script
process /path/to/your/process/script
load /path/to/your/load/script
verify /path/to/your/verify/script
push /path/to/your/push/script
cleanup /path/to/your/cleanup/script

Each script should be able to determine if it can run due to output from
  previous steps.  Return 0 for success, any non-zero for non-success.
  Doing nothing can be success when appropriate.
_EOF_
  }
  print "\n";
  exit $status;
}

# Globals:
my ($logDir, $logFile);
# Required command line arguments:
my $CONFIG;
# Required config.ra parameters:
my ($workDir, $downloadScript, $processScript, $loadScript, $verifyScript,
    $pushScript, $cleanupScript);

#########################################################################
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
}

#########################################################################
# * step: download
sub doDownload {
  &HgAutomate::run("$downloadScript $buildDir >> $logFile 2>&1");
} # doDownload

#########################################################################
# * step: process
sub doProcess {
  &HgAutomate::run("$processScript $buildDir >> $logFile 2>&1");
} # doProcess

#########################################################################
# * step: verify
sub doVerify {
  &HgAutomate::run("$verifyScript $buildDir >> $logFile 2>&1");
} # doVerify

#########################################################################
# * step: load
sub doLoad {
  &HgAutomate::run("$loadScript $buildDir >> $logFile 2>&1");
} # doLoad

#########################################################################
# * step: push
sub doPush {
  &HgAutomate::run("$pushScript $buildDir >> $logFile 2>&1");
} # doPush

#########################################################################
# * step: cleanup
sub doCleanup {
  &HgAutomate::run("$cleanupScript $buildDir >> $logFile 2>&1");
} # doCleanup

#########################################################################
sub fileExists($) {
  # verify files mentioned in config file actually exist
  my ($fileName) = @_;
  if (-s $fileName) {
    return 0;
  } else {
    printf STDERR "ERROR: file: '$fileName' does not exist\n";
    return 1;
  }
}

#########################################################################
sub requireVar($$) {
  # Ensure that var is in %config and return its value.
  # Remove it from %config so we can check for unrecognized contents.
  my ($var, $config) = @_;
  my $val = $config->{$var}
    || die "ERROR: requireVar: $CONFIG is missing required variable \"$var\".\n" .
      "For a detailed list of required variables, run \"$base -help\".\n";
  delete $config->{$var};
  return $val;
} # requireVar

#########################################################################
sub optionalVar {
  # If var has a value in %config, return it.
  # Remove it from %config so we can check for unrecognized contents.
  my ($var, $config) = @_;
  my $val = $config->{$var};
  delete $config->{$var} if ($val);
  return $val;
} # optionalVar

#########################################################################
sub parseConfig($) {
  # Parse config.ra file, make sure it contains the required variables.
  my ($configFile) = @_;
  my %config = ();
  if (! -s $configFile) {
    printf STDERR "ERROR: can not read specified config file:\n\t'$configFile'\n";
    &usage(1)
  }
  my $fh = &HgAutomate::mustOpen($configFile);
  while (<$fh>) {
    next if (/^\s*#/ || /^\s*$/);
    if (/^\s*(\w+)\s*(.*)$/) {
      my ($var, $val) = ($1, $2);
      if (! exists $config{$var}) {
	$config{$var} = $val;
      } else {
	die "ERROR: parseConfig: Duplicate definition for $var line $. of config file $configFile.\n";
      }
    } else {
      die "ERROR: parseConfig: Can't parse line $. of config file $configFile:\n$_\n";
    }
  }
  close($fh);

  # Required variables in configure.ra file:
  $workDir = &requireVar('workDir', \%config);
  $downloadScript = &requireVar('download', \%config);
  $processScript = &requireVar('process', \%config);
  $loadScript = &requireVar('load', \%config);
  $verifyScript = &requireVar('verify', \%config);
  $pushScript = &requireVar('push', \%config);
  $cleanupScript = &requireVar('cleanup', \%config);

  die "ERROR: can not find specified workDir:\n\t'$workDir'"
	if (! -d $workDir );
  # verify existence of all these files
  my $existCount = 0;
  # 1 is returned from fileExists if file does not exist, 0 if exists
  $existCount += &fileExists($downloadScript);
  $existCount += &fileExists($processScript);
  $existCount += &fileExists($loadScript);
  $existCount += &fileExists($verifyScript);
  $existCount += &fileExists($pushScript);
  $existCount += &fileExists($cleanupScript);
  # any count indicates something was missing
  die "ERROR: config file specifies files that do not exist" if ($existCount);

  # Make sure no unrecognized variables were given.
  my @stragglers = sort keys %config;
  if (scalar(@stragglers) > 0) {
    die "ERROR: parseConfig: config file $CONFIG has unrecognized variables:\n"
      . "    " . join(", ", @stragglers) . "\n" .
      "For a detailed list of supported variables, run \"$base -help\".\n";
  }
} # parseConfig

#########################################################################
# main

# Prevent "Suspended (tty input)" hanging:
&HgAutomate::closeStdin();

# Make sure we have valid options and exactly 1 argument:
&checkOptions();
&usage(1) if (scalar(@ARGV) != 1);

($CONFIG) = @ARGV;
&parseConfig($CONFIG);

# Force debug and verbose until this is looking pretty solid:
# $opt_debug = 1;
# $opt_verbose = 3 if ($opt_verbose < 3);

# Establish directory to work in, log file name, add a date stamp
my $now = `date "+%Y-%m-%d %H:%M:%S"`;
chomp $now;
my $yearMonth = `date +%Y/%m`;
chomp $yearMonth;
my $day = `date +%d`;
chomp $day;
my $dataDir = "$workDir/$yearMonth/$day";
$logDir = "$workDir/log/$yearMonth";
&HgAutomate::mustMkdir($logDir) if ( ! -d $logDir );
$logFile = "$logDir/${day}_" . `date +%H:%M`;
chomp $logFile;
# can override directory to work in via -buildDir=<pathName>
$buildDir = $opt_buildDir ? $opt_buildDir : $dataDir;
&HgAutomate::mustMkdir($buildDir) if ( ! -d $buildDir );

open (FH, ">$logFile") or die "ERROR: can not write to log file:\n\t'$logFile'";
printf FH "# $now starting doAutoTrack.sh in\n#\t'$buildDir'\n";
close (FH);

# Do everything.
$stepper->execute();

# Tell the user anything they should know.
my $stopStep = $stepper->getStopStep();
my $upThrough = ($stopStep eq 'cleanup') ? "" :
  "  (through the '$stopStep' step)";

&HgAutomate::verbose(1,
	"\n *** All done!$upThrough\n");
&HgAutomate::verbose(1,
	" *** Steps were performed in $buildDir\n");
&HgAutomate::verbose(1, "\n");
open (FH, ">>$logFile") or die "ERROR: can not add to log file:\n\t'$logFile'";
printf FH "# $now finished doAutoTrack.sh in\n#\t'$buildDir'\n";
close (FH);
