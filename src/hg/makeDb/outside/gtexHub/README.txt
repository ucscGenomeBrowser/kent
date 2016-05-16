This directory hosts build tools and generated files (under source control) for the
GTEx RNA-Seq Signal hub.

Subdirs and files are:
    buildNotes.txt      Some notes on building the hub
    tools/              Scripts for post-processing data files and creating trackDb files
                                (Install these in the bin directory of a build dir)
    metadata/           Metadata files used by build tools.  (From GTEx portal and track tables).
    hub/                Source controlled version of hub files

The build directory for this hub is:
        /hive/data/outside/GTEx/signal/

The build tools rely on these dirs in the build dir:
        metadata/
        run/
        in/
        out/
        hub/

The hub is here:
        /hive/data/gbdb/hubs/gtex
                (linked from /hive/data/outside/GTEx/signal/hub)

Data files for the hub are here:
        /hive/data/gbdb/hubs/gtex
                (linked from /hive/data/outside/GTEx/signal/hub/out)

Data files are named:
        gtexRnaSignalSRR#.{hg19,hg38}.bigWig

                where SRR# is the read file identifier at the short read archive
    

