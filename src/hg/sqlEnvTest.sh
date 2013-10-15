#!/bin/sh

# special test to avoid this during external userApps build
if test "$NOSQLTEST" = "1" ; then
  exit 0
fi

export UNAME=`uname -n`

if test "$UNAME" = "hgwdev" ; then
  exit 0
fi

if test -f `which mysql_config` ; then
  exit 0
fi

if test "$MYSQLLIBS x" = " x" -o "$MYSQLINC x" = " x" ; then
    echo "ERROR: missing MYSQLLIBS or MYSQLINC definitions in environment"
    echo "these are typically,"
    echo "  for bash shell users:"
    echo '    $ export MYSQLLIBS="/usr/lib/mysql/libmysqlclient.a -lz"'
    echo "    $ export MYSQLINC=/usr/include/mysql"
    echo "  for csh/tcsh shell users:"
    echo '    % setenv MYSQLLIBS "/usr/lib/mysql/libmysqlclient.a -lz"'
    echo "    % setenv MYSQLINC /usr/include/mysql"
    echo "See also: http://genome.ucsc.edu/admin/jk-install.html"
    exit 255
fi
exit 0
