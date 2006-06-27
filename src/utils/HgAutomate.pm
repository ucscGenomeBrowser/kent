#
# HgAutomate: common cluster, postprocessing and database loading operations.
#
# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/utils/HgAutomate.pm instead.

# $Id: HgAutomate.pm,v 1.3 2006/06/27 22:47:22 angie Exp $
package HgAutomate;

use warnings;
use strict;
use vars qw(@ISA @EXPORT_OK);
use Exporter;

@ISA = qw(Exporter);
@EXPORT_OK = qw( makeGsub mustMkdir mustOpen nfsNoodge run verbose
		 getCommonOptionHelp processCommonOptions
		 @commonOptionVars @commonOptionSpec
	       );

use vars qw( @commonOptionVars @commonOptionSpec );

# Common option defaults:
my $defaultVerbose = 1;

@commonOptionVars = qw(
    $opt_workhorse
    $opt_fileServer
    $opt_bigClusterHub
    $opt_smallClusterHub
    $opt_debug
    $opt_verbose
    $opt_help
    );

@commonOptionSpec = ("workhorse=s",
		     "fileServer=s",
		     "bigClusterHub=s",
		     "smallClusterHub=s",
		     "verbose=n",
		     "debug",
		     "help",
		    );

sub getCommonOptionHelp {
  # Return description of common options, given defaults, for usage message.
  my ($workhorse, $bigClusterHub, $smallClusterHub) = @_;
  my $help = <<_EOF_
    -workhorse machine    Use machine (default: $workhorse) for compute or
                          memory-intensive steps.
    -fileServer mach      Use mach (default: fileServer of the build directory)
                          for I/O-intensive steps.
    -bigClusterHub mach   Use mach (default: $bigClusterHub) as parasol hub
                          for blastz cluster run.
    -smallClusterHub mach Use mach (default: $smallClusterHub) as parasol hub
                          for cat & chain cluster runs.
    -debug                Don't actually run commands, just display them.
    -verbose num          Set verbose level to num (default $defaultVerbose).
    -help                 Show detailed help and exit.
_EOF_
  ;
  return $help;
}

sub processCommonOptions {
  # Process common command line options as specified above
  # (except -help is up to caller):
  $main::opt_verbose = $defaultVerbose if (! defined $main::opt_verbose);
}

sub makeGsub {
  # Create a gsub file in the given dir with the given contents.
  my ($runDir, $templateCmd) = @_;
  $templateCmd =~ s/\n$//;
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
  system("mkdir -p $dir") == 0 || die "Couldn't mkdir $dir\n";
}

sub mustOpen {
  # Open a file or else die with informative error message.
  my ($fileSpec) = @_;
  open(my $handle, $fileSpec)
    || die "Couldn't open \"$fileSpec\": $!\n";
  return $handle;
}

sub nfsNoodge {
  # sometimes localhost can't see the newly created file immediately,
  # so insert some artificial delay in order to prevent the next step
  # from dieing on lack of file:
  return if ($main::opt_debug);
  my ($file) = @_;
  for (my $i=0;  $i < 5;  $i++) {
    last if (system("ls $file >& /dev/null") == 0);
    sleep(2);
  }
}

sub run {
  # Run a command in sh (unless -debug).
  my ($cmd) = @_;
  if ($main::opt_debug) {
    print "# $cmd\n";
  } else {
    verbose(1, "# $cmd\n");
    system($cmd) == 0 || die "Command failed:\n$cmd\n";
  }
}

sub verbose {
  my ($level, $message) = @_;
  print STDERR $message if ($main::opt_verbose >= $level);
}

# perl packages need to end by returning a positive value:
1;
