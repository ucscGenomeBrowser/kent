108 - Intronerator first version with C. briggsae alignments
      uploaded to ~zahler
109 - Gathered some statistics on BWANA alignments in 
      oneshot/cbCountOver.  Made 7-State Aligner take
      GC/AT richness into account, and made wobble
      usage take into account preference for purine-purine
      or pyrimidine/pyrimidine substitutions.
110 - Made stitcher require 15 bases of matching overlap
      rather than 4 to merge. This fixed the bug where
      you click on the purple alignment in Intronerator
      and it hits an assert().  Also did EST alignments
      with fly adh region.
111 - Fixed bug where BWANA ignored minus strand. 
      Moved BWANA, 7-State, and stitcher into lib.
      Made 7-State web page handle up to 10,000x10,000
      alignments by doing it piecemeal and stitching
      together.
112 - Made program that will align pasted in sequence
      against worm.  Did 13 mouse/human alignments.
113 - Sped up 7-state aligner by replacing 26x26 2-d
      array index with 32x26 1-d one.  Decreased gap
      probability from 0.1% to 0.04% in hiFi state.
	Made program to collect 0th and 1st order Markov
      models from FA files. Made program to collect
      codon usage stats from .GB and .FA files. Started
      on Aladdin the exon/intron recognizer.
114 - Converted Improbizer to scaled integer arithmetic.
      Still a few bugs.
115 - Made new html page, Motif Matcher, that can call
      ameme.exe to display where precalculated motifs
      match.  (Still a few bugs in scaled integer
      arithmetic.)
116 - Noticed descrepency in scores between scaled
      integer and floating point version was actually
      do to an inconsistency in floating point version.
      Alas this means I'll have to throw out the
      old precomputed score tables. The other scaled
      integer bug was due to overflow in gaussLocale().
          Reworked colors in display routines so that
      they are relative to the strongest hit in all
      profiles rather than to the strongest hit in
      a particular profile.  Also made "probabalistic
      erasing" happen during color display to fix 
      inconsistency between black and white and color
      displays.
117 - Put in coding background.
118 - Made Aladdin not a totally embarrassing Worm 
      gene-finder. Processes just under 100,000 bases
      per second in 39 states.
119 - Expanded parts of consensus sequence at either
      end of intron that Aladdin looks at.  Made it
      use precomputer tables instead of scanning files
      for Markov models.
120 - Built hidden symbol state length counter for 7-state
      output.  It found these stats:
        C length distribution (average 148.14 inProb 0.993250 outProb 0.006750)
        H length distribution (average 50.16 inProb 0.980063 outProb 0.019937)
        L length distribution (average 105.42 inProb 0.990514 outProb 0.009486)
        Q length distribution (average 94.25 inProb 0.989389 outProb 0.010611)
        T length distribution (average 56.92 inProb 0.982431 outProb 0.017569)
      I put this info into the 7-state aligner.
         Started on idbQuery program for extracting sequence data
      out of the Intronerator database.  Ended up fixing a bug
      in void unalignedUnpackDna(bits32 *tiles, int start, int size, DNA *unpacked)
      which couldn't handle short sizes.
         Added motifOutput=xxxx parameter to ameme.
121 - Continued work on idbQuery.  Skipped exon region works.
122 - Continued work on idbQuery.  Skipped intron region works. 
123 - Continued work on idbQuery.  Alt3 and alt5 intron regions work.  Skipped
      exon and skipped intron regions changed from N^2 time to close to N time.
      (Just a couple minutes for whole genome now.)      
124 - Continued worm on idbQuery. C. briggsae homology now works fully:
      high or low homology, any, most, full, or piecemeal.  cDNA aspect
      sped up but any/most/full/piecemeal not implemented.
125 - Implemented any/mostly/throughout/piecemeal on cDNA restriction.
      Implemented rest of idbQuery including recursive input/output and
      restrictions based on gene predictions. Wrote help text for .html
      page.
126 - Various refinements to idbQuery and small changes from Unix port.
127 - Made Improbizer/Motif Matcher aware of motifs on the opposite
      strand for the most part.  (Improbizer isn't expanding motifs
      in this case.)
