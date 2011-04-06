include ../../../../inc/common.mk

PROG = ../encodeExp

TABLE = encodeExpTest_${USER}

test: addTest addTest.done showTest showTest.done idTest
	echo tested all
	@#-rm -rf output

# Add experiments from .ra file
addTest: setup
	@echo
	@echo "**Running $@**"
	${PROG} add input/addTest.in.ra -table=${TABLE}
	${PROG} acc 2 -table=${TABLE}
	${PROG} acc 3 -table=${TABLE}
	${PROG} revoke 3 -table=${TABLE}
	${PROG} dump output/$@.out.ra -table=${TABLE}
	@grep -v updateTime output/$@.out.ra | diff - expected/$@.out.ra

showTest: setup
	@echo
	@echo "**Running $@**"
	${PROG} add input/addTest.in.ra -table=${TABLE}
	${PROG} acc 3 -table=${TABLE} 
	${PROG} show 2 -table=${TABLE} > output/$@.out.ra
	${PROG} show 3 -table=${TABLE} >> output/$@.out.ra
	@grep -v updateTime output/$@.out.ra | diff - expected/$@.out.ra

idTest: setup
	@echo
	@echo "**Running $@**"
	${PROG} add input/addTest.in.ra -table=${TABLE}
	${PROG} id human Stanford ChipSeq GM12878 Antibody:Pol2 -table=${TABLE} > output/$@.out.ra
	${PROG} id human Duke DNaseSeq K562 -table=${TABLE} >> output/$@.out.ra
	@grep -v updateTime output/$@.out.ra | diff - expected/$@.out.ra

clean:
	hgsql hgFixed -e "drop table if exists ${TABLE}"
	@rm -rf output

setup addTest.done showTest.done:
	hgsql hgFixed -e "drop table if exists ${TABLE}"
	@rm -rf output
	@${MKDIR} output
	${PROG} create -verbose=0
	@hgsql hgFixed -e "alter table encodeExpNew rename to ${TABLE}"
