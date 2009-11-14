#!/bin/tcsh
cd $WEEKLYBLD

if ( "$HOST" != "hgwdev" ) then
	echo "error: you must run this script on hgwdev!"
	exit 1
endif

hgsqldump --all -d -c -h genome-centdb hgcentral \
sessionDb userDb | sed -e "s/genome-centdb/localhost/" > \
/tmp/hgcentraltemp.sql

hgsqldump --all -c -h genome-centdb hgcentral \
defaultDb blatServers dbDb dbDbArch gdbPdb liftOverChain clade genomeClade targetDb | \
sed -e "s/genome-centdb/localhost/" >> /tmp/hgcentraltemp.sql


# TODO --skip-extended-insert 
#   is supposed to make it dump rows as separate insert statements
#
# get rid of some mysql5 trash in the output we don't want.
# also need to break data values at rows so the diff and cvs 
# which are line-oriented work better.
grep -v "Dump completed on" /tmp/hgcentraltemp.sql | \
sed -e "s/AUTO_INCREMENT=[0-9]* //" \
 | sed -e 's/),(/),\n(/g'  > /tmp/hgcentral.sql

echo
echo "*** Diffing old new ***"
diff /usr/local/apache/htdocs/admin/hgcentral.sql /tmp/hgcentral.sql
if ( ! $status ) then
	echo
	echo "No differences."
	echo
	exit 0
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

# push to hgdownload
ssh -n qateam@hgdownload "rm /mirrordata/apache/htdocs/admin/hgcentral.sql"
scp -p /usr/local/apache/htdocs/admin/hgcentral.sql qateam@hgdownload:/mirrordata/apache/htdocs/admin/


# OLD WAY NOT NEEDED
#echo
#echo "Push request:"
#echo "Please push from dev --> hgdownload "
#echo "  /usr/local/apache/htdocs/admin/hgcentral.sql"
#echo
#echo "reason: (describe here)"

echo
echo "NOTE:  If this is an update of hgcentral that is not part of a new"
echo "build, also ask for the relevant tables to be pushed to hgdownload"
echo "for mirror site access. For example:"
echo "  Please also push the hgcentral/blatServers table"
echo "  from hgnfs1 --> hgdownload."
echo

exit 0

