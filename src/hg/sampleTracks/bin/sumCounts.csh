#!/bin/csh
if( -e count.out ) rm count.out
foreach f (*.cnts)
    awk 'BEGIN{m=0;t=0} ($1 !~ "#"){t+=$4;m+=$5+$10+$15+$20} END{print(m,t,m/t)}' $f >> count.out
end
awk 'BEGIN{a=0;b=0} {a+=$1;b+=$2} END{print(a,b,a/b)}' count.out
rm count.out
