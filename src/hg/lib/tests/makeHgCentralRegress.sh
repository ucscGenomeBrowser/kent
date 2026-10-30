#!/bin/bash
# makeHgCentralRegress.sh - make the central database that tests are allowed to write to.
#
# Every hgcentral a developer has is shared: hgcentraltest holds thousands of rows of other
# people's sandbox sessions, and the production centrals are not reachable from here at all.
# So a test that needs to create a session, a login or an api key has had nowhere to put it,
# and the tests written so far can only read.  That is the limit this lifts.  refs #38391.
#
# hgcentraltestregress exists for tests only.  Nothing else reads it, no CGI is configured to
# use it, and anything in it may be deleted by the next run, so a test must create the rows it
# needs and clean up after itself rather than expect to find anything.
#
# IT NEEDS A GRANT BEFORE ANY TEST CAN USE IT, and this script cannot make one.  On hgwdev
# hgtestuser has had it since 2026-10-24; any other account needs its own.  The account
# the browser uses for the central holds global SELECT, INSERT, CREATE and DROP, but UPDATE
# and DELETE only on the databases it has been granted them on one at a time.  So a test can
# create this database and insert into it, and cannot clean up after itself: the delete comes
# back 1142.  Measured, not assumed -- and the guess that the hgcentraltest prefix carried a
# wildcard grant is wrong, hgcentraltestregress behaves exactly the same way.
#
# What to ask the admins for, once, against whatever account central.user names:
#
#     GRANT UPDATE, DELETE ON hgcentralregress.* TO '<central.user>'@'localhost';
#
# Without it, a test pointed here can set rows up but not take them down, and
# hubSpaceKeysTest fails.
#
# Schemas are copied from the central the caller's hg.conf already names, with CREATE TABLE
# LIKE, rather than kept as a second copy of the schema in the tree.  A column added to
# namedSessionDb therefore reaches this database the next time the script runs, and there is
# no second definition to forget.
#
# Idempotent: safe to run before every test, and it is.  Existing tables are left alone,
# along with whatever rows they hold.
#
# Usage:
#     makeHgCentralRegress.sh [conf]
# conf defaults to $HGDB_CONF, then to ~/.hg.conf.  The central.host, central.user and
# central.password in it are used, so the account needs CREATE on the new database; on hgwdev
# the ordinary hgtestuser has it.

set -e

conf="${1:-${HGDB_CONF:-$HOME/.hg.conf}}"
if [ ! -r "$conf" ]; then
    echo "makeHgCentralRegress.sh: cannot read $conf" >&2
    exit 1
fi

# The settings are read by hgConfValue, which uses hgConfig.c, so includes are followed to any
# depth and relative to the file that names them, exactly as a CGI follows them.  An earlier
# copy of that logic here followed one level, relative to the current directory, and missed
# central settings three includes down.  The makefile builds hgConfValue before calling this.
hgConfValue="$(dirname "$0")/bin/$(uname -m)/hgConfValue"
if [ ! -x "$hgConfValue" ]; then
    echo "makeHgCentralRegress.sh: no $hgConfValue;" \
        "run make bin/$(uname -m)/hgConfValue in $(dirname "$0")" >&2
    exit 1
fi
{ read -r host; read -r user; read -r password; read -r source; } \
    < <(HGDB_CONF="$conf" "$hgConfValue" central.host central.user central.password central.db)
target=hgcentralregress

if [ -z "$host" ] || [ -z "$user" ] || [ -z "$source" ]; then
    echo "makeHgCentralRegress.sh: $conf has no central.host/user/db" >&2
    exit 1
fi
if [ "$source" = "$target" ]; then
    echo "makeHgCentralRegress.sh: central.db is already $target; nothing to copy from" >&2
    exit 1
fi

# The tables tests have needed so far.  Add to this list rather than creating a table by hand,
# so the next person's run has it too.
tables="apiKeys namedSessionDb sessionDb userDb gbMembers gbNode genark asmAlias dbDb hubStatus"

sql() {
    mysql -h "$host" -u "$user" -p"$password" -N -B -e "$1"
}

sql "create database if not exists $target"
for t in $tables; do
    sql "create table if not exists $target.$t like $source.$t"
done

echo "$target is ready on $host, with: $tables"
