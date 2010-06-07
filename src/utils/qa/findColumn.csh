#!/bin/tcsh
source `which qaConfig.csh`

################################
#  04-21-04
#  searches entire database for column that matches input
#  Robert Kuhn
################################

set db=""
set field=""
set machine="beta"
set host="-h $sqlbeta"

if ( $#argv < 2 || $#argv > 3 ) then
  echo
  echo "  searches database for all tables containing a specified column name."
  echo
  echo "    usage:  database, field, [machine = hgwdev|hgwbeta] "
  echo "      (defaults to beta)"
  echo
  exit
else
  set db=$argv[1]
  set field=$argv[2]
endif


if ( $#argv == 3 ) then
  set machine=$argv[3]
  if ( $machine == "hgwdev" ) then
    echo "using hgwdev"
    set machine="dev"
    set host=""
  else
    echo "  ${0}:"
    $0
    exit 1
  endif
else
  echo "using hgwbeta"
endif

# --------------------------------------------
#  get list of all tables:

hgsql -N $host -e "SHOW TABLES" $db > $db.$machine.tables


# --------------------------------------------
#  search list for given field:

echo
foreach table (`cat $db.$machine.tables`)
  hgsql -N $host -e "DESC $table" $db | gawk '{print $1}' > $db.$table.Cols
  foreach column (`cat $db.$table.Cols`)
    if ($column == $field) then
      echo $table
    endif
  end
  rm -f $db.$table.Cols
end
echo


rm -f $db.$machine.tables
