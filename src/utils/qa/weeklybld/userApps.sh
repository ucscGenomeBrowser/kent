#!/bin/bash

set -beEu -o pipefail

cd /hive/groups/browser/newBuild/v${BRANCHNN}_branch/kent/src/userApps
rm -fr ./kent ./samtabix *.zip
make fetchSource
rm *.zip
cd /hive/groups/browser/newBuild/v${BRANCHNN}_branch/kent/src
tar cvzf ./userApps.src.tgz ./userApps/

# note that hard links are used below
downloadDir=/mirrordata/apache/htdocs/admin/exe
versionedTgz=userApps.v${BRANCHNN}.src.tgz

# hgwdownload
ssh -n qateam@hgdownload "rm -f $downloadDir/userApps.src.tgz"
# in case the script was already run before:
ssh -n qateam@hgdownload "rm -f $downloadDir/userApps.archive/$versionedTgz"
scp -p userApps.src.tgz qateam@hgdownload:$downloadDir/
ssh -n qateam@hgdownload "cd $downloadDir/userApps.archive/; ln ../userApps.src.tgz $versionedTgz"

# hgwdownload2
#ssh -n qateam@hgdownload2 "rm -f $downloadDir/userApps.src.tgz"
# in case the script was already run before:
#ssh -n qateam@hgdownload2 "rm -f $downloadDir/userApps.archive/$versionedTgz"
#scp -p userApps.src.tgz qateam@hgdownload2:$downloadDir/
#ssh -n qateam@hgdownload2 "cd $downloadDir/userApps.archive/; ln ../userApps.src.tgz $versionedTgz"

# hgwdownload3
ssh -n qateam@hgdownload3 "rm -f $downloadDir/userApps.src.tgz"
# in case the script was already run before:
ssh -n qateam@hgdownload3 "rm -f $downloadDir/userApps.archive/$versionedTgz"
scp -p userApps.src.tgz qateam@hgdownload3:$downloadDir/
ssh -n qateam@hgdownload3 "cd $downloadDir/userApps.archive/; ln ../userApps.src.tgz $versionedTgz"

# genome-euro
ssh -n qateam@genome-euro "rm -f $downloadDir/userApps.src.tgz"
# in case the script was already run before:
ssh -n qateam@genome-euro "rm -f $downloadDir/userApps.archive/$versionedTgz"
scp -p userApps.src.tgz qateam@genome-euro:$downloadDir/
ssh -n qateam@genome-euro "cd $downloadDir/userApps.archive/; ln ../userApps.src.tgz $versionedTgz"

rm -f userApps.src.tgz
cd /hive/groups/browser/newBuild/v${BRANCHNN}_branch/kent/src/userApps
rm -fr ./kent ./samtabix *.zip
