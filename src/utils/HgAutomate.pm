#
# HgAutomate: common cluster, postprocessing and database loading operations.
#
# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/utils/HgAutomate.pm instead.

# $Id: HgAutomate.pm,v 1.1 2006/03/17 21:24:15 angie Exp $
package HgAutomate;

use warnings;
use strict;
use vars qw(@ISA @EXPORT_OK);
use Exporter;

@ISA = qw(Exporter);
@EXPORT_OK = qw( mustMkdir nfsNoodge run verbose );

sub mustMkdir {
  # mkdir || die.  Immune to -debug -- we need to create the dir structure 
  # and dump out the scripts even if we don't actually execute the scripts.
  my ($dir) = @_;
  system("mkdir -p $dir") == 0 || die "Couldn't mkdir $dir\n";
}

sub nfsNoodge {
  # sometimes localhost can't see the newly created file immediately,
  # so insert some artificial delay in order to prevent the next step
  # from dieing on lack of file:
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
