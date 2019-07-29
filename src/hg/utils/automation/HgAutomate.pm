#
# HgAutomate: common cluster, postprocessing and database loading operations.
#
# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/HgAutomate.pm instead.

# $Id: HgAutomate.pm,v 1.29 2010/05/20 23:13:33 hiram Exp $
package HgAutomate;

use warnings;
use strict;
use Carp;
use vars qw(@ISA @EXPORT_OK);
use Exporter;
use File::Basename;
use File::Spec;

@ISA = qw(Exporter);

# This is a listing of the public methods and variables (which should be
# treated as constants) exported by this module:
@EXPORT_OK = (
    # Support for common command line options:
    qw( getCommonOptionHelp processCommonOptions
	@commonOptionVars @commonOptionSpec
      ),
    # Some basic smarts about our compute infrastructure:
    qw( choosePermanentStorage
	chooseWorkhorse chooseFileServer
	chooseClusterByBandwidth chooseSmallClusterByBandwidth
	chooseFilesystemsForCluster checkClusterPath
      ),
    # General-purpose utility routines:
    qw( checkCleanSlate checkExistsUnlessDebug closeStdin
	getAssemblyInfo getSpecies gensub2 machineHasFile databaseExists
	makeGsub mustMkdir mustOpen nfsNoodge paraRun run verbose
      ),
    # Hardcoded paths/commands/constants:
    qw( $centralDbSql $git
	$clusterData $trackBuild $goldenPath $images $gbdb
	$splitThreshold $runSSH $setMachtype
      ),
);

#########################################################################
# A simple model of our local compute environment with some subroutines
# for checking the validity of path+machine combos and for suggesting
# appropriate storage and machines.

use vars qw( %cluster %clusterFilesystem $defaultDbHost );


sub readMainCluster(); # forward declaration to keep code order

# the name of the cluster is in a separate text file, so it's easier to
# use from bash scripts

%cluster =
    ( readMainCluster() =>
        { 'enabled' => 1, 'gigaHz' => 1.4, 'ram' => 8,
	  'hostCount' => 992, },
      'hgwdev-101' =>
        { 'enabled' => 1, 'gigaHz' => 2.1, 'ram' => 1,
	  'hostCount' => 32, },
      'hgwdev' =>
        { 'enabled' => 1, 'gigaHz' => 2.1, 'ram' => 1,
	  'hostCount' => 32, },
    );

my %obsoleteCluster =
    ( 'swarm' => ,
        { 'enabled' => 1, 'gigaHz' => 2.33, 'ram' => 8,
	  'hostCount' => 1024, },
      'memk' =>
        { 'enabled' => 1, 'gigaHz' => 1.0, 'ram' => 32,
	  'hostCount' => 32, },
      'encodek' =>
        { 'enabled' => 1, 'gigaHz' => 2.0, 'ram' => 16,
	  'hostCount' => 48, },
    );

my @allClusters = (keys %cluster);

%clusterFilesystem =
    ( 'scratch' =>
        { root => '/scratch/data', clusterLocality => 1.0,
	  distrHost => [], distrCommand => '',
	  inputFor => \@allClusters, outputFor => [], },
      'hive' =>
        { root => '/hive/data/genomes', clusterLocality => 0.3,
	  distrHost => ['ku'], distrCommand => '',
	  inputFor => ['ku'],
	  outputFor => ['ku'], },
    );

my %obsoleteClusterFilesystem =
    ( 'scratch' =>
        { root => '/scratch/data', clusterLocality => 1.0,
	  distrHost => [], distrCommand => '',
	  inputFor => \@allClusters, outputFor => [], },
      'hive' =>
        { root => '/hive/data/genomes', clusterLocality => 0.3,
	  distrHost => ['ku'], distrCommand => '',
	  inputFor => ['memk', 'encodek', 'swarm'],
	  outputFor => ['memk', 'encodek', 'swarm'], },
    );

$defaultDbHost = 'hgwdev';

