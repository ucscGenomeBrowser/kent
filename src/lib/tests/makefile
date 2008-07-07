include ../../inc/common.mk

MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a
BIN_DIR = bin/${MACHTYPE}

test: errCatchTest htmlPageTest htmlExpandUrlTest pipelineTests dyStringTest \
    mimeTests base64Tests quotedPTests safeTest hashTest
	rm -r output
	@echo tested all

mkdirs:
	${MKDIR} output ${BIN_DIR}

errCatchTest: errCatchTest.o ${MYLIBS} mkdirs
	${CC} ${COPT} -o ${BIN_DIR}/errCatchTest errCatchTest.o ${MYLIBS}
	${STRIP} ${BIN_DIR}/errCatchTest${EXE}
	${BIN_DIR}/errCatchTest secret > output/errCatch.good
	diff expected/errCatch.good output/errCatch.good
	${BIN_DIR}/errCatchTest bad > output/errCatch.bad
	diff expected/errCatch.bad output/errCatch.bad

htmlExpandUrlTest: htmlExpandUrlTest.o ${MYLIBS} mkdirs
	${CC} ${COPT} -o ${BIN_DIR}/htmlExpandUrlTest htmlExpandUrlTest.o ${MYLIBS}
	${STRIP} ${BIN_DIR}/htmlExpandUrlTest${EXE}
	${BIN_DIR}/htmlExpandUrlTest > output/htmlExpandUrlTest 2>&1
	diff expected/htmlExpandUrlTest output/htmlExpandUrlTest

htmlPageTest: htmlPageTest.o ${MYLIBS} mkdirs
	${CC} ${COPT} -o ${BIN_DIR}/htmlPageTest htmlPageTest.o ${MYLIBS}
	${STRIP} ${BIN_DIR}/htmlPageTest${EXE}
	${BIN_DIR}/htmlPageTest input/google.html > output/google.out
	diff expected/google.out output/google.out

pipelineTests: 	pipelineWrite pipelineWriteMult pipelineWriteFd \
		pipelineRead pipelineReadMult pipelineReadFd pipelineReadMem \
		pipelineExitCode pipelineWriteErr

pipelineWrite: ${BIN_DIR}/pipelineTester mkdirs
	${BIN_DIR}/pipelineTester -write -pipeData=input/simple1.txt -otherEnd=output/$@.out.gz "gzip -1"
	gunzip -c output/$@.out.gz > output/$@.out
	diff -b input/simple1.txt output/$@.out

# add come junk to make sure output gets truncated
pipelineWriteMult: ${BIN_DIR}/pipelineTester mkdirs
	cat input/google.html > output/$@.wc
	${BIN_DIR}/pipelineTester -write -pipeData=input/simple1.txt -otherEnd=output/$@.wc "gzip -1" "gzip -dc" "wc"
	diff -b expected/simple1.wc output/$@.wc

pipelineWriteFd: ${BIN_DIR}/pipelineTester mkdirs
	${BIN_DIR}/pipelineTester -fdApi -write -pipeData=input/simple1.txt -otherEnd=output/$@.out.gz "gzip -1"
	gunzip -c output/$@.out.gz > output/$@.out
	diff -b input/simple1.txt output/$@.out

pipelineRead: ${BIN_DIR}/pipelineTester mkdirs
	gzip -1c input/simple1.txt >output/$@.in.gz
	${BIN_DIR}/pipelineTester -otherEnd=output/$@.in.gz -pipeData=output/$@.out "gzip -dc"
	diff -b input/simple1.txt output/$@.out

pipelineReadMult: ${BIN_DIR}/pipelineTester mkdirs
	${BIN_DIR}/pipelineTester -pipeData=output/$@.wc -otherEnd=input/simple1.txt "gzip -1" "gzip -dc" "wc"
	diff -b expected/simple1.wc output/$@.wc

