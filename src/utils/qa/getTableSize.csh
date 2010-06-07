#!/bin/tcsh
source `which qaConfig.csh`

################################
#  
#  01-24-07
#  Robert Kuhn
#
#  gets size of table from TABLE STATUS dumps
#
################################

set tableinput=""
set tables=""
set db=""
set list="false"
set expand=""
set tableString=""
set wildTables=""
set wildTables2=""
set first=""
set firstIndex=""
set second=""
set secondIndex=""
set machine1="hgwbeta"
set machine2=""
set mach1Tot=0
set mach2Tot=0

if ( $#argv < 2 || $#argv > 4 ) then
  echo
  echo "  gets size of table from TABLE STATUS dumps"
  echo
  echo "    usage:  database tablelist [machine1] [machine2]"
  echo ""
  echo "           tablelist may be single table"
  echo "           default to hgwbeta.  machines accept hgwdev, hgwbeta, rr "
  echo "           uses overnight STATUS dumps.  not real time"
  echo '             accepts "%" wildcard'
  echo
  exit
else
  set db=$argv[1]
  set tableinput=$argv[2]
endif

if ( "$HOST" != "hgwdev" ) then
 echo "\n error: you must run this script on dev!\n"
 exit 1
endif

# set machine names and check validity
if ( $#argv > 2 ) then
  set machine1="$argv[3]"
  echo $machine1 | egrep -q "hgwdev|hgwbeta|rr"
  if ( $status ) then
    echo
    echo "$machine1 is not a valid argument"
    echo
    exit 1
  endif
endif
if ( $#argv == 4 ) then
  set machine2="$argv[4]"
  echo $machine2 | egrep -q "hgwdev|hgwbeta|rr"
  if ( $status ) then
    echo
    echo "$machine2 is not a valid argument"
    echo
    exit 1
  endif
endif

# check if it is a file or a tablename and set list
file $tableinput | egrep -q "ASCII text"
if ( $status ) then
  set tables=`echo $tableinput`
else
  set tables=`cat $tableinput`
  set list="true"
endif

# check for wildcards
echo $tables | grep -q % 
if ( ! $status ) then
  # there are wildcards. expand substitution to get all tables
  set list="true"
  set wildcards=`echo $tables | sed -e "s/ /\n/g" | grep %`
  foreach wildcard ( $wildcards )
    # find all the tables that meet the wildcard criterion
    echo $wildcard | egrep -q '.%.'
    if ( ! $status ) then
      # wildcard in middle
      set frags=`echo $wildcard | awk -F% '{print NF}'`
      if ( $frags > 2 ) then
        echo
        echo "  sorry, only one internal wildcard is supported."
        echo
        exit 1
      endif
      set part1=`echo $wildcard | awk -F% '{print $1}'`
      set part2=`echo $wildcard | awk -F% '{print $2}'`
      set tableString='^'${part1}'.*'${part2}'$'

    else 
      echo $wildcard | egrep -q '^%.'
      if ( ! $status ) then
        # wildcard at beginning
        set tableString=`echo $wildcard | sed -e "s/%//"`
        echo $tableString | egrep -q '.%$'
        if ( $status ) then
          # wildcard at beginning only
          set tableString=`echo ${tableString}'$'`
        else
          # wildcard at end, too 
          set tableString=`echo $wildcard | sed -e "s/%//g"`
        endif

      else
        echo $wildcard | egrep -q '.%$'
        if ( ! $status ) then
          # wildcard at end only
          set tableString=`echo $wildcard | sed -e "s/%//g"`
          set tableString=`echo '^'${tableString}`
        endif
      endif
    endif

    # add in all the tables that meet the wildcard criterion
    set wildTables=`getRRdumpfile.csh $db $machine1 | xargs awk '{print $1}' \
      | egrep "$tableString"`
    if ( "" != $machine2 ) then
      set wildTables2=`getRRdumpfile.csh $db $machine2 | xargs awk '{print $1}' \
        | grep "$tableString"`
    endif
    set expand=`echo $wildTables $wildTables2 | sed -e "s/ /\n/"g | sort -u `  
    set tables=`echo $tables | sed -e "s/$wildcard/& $expand/"`
    set tableString=""
  end
endif

# make headers for output table
# get width of first column from longest tablename
set longtable="table"
set length=5
foreach table (`echo $tables`)
  set len=`echo $table | awk '{print length($1)}'`
  if ( $len > $length ) then
    set length=$len
    set longtable=$table
  endif
end
set length=`echo $length | awk '{print $1+1}'`
set offset=`echo $longtable | sed -e "s/./-/g"`
set offset2=`echo $longtable | sed -e "s/./x/g"`
echo

# print headers
if ( "" == $machine2 ) then
  # print top header
  echo "xxxxx" "$machine1" \
  | gawk '{ printf("%-'${length}'s %17s \n", $1, $2) }' \
  | sed -e "s/xxxxx/     /"
  echo "$offset2 " "-----------------" \
  | gawk '{ printf("%-'${length}'s %17s \n", $1, $2) }' \
  | sed -e "s/x/ /g"
  
  # print second header
  echo "table" "Mbytes" "index" \
  | gawk '{ printf("%-'${length}'s %8s %8s \n", $1, $2, $3) }'
  echo "$offset" "--------" "--------" \
  | gawk '{ printf("%-'${length}'s %8s %8s \n", $1, $2, $3) }'
else
  # print top header
  echo "xxxxx" "$machine1" "$machine2" \
  | gawk '{ printf("%-'${length}'s %17s %17s \n", $1, $2, $3) }' \
  | sed -e "s/xxxxx/     /"
  echo "$offset2 " "-----------------" "-----------------" \
  | gawk '{ printf("%-'${length}'s %17s %-17s \n", $1, $2, $3) }' \
  | sed -e "s/x/ /g"
  
  # print second header
  echo "table" "Mbytes" "index" "Mbytes" "index" \
  | gawk '{ printf("%-'${length}'s %8s %8s %8s %8s \n", \
  $1, $2, $3, $4, $5) }'
  echo "$offset" "--------" "--------" "--------" "--------" \
  | gawk '{ printf("%-'${length}'s %-8s %-8s %-8s %-8s \n", \
    $1, $2, $3, $4, $5) }'
endif

# process sizes
foreach table ($tables)
  # print place-saver for wildcards
  echo $table | egrep -q %
  if ( ! $status ) then
    echo " -${table}-"
    continue
  endif

  set first=`getRRtableStatus.csh $db $table Data_length $machine1`
  if ( $status ) then
    set first=0
  else
    set first=`echo $first | awk '{printf("%0.2f", $1/1000000) }'`
  endif 

  set firstIndex=`getRRtableStatus.csh $db $table Index_length $machine1`
  if ( $status ) then
    set firstIndex=0
  else
    set firstIndex=`echo $firstIndex | awk '{printf("%0.2f", $1/1000000) }'`
  endif 

  # process second machine, if needed
  if ( "" != $machine2) then
    set second=`getRRtableStatus.csh $db $table Data_length $machine2`
    if ( $status ) then
      set second=0
    else
      set second=`echo $second | awk '{printf("%0.2f", $1/1000000) }'`
    endif 
    
    set secondIndex=`getRRtableStatus.csh $db $table Index_length $machine2`
    if ( $status ) then
      set secondIndex=0
    else
      set secondIndex=`echo $secondIndex | awk '{printf("%0.2f", $1/1000000) }'`
    endif 
  endif 

  # increment totals
  # increment index separately?
  set mach1Tot=`echo $mach1Tot $first $firstIndex | awk '{print $1+$2+$3}'`
  set mach2Tot=`echo $mach2Tot $second $secondIndex | awk '{print $1+$2+$3}'`

  # output results
  if ( "" == $machine2 ) then
      # print results
      echo "$table" "$first" "$firstIndex" \
      | gawk '{ printf("%-'${length}'s %8s %8s \n", $1, $2, $3) }'
  else
      # print results
      echo "$table" "$first" "$firstIndex" "$second" "$secondIndex" \
      | gawk '{ printf("%-'${length}'s %8s %8s %8s %8s \n", \
      $1, $2, $3, $4, $5) }'
  endif
end 

# output totals if more than one table
echo
if ( "true" == $list ) then
  echo $machine1 "total" "=" $mach1Tot megabytes \
    | awk '{printf("%7s %5s %1s %8s %9s\n", $1, $2, $3, $4, $5)}'
  if ( "" != $machine2 ) then
    echo $machine2 "total" "=" $mach2Tot megabytes \
      | awk '{printf("%7s %5s %1s %8s %9s\n", $1, $2, $3, $4, $5)}'
  endif
  echo "(total includes index)"
  echo
endif 

echo "as of STATUS dumps:"
checkTableStatus.csh | grep $machine1 \
   | awk '{ printf("%8s %9s\n", $1, $2)}'
if ( "" != $machine2 ) then
  checkTableStatus.csh | grep $machine2 \
     | awk '{ printf("%8s %9s\n", $1, $2)}'
endif
echo