128 - Seem to have got Improbizer to expand motifs when searching on both
      strands.
129 - Experimenting with a new algorithm for finding motifs - FragFinder.
      It looks for the most common inexact match.  I put it as the
      tile finder in Improbizer with mixed results.  It finds Rap1
      about 50x faster than the old way.  It only finds UACUAAC
      if the tile size is set to seven.  It doesn't find the yeast
      3' splice site.  For now I've disabled it.
          Meanwhile Improbizer isn't mixing motifs into their
      average on the classical weights.  I abolished the Kentish
      weights.
130 - Finishing up C. elegans/C. briggsae alignment.  Fixed bug where 
      reverse strand alignments werent' being stitched because offsets
      weren't correctly reversed. Put in a repeating element filter
      (still not perfect).
131 - Various small changes to Improbizer/Motif Matcher to make it 
      easier to use from the command line.  Wrote newIntron which
      finds introns that are in C. briggsae but not C. elegans and
      vice versa.  (Still a few bugs I think.)
132 - Debugged newIntron.  Also made tracks.exe not crash when
      you give it a zero-length chromosome range.
133 - Wrote aliStats - to figure out what percentage of the C. briggsae
      sequence are aligning to what parts of the C. elegans genome.
134 - Wrote synteny finding program.  Made newIntron slide and reverse
      complement introns when necessary before displaying, and
      show profile around intron ends.
135 - Added directory wildcard searches to library (just implemented
      under Windows).  Made exonAli and refiAli take command line
      arguments for which cDNA and which genomic data files to use.
      Made yeastInt to look for yeast introns (just bypassing some
      worm stuff and alt-splicing stuff from introns.c)
136 - Started implementing patSpace - a pattern space based cDNA alignment
      algorithm. Various odds and ends. Updating ~zahler, and hopefully, 
      later, ~kent intronerator both database and code. (dEC 17, 1999).
137 - Back port from Unix.
138 - Adapting PatSpace to problem of bridging gaps in BACs with cDNAs.
139 - Continued work on PatSpace. 
140 - Continues work on PatSpace.  It's working on the worm test cases
      now.  Next step - human.
141 - Starting to put PatSpace onto humans.  New gbToFaRa that defines
      ra (rna annotation) file format and creates fa files too.
142 - Continued word on gbToFaRa.  Gave it a little expression evaluator
      to decide what to filter out.
143 - Continued work on gbToFaRa. Wrote PatCount which generates .ooc
      files containing a list of overrepresented oligomers for PatSpace.
      Made PatSpace use these.
144 - Debugging PatSpace on human.  Was an error that occurred on sequences
      between 1012 and 1023 bases long that was a bitch to find and fix!
      Added a confidence value to alignments and pairs.
145 - More PatSpace debugging.  Replaced complex, fragile calculations of
      what sequence a block index corresponded to with a lookup table that
      appears more robust.
146 - More PatSpace debugging.  Was not lower-casing all nucleotides leading
      to some misses.  Test runs are starting to look good vs. BLAST on human.
147 - Adapting gbToFaRa to handle unfinished BACs.  The main challenge is
      locating the contig information in the comment field, which varies
      from group to group, and sometimes (particularly for the Beutenbergstrasse
      group) from submission to submission from the same group. Looks like
      this is good enough for our purposes. Writes BACs to individual FA files.
148 - Made pairClo program to find 3' 5' pairs of ESTs.
149 - Adapted patSpace to use 3' 5' pair list.  Fixed bug in fuzzyFind.c's
      hasRepeat.
150 - Made fof.c/fof.h - improved index file handlers.  These will index multiple
      files. They will optionally only take the first of duplicate names. The
      interface is actually slightly easier than snof too.
151 - Built sortFilt for filtering patSpace output.
152 - Built scanRa to query a .ra file.
153 - Started on chkGlue to test gluing.  It currently displays graphically
	an unfinished BAC's contigs aligned against a finished BAC.
154 - Refining chkGlue a bit. Fixing back-tracking bug in fuzzyFinder.
      Working on hgGenCon - to generate constrainst for Genie on chromosme
      22.
155 - Fixed some bugs in hgGenCon and generally improved it.
156 - More bug fixing and improvement on hgGenCon. Bumped
      maxIntron size in PatSpace from 32k to 48k.
