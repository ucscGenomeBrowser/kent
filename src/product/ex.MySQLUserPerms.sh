#!/bin/sh
#
#	UCSC Genome Browser - Version VER DATE_STAMP
#		MySQL user permissions setup
#
#	$Id: ex.MySQLUserPerms.sh,v 1.7 2008/01/03 19:44:56 hiram Exp $
#

SQL_USER=""	# set this to "-u<user_name>" if different than your login name
SQL_PASSWORD=""	# set this to the MySQL password for the SQL_USER
MySQL_USER=${SQL_USER}
export SQL_USER MySQL_USER SQL_PASSWORD

if [ -z "${SQL_USER}" ]; then
    MySQL_USER="-u`id -u -n`"
fi

if [ -z "${SQL_PASSWORD}" ]; then
	echo "This scrip needs a MySQL password for MySQL user: ${MySQL_USER}."
	echo -n "Please enter a MySQL password: "
	read SQL_PASSWORD
fi

MYSQL="mysql ${MySQL_USER} -p${SQL_PASSWORD}"
export MYSQL

echo "Testing MySQL password"

${MYSQL} -e "show tables;" mysql

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
#    MySQL version 5.5 requires the LOCK TABLES permission here
#    CREATE, DROP, ALTER, LOCK TABLES, CREATE TEMPORARY TABLES on ${DB}.*
#
for DB in cb1 hgcentral hgFixed hg17 proteins040315
do
    ${MYSQL} -e "GRANT SELECT, INSERT, UPDATE, DELETE, FILE, \
	CREATE, DROP, ALTER, CREATE TEMPORARY TABLES on ${DB}.* TO browser@localhost \
	IDENTIFIED BY 'genome';" mysql
done
# FILE permission for this user to all databases to allow DB table loading with
#	statements such as: "LOAD DATA INFILE file.tab"
#	Considerations to allow LOCAL in such a load statement should
#	be investigated with MySQL documentation, for example:
#	http://dev.mysql.com/doc/refman/5.1/en/load-data.html
#	http://dev.mysql.com/doc/refman/5.1/en/load-data-local.html
${MYSQL} -e "GRANT FILE on *.* TO browser@localhost \
	IDENTIFIED BY 'genome';" mysql
#
#	Read only access to genome databases for the browser CGI binaries
#
for DB in cb1 hgFixed hg17 proteins040315
do
    ${MYSQL} -e "GRANT SELECT, CREATE TEMPORARY TABLES on \
	${DB}.* TO readonly@localhost IDENTIFIED BY 'access';" mysql
done

#
#	Read only access to mysql database for browser user
#
for DB in mysql
do
    ${MYSQL} -e "GRANT SELECT on \
	${DB}.* TO browser@localhost IDENTIFIED BY 'genome';" mysql
done

#
#	Readwrite access to hgcentral for browser CGI binaries to
#	maintain user's "shopping" cart cookie settings
#
for DB in hgcentral
do
    ${MYSQL} -e "GRANT SELECT, INSERT, UPDATE, \
	DELETE, CREATE, DROP, ALTER on ${DB}.* TO readwrite@localhost \
	IDENTIFIED BY 'update';" mysql
done

${MYSQL} -e "FLUSH PRIVILEGES;"

