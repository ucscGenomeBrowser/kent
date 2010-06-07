#!/bin/csh -ef
awk -v m=$2 '($5 > m-1) {printf("%s\n", $0)}' $1 > $1.atleast${2}