sub readMainCluster() {
    # return the first line of the file cluster.txt in same directory as
    # HgAutomate.pm. This file is easy to parse from bash scripts and
    # other languages, easier than to have the value in this .pm file
    #
    my ($volume, $directory, $file) = File::Spec->splitpath(__FILE__);
    my $mainClusterFname = $directory."cluster.txt";
    open (my $clusterFile, '<', $mainClusterFname) || die "Couldn't open \"$mainClusterFname\": $!\n";
    my $mainCluster = <$clusterFile>; 
    close $clusterFile;
    chomp $mainCluster;
    return $mainCluster;
}

sub choosePermanentStorage {
  # Return the disk drive with the most available space.
  #*** would be good to parameterize instead of hardcoding this!
  confess "Too many arguments" if (scalar(@_) != 0);
  my $maxAvail;
  my $bestRaid;
  for (my $i=1;  $i < 20;  $i++) {
    my $raid = "/cluster/store$i";
    my $df = `df $raid/ 2>&1 | grep -v "No such" | egrep -v '^[A-Za-z]'`;
    if ($df =~ s/.*\s+(\d+)\s+\d+\%.*/$1/) {
      if (! defined $maxAvail || $df > $maxAvail) {
	$maxAvail = $df;
	$bestRaid = $raid;
      }
    }
  }
  if (! defined $bestRaid) {
    $bestRaid = "/hive/data/genomes";
  }
  confess "Could not df any /cluster/store's" if (! defined $bestRaid);
  return $bestRaid;
}

sub getMountPoint {
  # Extract the mount point for a given path from df.
  # This can hang if filesystem is unhappy -- c'est la vie.
  my ($path) = @_;
  my $df = `df $path`;
  if ($df =~ m@\d+\s+\d+\%\s+([/\w]+)$@) {
    return $1;
  } else {
    return undef;
  }
}

sub getClusterFsInfo {
  # Get clusterFilesystem record for the given path, if there is one.
  # Unless path starts with /scratch or /iscratch which may not be the
  # same on localhost as on the cluster nodes,
  #*** would be good to parameterize instead of hardcoding this!
  # use df to determine real location of path.
  my ($path) = @_;
  confess "must have complete, not relative, path" if ($path !~ m@^/@);
  if ($path =~ m@^/(scratch|iscratch)/@) {
    return $clusterFilesystem{$1};
  } else {
    my $mountPoint = &getMountPoint($path);
    foreach my $fs (keys %clusterFilesystem) {
      my $info = $clusterFilesystem{$fs};
      return $info if ($info->{'root'} =~ /^$mountPoint/);
    }
  }
  return undef;
}

sub getOkClusters {
  # Return a list of clusters that are known to be OK for the given path.
  my ($path, $isInput) = @_;
  my $fsInfo = &getClusterFsInfo($path);
  my @okClusters = ();
  if ($fsInfo) {
    @okClusters = $isInput ?
                       @{$fsInfo->{'inputFor'}} : @{$fsInfo->{'outputFor'}};
  }
  return @okClusters;
}

sub getWarnClusters {
  # If path is not on a clusterFilesytem, and it is used as input to a big
  # cluster or output from small cluster, warn but don't die.
  # Would be nice to use cluster parameters here instead of hardcoding.
  my ($path, $isInput) = @_;
  my $fsInfo = &getClusterFsInfo($path);
  if (! $fsInfo) {
    if ($isInput) {
      return @allClusters;
    } else {
      return ('encodek');
    }
  }
}

sub checkClusterPath {
  # Make sure that the list of paths is OK for the given cluster and in/out.
  my ($cluster, $inOrOut, @pathList) = @_;
  confess "Must have at least 3 arguments" if (scalar(@_) < 3);
  my $clusterInfo = $cluster{$cluster};
  if (! defined $clusterInfo) {
    confess "Unrecognized cluster \"$cluster\"";
  }
  if ($inOrOut ne "in" and $inOrOut ne "out") {
    confess "\$inOrOut must be either \"in\" or \"out\"";
  }
  foreach my $p (@pathList) {
    my $isInput = ($inOrOut eq 'in');
    my @okClusters = &getOkClusters($p, $isInput);
    my @warnClusters = &getWarnClusters($p, $isInput);
    my $do = $isInput ? 'take input from' : 'send output to';
    if (scalar(grep /^$cluster$/, @warnClusters)) {
      warn "Warning: Cluster $cluster probably should not $do $p .\n";
    } elsif (! scalar(grep /^$cluster$/, @okClusters)) {
      die "Error: Cluster $cluster cannot $do $p .\n";
    }
  }
}

