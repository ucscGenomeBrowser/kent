#!/bin/bash

set -beEu -o pipefail

cd /hive/groups/browser/newBuild/v${BRANCHNN}_branch/kent/src/userApps
rm -fr ./kent ./samtabix *.zip
make fetchSource
rm *.zip
cd /hive/groups/browser/newBuild/v${BRANCHNN}_branch/kent/src
tar cvzf ./userApps.src.tgz ./userApps/

# note that hard links are used below

# hgwdownload
ssh -n qateam@hgdownload.soe.ucsc.edu "rm -f /mirrordata/apache/htdocs/admin/exe/userApps.src.tgz"
# in case the script was already run before:
ssh -n qateam@hgdownload.soe.ucsc.edu "rm -f /mirrordata/apache/htdocs/admin/exe/userApps.v${BRANCHNN}.src.tgz"
scp -p userApps.src.tgz qateam@hgdownload.soe.ucsc.edu:/mirrordata/apache/htdocs/admin/exe/
ssh -n qateam@hgdownload.soe.ucsc.edu "cd /mirrordata/apache/htdocs/admin/exe; ln userApps.src.tgz ./userApps.v${BRANCHNN}.src.tgz"

# hgwdownload-sd
### ssh -n qateam@hgdownload-sd.sdsc.edu "rm -f /mirrordata/apache/htdocs/admin/exe/userApps.src.tgz"
# in case the script was already run before:
### ssh -n qateam@hgdownload-sd.sdsc.edu "rm -f /mirrordata/apache/htdocs/admin/exe/userApps.v${BRANCHNN}.src.tgz"
### scp -p userApps.src.tgz qateam@hgdownload-sd.sdsc.edu:/mirrordata/apache/htdocs/admin/exe/
### ssh -n qateam@hgdownload-sd.sdsc.edu "cd /mirrordata/apache/htdocs/admin/exe; ln userApps.src.tgz ./userApps.v${BRANCHNN}.src.tgz"

# genome-euro
ssh -n qateam@genome-euro "rm -f /mirrordata/apache/htdocs/admin/exe/userApps.src.tgz"
# in case the script was already run before:
ssh -n qateam@genome-euro "rm -f /mirrordata/apache/htdocs/admin/exe/userApps.v${BRANCHNN}.src.tgz"
scp -p userApps.src.tgz qateam@genome-euro:/mirrordata/apache/htdocs/admin/exe/
ssh -n qateam@genome-euro "cd /mirrordata/apache/htdocs/admin/exe; ln userApps.src.tgz ./userApps.v${BRANCHNN}.src.tgz"

rm -f userApps.src.tgz
cd /hive/groups/browser/newBuild/v${BRANCHNN}_branch/kent/src/userApps
rm -fr ./kent ./samtabix *.zip
