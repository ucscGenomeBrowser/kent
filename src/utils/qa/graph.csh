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
set xWidth=3  
set binsize=1

set debug=true 
set debug=false

if ( $#argv < 1 || $#argv > 2 ) then
  # no command line args
  echo
  echo "  make a bar graph from a two-column file."
  echo
  echo "    usage: $0 file [width]"
  echo
  echo "  where width is number of characters in longest bar"
  echo "  negative numbers in second field of file show as zero"
  echo "  ignores any columns beyond two"
  echo "  default width = 50 char"
  echo
  exit
else
  set file=$argv[1]
endif

if ( $#argv == 2 ) then
  set width=$argv[2]  # width of graph in chars
endif

# clean out blank lines
cat $file | egrep "." > Xfile$$
set max=`cat $file | grep "." | awk '{print $2}' | sort -nr | head -1`
if ( $width > $max ) then
  set binsize=1
else
  set binsize=`echo $max $width | awk '{printf("%3.0f", $1/$2)}'`
endif

# find  width of first col for pretty output
foreach item ( `cat Xfile$$ | awk '{print $1}'` )
  set len=`echo $item | awk '{print length($1)}'`
  if ( $len > $xWidth ) then
    set xWidth=$len
  endif
end

if (  $debug == "true" ) then
  echo max $max
  echo binsize $binsize
  echo $max $binsize | awk '{print "max/binsize:", $1/$2}'
  echo width $width
  echo xWidth $xWidth
  awk '{printf "%5s %3.0f \n", $1, $2/n}' n=$binsize $file 
endif

echo
awk '{y=x; i=1; while ( i <= $2/n ) (y="x"y) i++} {printf("%'$xWidth's %-'$width's\n", $1, y)}' n=$binsize $file 
echo

rm -f Xfile$$
exit
