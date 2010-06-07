#!/bin/csh
#Should be run in lav directory.

foreach i (chr*)
    foreach j ($i/*.lav)
        echo processing $j
	axtBest $j.axt $i $j.best.axt
    end
end

mkdir ../axtBest
foreach i (chr*)
    echo $i
    liftUp stdout -type=.axt ~/mm/jkStuff/liftAll.lft warn $i/*.best.axt -axtQ | axtBest stdin $i ../axtBest/$i.axt 
end

    
