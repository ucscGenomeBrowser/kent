#!/bin/sh
#
#	UCSC Genome Browser - Version VER DATE_STAMP
#		MySQL user permissions setup
#

SQL_USER=	# set this to "-u<user_name>" if different than your login name

if [ -z "${SQL_PASSWORD}" ]; then
	echo "For this scrip to function I need your MySQL password."
	echo -n "Please enter your MySQL password: "
	read SQL_PASSWORD
fi

echo "Testing your password:"

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
#	The addition of hgcentraltest here is for the example of
#	using an alternative hgcentral database.  These will be
#	specified in the cgi-bin/hg.conf file
#
for DB in cb1 hgcentral hgcentraltest hgFixed
do
    mysql ${SQL_USER} -p${SQL_PASSWORD} -e "GRANT SELECT, INSERT, UPDATE, \
	DELETE, CREATE, DROP, ALTER on ${DB}.* TO browser@localhost \
	IDENTIFIED BY 'genome';" mysql
    mysql ${SQL_USER} -p${SQL_PASSWORD} -e "GRANT SELECT, INSERT, UPDATE, \
	DELETE, CREATE, DROP, ALTER on ${DB}.* TO browser@\"%\" \
	IDENTIFIED BY 'genome';" mysql
done

#
#	Read only access to genome databases for the browser CGI binaries
#
for DB in cb1 hgFixed
do
    mysql ${SQL_USER} -p${SQL_PASSWORD} -e "GRANT SELECT on \
	${DB}.* TO readonly@localhost IDENTIFIED BY 'access';" mysql
    mysql ${SQL_USER} -p${SQL_PASSWORD} -e "GRANT SELECT on \
	${DB}.* TO readonly@\"%\" IDENTIFIED BY 'access';" mysql
done

#
#	Readwrite access to hgcentral for browser CGI binaries to
#	maintain user's "shopping" cart cookie settings
#
for DB in hgcentral hgcentraltest
do
    mysql ${SQL_USER} -p${SQL_PASSWORD} -e "GRANT SELECT, INSERT, UPDATE, \
	DELETE, CREATE, DROP, ALTER on ${DB}.* TO readwrite@localhost \
	IDENTIFIED BY 'update';" mysql
    mysql ${SQL_USER} -p${SQL_PASSWORD} -e "GRANT SELECT, INSERT, UPDATE, \
	DELETE, CREATE, DROP, ALTER on ${DB}.* TO readwrite@\"%\" \
	IDENTIFIED BY 'update';" mysql
done
