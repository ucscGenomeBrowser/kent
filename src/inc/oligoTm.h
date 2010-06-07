/* oligoTm - calculate melting temperature of relatively short DNA sequences.
 * This is based on the nearest-neighbor thermodynamics of bases from Breslauer,
 * Frank, Bloecker, and Markey, Proc. Natl. Acad. Sci. USA, vol 83, page 3748,
 * and uses work from see Rychlik, Spencer, Roads, Nucleic Acids Research, vol 18, 
 * no 21.  This code was imported from the oligotm module of Whitehead Institute's
 * primer3 program, and adapted into UCSC conventions by Jim Kent.  Any redistribution
 * of this code should contain the following copyright notice from Whitehead:
 *
 * Copyright (c) 1996,1997,1998,1999,2000,2001,2004
 *         Whitehead Institute for Biomedical Research. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1.      Redistributions must reproduce the above copyright notice, this
 * list of conditions and the following disclaimer in the  documentation
 * and/or other materials provided with the distribution.  Redistributions of
 * source code must also reproduce this information in the source code itself.
 * 
 * 2.      If the program is modified, redistributions must include a notice
 * (in the same places as above) indicating that the redistributed program is
 * not identical to the version distributed by Whitehead Institute.
 * 
 * 3.      All advertising materials mentioning features or use of this
 * software  must display the following acknowledgment:
 *         This product includes software developed by the
 *         Whitehead Institute for Biomedical Research.
 * 
 * 4.      The name of the Whitehead Institute may not be used to endorse or
 * promote products derived from this software without specific prior written
 * permission.
 * 
 * We also request that use of this software be cited in publications as 
 * 
 *   Rozen, S., Skaletsky, H.  \"Primer3 on the WWW for general users
 *   and for biologist programmers.\"  In S. Krawetz and S. Misener, eds.
 *   Bioinformatics Methods and Protocols in the series Methods in 
 *   Molecular Biology.  Humana Press, Totowa, NJ, 2000, pages 365-386.
 *   Code available at
 *   http://fokker.wi.mit.edu/primer3/.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE WHITEHEAD INSTITUTE ``AS IS'' AND  ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE  IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE  ARE
 * DISCLAIMED. IN NO EVENT SHALL THE WHITEHEAD INSTITUTE BE LIABLE  FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL  DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS  OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)  HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE. */

#ifndef OLIGOTM_H
#define OLIGOTM_H

double oligoTm(char *dna, double DNA_nM, double K_mM);
/* Calculate melting point of short DNA sequence given DNA concentration in 
 * nanomoles, and salt concentration in millimoles.  This is calculated using eqn
 * (ii) in Rychlik, Spencer, Roads, Nucleic Acids Research, vol 18, no 21, page
 * 6410, with tables of nearest-neighbor thermodynamics for DNA bases as
 * provided in Breslauer, Frank, Bloecker, and Markey,
 * Proc. Natl. Acad. Sci. USA, vol 83, page 3748. */

double oligoDg(char *dna);
/* Calculate dg (change in Gibb's free energy) from melting oligo
 * the nearest neighbor model. Seq should be relatively short, given 
 * the characteristics of the nearest neighbor model (36 bases or less
 * is best). */

double longSeqTm(char *s, int start, int len, double salt_conc);
/* Calculate the melting temperature of substr(seq, start, length) using the
 * formula from Bolton and McCarthy, PNAS 84:1390 (1962) as presented in
 * Sambrook, Fritsch and Maniatis, Molecular Cloning, p 11.46 (1989, CSHL
 * Press).
 *
 * Tm = 81.5 + 16.6(log10([Na+])) + .41*(%GC) - 600/length
 *
 * Where [Na+] is the molar sodium concentration, (%GC) is the percent of Gs
 * and Cs in the sequence, and length is the length of the sequence.
 *
 * A similar formula is used by the prime primer selection program in GCG
 * (http://www.gcg.com), which instead uses 675.0 / length in the last term
 * (after F. Baldino, Jr, M.-F. Chesselet, and M.E.  Lewis, Methods in
 * Enzymology 168:766 (1989) eqn (1) on page 766 without the mismatch and
 * formamide terms).  The formulas here and in Baldino et al. assume Na+ rather
 * than K+.  According to J.G. Wetmur, Critical Reviews in BioChem. and
 * Mol. Bio. 26:227 (1991) 50 mM K+ should be equivalent in these formulae to .2
 * M Na+.
 *
 * This function takes salt_conc to be the millimolar (mM) concentration,
 * since mM is the usual units in PCR applications.  */

double seqTm(char *seq, double dna_conc, double salt_conc);
/* Figure out melting temperature of sequence of any length given
 * dna and salt concentration. */
#endif /* OLIGOTM_H */

