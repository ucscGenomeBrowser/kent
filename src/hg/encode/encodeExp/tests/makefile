include ../../../../inc/common.mk

PROG = ../encodeExp

test: addTest
	echo tested all
	#rm -r output

# Add experiments from .ra file
addTest: mkdirs
	${PROG} create
	hgsql hgFixed -e "alter table encodeExpNew rename to encodeExpTest_${USER}"
	${PROG} add input/addTest.in.ra -table=encodeExpTest_${USER}
	${PROG} dump output/addTest.out.ra -table=encodeExpTest_${USER}
	grep -v lastUpdated output/addTest.out.ra | diff - expected/addTest.out.ra

mkdirs:
	@${MKDIR} output

clean:
	hgsql hgFixed -e "drop table if exists encodeExpTest_${USER}"
	rm -rf output
