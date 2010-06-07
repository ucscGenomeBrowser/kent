#!/bin/csh
#Should be run in lav directory.

foreach j (chr*)
    set r = $j:r
    cat $j/*.tight.axt | axtToBed stdin ../tightBed/$r:r.bed
    echo done $j
end
