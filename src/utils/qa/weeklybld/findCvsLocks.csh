#!/bin/tcsh
#
# This script attempts to find cvs locks that users
# have accidentally left on and which will never 
# go away without intervention.  Do not nuke locks
# that may be from real users.  Only permanent ones.
# If you run this script a few times, the perm ones
# will certainly stand out.
#
#

if ( "$HOST" != "hgwdev" ) then
 echo "Error: this script must be run from hgwdev."
 exit 1
endif

find /projects/compbiousr/cvsroot/kent -name '#cvs*' -printf "%p %TY-%Tm-%TdT%TT%TZ %u\n" |egrep -v "^(find)" > cvs.locks.found
find /projects/compbiousr/cvslock/kent -name '#cvs*' -printf "%p %TY-%Tm-%TdT%TT%TZ %u\n" |egrep -v "^(find)" >> cvs.locks.found

echo "cvs.locks.found:"
cat cvs.locks.found


