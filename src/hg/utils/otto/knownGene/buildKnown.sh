#!/bin/sh -ex
{
. buildEnv.sh
cd $dir

buildCore.sh
buildBlast.sh &
buildBioCyc.sh &
#buildCgap.sh &
buildFoldUtr.sh &
#buildKegg.sh &
buildPfamScop.sh &
buildTo.sh &
wait
echo "BuildKnown successfully finished"
} > doKnown.log < /dev/null 2>&1
