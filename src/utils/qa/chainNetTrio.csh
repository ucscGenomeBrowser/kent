#!/bin/tcsh

# This script runs the chain.csh, chain2.csh, and net.csh
# scripts in sucession.

# enter the name of the database and the name of the chains/nets.

if ($#argv < 1 || $#argv > 2) then
  echo
  echo " runs the three chain and net scripts in succession"
  echo " now works for unsplit assemblies"
  echo
  echo "    usage:  database Tablename"
  echo
  echo "    database is the 'from' database  "
  echo "    Tablename is the name of the chain tables (must be capitalized)"
  echo "    e.g. chainNetTrio.csh mm8 Hg18"
  echo
  exit
else
  set db=$argv[1]
  set table=$argv[2]
endif

# capitalize the first letter of the "other" chain table:
# just in case
set table=`echo $table | perl -wpe '$_ = ucfirst($_)'`

set split=`getSplit.csh $db chain$table hgwdev`

if ( $split == "unsplit" ) then
  # run the three scripts in order
  nice chain.csh  $db chain$table      >& $db.chain.$table
  nice chain2.csh $db chain$table      >& $db.chain2.$table
  nice net.csh    $db net$table        >& $db.net.$table
else
  # run the three scripts in order
  nice chain.csh  $db chrN_chain$table >& $db.chain.$table
  nice chain2.csh $db chrN_chain$table >& $db.chain2.$table
  nice net.csh    $db net$table        >& $db.net.$table
endif



