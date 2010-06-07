#!/bin/csh -ef

# This is used by cron jobs.  Angie's comments on what it is doing:
# The find command finds broken links, and passes them to ls -l, and that is
# what is in the email output.  If ls -l has no arguments, by default it will
# list the current directory, which I don't want to see in email, so the
# --no-run-if-empty is to suppress that.

find -L $argv -type l -lname \* \
| xargs --no-run-if-empty ls -l