sub getLoadFactor {
  # Return the load factor (most-recent) for the given machine.
  # If it doesn't produce a recognizable uptime result, return a
  # very high load.
  my ($mach) = @_;
  confess "Must have exactly 1 argument" if (scalar(@_) != 1);
  my $cmd = "$HgAutomate::runSSH $mach uptime 2>&1 | grep load";
  verbose(4, "about to run '$cmd'\n");
  my $load = `$cmd`;
  if ($load =~ s/.*load average: (\d+\.\d+).*/$1/) {
    return $load;
  }
  return 1000;
}

sub getWorkhorseLoads {
  #*** Would be nice to parameterize instead of hardcoding hostnames...
  # Return a hash of workhorses (all idle small cluster machines),
  # associated with their load factors.
  # a valid workhorse needs to have access to hive.
  confess "Too many arguments" if (scalar(@_) != 0);
  my %horses = ();
  foreach my $machLine ('ku', 'hgwdev') {
    my $mach = $machLine;
    $mach =~ s/[\. ].*//;
    chomp $mach;
    $horses{$mach} = &getLoadFactor($mach) if (! exists $horses{$mach});
  }
  return %horses;
}

sub chooseWorkhorse {
  # Choose a suitable "workhorse" machine.  If -workhorse was given, use that.
  # Otherwise, randomly pick a fast machine with low load factor, or wait if
  # none are available.  This can wait indefinitely, so if it's broken or if
  # all workhorses are down, it's up to the engineer to halt the script.
  confess "Too many arguments" if (shift);
  if ($main::opt_workhorse) {
    return $main::opt_workhorse;
  }
  &verbose(2, "chooseWorkhorse: polling load factors of " .
	   "idle small cluster machines.  This may take a minute...\n");
  while (1) {
    my %horses = &getWorkhorseLoads();
    foreach my $maxLoad (0.1, 0.5, 1.0, 2.0) {
      my @fastHorses = ();
      foreach my $horse (keys %horses) {
	push @fastHorses, $horse if ($horses{$horse} <= $maxLoad);
      }
      if (scalar(@fastHorses) > 0) {
	my $randomFastEnough = $fastHorses[int(rand(scalar(@fastHorses)))];
	&verbose(2, "chooseWorkhorse: $randomFastEnough meets load " .
		 "threshold of $maxLoad.\n");
	return $randomFastEnough;
      }
    }
    my $delay = 120;
    &HgAutomate::verbose(1, "chooseWorkhorse: all machines have high load." .
			 "  waiting $delay seconds...\n");
    sleep($delay);
  }
}

sub getFileServer {
  # Use df to determine the fileserver for $path.
  my ($path) = @_;
  confess "Must have exactly 1 argument" if (scalar(@_) != 1);
  my $host = `df $path 2>&1 | grep -v Filesystem`;
  if ($host =~ /(\S+):\/.*/) {
    return $1;
  } else {
    my $localhost = `uname -n`;	# HOST not always present
    if ($localhost =~ s/^(\w+)(\..*)?$/$1/) {
      return $localhost;
    }
  }
  confess "Could not extract server from output of \"df $path\":\n$host\n";
}

sub canLogin {
  # Return true if logins are enabled on the given fileserver.
  #*** hardcoded
  my ($mach) = @_;
  return ($mach =~ /^kkstore/ || $mach eq 'eieio');
  confess "Must have exactly 1 argument" if (scalar(@_) != 1);
}

