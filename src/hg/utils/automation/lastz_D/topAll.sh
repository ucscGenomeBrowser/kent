#!/bin/bash

# unfortunately, there are commands below that have a fail status
# but it isn't really an error, can not use this 'exit on any failure'
# signal to bash:
# set -beEu -o pipefail

# expects to find script available:
#	./selectedFasta.sh

# the selectedFasta.sh script will create these scripts:
# tune.top100.sh
# tune.top200.sh
# tune.top300.sh
# tune.top400.sh

# runs them in the background and waits for them all to complete, this
# can take some time

# ensure sorting always functions the same:
export LANG=C

if [ $# -lt 2 ]; then
  echo "usage: topAll.sh <target> <query>" 1>&2
  exit 255
fi

export target=$1
export query=$2

if [ -s top.all.list ]; then
  echo "done: $target.$query" 1>&2
else
  if [ -s mafScoreSizeScan.Q ]; then
    mv mafScoreSizeScan.Q mafScoreSizeScan.Q3
  fi
  topDone=`ls tune.top*.sh 2> /dev/null | wc -l`
  if [ $topDone -gt 0 ]; then
      echo 'other tune.top*.sh already exist' 1>&2
  else
      Q3=`cat mafScoreSizeScan.Q3`
      awk '$1 > '$Q3' && $2 > 100 && $7 > 80 && $8 > 80' mafScoreSizeScan.list \
	> top.all.list
      cut -f3,4 top.all.list | sort > selected.topAll.list
      topCount=`cat selected.topAll.list | wc -l`
      echo "number of top scorers: $topCount"
      if [ $topCount -lt 100 ]; then
	echo "constructing top.$topCount.list out of $topCount"
	cut -f3,4 top.all.list | sort > selected.top$topCount.list
	./selectedFasta.sh $target $query top$topCount
	./tune.top$topCount.sh &
      fi
      for N in 100 200 300 400
    do
      if [ $topCount -gt $N ]; then
	echo "constructing top.$N.list out of $topCount"
	cat top.all.list | randomLines -decomment stdin $N top.$N.list
	cut -f3,4 top.$N.list | sort > selected.top$N.list
	./selectedFasta.sh $target $query top$N
	./tune.top$N.sh &
      fi
    done
    echo "waiting for tune jobs to finish"
    ps -f | grep tune.top | grep -v grep
    wait
  fi
fi
