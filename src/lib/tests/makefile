include ../../inc/common.mk
HG_WARN_ERR = -DJK_WARN -Wall -Werror

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a
BIN_DIR = bin/${MACHTYPE}

test: errCatchTest htmlPageTest htmlExpandUrlTest pipelineTests
	rm -r output
	@echo tested all

mkdirs:
	${MKDIR} output ${BIN_DIR}

errCatchTest: errCatchTest.o ${MYLIBS} mkdirs
	${CC} ${COPT} -o ${BIN_DIR}/errCatchTest errCatchTest.o ${MYLIBS} $L
	${STRIP} ${BIN_DIR}/errCatchTest${EXE}
	${BIN_DIR}/errCatchTest secret > output/errCatch.good
	diff expected/errCatch.good output/errCatch.good
	${BIN_DIR}/errCatchTest bad > output/errCatch.bad
	diff expected/errCatch.bad output/errCatch.bad

htmlExpandUrlTest: htmlExpandUrlTest.o ${MYLIBS} mkdirs
	${CC} ${COPT} -o ${BIN_DIR}/htmlExpandUrlTest htmlExpandUrlTest.o ${MYLIBS} $L
	${STRIP} ${BIN_DIR}/htmlExpandUrlTest${EXE}
	${BIN_DIR}/htmlExpandUrlTest > output/htmlExpandUrlTest 2>&1
	diff expected/htmlExpandUrlTest output/htmlExpandUrlTest

htmlPageTest: htmlPageTest.o ${MYLIBS} mkdirs
	${CC} ${COPT} -o ${BIN_DIR}/htmlPageTest htmlPageTest.o ${MYLIBS} $L
	${STRIP} ${BIN_DIR}/htmlPageTest${EXE}
	${BIN_DIR}/htmlPageTest input/google.html > output/google.out
	diff expected/google.out output/google.out

pipelineTests: 	pipelineWrite pipelineWriteMult pipelineWriteFd \
		pipelineRead pipelineReadMult pipelineReadFd pipelineReadMem \
		pipelineExitCode

pipelineWrite: ${BIN_DIR}/pipelineTester mkdirs
	${BIN_DIR}/pipelineTester -write -pipeData=input/simple1.txt -otherEnd=output/$@.out.gz "gzip -1"
	zcat output/$@.out.gz > output/$@.out
	diff input/simple1.txt output/$@.out

# add come junk to make sure output gets truncated
pipelineWriteMult: ${BIN_DIR}/pipelineTester mkdirs
	cat input/google.html > output/$@.wc
	${BIN_DIR}/pipelineTester -write -pipeData=input/simple1.txt -otherEnd=output/$@.wc "gzip -1" "gzip -dc" "wc"
	diff expected/simple1.wc output/$@.wc

pipelineWriteFd: ${BIN_DIR}/pipelineTester mkdirs
	${BIN_DIR}/pipelineTester -fdApi -write -pipeData=input/simple1.txt -otherEnd=output/$@.out.gz "gzip -1"
	zcat output/$@.out.gz > output/$@.out
	diff input/simple1.txt output/$@.out

pipelineRead: ${BIN_DIR}/pipelineTester mkdirs
	gzip -1c input/simple1.txt >output/$@.in.gz
	${BIN_DIR}/pipelineTester -otherEnd=output/$@.in.gz -pipeData=output/$@.out "gzip -dc"
	diff input/simple1.txt output/$@.out

pipelineReadMult: ${BIN_DIR}/pipelineTester mkdirs
	${BIN_DIR}/pipelineTester -pipeData=output/$@.wc -otherEnd=input/simple1.txt "gzip -1" "gzip -dc" "wc"
	diff expected/simple1.wc output/$@.wc

pipelineReadFd: ${BIN_DIR}/pipelineTester mkdirs
	gzip -1c input/simple1.txt >output/$@.in.gz
	${BIN_DIR}/pipelineTester -fdApi -otherEnd=output/$@.in.gz -pipeData=output/$@.out "gzip -dc"
	diff input/simple1.txt output/$@.out

pipelineReadMem: ${BIN_DIR}/pipelineTester mkdirs
	gzip -1c input/simple1.txt >output/$@.in.gz
	${BIN_DIR}/pipelineTester -memApi -otherEnd=output/$@.in.gz -pipeData=output/$@.out "gzip -dc"
	diff input/simple1.txt output/$@.out

pipelineExitCode: ${BIN_DIR}/pipelineTester
	${BIN_DIR}/pipelineTester -exitCode=1 false

${BIN_DIR}/pipelineTester:  mkdirs pipelineTester.o ${MYLIBS}
	${CC} ${COPT} -o ${BIN_DIR}/pipelineTester pipelineTester.o ${MYLIBS} $L

clean:
	rm -rf *.o bin output *.tmp
