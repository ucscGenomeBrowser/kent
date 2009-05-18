#!/usr/bin/env perl

# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/refreshNamedSessionCustomTracks/makeExclusionList.pl instead.

# $Id: makeExclusionList.pl,v 1.5 2009/05/18 21:38:07 galt Exp $

# Scan the -verbose=4 output of refreshNamedSessionCustomTracks for
# names of existing files that need to be ignored by the script that
# cleans trash.  Write filenames out to an exclusion file.  Pass stdin
# to stdout for further processing.

use warnings;
use strict;

my $apacheRoot = "/usr/local/apache";
my $outRoot = "/export";
my $doNotRmFile = "$apacheRoot/trash/ctDoNotRmNext.txt";
open(OUT, ">$doNotRmFile") || die "Couldn't open >$doNotRmFile: $!";

while (<>) {
  my $fileName;
  if (/^Found live custom track: (\S+)/ ||
      /^setting dataUrl: (\S+)/) ||
      /^setting \w+File: (\S+)/) {
    $fileName = $1;
    $fileName =~ s@^\.\./@$outRoot/@;
    print OUT "$fileName\n";
  }
  print;
}

close(OUT);
chmod 0644, $doNotRmFile;

