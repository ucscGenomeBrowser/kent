#!/usr/local/bin/perl -w

# cvsup - do a cvs update, report results concisely

use strict;

my $cvsdir = $ARGV[0] || "~/kent/src";
my $cvscmd = "cvs up -d -P $cvsdir";

#
# isInteresting: return true if it looks like source that should be checked in.
#
my @interestingFiles = ('makefile', 'Makefile');
my @interestingSuffixes = ('c', 'h', 'as', 'sql', 'html', 'ra');
sub isInteresting {
    my $fname = shift(@_);
    foreach my $i (@interestingFiles) {
	if ($fname eq $i) {
	    return 1;
	}
    }
    if ($fname =~ m/\.(.+)$/) {
	my $suffix = $1;
	foreach my $i (@interestingSuffixes) {
	    if ($suffix eq $i) {
		return 1;
	    }
	}
    }
    return 0;
}

open(CVSUP, "$cvscmd 2>&1 |") ||
  die "Couldn't open pipe from \"$cvscmd\": $!\n";

my $date = `date`;
chop($date);
print "Updating $cvsdir at $date\n\n";

# Pick out the interesting lines of output
my @additions;
my @updates;
my @modifieds;
my @conflicts;
my @noncvsInteresting;
my @noncvs;
my @unrecognized;
while (<CVSUP>) {
  if      (/^A (\S+)/) {
    push @additions, $1;
  } elsif (/^U (\S+)/) {
    push @updates, $1;
  } elsif (/^M (\S+)/) {
    push @modifieds, $1;
  } elsif (/^C (\S+)/) {
    push @conflicts, $1;
  } elsif (/^\? (\S+)/) {
    my $f = $1;
    if (isInteresting($f)) {
      push @noncvsInteresting, $f;
    } else {
      push @noncvs, $f;
    }
  } elsif (/^(\S \S+)/) {
    push @unrecognized, $1;
  }
}

# Report them.
if (scalar(@unrecognized) > 0) {
  print "Unrecognized update types:\n";
  foreach my $u (@unrecognized) {
    print "  $u\n";
  }
  print "\n";
}
if (scalar(@conflicts) > 0) {
  print "CONFLICTS -- manually edit to resolve:\n";
  foreach my $u (@conflicts) {
    print "  $u\n";
  }
  print "\n";
}
if (scalar(@additions) > 0) {
  print "Added (not checked in) files:\n";
  foreach my $u (@additions) {
    print "  $u\n";
  }
  print "\n";
}
if (scalar(@modifieds) > 0) {
  print "Modified (possibly merged) files:\n";
  foreach my $u (@modifieds) {
    print "  $u\n";
  }
  print "\n";
}
if (scalar(@updates) > 0) {
  print "Updated files:\n";
  foreach my $u (@updates) {
    print "  $u\n";
  }
  print "\n";
}
if (scalar(@noncvsInteresting) > 0) {
  print "Files/directories not checked in to CVS that look like source:\n";
  foreach my $u (@noncvsInteresting) {
    print "  $u\n";
  }
  print "\n";
}
if (scalar(@noncvs) > 0) {
  print "Files/directories not checked in to CVS that don't look like source:\n";
  foreach my $u (@noncvs) {
    print "  $u\n";
  }
  print "\n";
}
if (scalar(@conflicts) > 0) {
  print "CONFLICTS -- manually edit to resolve:\n";
  foreach my $u (@conflicts) {
    print "  $u\n";
  }
  print "\n";
}
