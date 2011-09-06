#!/bin/csh -f
set i = 0

#echo === trial $i ===
#make udcTest
#echo ""
#sleep 5
#set i = 1

while ($i < 1000)
  echo === trial $i ===
  # if you want to clear the cache before each trial
  rm -fr /data/tmp/galt/udcCache
  #./bin/x86_64/udcTest
  ./bin/x86_64/udcTest -protocol=http
  #./bin/x86_64/udcTest -protocol=http -raBuf
  #./bin/x86_64/udcTest -protocol=http -fork
  echo ""
  #sleep 5
  sleep 1
  set i = `expr $i + 1`
end
