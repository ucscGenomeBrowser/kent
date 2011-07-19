include ../../../../inc/common.mk

PROG = ../encodeExp

TABLE = encodeExpTest_${USER}

test: copyTest addTest accTest showTest renameTest modifyTest removeTest idTest restoreTest
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
	sleep 1
	${PROG} deacc 3 -table=${TABLE}
	${PROG} dump output/$@.out.ra -table=${TABLE}
	@grep -v updateTime output/$@.out.ra | diff - expected/$@.out.ra

removeTest:
	${MAKE} setup
	${PROG} add input/addTest.in.ra -table=${TABLE}
	sleep 1
	${PROG} remove 2 "bad experiment" -table=${TABLE}
	${PROG} dump output/$@.out.ra -table=${TABLE}
	@grep -v updateTime output/$@.out.ra | diff - expected/$@.out.ra

restoreTest:
	${MAKE} setup
	${PROG} add input/$@.in.ra -table=${TABLE}
	sleep 1
	${PROG} remove 1 "bad experiment 1" -table=${TABLE}
	${PROG} remove 4 "bad experiment 2" -table=${TABLE}
	${PROG} remove 5 "bad experiment 3" -table=${TABLE}
	${PROG} remove 7 "bad experiment 4" -table=${TABLE}
	${PROG} remove 8 "bad experiment 5" -table=${TABLE}
	${PROG} dump output/$@.tmp.ra -table=${TABLE}
	${PROG} drop -table=${TABLE}
	${PROG} create -table=${TABLE}
	${PROG} restore -table=${TABLE} output/$@.tmp.ra
	${PROG} dump output/$@.out.ra -table=${TABLE}
	@grep -v updateTime output/$@.out.ra | diff - expected/$@.out.ra

showTest:
	${MAKE} setup
	${PROG} add input/addTest.in.ra -table=${TABLE}
	${PROG} acc 3 -table=${TABLE} 
	${PROG} show 2 -table=${TABLE} > output/$@.out.ra
	${PROG} show 3 -table=${TABLE} >> output/$@.out.ra
	@grep -v updateTime output/$@.out.ra | diff - expected/$@.out.ra

copyTest:
	${MAKE} setup
	${PROG} add input/addTest.in.ra -table=${TABLE}
	${PROG} copy -table=${TABLE} ${TABLE}Copy
	${PROG} dump output/$@.out.ra -table=${TABLE}Copy
	@grep -v updateTime output/$@.out.ra | diff - expected/$@.out.ra

renameTest:
	${MAKE} setup
	${PROG} add input/addTest.in.ra -table=${TABLE}
	${PROG} copy -table=${TABLE} ${TABLE}Renamed
	${PROG} dump output/$@.out.ra -table=${TABLE}Renamed
	@grep -v updateTime output/$@.out.ra | diff - expected/$@.out.ra

idTest:
	${MAKE} setup
	${PROG} add input/addTest.in.ra -table=${TABLE}
	${PROG} id human Stanford ChipSeq GM12878 Antibody:Pol2 -table=${TABLE} > output/$@.out.ra
	${PROG} id human Duke DNaseSeq K562 -table=${TABLE} >> output/$@.out.ra
	@grep -v updateTime output/$@.out.ra | diff - expected/$@.out.ra

modifyTest:
	${MAKE} setup
	${PROG} add input/addTest.in.ra -table=${TABLE}
	${PROG} update 3 lab Duke UW -table=${TABLE}
	${PROG} update 3 cellType K562 GM12878 -table=${TABLE}
	${PROG} update 3 dataType DnaseSeq RnaSeq -table=${TABLE}
	${PROG} update 2 Antibody Pol2 CTCF -table=${TABLE}
	${PROG} update 2 treatment None TNFa -table=${TABLE}
	${PROG} show 3 -table=${TABLE} >> output/$@.out.ra
	${PROG} show 2 -table=${TABLE} >> output/$@.out.ra
	@grep -v updateTime output/$@.out.ra | diff - expected/$@.out.ra

clean:
	${PROG} drop -table=${TABLE}
	${PROG} drop -table=${TABLE}Copy
	${PROG} drop -table=${TABLE}Renamed
	@rm -rf output

setup:
	${MAKE} clean
	@${MKDIR} output
	${PROG} create -table=${TABLE}
