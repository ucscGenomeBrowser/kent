#!/bin/tcsh

cd ${HOME}

if ( "$HOST" != "hgwdev" ) then
    echo "error: you must run this script on dev!"
    exit 1
endif

hgsql -h genome-centdb hgcentral -B -N -e "SELECT db, host, port FROM blatServers \
  ORDER BY db, host, port" > blatList
set list = (`cat blatList`)

# next line just for testing:
#set list = (xx1 blat13 17779 xx2 blat14 17779 xx3 blat12 17779)
set problems = ()
while ( "$list" != "" )
    set db = $list[1]
    shift list
    set host = $list[1]
    shift list
    set port = $list[1]
    shift list
    #-- use to remove monotonous long-running blat failures:
    #if ("$db" != "rn2") then
     echo "$db $host $port"
     gfServer status $host $port > /dev/null
     set err = $status
     if ( $err ) then
 	echo "error $err on ${db} ${host}:${port}"
 	set problems = ($problems"${db} ${host}:${port}\n")
     endif
    #endif
end
echo
echo "Summary:"
if ( "$problems" != "") then
    echo "problems:"
    echo "$problems"
    echo "$problems" | mail -s "BLAT ping failures." galt kuhn donnak
    # use the next line to manually disable if too many failures annoying.
    #mv blatPing.csh blatPing.csh.hold 
else
    echo "blat ping all ok."
    rm -f blatList
endif