157 - Installing new hard drive.  Backing up.
158 - Playing around with "maximal" merging rule for hgGenCon.
      Decided not to go with it for now.  Renamed PatSpace
      the application to aliGlue, and made it use the library
      version of PatSpace.
159 - Wrote jkshell - something that does a lot of C shell things
      for windows.  (History, variables, wildcard expansion, foreach).
	  Sadly it can't get around MS-DOS command line 256 char limit 
	  which makes wildcard expansion less useful than it could be....
160 - Improvements on jkshell.  Can use arrow keys in history list.
      (Had to replace fgets with console io and do a line editor.)
	  The :r and :e modifiers to shell variables work to give
	  you just the prefix and just the suffix respectively.
161 - Put input/output redirection in jkshell.
162 - Added cat, mv, cp, and ls commands to shell.  Improved
      control C handling.  Wrote a fast grep (not in shell).
163 - Playing with Pipes.  I can't figure out how to determine
      end of file though.  The final _read() call just seems
	  to hang.  Nevermind.  Decided the shell is good enough.
	  Back to bioinformatics!  Sending off this batch of code
	  to Ewan Birney at ENSemble.
164 - Version sent over to Linux box.  Not many changes....
165 - Starting to interface with ENSemble's MySQL database.
166 - More interface with ENSembl database.  Have routines for
      getting feature, getting genes, tracking resources....
167 - Very beginnings of gene browser.  Displays EnsEMBL gene
      prediction.  Has zoom and scroll buttons but they don't
      work too well.
168 - Adapted things to contig rather than sequence coordinates
      (this was the problem with zoom and scroll).
169 - Loaded mySQL database with ESTs, bacEnds, fin, and unfin
      bacs.
170 - Have browser showing EnsEMBL features.
171 - Browser returns DNA when you click on ruler or transcript.
172 - Adapting browser to run at UCSC.
173 - Working on hgap database and loadDb.  Loads .fa and .ra files
      into normalized relational tables.
174 - More work on hgap database and loadDb. Put in ordering table
      (a tree structure).
175 - Refining ordering and contig tables and their C memory 
      representation.  Recovering from big Linux system meltdown.
176 - More refining hgap database composite coordinate system.
177 - Put cDNA hits for ens16 test set into database.
178 - Starting to display cDNA hits in browser.
179 - Parsing xblast hits and putting them into database.
180 - Added xblast hits to browser.
181 - Adding hide/dense/full option to browser display.
182 - Put shades of gray in UCSC homology displays.
183 - Made UCSC mrna tracks collectively just do one (big!) query.
      I don't know whether it will be faster or slower.  It might
      be necessary to separate the non-intron ESTs into another
      table....
184 - Put shades of gray into Ensembl tracks and linked them.
185 - Can click on EST or RNA in browser to get info and alignment.
186 - Starting on clustAlt - new alt-splicing savvy clusterer.
187 - Continued work on clustAlt.  Working in a few simple cases now.
188 - Continued work on clustAlt.  Working on moderately complex cases.
189 - Redid clustAlt using modified Ray Wheeler method.
190 - ClustAlt seems to be working quite well on clones now.
191 - Fixed a few clustAlt small bugs and modularized it.
192 - Timing mysql queries:
         10000 random lookups integer indexed 13.71 seconds first time.
	                                       6.73 seconds second time.
         10000 random lookup accession         8.01 second time.
	                                     (First time in cache I think)
         10000 random lookup fof system       42.7 seconds first time.
	       (index only)                   37.6 seconds second time.
         10000 random lookup fof system       62.6 seconds first time.
	       (keys too)                     46.1 seconds second time.
      MySQL is looking mighty fine!  4-5 times faster than the fof stuff.
           Started writing autoSql - something that generates a SQL
      create table statement, as C structure declaration, and code
      to load and save the structure to the database.
193 - autoSql is mostly working.
194 - Put altGraph (aka clustAlt) into database and doing a little
      modularization.
195 - Modularized altGraph into geneGraph.c and gg*.c and put into
      library.  altGraph is now viewing/database loading application.