sub chooseFileServer {
  # Choose a suitable machine for an I/O-intensive task.
  # If -fileServer was given, use that.
  # Otherwise, determine the fileserver for $path, and if we can log in
  # on the fileserver, and its load is not too high, return it.
  # Otherwise, use a workhorse machine.
  my ($path) = @_;
  confess "Must have exactly 1 argument" if (scalar(@_) != 1);
  if ($main::opt_fileServer) {
    return $main::opt_fileServer;
  }
  my $server = &getFileServer($path);
  verbose(4, "Fileserver from df is '$server'\n");
  $server =~ s/-10$//;
  if ($server && &canLogin($server) && (&getLoadFactor($server) < 2.0)) {
    return $server;
  }
#*** SMALL CLUSTER MACHINES CANNOT WGET OUTSIDE, SO NOT ALWAYS A GOOD CHOICE HERE
  return &chooseWorkhorse();
}

sub chooseClusterByBandwidth {
  # Choose cluster by apparent available bandwidth.
  # Note: this does not take I/O into account, so it's best to call this
  # before distributing inputs instead of after (unless they have been
  # distributed somewhere that is fast for all clusters like /scratch).
  my $onlySmallFast = shift;
  confess "Too many arguments" if (shift);
  my $maxOomph;
  my $bestCluster;
  foreach my $paraHub (keys %cluster) {
    my $clusterInfo = $cluster{$paraHub};
    next if (! $clusterInfo->{'enabled'});
    next if ($onlySmallFast && $clusterInfo->{'gigaHz'} < 2.0);
    my @machInfo = `$HgAutomate::runSSH $paraHub parasol list machines | grep -v dead`;
    my $idleCount = 0;
    my $busyCount = 0;
    foreach my $info (@machInfo) {
      if ($info =~ /idle$/) {
	$idleCount++;
      } else {
	$busyCount++;
      }
    }
    my $batchCount =
      `$HgAutomate::runSSH $paraHub parasol list batches | grep -v ^# | wc -l`;
    my $expectedPortion = 1 / (1 + $batchCount);
    my $oomph = (($idleCount + ($busyCount * $expectedPortion)) *
		 $clusterInfo->{'gigaHz'});
    &verbose(3, "chooseClusterByBandwidth: " .
	     "$paraHub: ((idle=$idleCount + " .
	     "(busy=$busyCount * portion=$expectedPortion)) " .
	     "* speed=$clusterInfo->{gigaHz}) = $oomph\n");
    if (! defined $maxOomph || ($oomph > $maxOomph)) {
      $maxOomph = $oomph;
      $bestCluster = $paraHub;
    }
  }
  if (! defined $bestCluster) {
    confess "Failed to find a live cluster";
  }
  &verbose(2, "chooseClusterByBandwidth: $bestCluster " .
	   "($maxOomph Gop/s est)\n");
  return $bestCluster;
}

sub chooseSmallClusterByBandwidth {
  # Choose small cluster (fast nodes) by apparent available bandwidth.
  # Note: this does not take I/O into account, so it's best to call this
  # before distributing inputs instead of after (unless they have been
  # distributed somewhere that is fast for all clusters like /scratch).
  return chooseClusterByBandwidth(1);
}

sub chooseFilesystemsForCluster {
  # Return a list of suitable filesystems for given cluster and direction.
  my ($cluster, $inOrOut) = @_;
  confess "Must have exactly 2 arguments" if (scalar(@_) != 2);
  my $clusterInfo = $cluster{$cluster};
  confess "Unrecognized cluster $cluster" if (! $clusterInfo);
  confess "Second arg must be either \"in\" or \"out\""
    if ($inOrOut ne 'in' && $inOrOut ne 'out');
  my @filesystems;
  foreach my $fs (keys %clusterFilesystem) {
    my $fsInfo = $clusterFilesystem{$fs};
    my @okClusters = ($inOrOut eq 'in') ?
       @{$fsInfo->{'inputFor'}} :  @{$fsInfo->{'outputFor'}};
    if (scalar(grep /^$cluster$/, @okClusters)) {
      push @filesystems, $fsInfo->{'root'} if (-d $fsInfo->{'root'});
    }
  }
  push @filesystems, '/hive/data/genomes' if (scalar(@filesystems)<1);
  return @filesystems;
}


#########################################################################
# Support for command line options expected to be common to many
# automation scripts:

use vars qw( @commonOptionVars @commonOptionSpec );

