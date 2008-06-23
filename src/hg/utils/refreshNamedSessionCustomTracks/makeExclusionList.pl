#!/usr/bin/env perl

# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/refreshNamedSessionCustomTracks/makeExclusionList.pl instead.

# $Id: makeExclusionList.pl,v 1.2 2008/06/23 17:34:30 angie Exp $

# Scan the -verbose=4 output of refreshNamedSessionCustomTracks for
# names of existing files that need to be ignored by the script that
# cleans trash.  Write filenames out to an exclusion file.

use warnings;
use strict;

my $apacheRoot = "/usr/local/apache";
my $outRoot = "/export";
my $doNotRmFile = "$apacheRoot/trash/ctDoNotRm.txt";
open(OUT, ">$doNotRmFile") || die "Couldn't open >$doNotRmFile: $!";

while (<>) {
  my $fileName;
  if (/^Found live custom track: (\S+)/ ||
      /^setting \w+File: (\S+)/) {
    $fileName = $1;
    $fileName =~ s@^\.\./@$outRoot/@;
    print OUT "$fileName\n";
  }
}

close(OUT);
chmod 0644, $doNotRmFile;

