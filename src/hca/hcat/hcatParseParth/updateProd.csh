#!/bin/tcsh -efx
cd parsedOut
hgsql hcat -e "delete from hcat_tracker"
sqlUpdateRelated hcat hcat_tracker.tsv
