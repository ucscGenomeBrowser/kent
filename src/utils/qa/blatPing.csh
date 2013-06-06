#!/bin/tcsh
source `which qaConfig.csh`

onintr cleanup

cd ${HOME}

if ( "$HOST" != "hgwdev" ) then
    echo "error: you must run this script on dev!"
    exit 1
endif

hgsql -h $sqlrr hgcentral -B -N -e "SELECT db, host, port FROM blatServers \
  ORDER BY db, host, port" > blatList$$

#Need to do it in two steps so list is not too long giving word too long error.
set list = (`head -100 blatList$$`)
set list2 = (`tail --lines=+101 blatList$$`)

# next line just for testing:
#set list = (xx1 blat13 17779 xx2 blat14 17779 xx3 blat12 17779)
set problems = ()
while ( "$list" != "" )
    while ( "$list" != "" )
	set db = $list[1]
	shift list
	set host = $list[1]
	shift list
	set port = $list[1]
	shift list
	#-- use to remove monotonous long running blat failures:
	#if ("$db" != "rn2") then
	 gfServer status $host $port > /dev/null
	 set err = $status
	 if ( $err ) then
	    echo "error $err on ${db} ${host}:${port}"
	    set problems = ($problems "${db} ${host}:${port}\n")
	 endif
	#endif
    end
    set list = ( $list2 )
    set list2 = ()
end
if ( "$problems" != "") then
    echo "Summary:"
    echo "problems:"
    echo "$problems"
endif
cleanup:
    rm -f blatList$$