# Common option defaults:
my $defaultVerbose = 1;

@commonOptionVars = qw(
    $opt_workhorse
    $opt_fileServer
    $opt_dbHost
    $opt_bigClusterHub
    $opt_smallClusterHub
    $opt_debug
    $opt_verbose
    $opt_help
    );

@commonOptionSpec = ("workhorse=s",
		     "fileServer=s",
		     "dbHost=s",
		     "bigClusterHub=s",
		     "smallClusterHub=s",
		     "verbose=n",
		     "debug",
		     "help",
		    );

my %optionHelpText = ( 'workhorse' =>
'    -workhorse machine    Use machine (default: %s) for compute or
                          memory-intensive steps.
',
		       'fileServer' =>
'    -fileServer mach      Use mach (default: fileServer of the build directory)
                          for I/O-intensive steps.
',
		       'dbHost' =>
'    -dbHost mach          Use mach (default: %s) as database server.
',
		       'bigClusterHub' =>
'    -bigClusterHub mach   Use mach (default: %s) as parasol hub
                          for cluster runs with very large job counts.
',
		       'smallClusterHub' =>
'    -smallClusterHub mach Use mach (default: %s) as parasol hub
                          for cluster runs with smallish job counts.
',
		       'debug' =>
'    -debug                Don\'t actually run commands, just display them.
',
		       'verbose' =>
'    -verbose num          Set verbose level to num (default %d).
',
		       'help' =>
'    -help                 Show detailed help and exit.
',
		     );

my %optionDefaultDefaults = ( 'workhorse' => 'least loaded',
			      'dbHost' => $defaultDbHost,
			      'bigClusterHub' => 'most available',
			      'smallClusterHub' => 'most available',
			      'verbose' => $defaultVerbose,
			    );


sub getCommonOptionHelp {
  # Return description of common options, given defaults, for usage message.
  # Input is a hash of applicable options and default values (which can be
  # empty, in which case %optionDefaultDefaults will be used).
  # debug, verbose and help will be added if not specified.
  my %optionSpec = @_;
  my $help = "";
  foreach my $opName (sort keys %optionSpec) {
    if (exists $optionHelpText{$opName}) {
      $help .= sprintf $optionHelpText{$opName},
	($optionSpec{$opName} || $optionDefaultDefaults{$opName});
    } else {
      die "HgAutomate::getCommonOptionHelp: unrecognized option '$opName'.\n" .
	"Supported values: " . join(", ", sort keys %optionHelpText) . ".\n";
    }
  }
  $help .= $optionHelpText{'debug'} if (! exists $optionSpec{'debug'});
  if (! exists $optionSpec{'verbose'}) {
    $help .= sprintf $optionHelpText{'verbose'},
      $optionDefaultDefaults{'verbose'};
  }
  $help .= $optionHelpText{'help'} if (! exists $optionSpec{'help'});
  return $help;
}

sub processCommonOptions {
  # Process common command line options as specified above
  # (except -help is up to caller):
  $main::opt_verbose = $defaultVerbose if (! defined $main::opt_verbose);
}


#########################################################################
#	These items should come from a configuration file so this
#	business can be easily set up in other environments.
# Hardcoded paths/command sequences:
use vars qw( 	$centralDbSql $git
		$clusterData $trackBuild $goldenPath $images $gbdb
		$splitThreshold $runSSH $setMachtype
	   );
use vars qw( $clusterData $trackBuild
	     $goldenPath $gbdb $centralDbSql $splitThreshold $runSSH );
$centralDbSql = "hgsql -h localhost -A -N hgcentraltest";
$git = "/usr/bin/git";

$clusterData = '/hive/data/genomes';
$trackBuild = 'bed';
my $apacheRoot = '/usr/local/apache';
$goldenPath = "$apacheRoot/htdocs-hgdownload/goldenPath";
$images = "$apacheRoot/htdocs/images";
$gbdb = '/gbdb';

# This is the max number of sequences in an assembly that we will consider
# "chrom-based" (allow split tables; per-seq files can fit in one directory)
# as opposed to "scaffold-based" (no split tables; multi-level directory for
# per-seq files, or use set of multi-seq files).
$splitThreshold = 100;

