include ../../../../inc/common.mk

PROG = ../encodeExp

test: addTest
	echo tested all
	#rm -r output

# Add experiments from .ra file
addTest: mkdirs
	${PROG} create
	hgsql hgFixed -e "alter table encodeExpNew rename to encodeExpTest_${USER}"
	${PROG} fileAdd input/addTest.in.ra -table=encodeExpTest_${USER}
	${PROG} fileDump output/addTest.out.ra -table=encodeExpTest_${USER}
	diff output/addTest.out.ra expected/

mkdirs:
	@${MKDIR} output

clean:
	hgsql hgFixed -e "drop table if exists encodeExpTest_${USER}"
	rm -rf output