pipelineReadFd: ${BIN_DIR}/pipelineTester mkdirs
	gzip -1c input/simple1.txt >output/$@.in.gz
	${BIN_DIR}/pipelineTester -fdApi -otherEnd=output/$@.in.gz -pipeData=output/$@.out "gzip -dc"
	diff -b input/simple1.txt output/$@.out

pipelineReadMem: ${BIN_DIR}/pipelineTester mkdirs
	gzip -1c input/simple1.txt >output/$@.in.gz
	${BIN_DIR}/pipelineTester -memApi -otherEnd=output/$@.in.gz -pipeData=output/$@.out "gzip -dc"
	diff -b input/simple1.txt output/$@.out

pipelineExitCode: ${BIN_DIR}/pipelineTester
	${BIN_DIR}/pipelineTester -exitCode=1 false

# test redirecting stderr, see that two process can write stderr, but only
# the second's stdout should make it to the end of the pipe.  Since order
# of writes to stderr is determined by process scheduling, sort the output
# before comparing.
pipelineWriteErr: ${BIN_DIR}/pipelineTester mkdirs
	${BIN_DIR}/pipelineTester -write -otherEnd=output/$@.out -stderr=output/$@.raw.err "sh -c 'echo OUT; echo ERR >&2'"  "sh -c 'echo OUT2; echo ERR2 >&2'"
	diff -b expected/$@.out output/$@.out
	sort output/$@.raw.err > output/$@.err
	diff -b expected/$@.err output/$@.err

${BIN_DIR}/pipelineTester:  mkdirs pipelineTester.o ${MYLIBS}
	${CC} ${COPT} -o ${BIN_DIR}/pipelineTester pipelineTester.o ${MYLIBS}


dyStringTest: ${BIN_DIR}/dyStringTester mkdirs
	${BIN_DIR}/dyStringTester

${BIN_DIR}/dyStringTester:  mkdirs dyStringTester.o ${MYLIBS}
	${CC} ${COPT} -o ${BIN_DIR}/dyStringTester dyStringTester.o ${MYLIBS}


mimeTests: mime1 mime2 mime3 mime4 mimeBin mime5 mimeAltHead mimeAutoBoundary mimeBlat

${BIN_DIR}/mimeTester:  mkdirs mimeTester.o ${MYLIBS}
	${CC} ${COPT} -o ${BIN_DIR}/mimeTester mimeTester.o ${MYLIBS}


mime1: ${BIN_DIR}/mimeTester mkdirs
	${BIN_DIR}/mimeTester < input/$@.txt > output/$@.out
	diff expected/$@.out output/$@.out

mime2: ${BIN_DIR}/mimeTester mkdirs
	${BIN_DIR}/mimeTester < input/$@.txt > output/$@.out
	diff expected/$@.out output/$@.out

mime3: ${BIN_DIR}/mimeTester mkdirs
	${BIN_DIR}/mimeTester < input/$@.txt > output/$@.out
	diff expected/$@.out output/$@.out

mime4: ${BIN_DIR}/mimeTester mkdirs
	${BIN_DIR}/mimeTester < input/$@.txt > output/$@.out
	diff expected/$@.out output/$@.out

mimeBin: ${BIN_DIR}/mimeTester mkdirs
	${BIN_DIR}/mimeTester < input/$@.txt > output/$@.out
	diff expected/$@.out output/$@.out

mime5: ${BIN_DIR}/mimeTester mkdirs
	${BIN_DIR}/mimeTester < input/$@.txt > output/$@.out
	diff expected/$@.out output/$@.out

mimeAltHead: ${BIN_DIR}/mimeTester mkdirs
	${BIN_DIR}/mimeTester -altHeader='CONTENT_TYPE=multipart/form-data; boundary=----------0xKhTmLbOuNdArY' < input/$@.txt > output/$@.out
	diff expected/$@.out output/$@.out

