#!/bin/csh
#Should be run in lav directory.

foreach j (chr*)
    foreach i ($j/*.lav.axt)
         set q = $i:r
	 set r = $q:r
	 subsetAxt $i $r.tight.axt ../../cmpAli/coding.mat 3400
	 echo done $i
    end
end
