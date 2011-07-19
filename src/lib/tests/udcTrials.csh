#!/bin/csh -f
set i = 0
echo === trial $i ===
make udcTest
echo ""
sleep 5

set i = 1
while ($i < 100)
  echo === trial $i ===
  ./bin/x86_64/udcTest
  echo ""
  sleep 5
  set i = `expr $i + 1`
end