196 - Put in "direction barbs" on introns in browser.  Also you
      can click on a dense track to get the full version of it.
197 - Put clusters into the browser.
198 - Made altGraph generate Genie constraints.
199 - Made autoSql produce separate .h, .c, and .sql files.  Converted
      tokenizer to lineFile oriented one.  Report position of error
      in spec files.
200 - autoSql more or less handles objects containing objects and lists
      of objects.
201 - Got a good start on the SuperStitcher.  Merges "no brainer" overlaps
      and builds graph.  Still have to do DP on graph.
202 - SuperStitcher seems to be working, though only tested on one clone.
203 - Modularized superStitcher some and moved most of it into library. 
      Tested on 16 clones.
204 - Made aliGlue use superStitcher.
205 - Odds and ends.  Just starting ldHgGene program for loading Genie
      predictions into database.
206 - ldHgGene seems to be working.
207 - Displaying Genie tracks in browser.
208 - Fixed ens.c so that DNA is reverse complemented when it should be.
      Fixed bug in aliGlue that messed up stitching.
      Made psHits stitch and load mrnaAli, estAli, and noIntronEstAli
      tables directly.
      Have browser using mrnaAli, estAli, and noIntronEstAli tables
      and clone coordinates for mrna alignment displays.
      Starting to make altGraph use mrnaAli and estAli tables.
209 - Fixed bug in SuperStitcher in findCrossover where off by one.
      (Led to bad introns...)  Updated altGraph to use clone
      coordinates in db, browser, and generator program.  Filtering
      out some of the trashier mrna alignments from browser.
210 - Simplified inequalities in ggGraph.c to fix problem Ray 
      spotted in constraints on AB023052.  Ported to UCSC.
211 - Fixed autoSql to generate correct "commaIn" code for 
      fixed length strings.
212 - Working on bac2bac test bac vs. bac alignment page.
      This has involved some changes to fuzzyFind.c (eep!)
213 - A little refinement of bac2bac.  Moving some code that
      drives superStitcher,fuzzyFinder, and patSpace together
      into supStitch.c/.h in the library.  Adding ffStringency
      parameter to ssStitch.
214 - Fixed bug in supStitch.c that was causing 120% scores
      in some tandem repeat cases.   Fixed some Intronerator
      database routines that had broken from side effects of
      changes to code from human genome stuff.
215 - Made psLayout - program that generates info for Nick's
      layout generator.
216 - Added float type to autoSql.  Tinkering on psLayout.
217 - Added repeat masker output to psLayout.
218 - Put copyright notice on files in lib and inc.  Improved an
      error message in Improbizer.  Put README in the backup.
219 - Added simple objects to autoSql.  (You can have arrays
      rather than lists of these.)  Wrote autoSql.doc file.
220 - Made aliGlue execute the script "getData" if it
      doesn't find the contents of the "other list".
      Hopefully this will make it fetch data when running
      on the cluster automagically....  Added version field to 
      gbToFaRa and .version to it's fa output on BACs.
      Program sfToHgap that uses simple autoSql objects
      reads .SF files, realligns, and saves alignments
      to database.
221 - Added command to altGraph that will generate GFF constraints
      just based on .fa file lists for mRNA and genomic. (Decoupled
      from database, does mRNA alignment.)
222 - A little more testing and prettying of the new altGraph option.
223 - Made waba program - does cross species alignments without
      depending on database.
224 - Wrote fa4blast - to convert big genomic fa files into something
      blast can handle.  Updated xenoAli/newIntron.  Minor change
      to psHits.
225 - Wrote blastParse module and put in library.  Wrote blastStats
      and updated aliStats in xenoAli to compare BLAST vs. WABA
      alignments.
226 - Misc little things.  Changed ext_file to extFile field in
      h database.
227 - Adapting things to UCSC machines.  Made "psCluster" program
      for doing 1st cDNA alignment on cluster.
228 - psLayout gets extra fields.  New clone-contig directory
      structure maker.  Work on ccCp cross cluster copy program.
229 - Made pslSort, pslReps, and pslSortAcc.
230 - Misc debugging of psLayout and other psl things.
231 - Misc debugging of psLayout and other psl things.
232 - Separated out more of lib into hg/lib. Incomplete work
      on AltGraph, which won't compile currently.
