#!/bin/tcsh
source `which qaConfig.csh`

###############################################
# 
#  01-23-2010
#  Robert Kuhn
# 
#  make a graph from a two-column file 
# 
###############################################

set max=1
set width=50 # width of graph in chars
set binsize=1

set debug=true 
set debug=false

if ( $#argv != 1 ) then
  # no command line args
  echo
  echo "  make a graph from a two-column file."
  echo
  echo "    usage: $0 file [width]"
  echo
  echo "  where width is number of characters in longest bar"
  echo "  negative numbers in second field of file show as zero"
  echo
  exit
else
  set file=$argv[1]
endif

cat $file | egrep "." > Xfile$$
set max=`cat $file | grep "." | awk '{print $2}' | sort -nr | head -1`
set binsize=`echo $max $width | awk '{printf("%3.0f", $1/$2)}'`
if (  $debug == "true" ) then
  echo max $max
  echo binsize $binsize
  echo $max $binsize | awk '{print "chars:", $1/$2}'
  awk '{printf "%5s %3.0f \n", $1, $2/n}' n=$binsize $file 
endif

echo
awk '{y=x; i=1; while ( i <= $2/n ) (y="x"y) i++} {print $1, y}' n=$binsize $file 
echo

rm -f Xfile$$
exit
