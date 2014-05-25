/* Copyright (C) 2002 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

Human-mouse alignments with Blastz
schwartz, kent, smit, zhang, baertsch, hardison, haussler, miller

Introduction:

One of the goals set by the Mouse Genome Analysis Consortium (cite Nature paper)
is to study mutation and selection, the main forces shaping the mouse and
human genomes.  Specific aims included
(1) estimating the fraction of the human genome that is under selection
(cite Krish et al.),
(2) determining the degree to which genome comparisons can pinpoint the
regions under selection (cite Elnitski et al.) and
(3) measuring regional variation in the rate and pattern of
neutral evolution (Hardison et al.).  Attaining these goals requires an
alignment program with higher sensitivity than is needed for other purposes
such as predicting novel protein-coding segments or identifying large
genomic intervals where gene order is conserved.

Ideally, our alignment program would identify orthologous regions
of the human and mouse genomes, whether or not they are under selection.
It would determine correspondences between genomic positions that are 
descended from the same position in the ancestral genome,
allowing nucleotide substitutions.
In practice, success in reaching that goal is measured by the program's
sensitivity (fraction of orthologous positions that it aligns) and
specificity (fraction of the aligned positions that are indeed orthologous).
Most of our aims could have been addressed by a program that aligned neutrally
evolving regions with a modest degree of sensitivity.  For instance,
regional variations (aim 3) could be assessed from a relatively small sample,
(say, 1 out of 10 orthologous regions), provided that there were no critical
biases in the sampling process.  Demands on specificity were higher,
but it was acceptable for perhaps 5-10% of the aligned positions to be
non-orthologous.

To meet out needs, we enhanced the Blastz alignment program (cite PipMaker),
and developed a new program, axtBest to separate paralogous from orthologous
alignment.  Here we describe the programs, the hardware environment, and 
several validation studies.  Our results indicate that we have correctly 
aligned the majority of what can be aligned between the human and mouse 
genomes.

Results:

Software Design Issues

Our goal is to align an appreciable fraction of the neutrally evolving DNA in
the human and mouse genomes.  This sensitivity requirement
disqualified several existing programs (cite SSAHA, BLAT)
that sacrifice sensitivity to attain very short running times.
Preliminary studies indicated that an appropriate level of sensitivity and
specificity was attained by a program called Blastz, which is used by the
PipMaker (cite Schwartz) Web server to align genomic sequences.
Blastz follows the three-step strategy used by Gapped Blast
(Altschul et al. 1997): find short near-exact matches,
extend each short match without allowing gaps, and extend each
ungapped match that exceeds a certain threshold by a dynamic programming
procedure that permits gaps.

Two differences between Blastz and Gapped Blast were exploited in our
whole-genome alignments.  First, Blastz has an option to require that 
the matching regions that it reports must occur in the same order and 
orientation in both sequences.  To enforce this restiction, it uses a 
method described by Zhang et al. 1994.  Second, Blastz uses an alignment 
scoring scheme derived and evaluated by Chiaromonte et al. 2002.  Nucleotide 
substitutions are scored by the matrix

     A    C    G    T
A   91 -114  -31 -123
C -114  100 -125  -31
G  -31 -125  100 -114
T -123  -31 -114   91

and a gap of length k is penalized by subtracting 400 + 30k from the score.
To determine whether an ungapped alignment is sufficiently promising to
warrant initiation of a dynamic programming extension step, the sum of scores
for its columns is multiplied by a value between 0 and 1 that measures sequence
complexity, as described in detail by Chiaromonte et al.
This makes it harder for a region of extremely biased nucleotide content to
trigger a gapped alignment.

Two changes to Blastz significantly improved its execution speed for
aligning entire genomes.  First, when the program realizes that many 
regions of the mouse genome align to the same human segment, that 
segment is dynamically masked, i.e., marked so that it will be 
ignored in later steps of the alignment process.
Second, we adapted a very clever idea of Wu el al. 2002 for determining the
initial short match that may seed an alignment.  Formerly, Blastz looked for 
identical runs of 8 consecutive nucleotides in each sequence. 
Wu et al. propose looking for runs of 19 consecutive nucleotides in each
sequence, within which the 12 positions indicated by a "1" in the string
1110100110010101111
are identical.  To increase sensitivity, we allow a transition
(A-G, G-A, C-T or T-C) in any one of the 12 positions.

Later, Blastz was further modified to utilize the increasing contiguity of
the mouse sequence.
For the earliest alignments, the mouse sequence was presented in unassembled
``reads'' of around 500 bp each.   An ungapped alignment was required to
score at least 3000 (relative to the above substitution scores as adjusted
by the measure of sequence complexity) before the dynamic programming step
was initiated.
Such a high threshold was needed to attain reasonable specificity.
Availability of longer mouse segments suggested looking for pairs of alignments
that connect nearby regions in both human and mouse genomes; the alignment
procedure can be run with a lower threshold in the regions between the
alignments.
If the separation between the two alignments is under 50 kb in both sequences,
then Blastz recursively searches the intervening regions for 7-mer exact
matches and requires a threshold of 2200 for initiating dynamic programming
(with the adjustment for sequence complexity).
If the separation is under 10 kb, the threshold is lowered to 2000.
In either case, the higher-sensitivity matches are required to occur with
an order and orientation consistent with the stronger flanking matches.

Although the fee for initiating a gapped alignment is steep (e.g., 3000),
once started the alignment keeps extending as long as the average score of the
added alignment remains positive.  This observation suggests a strategy of
physically removing lineage specific interspersed repeats (i.e., that inserted
after the human-mouse split).
Then, an alignment that is initiated on one side of the insertion point can
reach the other side, without incurring a penalty for jumping the
insertion, where it may detect alignable nucleotides
that may not meet the steep initiation fee on their own.
We now remove recent repeat elements, run Blastz, then adjust
positions in the alignment to refer to the original sequences.
These last two improvements, i.e., recursive application of Blastz between
adjacent alignments and excision of lineage specific repeats, increased
alignment coverage from 33% of the human genome to 40%. [Scott, check numbers.]

The modified Blastz was used to compare all the human sequence with all of the
mouse, i.e., to produce a complete catalog of matching regions.
more than one region of the mouse sequence aligned to the same
region of the human sequence.  [jk new] This is a natural consequence of
duplications in the mouse genome and the human/mouse common ancestor.
These duplications include paralogous genes, processed and unprocessed
psuedogenes, tandem repeats, simple repeats, etc.  For many purposes 
one wants the single best, orthologous, match for each human region .  
Typically when looking at a region spanning several thousand bases it is 
clear which alignment is the ortholog and which are the paralogs.  The 
orthologous alignment usually is longer,  and overall has a greater 
sequence identity.  On the other hand over a small regions by chance a 
paralog may have greater sequence identity than a paralog.  

We wrote a program, axtBest, which automatically picks out an alignment
likely to be the orthologous alignment.  The program works in two passes.
A window of 5000 bases on either side of a human base is used to score
that base.  The score is the blastz score at the end of the window minus
the blastz score at the beginning of the window.  Bases less than 5000
bases from the start or end of the alignment have to be handled specially,
and the details of this differ between the first and the second pass.
In the first pass the window is truncated to only cover the alignment
itself.  This tends to make bases on the edge of an alignment or bases in
a short alignment score less than bases in the middle of a long alignment.
Unless an alignment is the best scoring alignment over at least one
human base in the first pass, it is thrown out.   In the second pass
bases on the edges of an alignment are given the same score as the base
5000 bases into the alignment.  The portions of each alignment remaining
after the first pass that are the best scoring over at least 10 bases of
the human genome are then written out.  This two pass scheme with the
first pass being done on softer-edged windows and the second pass on 
harder-edged windows in most cases yields the same result one would
obtain with a simpler one pass scheme.  However when applied to regions
with multiple paralogs it results in less fragmented 'best' alignments
than the single pass schemes we have tried.


 Software Implementation [Scott]

Blastz, in common with other members of the blast family computes a set
of local alignments in stages.

The first stage reads a target sequence and builds a hash table relating
short patterns to locations.

Traditionally the patterns were k-mers, but following PatternHunter
blastz now allows a discontinuous selection of 12 base-pairs out of 19.
Because each bp consumes two bits, this requires 64 bit integers.
Compilers for 32 bit machines typically provide a synthesized 64 bit
data type (ISO C 1999 defines a standard int64_t), which is reasonably
inexpensive in the event that we only use simple logical operations.
At the same time, specialized bit extraction operators have been proposed
by some architects, and would be useful in this application.

We also store all possible transitions of a given sample, which
improves sensitivity at the expense of an order of magnitude greater
space consumption.

The current implementation sizes the table such that the load factor
will be very low, using 2^24 entries.  Space expended on the table is
balanced by not requiring that the hash code be stored in every node,
since every bit is used to select the current bucket.

The second stage reads a query sequence, looking up each 12of19 pattern
in the blast table, then doing the same for the reverse compliment of the
query sequence.  Each "high scoring pair" (HSP) that is selected in
this procedure is then extended into an ungapped alignment using a fixed
but well tuned scoring scheme.  Profiling shows that these two steps
typically consume the bulk of the runtime in an alignment.  In the case
where no matches are found, the hashing and lookup operations dominate.
In the case where matches are found, the ungapped extension dominates.
In a few other, less common, cases the following steps consume significant
time.

During this stage, but before extension, blastz supports the option of
chaining the HSP, reducing a cloud of hits to a single high scoring path.

The third stage involves a full blown dynamic programming step,
implementing the gapped blast algorithm.  Two innovations play a role
at this stage.  First, after aligning each contig, blastz counts
the number of times each base pair participated in an alignment.
In the event that this crosses a user supplied threshold, that base is
dynamically masked, and so will not participate in future alignments.
Second, blastz examines the list of alignments that were just generated,
and in the event of closely paired results, it attempts to fill the
gap between them by realigning those regions at reduced stringency.
The current implementation somewhat arbitrarily uses 7-mers, and chaining
in this step.


* 

In it's originally intended role, blastz consumed either two long
sequences, or one long sequence and a moderately sized collection of
contigs, with an output record reflecting each of these.  Whole genome
alignments required adjustments in a number of areas.

In our earliest experiments, finished human against unassembled mouse
reads, it was necessary to suppress output describing non-aligning
reads, simply to limit the size of the output we generated.  In later
experiments, when the mouse was well assembled, the situation was closer
to the originally conceived scenario of fairly long inputs.  In this
case, the main constraint was the 32bit address space on our CPUs, and
the amount of RAM available to each processor (512MB).  After a number
of trials, we decided to chop each human chromosome into 1Mbase long
sections with 10Kbase overlap, consuming approximately 128MB of memory.
The mouse sequence was chopped into 30Mbase sections, consuming about
240MB.  Additional space up to the limit was allocated for dynamic
programming and other bookkeeping.

Throughout the code, emphasis was on simplicity and maintainability.
While we sometimes expended effort to tune particular sections, by far
the greatest performance improvements came from large scale algorithmic
adjustments.  Thus, the organization of the program reflects our need to
make changes, rather than to efficiently realize current tactics.  So,
for example, the program occasionally chooses to consume extra memory
where it simplified or improved the implementation of some procedure.
Although we are happy with this tradeoff, profiling data revealed cases
where optimization was required, particularly in scenario, where large
numbers of small or medium sized contigs would be processed in each run.
There, we went to some trouble to minimize the overhead of memory
management.  The most significant of these changes involved the use of
a sparse array in the dynamic programming code.  Normally one maintains
an array to record the computation's progress along each diagonal,
but measurements found that the cost of maintaining (initializing in
particular) that array was a significant overhead.

While traditional implementations used sophisticated linear space
techniques in the computation of the alignment, this version uses a large,
but fixed sized traceback buffer.  In this way, we achieved significant
simplifications in the implementation and in memory management.

* 

To align a pair of genomes, we precompute a list of jobs, essentially
the cross product of the N/1MBase intervals of the first genome, by the
M/30Mbase intervals of the second.  The scheduling software available on
the 1024 CPU cluster that we use doesn't support processor affinity of any
sort; each job is entirely independent. 

Blastz is able to read traditional FASTA format inputs, but for this
application we prefer inputs in a packed, two base-pair per byte format,
which reduces the I/O by half, and simplifies and accelerates the
code that has to read the sequences.  We have sufficient local disk on
each node to provide a copy of all the input that might be required.
Only the output needs to be communicated to a shared resource, and this
is the main I/O bottleneck in our procedure, due to the latency of using
a network filesystem.

Dynamic masking was invented to handle cases like chr19, where zinc finger
or olfactory receptors match a huge number of times, but are not flagged
as repeats.  Oblivious scheduling partially defeats this optimization,
however, since each section of the target sequence will be compared
many times to different parts of the query sequence, and not be able
to share dynamic masking information.  Similarly, we could reduce disk
space requirements by devoting some nodes to particular portions of the
target sequence, at the expense of significant administrative overhead.

While we expect to see some improvement from better scheduling, the
current system works sufficiently well that we don't feel the need for
additional tuning at the present time.

*

Once the principle phase has been completed, we are left with a large
collection of output files containing blastz alignments of many sections
of the genome.  We then perform several post-processing steps, to refine
and improve the results.  One incidental operation is to transform
the output, "lav" format, into "axt" format.  The former is a space
efficient format which describes the alignments by coordinates within
named sequences.  The axt format is a textual representation that includes
the actual base pairs.  While this is very space inefficient, for many
post processing steps the improved locality of reference (avoiding the
need to retrieve parts of multi gigabyte datasets) is a clear necessity.

A second, and more important post-processing step is to compute the
single-coverage rendition of the alignment with axtBest.
Other sorts of post-processing include the generation of Percent Identity
Plots, Dotplots, and tracks on the Genome Browser.



Program Execution [Santa Cruz configuration and performance data -- Scott]

To align 2.8 Gb of human sequence vs. 2.5 Gb of mouse sequence
took 481 hours of CPU time and a half day of wall clock time on
a cluster of 1000 833 Mhz Pentium III CPUs.  This produced 9 Gig
of output in a relatively dense format, .lav.  The .lav files were
expanded to another format, .axt, which includes the mouse and
human sequence used in the alignment.  The axt files were 20 Gig.
The axt files after axtBest were 2.5 Gig.  Only 3.3% of the human 
genome is covered by multiple alignments, but some of these places,
particularly on chromosome 19, are covered to a great depth.


Software Evaluation

While Blastz's sensitivity (success at identifying orthologous,
or at least homologous, regions) was less critical than its specificity
(success at not aligning non-homologous regions), we were interesting in
estimating how much of the currently unaligned regions could rightfully be
aligned.
One hypothesis is that unaligned regions are actually orthologous, but
have suffered so many small mutations that Blastz no longer recognizes them as
related.
Another hypothesis is that the unaligned regions are not ortholgous to
sequence in the other genome, but instead were either inserted in one
species or deleted from the other after the human-mouse split.
The Nature paper (ref) argues that the second hypothesis correctly explains
most of the unaligned regions.
More precisely, the amount of DNA aligned by Blastz, 40% of the human genome,
is in good agreement with the amount that should align (i.e, that shares a
common ancestor with a region of the mouse genome).
We will not repeat that reasoning here.

Knowing that we're aligning approximately the proper number of nucleotides
in essence makes sensitivity equivalent to specificity, reasoning as follows.
High specificity implies high sensitivity, since if few alignments are
incorrect, then almost all of the aligned 40% is properly aligned,
which is almost everything that should align.
High sensitivity implies high specificity, since if almost all of the 40%
that should align in fact does align, then this accounts for almost all of
the alignments, leaving room for few incorrect alignments.
Thus, we are left with the problem of bounding the number of incorrect
alignments.

For an answer, we estimated the number of incorrect alignments that Blastz
would produce when comparing sequences of the same lengths and nucleotide
compositions as the human and mouse genomes.
Our strategy was to compare the human sequence with the reverse (NOT THE
REVERSE COMPLEMENT) of the mouse sequence.
Reversal retains the local complexity (e.g., a tandem dinucleotide repeat is
preserved) and guarantees that all generated alignments are inappropriate.

[jk new] Another test of the specificity of blastz alignments is based
on synteny.  Human chromosome 20 is considered to be completely 
syntenic to parts of mouse chromosome 2.  After filtering through axtBest
only 3.3% of the human chromosome 20 bases in alignments aligned outside
of mouse chromosome 2.   A certain degree of non-syntenic alignment is
to be expected from processed psuedogenes.  Since only approximately
96% of the mouse genome is sequenced, in some cases a paralog on another
chromosome will fill in for a non-sequenced ortholog on chromosome 2 as
well. 


