#!/bin/sh -ex
{
source ./buildEnv.sh
cd $dir

buildCore.sh
buildBlast.sh &
buildBioCyc.sh &
#buildCgap.sh &
buildFoldUtr.sh &
#buildKegg.sh &
buildPfamScop.sh &
buildTo.sh &
# buildMafGene.sh &
wait

hgsql $tempDb -Ne "drop view trackDb"
hgsql $tempDb -Ne "drop view chromInfo"

echo "BuildKnown successfully finished"
} > doKnown.log < /dev/null 2>&1
