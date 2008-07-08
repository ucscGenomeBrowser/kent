#!/bin/sh

if test "$MACHTYPE x" = " x" ; then
	echo "ERROR: missing MACHTYPE definition in environment"
	echo "it should be something simple: i386 i686 sparc alpha x86_64 ppc, etc..."
	exit 255
else
	WC=`echo $MACHTYPE | grep "\-" 2>/dev/null | wc -l`
	if test "$WC" -ne 0 ; then
		echo "ERROR: can not use a MACHTYPE with - character"
		echo "MACHTYPE is: '$MACHTYPE'"
		echo "it should be something simple: i386 i686 sparc alpha x86_64 ppc, etc..." 
		echo "try the commands: 'uname -m' or 'uname -p' or 'uname -a'"
		echo "to see a possible value for your system"
		exit 255
	fi
fi
exit 0
