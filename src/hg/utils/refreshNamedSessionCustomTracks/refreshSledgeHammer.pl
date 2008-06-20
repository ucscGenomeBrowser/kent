#!/usr/bin/env perl

# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/refreshNamedSessionCustomTracks/refreshSledgeHammer.pl instead.

# $Id: refreshSledgeHammer.pl,v 1.2 2008/06/20 20:49:34 angie Exp $

# Use the awesome power of Perl to force the access time of a file to be
# updated when read (the NFS cache can prevent that and must be bypassed)
# using O_DIRECT.  To be fair, that can be done in C -- on many platforms.
# So that we don't have yet another portability concern in our C codebase,
# I'll just use perl here.

# This script parses the -verbose=4 output of refreshNamedSessionCustomTracks
# looking for files which do exist, and then reads them with O_DIRECT which
# seems to force an access time update even on stubborn cached files.

use warnings;
use Fcntl;
use strict;

while (<>) {
  my $fileName;
  if (/^(Found live custom track: |setting \w+File: |\/)(\S+)/) {
    $fileName = $2;
    $fileName = $1 . $fileName if ($1 eq "/");
    $fileName =~ s@^\.\./@/usr/local/apache/@;
    sysopen(FH, $fileName, O_RDONLY | O_DIRECT)
      || die "Can't open $fileName: $!\n";
    <FH>;
    close(FH);
  }
}

