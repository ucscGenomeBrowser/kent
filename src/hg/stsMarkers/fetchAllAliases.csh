#!/bin/csh -ef

##	$Id: fetchAllAliases.csh,v 1.1 2007/08/06 22:11:58 hiram Exp $

set prevMm = $1

if ( "xx${prevMm}yy" == "xxyy" ) then
    echo "usage: fetchAllAliases.csh <previous db>"
    /bin/echo -e "\texample used for mm9: fetchAllAliases.csh mm8"
    /bin/echo -e "\tutilizes the previous stsInfoMouse.bed file:"
    /bin/echo -e "\t/cluster/data/<prevDb>/bed/STSmarkers/stsInfoMouse.bed"
    /bin/echo -e "\tto create a new stsInfoAliases.txt for this new assembly."
    echo "Also verifies UniSTS_mouse.sts and UniSTS.aliases"
    echo "Combines MGI.aliases, UniSTS.aliases with the new stsInfoAliases.txt"
    echo "to produce UniSTS_mouse.alias"
    exit 255
endif

if ( ! -f /cluster/data/${prevMm}/bed/STSmarkers/stsInfoMouse.bed ) then
    echo "ERROR: can not find previous stsInfoMouse.bed at:"
    echo "/cluster/data/${prevMm}/bed/STSmarkers/stsInfoMouse.bed"
    exit 255
endif
set BEDFile = "/cluster/data/${prevMm}/bed/STSmarkers/stsInfoMouse.bed"


echo "processing UniSTS_mouse.sts and UniSTS.aliases to find aliases"
rm -f UniSTS_mouse_alias.0
~/kent/src/hg/stsMarkers/UniSTSParse.pl UniSTS_mouse.sts UniSTS.aliases \
	> UniSTS_mouse_alias.0
echo "verification of UniSTS_mouse.sts and UniSTS.alias, one WARNING is normal"
egrep "WARNING|ERROR" UniSTS_mouse_alias.0

echo "processing MGI.aliases"
rm -f MGI.alises
grep MGI: UniSTS.aliases > MGI.aliases

echo "fetching existing aliases from previous stsInfoMouse.bed file"
rm -f stsInfoAliases.txt
~/kent/src/hg/stsMarkers/stsInfoMouseParse.pl ${BEDFile} > stsInfoAliases.txt
set errCount = `grep ERROR stsInfoAliases.txt | wc -l`
echo "found $errCount potential errors in $BEDFile"
echo "to see the errors: grep ERROR stsInfoAliases.txt"
set tmpFile = `mktemp /tmp/sts.${prevMm}.XXXXXXXX`
echo "some sample errors from ${tmpFile}:"
grep ERROR stsInfoAliases.txt | sort >> ${tmpFile}
head -3 ${tmpFile}
tail -3 ${tmpFile}
rm -f ${tmpFile}
echo "there are always a bunch of errors with that business."

echo "verify those stsInfoMouse.bed aliases with UniSTS.aliases"
rm -f stsInfo.aliases
~/kent/src/hg/stsMarkers/UniSTSParse.pl stsInfoAliases.txt UniSTS.aliases > stsInfo.aliases
set errCount = `/bin/csh -c 'grep "ERROR" stsInfo.aliases | wc -l || /bin/true'`
if ( $errCount > 0 ) then
    echo "there are errors in processing stsInfo.aliases"
    echo 'to see: grep "ERROR" stsInfo.aliases'
    exit 255
endif

cat UniSTS_mouse_alias.0 MGI.aliases stsInfo.aliases | sort -u \
	| sort -n > UniSTS_mouse.alias
echo "Completed"
