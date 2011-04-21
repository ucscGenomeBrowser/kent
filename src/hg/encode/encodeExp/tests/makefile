include ../../../../inc/common.mk

PROG = ../encodeExp

TABLE = encodeExpTest_${USER}

test: addTest accTest showTest idTest
	echo tested all
	@#-rm -rf output

# Add experiments from .ra file
addTest:
	${MAKE} setup
	${PROG} add input/addTest.in.ra -table=${TABLE}
	${PROG} dump output/$@.out.ra -table=${TABLE}
	@grep -v updateTime output/$@.out.ra | diff - expected/$@.out.ra

accTest:
	${MAKE} setup
	${PROG} add input/addTest.in.ra -table=${TABLE}
	${PROG} acc 2 -table=${TABLE}
	${PROG} acc 3 -table=${TABLE}
	${PROG} revoke 3 -table=${TABLE}
	${PROG} dump output/$@.out.ra -table=${TABLE}
	@grep -v updateTime output/$@.out.ra | diff - expected/$@.out.ra

showTest:
	${MAKE} setup
	${PROG} add input/addTest.in.ra -table=${TABLE}
	${PROG} acc 3 -table=${TABLE} 
	${PROG} show 2 -table=${TABLE} > output/$@.out.ra
	${PROG} show 3 -table=${TABLE} >> output/$@.out.ra
	@grep -v updateTime output/$@.out.ra | diff - expected/$@.out.ra

idTest:
	${MAKE} setup
	${PROG} add input/addTest.in.ra -table=${TABLE}
	${PROG} id human Stanford ChipSeq GM12878 Antibody:Pol2 -table=${TABLE} > output/$@.out.ra
	${PROG} id human Duke DNaseSeq K562 -table=${TABLE} >> output/$@.out.ra
	@grep -v updateTime output/$@.out.ra | diff - expected/$@.out.ra

clean:
	${PROG} drop -table=${TABLE}
	@rm -rf output

setup:
	${MAKE} clean
	@${MKDIR} output
	${PROG} create -table=${TABLE}
