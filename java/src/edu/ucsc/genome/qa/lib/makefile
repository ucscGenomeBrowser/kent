include ../java.mk

%.class : %.java
	javac $<

CLASS = ChromInfo.class ChromLibrary.class DBInfo.class KnownGene.class \
Position.class PositionIterator.class TrackContainer.class TrackDb.class \
QADBLibrary.class QAWebLibrary.class QALibrary.class

all: ${CLASS}
	@echo all done

