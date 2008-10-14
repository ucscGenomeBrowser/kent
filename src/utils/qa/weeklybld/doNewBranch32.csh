#!/bin/tcsh
cd $WEEKLYBLD

if ( "$1" != "opensesame" ) then
    echo
    echo "Do not run standalone. This script should be called from doNewBranch.csh. [${0}: `date`]"
    echo
    exit 0
endif 

if ( "$HOST" != "$BOX32" ) then
 echo "Error: this script must be run from $BOX32. [${0}: `date`]"
 exit 1
endif


echo "BRANCHNN=$BRANCHNN"
echo "TODAY=$TODAY       (last build day)"
echo "LASTWEEK=$LASTWEEK   (previous build day)"
echo "REVIEWDAY=$REVIEWDAY   (review day, day2)"
echo "LASTREVIEWDAY=$LASTREVIEWDAY   (previous review day, day2)"


if ( "$TODAY" == "" ) then
 echo "TODAY undefined."
 exit 1
endif
if ( "$BRANCHNN" == "" ) then
 echo "BRANCHNN undefined."
 exit 1
endif
if ( "$LASTWEEK" == "" ) then
 echo "LASTWEEK undefined."
 exit 1
endif
if ( "$REVIEWDAY" == "" ) then
 echo "REVIEWDAY undefined."
 exit 1
endif
if ( "$LASTREVIEWDAY" == "" ) then
 echo "LASTREVIEWDAY undefined."
 exit 1
endif

echo
echo  "NOW STARTING 32-BIT BUILD ON $HOST [${0}: `date`]"
echo

if (-e 32bitUtils.ok) then
    rm 32bitUtils.ok
endif

#echo debug: disabled 32-bit branchd sandbox checkout on $HOST
./coBranch.csh
if ( $status ) then
    echo "checkout 32-bit branch sandbox on $HOST failed [${0}: `date`]"
    exit 1
endif
#echo debug: disabled build 32-bit utils on $HOST
./buildUtils.csh
if ( $status ) then
    echo "build 32-bit utils on $HOST failed [${0}: `date`]"
    exit 1
endif

# do not do this here this way - we might as well wait until
#  later in the week to run this after most branch-tag moves will have been applied.
#ssh $BOX32 $WEEKLYBLD/buildCgi32.csh
if ( $status ) then
    echo "build 32-bit CGIs on $HOST failed [${0}: `date`]"
    exit 1
endif

echo "success 32-bit build utils v${BRANCHNN}" > 32bitUtils.ok
exit 0

