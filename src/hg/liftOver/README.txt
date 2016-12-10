
   kent/src/hg/liftOver/

LiftOver has additional optional arguments for use with cross-species lifts:

   -multiple               Allow multiple output regions
   -minSizeT, -minSizeQ    Minimum chain size in target/query,
                             when mapping to multiple output regions
                                     (default 0, 0)

Depending on the size of the region being mapped, the species distance
and the quality of the assembly, the user may need to adjust the
minimum chain sizes.  For ENCODE regions (.5 to 1.8 MB), I used

    -minSizeT=4000
and
    -minSizeQ=20000
        for mammals except chimp, which, due to the fragmented
        assembly, I reduced minSizeQ to 10000
    -minSizeQ=1000
        for non-mammals (bird & fish)

Also, for cross-species, they'll want to set the -minMatch very low
(e.g. .01), instead of the default .95, which is a reasonable default
for within species.

Hmmm, sounds like this should be a README in our liftOver downloads dirs.

Finally, there's an additional tool, liftOverMerge, which is useful
when lifting large regions.  I used it for ENCODE, but don't think
it likely that users will need it.

        Kate
