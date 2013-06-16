# makefile with sqlCheck tests
include ../../../inc/common.mk
include testCommon.mk

TEST = sqlCheck
TESTER = ${BIN_DIR}/sqlCheck
IN = ${IN_DIR}/${TEST}
EXP = ${EXP_DIR}/${TEST}
OUT = ${OUT_DIR}/${TEST}

test: simpleTest

# ID should only allow alphanum . _ (spaces are not allowed of course)
# QL should allow any values except the special chars that require escaping such as quotes or backSlash
# ES should escape the string at the cost of making the mysql escape call, but allows all chars.
# EE just provides a handy way to add all the evil forbidden chars to your test string so you can see them get escaped.

simpleTest: mkout
	${TESTER} ID 'myTable' > ${OUT}/sqlCheck.1
	diff ${OUT}/sqlCheck.1 ${EXP}/sqlCheck.1
	${TESTER} ID 'hg19.my_Table' > ${OUT}/sqlCheck.2
	diff ${OUT}/sqlCheck.2 ${EXP}/sqlCheck.2
	-${TESTER} ID 'my Table' 2>&1 | grep -v '0x' >& ${OUT}/sqlCheck.3
	diff ${OUT}/sqlCheck.3 ${EXP}/sqlCheck.3
	-${TESTER} ID "'myTable" 2>&1 | grep -v '0x' >& ${OUT}/sqlCheck.4
	diff ${OUT}/sqlCheck.4 ${EXP}/sqlCheck.4
	${TESTER} ES 'my "Table' >& ${OUT}/sqlCheck.7
	diff ${OUT}/sqlCheck.7 ${EXP}/sqlCheck.7
	${TESTER} EE "" >& ${OUT}/sqlCheck.8
	diff ${OUT}/sqlCheck.8 ${EXP}/sqlCheck.8
	${TESTER} IL "chromStart,chromEnd,'.',score" >& ${OUT}/sqlCheck.9
	diff ${OUT}/sqlCheck.9 ${EXP}/sqlCheck.9

mkout:
	@${MKDIR} ${OUT}
