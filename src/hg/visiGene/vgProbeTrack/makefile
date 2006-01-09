include ../../../inc/common.mk

# add the follow to makefiles to enable stronger warning checks
# HG_WARN = ${HG_WARN_ERR}

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = vgProbeTrack.o

vgProbeTrack: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/vgProbeTrack $O ${MYLIBS} $L
#	${STRIP} ${HOME}/bin/${MACHTYPE}/vgProbeTrack${EXE}

autoSql:
	autoSql probeExt.as probeExt -dbLink
	sed -i -e 's/    id int not null/    id int auto_increment not null/' probeExt.sql

clean:
	rm -f $O

fresh:
	vgProbeTrack working -kill -sqlPath=..
    
go:
	vgProbeTrack working
    
