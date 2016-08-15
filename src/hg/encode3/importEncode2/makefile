kentSrc = ../../..
# Build all subdirectories we care about

include ../../../inc/common.mk


# List of directories to build
USE_DIRS = $(DIRS)

DIRS = \
    encode2AddSumsAndSizes \
    encode2AddTgzContentsToManifest \
    encode2BedDoctor \
    encode2BedPlusDoctor \
    encode2CmpMd5 \
    encode2ExpDumpFlat \
    encode2FlattenFastqSubdirs \
    encode2GffDoctor \
    encode2MakeEncode3 \
    encode2Manifest \
    encode2Md5UpdateManifest \
    encode2Meta \
    encode2MetaPatchRenamed \
    encodeCvToDb \
    encodeExpToCvDb

TEST_DIRS = \
	encode2BedDoctor 

all:: utils

utils: $(USE_DIRS:%=%.utils)
%.utils:
	(cd $* && $(MAKE))

test::	${TEST_DIRS:%=%.test}
%.test:
	(cd $* && ${MAKE} test)

clean:: $(USE_DIRS:%=%.clean)
	- find . -name '*.o' -exec rm {} \;
%.clean:
	(cd $* && ${MAKE} clean)

