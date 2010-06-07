#!/usr/bin/env perl

# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/utils/mkMafFrames.pl instead.

# This script was adapted from MarkD's tclsh script
# /cluster/data/mm7/bed/multiz17wayFrames/mkMafFrames .
# It does not include tolerance for relative filenames that are in
# other directories -- use correct pathnames.

# $Id: mkMafFrames.pl,v 1.1 2006/04/10 16:24:26 angie Exp $

use Getopt::Long;
use warnings;
use strict;
use FindBin qw($Bin);
use lib "$Bin";
use HgAutomate;

# Option variable names:
use vars qw/
    $opt_mkBed
    $opt_debug
    $opt_verbose
    $opt_help
    /;

# Option defaults:
my $defaultVerbose = 1;


sub usage {
  # Usage / help / self-documentation:
  my ($status) = @_;
  my $base = $0;
  $base =~ s/^(.*\/)?//;
  print STDERR "
usage: $base qDb tDb genePred maf frOut
options:
    -mkBed file.bed.gz	Make gzipped bed output file too.
    -debug		Don't actually run commands, just display them.
    -verbose num	Set verbose level to num.  Default $defaultVerbose
    -help		Show detailed help (config.ra variables) and exit.
Generates a .frames for the given maf, genes, databases.
Uses qDb and tDb names but does not require databases -- don't run on hgwdev.
Can be run as a cluster job -- pass in correct paths to input and output files
if they are not in parasol run directory.

";
exit($status);
} # usage

# Get command line options
my $ok = GetOptions("mkBed=s",
		    "verbose=n",
		    "debug",
		    "help");
&usage(1) if (!$ok);
&usage(0) if ($opt_help);
&usage(1) if (scalar(@ARGV) != 5);
$opt_verbose = $defaultVerbose if (! defined $opt_verbose);

my ($qDb, $tDb, $genePred, $maf, $frOut) = @ARGV;

my $tmpDir = "/scratch/tmp";
if (! -d $tmpDir) {
  $tmpDir = "/tmp";
}
my $tmpSuf = `mktemp tmp.XXXXXX`;
chomp($tmpSuf);
my $frOutTmp = "$tmpDir/frOut.$tmpSuf";
my $bedOutTmp = "$tmpDir/bedOut.$tmpSuf";

my $cmd = "genePredToMafFrames -verbose=$opt_verbose";
if (defined $opt_mkBed) {
  $cmd .= " -bed=$bedOutTmp";
}
$cmd .= " $qDb $tDb $genePred $frOutTmp $maf";
&HgAutomate::run($cmd);

if (defined $opt_mkBed) {
  $opt_mkBed .= ".gz" if ($opt_mkBed !~ /\.gz$/);
  &HgAutomate::run("gzip -2f $bedOutTmp");
  &HgAutomate::run("mv -f $bedOutTmp.gz $opt_mkBed");
}

system("rm -f $tmpSuf");
&HgAutomate::run("mv -f $frOutTmp $frOut");
