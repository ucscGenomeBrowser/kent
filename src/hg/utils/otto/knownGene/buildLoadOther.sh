#!/bin/sh -ex
cd $dir

{
# # Finally, need to wait until after testing, but update databases in other organisms
# with blastTabs

# Load blastTabs
cd $dir/hgNearBlastp
hgLoadBlastTab $xdb $blastTab run.$xdb.$tempDb/out/*.tab
echo Loaded $xdb.$blastTab
hgLoadBlastTab $ratDb $blastTab run.$ratDb.$tempDb/out/*.tab
echo Loaded $ratDb.$blastTab
hgLoadBlastTab $flyDb $blastTab run.$flyDb.$tempDb/recipBest.tab
echo Loaded $flyDb.$blastTab
hgLoadBlastTab $wormDb $blastTab run.$wormDb.$tempDb/recipBest.tab
echo Loaded $wormDb.$blastTab
hgLoadBlastTab $yeastDb $blastTab run.$yeastDb.$tempDb/recipBest.tab
echo Loaded $yeastDb.$blastTab
hgLoadBlastTab $fishDb $blastTab run.$fishDb.$tempDb/recipBest.tab
echo Loaded $fishDb.$blastTab

# Do synteny on mouse/human/rat
synBlastp.csh $xdb $db

synBlastp.csh $ratDb $db ensGene knownGene

# Clean up
#rm -r run.*/out

echo "LoadOther successfully finished"
} > doLoadOther.log 2>&1
