#!/bin/sh
#
#	UCSC Genome Browser - Version 30 - MySQL user permissions setup
#

SQL_USER=	# set this to "-u<user_name>" if different than your login name

if [ -z "${SQL_PASSWORD}" ]; then
	echo "For this scrip to function I need your MySQL password."
	echo -n "Please enter your MySQL password: "
	read SQL_PASSWORD
fi

echo "Testing your password:"

echo "show tables;" | mysql ${SQL_USER} -p${SQL_PASSWORD} mysql

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
#	The addition of hgcentraltest here is for the example of
#	using an alternative hgcentral database.  These will be
#	specified in the cgi-bin/hg.conf file
#
for DBVERSION in cb1 hgcentral hgcentraltest hgFixed
do
    echo "GRANT SELECT, INSERT, UPDATE, DELETE, CREATE, DROP, ALTER \
	on ${DBVERSION}.* TO browser@localhost \
	IDENTIFIED BY 'genome';" | \
	mysql ${SQL_USER} -p${SQL_PASSWORD} mysql
    echo "GRANT SELECT, INSERT, UPDATE, DELETE, CREATE, DROP, ALTER \
	on ${DBVERSION}.* TO browser@\"%\" \
	IDENTIFIED BY 'genome';" | \
	mysql ${SQL_USER} -p${SQL_PASSWORD} mysql
done

#
#	Read only access to genome databases for the browser CGI binaries
#
for DBVERSION in cb1 hgFixed
do
    echo "GRANT SELECT on ${DBVERSION}.* TO readonly@localhost \
	IDENTIFIED BY 'access';" | \
	mysql ${SQL_USER} -p${SQL_PASSWORD} mysql
    echo "GRANT SELECT on ${DBVERSION}.* TO readonly@\"%\" \
	IDENTIFIED BY 'access';" | \
	mysql ${SQL_USER} -p${SQL_PASSWORD} mysql
done

#
#	Readwrite access to hgcentral for browser CGI binaries to
#	maintain user's "shopping" cart cookie settings
#
for DBVERSION in hgcentral hgcentraltest
do
    echo "GRANT SELECT, INSERT, UPDATE, DELETE, CREATE, DROP, ALTER \
	on ${DBVERSION}.* TO readwrite@localhost \
	IDENTIFIED BY 'update';" | \
	mysql ${SQL_USER} -p${SQL_PASSWORD} mysql
    echo "GRANT SELECT, INSERT, UPDATE, DELETE, CREATE, DROP, ALTER \
	on ${DBVERSION}.* TO readwrite@\"%\" \
	IDENTIFIED BY 'update';" | \
	mysql ${SQL_USER} -p${SQL_PASSWORD} mysql
done