mimeAutoBoundary: ${BIN_DIR}/mimeTester mkdirs
	${BIN_DIR}/mimeTester -autoBoundary < input/$@.txt > output/$@.out
	diff expected/$@.out output/$@.out

mimeBlat: ${BIN_DIR}/mimeTester mkdirs
	${BIN_DIR}/mimeTester -altHeader='CONTENT_TYPE=multipart/form-data; boundary=----------0xKhTmLbOuNdArY' < input/$@.txt > output/$@.out
	diff expected/$@.out output/$@.out

mimeSeries: ${BIN_DIR}/mimeTester mkdirs
	${BIN_DIR}/mimeTester -sizeSeries=3000

${BIN_DIR}/htmlMimeTest:  mkdirs htmlMimeTest.o ${MYLIBS}
	${CC} ${COPT} -o ${BIN_DIR}/htmlMimeTest htmlMimeTest.o ${MYLIBS}

htmlMime1: ${BIN_DIR}/htmlMimeTest mkdirs
	${BIN_DIR}/htmlMimeTest http://hgwdev.cse.ucsc.edu/cgi-bin/hgBlat input/htmlMime.txt 3490 3502 > output/$@.out
	diff expected/$@.out output/$@.out


base64Tests: base64Encode base64Decode

${BIN_DIR}/testBase64:  mkdirs testBase64.o ${MYLIBS}
	${CC} ${COPT} -o ${BIN_DIR}/testBase64 testBase64.o ${MYLIBS}

base64Encode: ${BIN_DIR}/testBase64 mkdirs
	${BIN_DIR}/testBase64 'My Test String' > output/$@.out
	diff expected/$@.out output/$@.out

base64Decode: ${BIN_DIR}/testBase64 mkdirs
	${BIN_DIR}/testBase64 'TXkgVGVzdCBTdHJpbmc=' > output/$@.out
	diff expected/$@.out output/$@.out



quotedPTests: quotedPEncode quotedPDecode

${BIN_DIR}/testQuotedP:  mkdirs testQuotedP.o ${MYLIBS}
	${CC} ${COPT} -o ${BIN_DIR}/testQuotedP testQuotedP.o ${MYLIBS}

quotedPEncode: ${BIN_DIR}/testQuotedP mkdirs
	${BIN_DIR}/testQuotedP 'taxes are quite high ' > output/$@.out
	diff expected/$@.out output/$@.out

quotedPDecode: ${BIN_DIR}/testQuotedP mkdirs
	${BIN_DIR}/testQuotedP 'taxes=20are=20quite=20high=20=' > output/$@.out
	diff expected/$@.out output/$@.out

${BIN_DIR}/mimeDecodeTest:  mkdirs mimeDecodeTest.o ${MYLIBS}
	${CC} ${COPT} -o ${BIN_DIR}/mimeDecodeTest mimeDecodeTest.o ${MYLIBS}

mimeDecodeTest: ${BIN_DIR}/mimeDecodeTest mkdirs
	${BIN_DIR}/mimeDecodeTest -cid -autoBoundary output < input/$@.txt
	diff expected/noName1.html output/noName1.html

${BIN_DIR}/safeTester:  mkdirs safeTester.o ${MYLIBS}
	${CC} ${COPT} -o ${BIN_DIR}/safeTester safeTester.o ${MYLIBS}

safeTest: ${BIN_DIR}/safeTester mkdirs
	${BIN_DIR}/safeTester

hashTest: hashTest1

${BIN_DIR}/testHash:  mkdirs testHash.o ${MYLIBS}
	${CC} ${COPT} -o ${BIN_DIR}/testHash testHash.o ${MYLIBS}

hashTest1: ${BIN_DIR}/testHash mkdirs
	${BIN_DIR}/testHash input/$@.txt > output/$@.out
	diff expected/$@.out output/$@.out


clean:
	rm -rf *.o bin output *.tmp mimeTester.tmp mimeTester.out