233 - Got AltGraph to work on .psl files.
234 - Starting to work on ooGreedy o+o program.
235 - A bit more work on ooGreedy.
236 - More work on ooGreedy.  Extends rafts, merges on forward strand.
237 - ooGreedy seems to be making rafts correctly on both strands.
238 - ooGreedy building linear mRNA linkages between rafts ok I think.
239 - Made directed graph module.  Putting mRNA linkages into graphs in
      ooGreedy
240 - Debugging of mRNA linkage graphs in ooGreedy.
241 - Put default position into clones and frags in ooGreedy.
242 - Put in threading/default ordering graph traversal algorithm into
      ooGreedy.
242 - Put in flipping connected components to maximize agreement with defaults.
243 - Starting to work on getting best path through a raft in ooGreedy.
244 - Best path works now for cases with no reverse strand involvement.
245 - Working on reverse strand best path in ooGreedy
246 - ooGreedy writing out golden path fa's and files.  Needs more
      testing but so far looks good.
247 - Fixed ooGreedy bug that sometimes put cycles in it's mrna graphs.
      Added est to mrna.  Made it join *slightly* less perfect matchers into
      rafts.
248 - Made ooGreedy allow slightly less perfect matches into rafts.  Just
      setting up things to do bacEnds as well.
249 - Looks like ooGreedy is doing bacEnds.
250 - Starting to put barges (clone ordering) into ooGreedy.
251 - Barges seem to be working.  Did goldToAgp translator.
252 - Version 21 of ooGreedy (used for "provisional golden path through
      practice first draft).
253 - Version 24 of ooGreedy.  Barging reworked in a fairly major way.
254 - Version 25 of ooGreedy.  Fixed a few bugs.
255 - Version 26 of ooGreedy.  Reworking sequence path through rafts.
256 - Version 27 of ooGreedy.  Sequence path through rafts theoretically
      working in more robust fashion.  Needs testing.
257 - Version 28 of ooGreedy.  Tested.  Some very minor fixes.  Rewrote
      ffaToFa and cmParse.  (The former because lost (argh!) latest version
      of source.)
258 - Apparently lost a few pieces of code.  Rats.  Made
      pslSort to actually delete it's temp files again.
      psLayout was saving more than it used to, resulting
      in 10x longer output (again).  Fixed this but couldn't
      quite remember how I did it before, so it's a little
      different.  (No changes to ooGreedy for a change!)
259 - Transferring source bac to garage for primary development.
260 - Started work on gapper - to find gaps in sequence bridged by bac end pairs.
261 - A littler more gapper work.
262 - More gapper.
263 - Gapper version 2 - pretty nice interpretation of bac-end-pairs.
264 - A little more gapper and ooGreedy work.
265 - ooGreedy version 33.  Fixed (I hope) reversals.
266 - ooGreedy version 34.  80% of reversals seem well fixed.  There's a
      bandaide over the last 20% (about 100 in genome).  Constrained priority
      graph traversal in place, working very nicely and quickly.  Graph
      package should let you attatch values to edges as well as nodes now.
267 - ooGreedy version 36.  Minor changes in BAC end scoring.  Version
      that created oo.9.
268 - ooGreedy version 37.  Added in constraints to keep fragments inside
      of clones.
269 - ooGreedy version 40. Debugging of keep-in-clone constraints.
270 - Made ooTester to create pure synthetic test sets for ooGreedy to
      help make sure I'm dealing with reverse strand issue correctly
      everywhere.  It's actually passing the tests with flying colors!
271 - New version of ccCp.
272 - ooGreedy version 46. Noodling with BAC end scoring function.
      Recalculating rafts after barge building to keep rafts in barges.
273 - ooGreedy version 52.  Easing up of BAC end scoring with very
      good results now that rafts are better constrained.  Numerous
      changes to programs that build oo.N directories.
274 - Some very small changes.  Version used to build oo.12.
275 - Started on parasol - a parallel scheduler.
276 - More work on parasol - a parallel scheduler.
277 - Did liftUp and ooLiftSpec to move .psl, .out, and .agp files
      from contig to chromosome coordinates.
