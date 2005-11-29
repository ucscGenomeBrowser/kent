#!/bin/csh -ef

# $Header: /projects/compbio/cvsroot/kent/src/hg/encode/listEncodeTables.csh,v 1.4 2005/11/29 22:08:07 kate Exp $

# listEncodeTables <db>
# Lists all tables on hgwbeta hg16, beginning with 'encode'
# The track type is listed in the second column
set db = $1
ssh hgwbeta "echo select tableName from trackDb where tableName like \'encode%\' and settings not like \'%composite%\' order by tableName | hgsql -N $db" > tables.txt
foreach t (`cat tables.txt`)
    set type = `ssh hgwbeta "echo select type from trackDb where tableName=\'$t\' | hgsql -N $db"`
    set grp = `ssh hgwbeta "echo select grp from trackDb where tableName=\'$t\' | hgsql -N $db"`
    if ( "$type" == "" ) then
        set settings = `ssh hgwbeta "echo select settings from trackDb where tableName=\'$t\' | hgsql -N $db"`
        set composite = `echo $settings | perl -wpe 's/.*subTrack (.*)\\n.*/$1/'`
        set type = `ssh hgwbeta "echo select type from trackDb where tableName=\'$composite\' | hgsql -N $db"`
        set grp = `ssh hgwbeta "echo select grp from trackDb where tableName=\'$composite\' | hgsql -N $db"`
    endif
    echo "$t	$grp	$type"
end


