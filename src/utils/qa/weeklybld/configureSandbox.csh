#!/bin/tcsh

# configures sandbox $1 by prepending settings file $2 
#  to the common.mk in the sandbox indicated
# currently $2 must be an absolute path
# because we change dir to $1.

if ("$1" == "") then
  echo "first parameter required, must specify sandbox location"
  exit 1
endif

if ("$2" == "") then
  echo "second parameter required, must specify settings file location"
  exit 1
endif

if (! -e "$2") then
  echo "second parameter required, must specify settings file that exists!"
  exit 1
endif

cd $1
if (! -e kent/src/inc/common.mk) then
  pwd
  echo "unexpected error kent/src/inc/common.mk not found!"
  exit 1
endif

echo
echo "now configuring settings on sandbox $PWD [${0}: `date`]"

# fix settings SSL and BAM on in common.mk
if (! -e kent/src/inc/common.mk.orig) then
 cp kent/src/inc/common.mk kent/src/inc/common.mk.orig
endif
cp $2 kent/src/inc/common.mk
cat kent/src/inc/common.mk.orig >> kent/src/inc/common.mk

echo "Done configuring sandbox $PWD"

exit 0