278 - Starting to do new browser (hTracks).  Did nib code for
      1/2 byte representation of a nucleotide (A, C, G, T, N).
279 - Can click on assembly and mRNA tracks in browser.
280 - Added contig and clone fragment tracks to browser.
      Implemented spaceSaver layout module in library.
281 - Added coverage track.  Made coverage and clone realignment
      track dense view show meaningful gray level for coverage.
282 - Misc refinements to browser.
283 - Made pslGenoJobs to make condor submission file for gene vs.
      geno alignment.  Added 'g2g' option to psLayout which is
      genomic but only writes out very good matches (so as not
      to fill up disk doing genome vs. genome).  Improved ccCp
      to stay in switch.
284 - ooGreedy version 55.  Raft building algorithm uses global
      rather than local max in sequence overlap.  Also it 
      won't overlap sequences that are farther apart than
      500000 bases in map.
285 - Added Genie predictions to chromosome 18 browser.
286 - Misc little changes mostly to browser.
287 - ooGreedy version 59. (Three steps forward two steps back).
      Made fragPart sequence extraction utility.
288 - Click on mRNA in browser looks up alignment in database
      rather than redoing alignment.  Made g2gSeqOverlap for
      David's contamination search.
289 - Made barger (up to version 3) to do sequence overlap and
      STS based clone map.
290 - Barger v. 4.  Made cmpMap to compare barger w/ Wash U
      maps.
291 - Barger v. 5.  Work on hTracks browser in connection
      with getting it on genome.ucsc.edu and DK's latest
      alignments.
292 - ooGreedy version 65. Reinstated more rigorous raft merging
      from ooGreedy 53.  Fixed bug in raftOfTwo producing unnormalized
      rafts that make it crash sometimes.
293 - Transferring code to Sanger Centre
294 - Added 'copper.fa' output to ooGreedy (raft by raft output).
295 - ooGreedy68.  Building barges with algorithms from barger.
296 - Various small changes.  Made 'g2g' parameter in psLayout
      use an 11 rather than a 10 seed size. Made (yet another)
      run length compresser decompresser.  Started to implement
      compressed quality scores.  (I can't really spare 12 more
      gigabytes for quality scores!)
297 - Finished implementing compressed quality scores.
298 - ooGreedy70 - quality scores read in but not used.  
299 - ooGreedy73 - quality scores used in ooGreedy71, then
      taken out again.  Map taken literally in parts in ooGreedy72.
      NT contigs used in ooGreedy73.
300 - Code back in California.
301 - ooGreedy74 - Fixed and refined handling of NT contigs.  Version
      used on July 17, 2000 freeze.
302 - ooGreedy75 - Fixed dropped edge problem introduced in ooGreedy66.
      Fixed fuzzyFinder huge contig slowdown.  (looking for exact
      match with strstr causeing it to search to zero char at end of
      contig rather than just a range.)
303 - Made newProg for making a code skeleton, rmskOoJobs for creating
      repeat masker jobs on assembled FPC contigs, and cdnaOnOoJobs
      for creating cDNA alignment jobs on assembled FPC contigs.
304 - Made fakeOut - to generate a .out file from a .masked file.
      Fixed slowdown on long sequences in fuzzyFinder/PatSpace
      /SuperStitcher.  Made these programs find short internal
      exons.
