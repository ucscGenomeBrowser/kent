#!/bin/tcsh
source `which qaConfig.csh`

###############################################
# 
#  12-13-05
#  Robert Kuhn
#
#  check to see if there are genes on all chroms.
# 
###############################################


if ( "$HOST" != "hgwdev" ) then
 echo "\n  error: you must run this script on dev!\n"
 exit 1
endif

set db=""
set oldDb=""
set table=""
set host2=""
set chrom=""
set chroms=""
set old=""
set new=""
set machineOut=""
set split=""
set regular=""
set random=""
set max=""
set eachX=1
set histo="false"
set histosize=35
set histosize1=35
set histosize2=35

set debug=true 
set debug=false

if ( $#argv < 2 ||  $#argv > 5 ) then
  # no command line args
  echo
  echo "  check to see if there are annotations on all chroms."
  echo "  will check to see if chrom field is named tName or genoName."
  echo
  echo "    usage:  database1 table [database2] [RR] [histogram]"
  echo
  echo "      checks database1 on dev"
  echo "      database2 will be checked on beta by default"
  echo "        if RR is specified, will use genome-mysql"
  echo "      histogram option prints bar graph, not values"
  echo
  exit
else
  set db=$argv[1]
  set table=$argv[2]
endif

if ( $#argv == 3 || $#argv == 4 ) then
  if ( $argv[3] == "histogram" ) then
    set histo="true"
  else
    if ( $argv[3] == "RR" || $argv[3] == "rr" ) then
      set host2="mysql -h genome-mysql -u genome -A"
      set oldDb=$db
      set machineOut="(${argv[3]})"
    else
      set host2="hgsql -h $sqlbeta"
      set machineOut="(hgwbeta)"
      if ( $argv[3] == "hgwbeta" ) then
        # allow use of "hgwbeta" to check same db in two places
        set oldDb=$db
      else
        # argv[3] must be a db
        set oldDb=$argv[3] 
      endif
    endif
  endif
endif

if ( $#argv > 3 ) then
  if ( $argv[4] == "histogram" ) then
    set histo="true"
  else
    set oldDb=$argv[3]
    set machineOut="(${argv[4]})"
    if ( $argv[4] == "hgwbeta" ) then
      set host2="hgsql -h $sqlbeta"
    else 
      if ( $argv[4] == "RR" || $argv[4] == "rr" ) then
        set host2="mysql -h genome-mysql -u genome -A"
      else
        echo
        echo "4th parameter must be RR or hgwbeta"
        echo
        $0
        exit 1
      endif
    endif
  endif
endif

if ( $#argv == 5 ) then
  if ( $argv[5] == "histogram" ) then
    set histo="true"
  endif
endif

if ( $debug == true) then
  echo "db = $db"
  echo "oldDb = $oldDb"
  echo "machineOut = $machineOut"
  echo "table = $table"
  echo "host2 = $host2"
endif

set chroms=`hgsql -N -e "SELECT chrom FROM chromInfo" $db`
set split=`getSplit.csh $db $table`
if ( $status ) then
  echo "\n  the database or table may not exist\n"
  exit
endif

if ( $split == "unsplit" ) then
  set split=""
else
  set split=${split}_
  echo "\n  split tables. e.g., $split$table"
endif

set chrom=`getChromFieldName.csh $db $split$table`
if ( $status ) then
  echo "  error getting chromFieldName."
  echo "   chrom, genoName or tName required."
  echo
  exit 1
endif 

# do randoms last (if no histogram)
if ( $histo == "true" ) then
  set regular=`echo $chroms | sed -e "s/ /\n/g" | grep chr`
else
  set regular=`echo $chroms | sed -e "s/ /\n/g" | grep -v random`
  set  random=`echo $chroms | sed -e "s/ /\n/g" | grep random`
endif

rm -f Xout$$
rm -f XgraphFile0$$
foreach c ( $regular $random )
  if ( $split != "" ) then
    set table="${c}_$table"
  endif
  set new=`nice hgsql -N -e 'SELECT COUNT(*) FROM '$table' \
     WHERE '$chrom' = "'$c'"' $db`
  if ( $machineOut != "" ) then
    set old=`nice $host2 -Ne 'SELECT COUNT(*) FROM '$table' \
      WHERE '$chrom' = "'$c'"' $oldDb`
  endif 
  # output
  echo "$c\t$new\t$old" >> Xout$$
  set table=$argv[2]
end

if ( $histo == "true" ) then
  cat Xout$$ | grep chr | egrep -v "random|hap|Un|$db" | sed "s/chr//" \
    | sort -n -k1,1  > XgraphFile0$$
  set max1=`cat XgraphFile0$$ | awk '{print $2}' | sort -n | tail -1`
  set max=$max1
  if ( $machineOut != "" ) then
    # get max values for 2nd dataset for scaling purposes
    set max2=`cat XgraphFile0$$ | awk '{print $3}' | sort -n | tail -1`
    if ( $max1 > $max2 ) then
      # shrink second histogram proportionately
      set histosize2=`echo $max1 $max2 $histosize | awk '{printf("%2d", $2/$1*$3)}'`
      set max=$max1
    else
      # shrink first histogram proportionately
      set histosize1=`echo $max1 $max2 $histosize | awk '{printf("%2d", $1/$2*$3)}'`
      set max=$max2
    endif

    if ( $debug == true) then
      echo max1 max2 eachX $max1 $max2 $eachX
      echo histosize.histosize1.histosize2 $histosize.$histosize1.$histosize2
    endif

    # make two-col files for graph.csh
    cat XgraphFile0$$ | awk '{print $1, $2}' > XgraphFile1$$ 
    cat XgraphFile0$$ | awk '{print $1, $3}' > XgraphFile2$$ 
    graph.csh XgraphFile1$$ $histosize1 > Xgraph1$$
    graph.csh XgraphFile2$$ $histosize2 > Xgraph2$$
    # put a . into files where the value is blank, to keep join from collapsing
    cat Xgraph1$$ | egrep "." | awk '{ if ($2 == "") { $2 = "."; } print $1, $2; }' > Xgraph1b$$
    cat Xgraph2$$ | egrep "." | awk '{ if ($2 == "") { $2 = "."; } print $1, $2; }' > Xgraph2b$$
    # output header
    echo
    echo "chr \t$db \t$oldDb$machineOut" | awk '{printf("%3s %'$histosize1's %-'$histosize2's\n", $1, $2, $3)}'
    echo
    # join on first col, retaining everything from first col.  print both graphs side by side.
    join -a1 -j1 Xgraph1b$$ Xgraph2b$$ | awk '{printf("%3s %'$histosize1's %-'$histosize2's\n", $1, $2, $3)}'
  else
    # only one data set. graph it.
    graph.csh XgraphFile0$$ | awk '{printf("%3s %-'$histosize's\n", $1, $2)}'
  endif

  # print some stats
  if ($max > $histosize) then
    set eachX=`echo $max1 $histosize | awk '{printf("%d", $1/$2)}'`
  else
    set eachX=1
  endif
  echo "max = $max1 | each x = $eachX"
  echo
else
  # print text, not histogram
  # output header
  echo "chrom \t$db \t$oldDb$machineOut" 
  cat Xout$$
endif

if ( $debug == false ) then
  rm -f Xgraph1$$
  rm -f Xgraph2$$
  rm -f Xgraph1b$$
  rm -f Xgraph2b$$
  rm -f XgraphFile0$$
  rm -f XgraphFile1$$
  rm -f XgraphFile2$$
  rm -f Xout$$
endif 
