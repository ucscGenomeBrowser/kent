#!/bin/sh
#
#	UCSC Genome Browser - Version VER DATE_STAMP
#		MySQL user permissions setup
#
#	$Id: ex.MySQLUserPerms.sh,v 1.5 2005/03/07 18:31:56 hiram Exp $
#

SQL_USER=	# set this to "-u<user_name>" if different than your login name
MySQL_USER=${SQL_USER}
export SQL_USER MySQL_USER

if [ -z "${SQL_USER}" ]; then
    MySQL_USER=`id -u -n`
fi

if [ -z "${SQL_PASSWORD}" ]; then
	echo "This scrip needs a MySQL password for MySQL user: ${MySQL_USER}."
	echo -n "Please enter a MySQL password: "
	read SQL_PASSWORD
fi

echo "Testing MySQL password"

mysql ${SQL_USER} -p${SQL_PASSWORD} -e "show tables;" mysql

if [ "$?" -ne 0 ]; then
	echo "I gave that a try, it did not work."
	echo "If your DB access is via a different user than your login name"
	echo "	edit this script and set the SQL_USER variable"
	exit 255
else
	echo "That seems to be OK.  Will now grant permissions to"
	echo "sample browser database access users:"
	echo \
	"User: 'browser', password: 'genome' - full database access permissions"
	echo \
"User: 'readonly', password: 'access' - read only access for CGI binaries."
	echo \
"User: 'readwrite', password: 'update' - readwrite access for hgcentral DB"
fi

#
#	Full access to all databases for the user 'browser'
#	This would be for browser developers that need read/write access
#	to all database tables.  In this example, we are practicing
#	strict MySQL security by specifically specifying each database.
#	When new databases are added, this would need to be repeated.
#	The alternative would be to open up access to all databases *.*
#	The permissions here are also limited to localhost only.
#
for DB in cb1 hgcentral hgFixed hg17 proteins040315
do
    mysql ${SQL_USER} -p${SQL_PASSWORD} -e "GRANT SELECT, INSERT, UPDATE, \
	DELETE, CREATE, DROP, ALTER on ${DB}.* TO browser@localhost \
	IDENTIFIED BY 'genome';" mysql
done

#
#	Read only access to genome databases for the browser CGI binaries
#
for DB in cb1 hgFixed hg17 proteins040315
do
    mysql ${SQL_USER} -p${SQL_PASSWORD} -e "GRANT SELECT on \
	${DB}.* TO readonly@localhost IDENTIFIED BY 'access';" mysql
done

#
#	Readwrite access to hgcentral for browser CGI binaries to
#	maintain user's "shopping" cart cookie settings
#
for DB in hgcentral
do
    mysql ${SQL_USER} -p${SQL_PASSWORD} -e "GRANT SELECT, INSERT, UPDATE, \
	DELETE, CREATE, DROP, ALTER on ${DB}.* TO readwrite@localhost \
	IDENTIFIED BY 'update';" mysql
done