305 - Various programs to load browser database.
306 - More programs to load browser database (in src/hg/makeDb/*/*.c)
307 - Browser for full genome is up!
308 - Refinements to browser and (mostly) click processor.
309 - More browser refinements - can jump to an mRNA.  It gives
      you a list when mRNA aligns more than one place.
310 - More browser refinements.  There's an index pane to the
      left of the mRNA alignments now.  The browser search
      criteria can include bits of the gene or product name.
311 - More browser refinements.  Search criteria expanded to
      include authors, cell types, description, etc.  Individual
      words are anded together in search.  Improved the
      gray-scale dense EST display.
312 - More browser refinements.  Hyperlinks from mRNA gene, product
      and author fields to Pubmed.
313 - RangeGraph routines for tracking consistency of a graph
      with ranges attatched to each edge.
314 - Put RepeatMasker track in browser.  Starting on Tetraodon.
315 - Wrote hgWaba and used it to load database with Tetraodon
      alignment on chromosome 18-21.  (17 and 22 are still
      aligning).
316 - Displaying tetraodon in browser.
317 - Misc little improvements to browser and waba.  Made ppAnalyse
      program that looks around and processes Whitehead paired
      reads.
318 - Changes to ppAnalyse.
319 - Working on putting paired clone reads into ooGreedy.  Have
      added "bridge" and "cable" structures to edges of graph.
      These include range information and also provide evidence
      for each bridge between rafts.
320 - Have added distance constraints to mRNA, EST and BAC end
      parts of graph.
321 - Modified ppAnalyse to make size distributions of inserts
      in various projects.  Modified relPairs to distribute
      pair files with extra info across oo.N directories.
322 - ooGreedy 76.  Paired reads seem to be working.
323 - Wrote ooChains - to add fragChains to each contig in ooDir.
324 - ooGreedy 77.  Added fragChains to ooGreedy.
325 - ooGreedy 78.  Sped up from ~28 hours to do genome
      to ~4.5 hours by removing edges with no distance
      constraints from Belmann-Ford computation.
326 - ooGreedy 80.  Changed default frag positions.  Added
      some packing heuristics.  Commented out most but not
      all of them.
327 - ooGreedy 82. Added back in packing heuristics; wrote
      program to measure clone density; found density actually
      decreased; removed all packing heuristics.
328 - Made qaGold program which makes up a .qa file for a
      contig given a gold.N file and .qac files for clones.
329 - Added isochores track to browser.
330 - Updated liftUp to handle BED files.  Wrote ctgToChromFa
      to merge contig level .fa files to chromosome level ones.
331 - Various small changes in course of processing map for
      Sept 2000 freeze.
332 - Made liftUp handle .gtf and .gff files.  Made lib/gff.c
      work on .gtf as well as .gff files.  Reworking ldHgGene.
      Displaying Genie predictions in browser. Displaying
      Tandem Repeats in browser.
333 - Added CpG Island track.
334 - Added Ensembl gene track.  Put in click action for CpG Islands.
335 - Added in Chromosome band track.  Clicking on it zooms you to band.
      Put a horizontal white line through gaps that are bridged.
336 - Coverage track is better reflection of quality of sequence.
337 - Put ensembl and genie peptide predictions into database, writing
      hgPepPred in the process.  Added 'lstring' type to autoSql.
338 - Put protein predictions up on click on gene track.  Put up
      gcPercent track.
339 - Put in duplication track.
340 - Various small utilities for gathering statistics and 
      processing WashU paired reads.
341 - Work on stuff to make graphics for foldout.
      Fix I hope for negative overlaps in psLayout.
342 - Added knownInfo table to database.  Reworked hgap.h/.c
      module into hgRelate.h/.c, moving some to hdb as well.
      More work on foldout program (hg/foldDb).
343 - Odds and ends on foldout program.  ooGreedy85 -
      making it track map a little better.
344 - Working on ooGreedy to fuse NT contigs into a single
      fragment early on.  This is coming ok, but all but
      only the initial part of ooGreedy is running.
345 - Fixed bug where psLayout sometimes made negative gap
      between target blocks.  More work on ooGreedy and NT
      contigs.  Seems to be transforming the self.psl file
      into NT coordinates and fusing blocks where appropriate
      on both strands of at least the Q side.
346 - ooGreedy88 - fuses NT contigs and once again generates
      .agp files.  Still a few rough spots (see version.doc
      in ooGreedy dir for details.)
347 - ooGreedy88 still.  Can handle split finished sequences 
      if there's a splitFin file describing them.  (Still need 
      to make splitFin file generator.)
348 - ooGreedy90.  Preventing inversions of large rafts.
349 - Added database parameter to the browser.  Started building
      Sept. freeze browser/database.  Modified psLayout so that
      it will do 120 meg sequences without squawking, but will
      thrash horribly if not enough RAM to do so (requires about
      4 meg ram per meg of sequence on genomic side).
350 - ooGreedy93 (fixing rare spurious raft orientation).  
      Sept browser online - still missing most 3rd party tracks,
      but with 2 new SNP tracks.
351 - Put exoFish and rnaGene tables into database and foldout
      data generater.  
352 - Added rnaGene track to browser.  Made browser pass
      start coordinate of item to hgc on clicks.
353 - Added improved STS marker track to Sept. database and
      browser.
354 - Added mouse synteny track.
355 - Reverse complement RNA alignments on opposite strand on
      display, so get GT/AG rather than CT/AC introns.
      Added track for subset of ESTs that have introns.
356 - UTRs are drawn as shorter than coding regions.
      OMIM genes added to foldDb
      Showing names of known genes in the browser.  Clicking
      on the ruler now zooms, and clicking on cyto-band
      track brings up info.
357 - Added aliases to STS track, and more info about FISH clones.
358 - Fixed waba bug where it was showing NULL query file name.
      Various fiddling with the browser, mostly per Tom Pringle's
      requests.  You can set the pixel width.  Feature start and
      end gets passed to the click manager.
      Also some changes to foldDb.
359 - Modified cheapcgi to parse the CGI string once into a
      list/hash rather than parse it each time you look up a variable.
      This is faster and will make it easier to have a routine that
      just dumps all of the cgi variables as hidden....
      Reworking search engine so that it can display results from
      multiple searches at once (that is on mRNA and known genes).
360 - Made pslCoverage program to analyse genomic coverage and
      missassembly based on cDNA alignments.
361 - Fiddled with hg/makeDb/hgGoldGlGap and hgClonePos to make
      it deal with NT contigs.  Browser is now up on October
      assembly with mRNA and assembly tracks.
362 - Various tweaks to browser and database loading program to
      get other tracks on October browser.  I'm well into
      reworking the known genes with information more or less
      straight from OMIM, NCBI and HUGO.  Not visible yet though.
363 - Improved known gene handling visible in browser.  Have
      OMIM, LocusLink, and GeneCards links.
364 - Changes to foldDb.c, the foldout/poster generator mostly.
365 - Programs to set up oo contig directories with NT contig
      info including orientation (ooFinMeld) and with clone
      end info (ooCloneEnds).  Start of ooMendMap program
      which is basically the barge building part of ooGreedy
      separated out for quicker development.
366 - Map simulator (mmTestSet) and scoring function for
      overlapping clone pairs with some ends.  
367 - Start of map maker (mm) program to reconstruct simulator
      output.  (Eventually will get integrated into ooMendMap
      and ooGreedy.
368 - Emergency quickie reassembly treating NT_ contigs as
      clones.  Modifications to psLayout to allow larger
      query sequences.  Modifications to some of the 
      ooDir setup programs.
369 - More tinkering with reassembly.
370 - A lot of work on new clone end based mapping stuff.
371 - Working on ooCloneOverlap - to compute overlap between
      clones based on self.psl and lists of end sequences.
      Clone overlap calculations seem to be mostly the same, but
      slightly better than ooGreedy.
372 - Wrote ooCloneSizes and ooCloneInfo to help set up
      oo directories for clone ends.
373 - More or less working ooCloneOverlap.
374 - Refining ooCloneOverlap, and evaluating mislabeling of ends
      in data.
375 - Reworked mm to take input from contig directories.  Starting
      to output revised barge files - still some work to do.
376 - Wrote program to patch .agp files with NT info.  (patchNtAgp)
377 - MM is putting out pretty nice looking barge files with a handful
      of exceptions on the full genome.  Version mm.13.
378 - Syncing home/school source directories.
379 - ooGreedy.94, mm.15.  First time these work together.
380 - Fixed a couple of obscure psLayout bugs for Neomorphic.
381 - Various odds and ends on browser and ooGreedy.95
382 - Fixed up known genes track a bit.
383 - Backing up before short vacation.  Mostly little browser
      tweaks.
384 - Various small browser oriented fixes.  Link into Ensembl.
385 - Minor tweaking of Ensembl link.
386 - Constraining ooGreedy to prevent rafts from forming involving
      fragments from clones that don't overlap according to the map.
      ooGreedy version 96.
387 - Up to ooGreedy 97 and then back down to 96.  (See ooGreedy/version.doc
      for details).
388 - Trying to set things up for CVS.
