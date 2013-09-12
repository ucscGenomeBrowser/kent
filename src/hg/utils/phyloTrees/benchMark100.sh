#!/bin/sh
/cluster/bin/phast/tree_doctor --prune panPan1,tarSyr1,micMur1,tupBel1,dipOrd1,proCap1,choHof1,croPor1,gavGan1,myoBra1,strCam1,allSin1 112way.nh \
  | ./asciiTree.pl /dev/stdin | sed -e 's/00*,/,/g; s/00*)/)/g'
