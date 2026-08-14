#!/bin/bash

# Verify that no directory makefile overrides the compiler flags that
# inc/common.mk sets for the whole tree.
#
# hg/hgTracks/makefile carried "COPT = -ggdb" from 2019 to 2026.  Every
# directory makefile includes inc/common.mk on its first line, so that
# assignment came after the tree default and silently replaced it.  Shipped
# hgTracks binaries were built with no optimization at all while the libraries
# they linked against were built with -O3.  Wide views cost up to 1.95 times
# more than they needed to.
#
# The 2026 conversion of the tree to -O3 did not find it.  That work was driven
# by the warnings -O3 produces, and a directory that overrides COPT never
# receives the flag, so it never warns.  A warning driven sweep cannot see code
# that opted out.  This check can.  refs #38094
#
# To build one directory unoptimized for a debugging session, put the flag on
# the make command line:
#
#     make COPT='-O0 -g'
#
# Command line variables override everything, and nothing here reads the
# command line, so debugging builds are unaffected.

set -u

# Only makefiles that git tracks.  This skips vendored submodules, stray
# worktrees and untracked scratch makefiles, none of which we build or ship.
makefiles=`git ls-files 2>/dev/null | grep -E '(^|/)[Mm]akefile(\..*)?$'`
if [ -z "${makefiles}" ]; then
    # not a git checkout, for instance a source distribution: nothing to check
    exit 0
fi

# Read every makefile once and pull out each assignment to COPT or CFLAGS.
# Most of these are legal, so sort them into the three bad shapes below.
assigns=`grep -HnE '^[[:space:]]*(COPT|CFLAGS)[[:space:]]*[:+?]*=' ${makefiles} 2>/dev/null`

# COPT holds the optimization level.  Any assignment to it in a directory
# makefile replaces the tree default, including the empty assignment "COPT=".
coptHits=`printf "%s\\n" "${assigns}" | grep -E ':[0-9]+:[[:space:]]*COPT[[:space:]]*[:+?]?='`

# CFLAGS is built up with += in inc/common.mk.  Assigning to it with = drops
# -std=c99 and -fno-strict-aliasing along with anything else the tree added.
cflagsHits=`printf "%s\\n" "${assigns}" | grep -E ':[0-9]+:[[:space:]]*CFLAGS[[:space:]]*[:?]?='`

# Appending to CFLAGS is fine for defines and include paths, but an -O flag
# appended here lands after COPT on the compiler command line and wins.
optHits=`printf "%s\\n" "${assigns}" | grep -E ':[0-9]+:[[:space:]]*CFLAGS[[:space:]]*\+=.*-O'`

if [ -z "${coptHits}${cflagsHits}${optHits}" ]; then
    exit 0
fi

echo "ERROR: a makefile overrides the compiler flags set in inc/common.mk" 1>&2

if [ -n "${coptHits}" ]; then
    echo "" 1>&2
    echo "  These set COPT, which replaces the tree optimization level:" 1>&2
    echo "${coptHits}" | sed 's/^/    /' 1>&2
fi

if [ -n "${cflagsHits}" ]; then
    echo "" 1>&2
    echo "  These assign to CFLAGS with =, which discards the flags the tree" 1>&2
    echo "  already added.  Use += instead:" 1>&2
    echo "${cflagsHits}" | sed 's/^/    /' 1>&2
fi

if [ -n "${optHits}" ]; then
    echo "" 1>&2
    echo "  These append an -O flag to CFLAGS, which overrides COPT:" 1>&2
    echo "${optHits}" | sed 's/^/    /' 1>&2
fi

echo "" 1>&2
echo "  Remove the line.  For a one off debugging build put the flag on the" 1>&2
echo "  make command line instead:  make COPT='-O0 -g'" 1>&2
echo "" 1>&2

exit 255
