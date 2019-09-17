#!/bin/csh -f

# Author: Charles Sugnet <sugnet@soe.ucsc.edu>
# Date: 06-16-02
# Project: MGC clone selection.
# Description: Selects the accesions from the database that are from the
#              Athersys RAGE Library so we can feed that list into clusterRna
#              for exclusion.
if($#argv != 2) then
    echo "Selects the accesions from the database that are from the"
    echo "Athersys RAGE Library so we can feed that list into clusterRna"
    echo "for exclusion."
    echo "Usage:"
    echo "   generateRageAccList.csh <db {hg10,hg11,hgN> <outputFile>"
    exit 0
endif
set db = $1
set out = $2

echo "Doing select on $db into $db.$out"
hgsql -N -e 'select distinct(acc) from gbCdnaInfo,library where library.name like "%Athersys%" and library.id=gbCdnaInfo.library' $db > $db.$out
echo "Done."
