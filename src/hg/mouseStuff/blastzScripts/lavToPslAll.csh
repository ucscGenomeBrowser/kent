#!/bin/csh
#Should be run in lav directory.

foreach i (chr*)
    foreach j ($i/*.lav)
        echo processing $j
	lavToPsl $j $j:r.psl
    end
end
