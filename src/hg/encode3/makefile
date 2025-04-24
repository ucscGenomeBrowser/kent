# Build all programs in encode directory
# much of this file is leftover from encode2

# List of directories to build

DIRS = \
	encodeDataWarehouse \
	validateManifest \
	validateFiles

USE_DIRS = $(DIRS)

all: ${USE_DIRS:%=%_all}
%_all:
	${MAKE} -C $*

# nothing to clean in hg/encode3/
clean::