# ssh command and its options, the extra -o options prevent asking
# questions about adding machines to known hosts
$runSSH = "ssh -x -o 'StrictHostKeyChecking = no' -o 'BatchMode = yes'";

$setMachtype = "setenv MACHTYPE `uname -m | sed -e 's/i686/i386/;'`";

#########################################################################
# General utility subroutines:

sub checkCleanSlate {
  # Exit with an error message if it looks like this step has already been run
  # based on the existence of the given file(s) or directory(ies).
  my ($step, $nextStep, @files) = @_;
  confess "Must have at least 3 arguments" if (scalar(@_) < 3);
  confess "undef input" if (! defined $step || ! defined $nextStep);
  my $problem = 0;
  foreach my $f (@files) {
    confess "undef input" if (! defined $f);
    if (-e $f) {
      warn "$step: looks like this was run successfully already " .
	"($f exists).  Either run with -continue $nextStep or some later " .
	"step, or move aside/remove $f and run again.\n";
      $problem = 1;
    }
  }
  exit 1 if ($problem);
}

sub checkExistsUnlessDebug {
  # Exit with an error message if required files don't exist,
  # unless $opt_debug.
  my ($prevStep, $step, @files) = @_;
  confess "Must have at least 3 arguments" if (scalar(@_) < 3);
  confess "undef input" if (! defined $prevStep || ! defined $step);
  return if ($main::opt_debug);
  my $problem = 0;
  foreach my $f (@files) {
    confess "undef input" if (! defined $f);
    if (! -e $f) {
      warn "$step: output of previous step $prevStep, $f , is required " .
	"but does not appear to exist.\n" .
	"If it actually does exist, then this error is probably due to " .
	"network/filesystem delays -- wait a minute and restart with " .
	"-continue $step.\n" .
	"If it really doesn't exist, either fix things manually or " .
	"try -continue $prevStep\n";
      $problem = 1;
    }
  }
  exit 1 if ($problem);
}

sub paraRun {
 my $para = '/parasol/bin/para';
 if ( ! -e "$para" ) {
    # allow PATH to find the para command
    $para = "para";
  }
 return ("$para make jobList\n" .
"$para check\n" .
"$para time > run.time\n" .
'cat run.time');
}

sub gensub2 {
 my $answer = '/parasol/bin/gensub2';
 if ( ! -s "$answer" ) {
    # allow PATH to find the gensub2 command
    $answer = "gensub2";
  }
 return $answer;
}

sub closeStdin {
  # If we don't do this, the script can hang ("Suspended (tty input)")
  # when it is run backgrounded (&) and then something is typed into the
  # terminal... or something like that.  Anyway, doesn't hurt.  It does not
  # prevent hanging on ssh prompts, however.
  close(STDIN);
  open(STDIN, '/dev/null');
}

sub getAssemblyInfo {
  # Do a quick dbDb lookup to get assembly descriptive info for README.txt.
  my ($dbHost, $db) = @_;
  confess "Must have exactly 2 arguments" if (scalar(@_) != 2);
  my $query = "select genome,description,sourceName from dbDb " .
              "where name = \"$db\";";
  my $line = `echo '$query' | $HgAutomate::runSSH $dbHost $centralDbSql`;
  chomp $line;
  my ($genome, $date, $source) = split("\t", $line);
  return ($genome, $date, $source);
}

sub getSpecies {
  # fetch scientificName from dbDb
  my ($dbHost, $db) = @_;
  confess "Must have exactly 2 arguments" if (scalar(@_) != 2);
  my $query = "select scientificName from dbDb " .
              "where name = \"$db\";";
  my $line = `echo '$query' | $HgAutomate::runSSH $dbHost $centralDbSql`;
  chomp $line;
  my ($scientificName) = split("\t", $line);
  if (length($scientificName) < 1) {
     if ( -s "$HgAutomate::clusterData/$db/species.name.txt" ) {
        $scientificName = `cat $HgAutomate::clusterData/$db/species.name.txt`;
        chomp $scientificName;
     } else {
        $scientificName = "species name not found";
     }
  }
  return ($scientificName);
} # getSpecies

