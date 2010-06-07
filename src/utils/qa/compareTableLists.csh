#!/bin/tcsh
source `which qaConfig.csh`

if ( $1 == "" ) then
 echo ""
 echo "please specify an assembly to compare tables lists between hgwdev and hgwbeta."
 echo "example> $0 hg16"
 echo ""
 exit 1
endif

set machine1 = "hgwdev"
set machine2 = "hgwbeta"

set db = $1

rm -f $machine1.$db.tables 
rm -f $machine2.$db.tables 

hgsql $db -e "show tables;" > $machine1.$db.tables 

hgsql -h $machine2 $db -e "show tables;" > $machine2.$db.tables 

"diff" $machine1.$db.tables $machine2.$db.tables
if ( $status ) then
 echo "Differences in tables found between $machine1 and $machine2"
else
 echo "No differences in trackDb."
endif

