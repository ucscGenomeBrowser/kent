SUBDIRS = 

all: ${SUBDIRS:%=%.suball}

%.suball:
	${MAKE} -C $* all

OTTO_SCRIPTS =	buildGeneReviews.sh \
		checkGeneReviews.sh \
		geneReviewsWrapper.sh \
		validateGeneReviews.sh

BUILD_FILES =	geneReviews.tables \
		geneReviews.as

BUILD_SCRIPTS =	geneRevsAddDiseases.pl \
		geneRevsListDiseases.pl

install: check-prefix installSub
	cp ${OTTO_SCRIPTS} ${BUILD_SCRIPTS} ${BUILD_FILES} \
		${PREFIX}

installSub: ${SUBDIRS:%=%.install}

%.install:
	${MAKE} -C $* install

validate: check-prefix
	$(foreach i, $(OTTO_SCRIPTS) $(BUILD_FILES) $(BUILD_SCRIPTS), echo $(i); diff ${PREFIX}/$(i) $(i);)

make-prefix: check-prefix
	mkdir -p ${PREFIX}

check-prefix:
	@test -n "${PREFIX}" || (echo "must specify PREFIX= to install"; exit 1)
