#!/bin/tcsh
#
if ( -e ~/bin/${MACHTYPE}.cluster ) then
    mv ~/bin/$MACHTYPE ~/bin/${MACHTYPE}.orig
    mv ~/bin/${MACHTYPE}.cluster ~/bin/$MACHTYPE
else
    echo "symtrick: $MACHTYPE.cluster not found."
endif

