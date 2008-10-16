#!/bin/tcsh
cd $WEEKLYBLD

if ( "$HOST" != "hgwdev" ) then
	echo "error: you must run this script on hgwdev!"
	exit 1
endif

hgsqldump --all -d -c -h genome-centdb hgcentral \
sessionDb userDb | sed -e "s/genome-centdb/localhost/" > \
/tmp/hgcentral.sql

hgsqldump --all -c -h genome-centdb hgcentral \
defaultDb blatServers dbDb dbDbArch gdbPdb liftOverChain clade genomeClade | \
sed -e "s/genome-centdb/localhost/" >> /tmp/hgcentral.sql
echo
echo "*** Diffing old new ***"
diff /usr/local/apache/htdocs/admin/hgcentral.sql /tmp/hgcentral.sql
if ( ! $status ) then
	echo
	echo "No differences."
	echo
endif 

if ( "$1" != "real" ) then
	echo
	echo "Not real.   To make real changes, put real as cmdline parm."
	echo
	exit 0
endif 

rm /usr/local/apache/htdocs/admin/hgcentral.sql
cp -p /tmp/hgcentral.sql /usr/local/apache/htdocs/admin/hgcentral.sql
rm hiding/hgcent/hgcentral.sql
cp -p /tmp/hgcentral.sql hiding/hgcent/hgcentral.sql
cd hiding/hgcent
set temp = '"'"v${BRANCHNN}"'"'
cvs -d hgwdev:/projects/compbio/cvsroot commit -m $temp  hgcentral.sql
if ( $status ) then
	echo "error during cvs commit of hgcentral.sql."
	exit 1
endif


echo
echo "Push request:"
echo "Please push from dev-->hgdownload "
echo "  /usr/local/apache/htdocs/admin/hgcentral.sql"
echo
echo "Please also push the raw hgcentral mysql tables"
echo "from the RR to hgdownload."
echo
echo "reason: (describe here)"
echo

exit 0