sub getOrganism {
  # fetch organism from dbDb
  my ($dbHost, $db) = @_;
  confess "Must have exactly 2 arguments" if (scalar(@_) != 2);
  my $query = "select organism from dbDb " .
              "where name = \"$db\";";
  my $line = `echo '$query' | $HgAutomate::runSSH $dbHost $centralDbSql`;
  chomp $line;
  my ($organism) = split("\t", $line);
  return ($organism);
} # getOrganism

sub machineHasFile {
  # Return a positive integer if $mach appears to have $file or 0 if it
  # does not.
  my ($mach, $file) = @_;
  confess "Must have exactly 2 arguments" if (scalar(@_) != 2);
  confess "undef input" if (! defined $mach || ! defined $file);
  my $count = `$HgAutomate::runSSH $mach ls -1 $file 2>>/dev/null | wc -l`;
  chomp $count;
  return $count + 0;
}

sub databaseExists {
  my ($dbHost, $db) = @_;
  return 0 if ($dbHost =~ m/nohost/i);
  confess "Must have exactly 2 arguments" if (scalar(@_) != 2);
  my $query = "show databases like \"$db\";";
  my $line = `echo '$query' | $HgAutomate::runSSH $dbHost $centralDbSql`;
  chomp $line;
  return length($line);     # will be zero if not existing, >0 if exists
}

sub makeGsub {
  # Create a gsub file in the given dir with the given contents.
  my ($runDir, $templateCmd) = @_;
  confess "Must have exactly 2 arguments" if (scalar(@_) != 2);
  confess "undef input" if (! defined $runDir || ! defined $templateCmd);
  chomp $templateCmd;
  my $fh = mustOpen(">$runDir/gsub");
  print $fh  <<_EOF_
#LOOP
$templateCmd
#ENDLOOP
_EOF_
    ;
  close($fh);
}

sub mustMkdir {
  # mkdir || die.  Immune to -debug -- we need to create the dir structure
  # and dump out the scripts even if we don't actually execute the scripts.
  my ($dir) = @_;
  confess "Must have exactly 1 argument" if (scalar(@_) != 1);
  confess "undef input" if (! defined $dir);
  system("mkdir -p $dir") == 0 || die "Couldn't mkdir $dir\n";
}

sub mustOpen {
  # Open a file or else die with informative error message.
  my ($fileSpec) = @_;
  confess "Must have exactly 1 argument" if (scalar(@_) != 1);
  confess "undef input" if (! defined $fileSpec);
  open(my $handle, $fileSpec)
    || die "Couldn't open \"$fileSpec\": $!\n";
  return $handle;
}

sub nfsNoodge {
  # the touch of the directory causes NFS to refresh its directory
  # information and thus pick up status change to the file.
  # sometimes localhost can't see the newly created file immediately,
  # so insert some artificial delay in order to prevent the next step
  # from dieing on lack of file:
  my ($file) = @_;
  confess "Must have exactly 1 argument" if (scalar(@_) != 1);
  confess "undef input" if (! defined $file);
  return if ($main::opt_debug);
  my $dir = dirname($file);
  for (my $i=0;  $i < 5;  $i++) {
    `touch $dir`;
    sleep(4);
    last if ( -s $file );
  }
}

sub run {
  # Run a command in sh (unless -debug).
  my ($cmd) = @_;
  confess "Must have exactly 1 argument" if (scalar(@_) != 1);
  confess "undef input" if (! defined $cmd);
  if ($main::opt_debug) {
    print "#DEBUG# $cmd\n";
  } else {
    verbose(1, "# $cmd\n");
    system($cmd) == 0 || die "Command failed:\n$cmd\n";
  }
}

sub verbose {
  my ($level, $message) = @_;
  confess "Must have exactly 2 arguments" if (scalar(@_) != 2);
  confess "undef input" if (! defined $level || ! defined $message);
  print STDERR $message if ($main::opt_verbose >= $level);
}


# perl packages need to end by returning a positive value:
1;
