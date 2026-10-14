#!/usr/bin/env python3
"""Unit tests for the placement rules in mavedbPlace.py, on small synthetic proteins.

No database: a FakeReference stands in for mavedbPlace.Reference. Run with
    python3 tests/testMavedbPlace.py
from the mavedb otto directory.
"""

import collections
import os
import random
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
import mavedbPlace as mp   # noqa: E402

AAS = 'ACDEFGHIKLMNPQRSTVWY'


def randomProtein(n, seed):
    rng = random.Random(seed)
    return ''.join(rng.choice(AAS) for _ in range(n))


class FakeReference(object):
    """Just the parts of mavedbPlace.Reference the protein-level rules use."""

    def __init__(self, peptides, genes=None, mane=()):
        self.peptides = peptides
        self.geneNps = genes or {}
        self.maneSelect = set(mane)
        self.manePlus = set()
        self.npToNm = {np: 'NM_' + np[3:] for np in peptides if np.startswith('NP_')}
        self.npToNmHist = {}
        self.npGene = {np: g for g, nps in self.geneNps.items() for np in nps}
        self.geneSymbols = set(self.geneNps)
        self.uniprotGene = {'P00001': 'GENEA'}
        self.ensemblGene = {}
        self.enspToEnst = {}

    def peptide(self, acc):
        return self.peptides.get(acc)

    def proteinsOfGene(self, gene):
        return mp.Reference.proteinsOfGene(self, gene)

    def otherVersions(self, prot):
        base = prot.split('.')[0]
        return [a for a in self.peptides if a.split('.')[0] == base and a != prot]


def term(wt, pos, alt, prefix=None):
    head = (prefix + ':') if prefix else ''
    return '%sp.%s%d%s' % (head, mp.ONE_TO_THREE[wt], pos, alt)


def rowsForFragment(seq, start, length, prefix=None, mavedb=None, construct=None):
    """Experiment rows for a fragment seq[start:start+length], numbered from 1.

    mavedb: (protein, offset) MaveDB maps with, or None for unmapped rows.
    construct: {experiment position: residue} the construct carries instead of seq's.
    """
    rows = []
    for i in range(length):
        pos = i + 1
        wt = (construct or {}).get(pos, seq[start + i])
        alt = 'A' if wt != 'A' else 'G'
        row = {'accession': 'v%d' % pos, 'hgvs_nt': 'NA', 'hgvs_splice': 'NA',
               'hgvs_pro': term(wt, pos, mp.ONE_TO_THREE[alt], prefix),
               'mavedb.post_mapped_hgvs_p': 'NA', 'mavedb.post_mapped_hgvs_g': 'NA'}
        if mavedb:
            prot, off = mavedb
            row['mavedb.post_mapped_hgvs_p'] = term(wt, pos + off, mp.ONE_TO_THREE[alt], prot)
        rows.append(row)
    return rows


def scoreSet(name='GENEA domain', **target):
    t = {'name': name, 'category': 'protein_coding', 'externalIdentifiers': []}
    t.update(target)
    return {'urn': 'urn:mavedb:test', 'targetGenes': [t]}


def placedLabels(placements):
    return collections.Counter(p.label for p in placements if p.status == 'placed')


class TermParsing(unittest.TestCase):

    def check(self, text, expect):
        t, why = mp.parseProteinTerm(text)
        got = (t.wt, t.pos, t.alt, t.kind) if t else why
        self.assertEqual(got, expect, text)

    def testShapes(self):
        self.check('p.Thr3Met', ('T', 3, 'M', 'sub'))
        self.check('p.Arg12Ter', ('R', 12, '*', 'nonsense'))
        self.check('p.Arg12*', ('R', 12, '*', 'nonsense'))
        self.check('p.Leu5=', ('L', 5, '=', 'syn'))
        self.check('p.Gly7del', ('G', 7, '-', 'del'))
        self.check('p.Arg12delinsTrp', ('R', 12, 'W', 'sub'))
        self.check('p.Ter101Trp', ('*', 101, 'W', 'sub'))
        self.check('NP_000570.1:p.Asp2Ala', ('D', 2, 'A', 'sub'))
        self.check('p.=', 'synonymousNoPosition')
        self.check('NA', 'noTerm')
        self.check('p.[Thr3Met;Gly4Ala]', 'multiVariant')
        self.check('p.Gly7_Ala9del', 'multiResidue')
        self.check('p.Gly7_Ala8insVal', 'multiResidue')
        self.check('p.Arg12delinsTrpGly', 'multiResidue')
        self.check('p.Arg12fs', 'frameshift')
        self.check('p.Arg12ProfsTer3', 'frameshift')

    def testPrefix(self):
        t, _ = mp.parseProteinTerm('TP53 DBD:p.Arg175His')
        self.assertEqual(t.prefix, 'TP53 DBD')

    def testMultiVariantByProteinFirst(self):
        row = {'hgvs_pro': 'p.Thr3Met', 'hgvs_nt': 'c.[8C>T;9C>G]', 'hgvs_splice': 'NA'}
        self.assertFalse(mp.isMultiVariant(row))
        row = {'hgvs_pro': 'p.[Thr3Met;Gly4Ala]', 'hgvs_nt': 'NA'}
        self.assertTrue(mp.isMultiVariant(row))
        row = {'hgvs_pro': 'NA', 'hgvs_nt': 'c.[8C>T;20C>G]'}
        self.assertTrue(mp.isMultiVariant(row))

    def testGenomic(self):
        g = mp.parseGenomicTerm('NC_000017.11:g.7675088C>T')
        self.assertEqual((g.acc, g.start, g.end, g.snvRef()), ('NC_000017.11', 7675088,
                                                              7675088, 'C'))
        self.assertEqual(g.bedRange(), (7675087, 7675088))
        g = mp.parseGenomicTerm('NC_000017.11:g.100_101insA')
        self.assertEqual(g.bedRange(), (100, 100))
        self.assertIsNone(mp.parseGenomicTerm('NA'))


class Offsets(unittest.TestCase):

    def setUp(self):
        self.seq = randomProtein(400, 1)
        self.ref = FakeReference({'NP_1.1': self.seq}, {'GENEA': ['NP_1.1']}, ['NP_1.1'])

    def place(self, rows, ss=None):
        return mp.placeScoreSet(ss or scoreSet(), rows, self.ref)

    def testMavedbOffsetExact(self):
        rows = rowsForFragment(self.seq, 100, 40, mavedb=('NP_1.1', 100))
        pls, choices, _ = self.place(rows)
        c, _ = choices[None]
        self.assertEqual((c.prot, c.offset, c.route), ('NP_1.1', 100, 'mavedb'))
        self.assertEqual(placedLabels(pls), {mp.PLACED_MAVEDB: 40})

    def testUnmappedRowsUseMavedbOffset(self):
        """R1: MaveDB skipped some rows (multi-base codon changes); its offset places them."""
        rows = rowsForFragment(self.seq, 100, 40, mavedb=('NP_1.1', 100))
        for r in rows[:15]:
            r['mavedb.post_mapped_hgvs_p'] = 'NA'
        pls, _, _ = self.place(rows)
        self.assertEqual(placedLabels(pls), {mp.PLACED_MAVEDB: 25, mp.PLACED_MAVEDB_OFFSET: 15})

    def testWrongMavedbOffsetIsCorrected(self):
        """CCR5-like: MaveDB maps at +2, residues fit at +0. Nothing of MaveDB's is kept,
        not even the rows that agree by chance."""
        rows = rowsForFragment(self.seq, 0, 60)
        for r in rows:
            t, _ = mp.parseProteinTerm(r['hgvs_pro'])
            wt2 = mp.residueAt(self.seq, t.pos + 2)
            r['mavedb.post_mapped_hgvs_p'] = term(wt2, t.pos + 2, 'Ala', 'NP_1.1')
        pls, choices, _ = self.place(rows)
        c, _ = choices[None]
        self.assertEqual((c.offset, c.route), (0, 'label'))
        self.assertEqual(c.mavedbOffset, ('NP_1.1', 2))
        self.assertEqual(placedLabels(pls), {mp.PLACED_CORRECTED: 60})
        self.assertTrue(all(p.genomic is None for p in pls))

    def testDominantOffsetUsesAllMappedRows(self):
        """The dominant MaveDB offset is counted over every mapped row, not only the ones
        that already agree."""
        rows = rowsForFragment(self.seq, 50, 30, mavedb=('NP_1.1', 50))
        for r in rows[:20]:
            t, _ = mp.parseProteinTerm(r['hgvs_pro'])
            r['mavedb.post_mapped_hgvs_p'] = term('A', t.pos + 7, 'Gly', 'NP_1.1')
        _, choices, _ = self.place(rows)
        c, _ = choices[None]
        self.assertEqual(c.route, 'label')
        self.assertEqual(c.offset, 50)

    def testLabelSearchNoMavedb(self):
        """MPL-like: MaveDB mapped nothing; the residues fit one offset on the gene."""
        rows = rowsForFragment(self.seq, 250, 30)
        pls, choices, _ = self.place(rows)
        c, _ = choices[None]
        self.assertEqual((c.offset, c.route, c.mavedbOffset), (250, 'label', None))
        self.assertEqual(placedLabels(pls), {mp.PLACED_ASSAY: 30})

    def testOneMismatchConstruct(self):
        """MPL S505N-like: the construct differs at one position; that position's variants
        are dropped and the rest placed."""
        pos = 12
        wt = 'N' if self.seq[250 + pos - 1] != 'N' else 'Q'
        rows = rowsForFragment(self.seq, 250, 30, construct={pos: wt})
        pls, choices, reasons = self.place(rows)
        c, _ = choices[None]
        self.assertEqual((c.offset, c.mismatched), (250, {pos}))
        self.assertEqual(reasons['constructDiffers'], 1)
        self.assertEqual(placedLabels(pls), {mp.PLACED_ASSAY: 29})
        self.assertEqual(pls[1].construct,
                         'differs from NP_1.1 at position %d (%s in the reference, %s in the '
                         'construct)' % (pos + 250, mp.ONE_TO_THREE[self.seq[250 + pos - 1]],
                                         mp.ONE_TO_THREE[wt]))

    def testOneMismatchMavedbRoute(self):
        rows = rowsForFragment(self.seq, 100, 40, mavedb=('NP_1.1', 100),
                               construct={5: 'W' if self.seq[104] != 'W' else 'C'})
        pls, choices, reasons = self.place(rows)
        c, _ = choices[None]
        self.assertEqual((c.route, c.mismatched), ('mavedb', {5}))
        self.assertEqual(reasons['constructDiffers'], 1)

    def testTwoMismatchesRejected(self):
        rows = rowsForFragment(self.seq, 250, 30,
                               construct={3: self.otherThan(252), 9: self.otherThan(258)})
        _, choices, reasons = self.place(rows)
        self.assertIsNone(choices[None][0])
        self.assertEqual(reasons['noOffset:noFit'], 30)

    def testOneMismatchNeedsTenPositions(self):
        rows = rowsForFragment(self.seq, 100, 9, mavedb=('NP_1.1', 100),
                               construct={3: self.otherThan(102)})
        _, choices, _ = self.place(rows)
        self.assertEqual(choices[None], (None, 'tooFewPositions'))

    def testLabelSearchNeedsTenPositions(self):
        rows = rowsForFragment(self.seq, 250, 9)
        _, choices, _ = self.place(rows)
        self.assertEqual(choices[None], (None, 'tooFewPositions'))

    def testRepeatIsAmbiguous(self):
        """UBC-like: an exact repeat gives two offsets; nothing is placed."""
        unit = randomProtein(76, 7)
        ref = FakeReference({'NP_9.1': unit * 3}, {'UBC': ['NP_9.1']})
        rows = rowsForFragment(unit * 3, 0, 40)
        _, choices, _ = mp.placeScoreSet(scoreSet('UBC'), rows, ref)
        self.assertEqual(choices[None], (None, 'ambiguousOffset'))

    def testStopLoss(self):
        rows = rowsForFragment(self.seq, 380, 20, mavedb=('NP_1.1', 380))
        rows.append({'accession': 'stop', 'hgvs_pro': 'p.Ter21Trp', 'hgvs_nt': 'NA',
                     'mavedb.post_mapped_hgvs_p': 'NP_1.1:p.Ter401Trp',
                     'mavedb.post_mapped_hgvs_g': 'NA'})
        pls, _, _ = self.place(rows)
        self.assertEqual(pls[-1].status, 'placed')
        self.assertEqual(pls[-1].protPos, 401)

    def testGeneFromUniProt(self):
        rows = rowsForFragment(self.seq, 250, 30)
        ss = scoreSet('designed thing', externalIdentifiers=[
            {'identifier': {'dbName': 'UniProt', 'identifier': 'P00001'}}])
        _, choices, _ = self.place(rows, ss)
        self.assertEqual(choices[None][0].offset, 250)

    def testNoGeneNoLabelSearch(self):
        rows = rowsForFragment(self.seq, 250, 30)
        _, choices, _ = self.place(rows, scoreSet('Exons 1-3 minigene'))
        self.assertEqual(choices[None], (None, 'noGene'))

    def testGeneFromNameWord(self):
        rows = rowsForFragment(self.seq, 250, 30)
        _, choices, _ = self.place(rows, scoreSet('S505N GENEA'))
        self.assertEqual(choices[None][0].offset, 250)

    def testIsoformMovePending(self):
        """MaveDB placed the row on another protein with the same residue: kept as MaveDB's
        pending a genomic codon comparison."""
        self.ref.peptides['NP_2.1'] = 'MK' + self.seq
        rows = rowsForFragment(self.seq, 100, 40, mavedb=('NP_1.1', 100))
        t, _ = mp.parseProteinTerm(rows[0]['hgvs_pro'])
        rows[0]['mavedb.post_mapped_hgvs_p'] = term(t.wt, t.pos + 102, 'Ala', 'NP_2.1')
        pls, _, _ = self.place(rows)
        self.assertEqual((pls[0].prot, pls[0].mavedbProt, pls[0].label),
                         ('NP_1.1', 'NP_2.1', mp.PLACED_MAVEDB))

    def testNucleotideOnlyUsesMavedbProtein(self):
        rows = [{'accession': 'n1', 'hgvs_pro': 'NA', 'hgvs_nt': 'c.10A>G',
                 'mavedb.post_mapped_hgvs_p': term(self.seq[9], 10, 'Ala', 'NP_1.1'),
                 'mavedb.post_mapped_hgvs_g': 'NC_000001.11:g.1000A>G'},
                {'accession': 'n2', 'hgvs_pro': 'NA', 'hgvs_nt': 'c.-5A>G',
                 'mavedb.post_mapped_hgvs_p': 'NA',
                 'mavedb.post_mapped_hgvs_g': 'NC_000001.11:g.900A>G'},
                {'accession': 'n3', 'hgvs_pro': 'p.=', 'hgvs_nt': 'c.30A>G',
                 'mavedb.post_mapped_hgvs_p': 'NA',
                 'mavedb.post_mapped_hgvs_g': 'NC_000001.11:g.1020A>G'},
                {'accession': 'n4', 'hgvs_pro': 'NA', 'hgvs_nt': 'c.-9A>G',
                 'mavedb.post_mapped_hgvs_p': 'NA', 'mavedb.post_mapped_hgvs_g': 'NA'}]
        pls, _, reasons = self.place(rows)
        self.assertEqual([p.status for p in pls], ['placed', 'placed', 'placed', 'rejected'])
        self.assertEqual((pls[0].prot, pls[0].protPos), ('NP_1.1', 10))
        self.assertIsNone(pls[1].prot)
        self.assertEqual(reasons['notMapped'], 1)

    def tagged(self, start, length, positions):
        return rowsForFragment(self.seq, start, length,
                               construct={p: self.otherThan(start + p - 1) for p in positions})

    def testTagAtStart(self):
        """PIN1-like: the first residues are a tag; the rest matches exactly."""
        rows = self.tagged(250, 30, [1, 2, 3])
        pls, choices, reasons = self.place(rows)
        c, _ = choices[None]
        self.assertEqual((c.offset, c.kind, c.mismatched), (250, 'tagStart', {1, 2, 3}))
        self.assertEqual(reasons['constructTag'], 3)
        self.assertEqual(placedLabels(pls), {mp.PLACED_ASSAY: 27})
        self.assertEqual(pls[5].construct, '3 added residues at its start')

    def testTagAtEnd(self):
        rows = self.tagged(250, 30, [27, 28, 29, 30])
        _, choices, _ = self.place(rows)
        self.assertEqual(choices[None][0].kind, 'tagEnd')

    def testTagOnMavedbOffsetIsUcsc(self):
        """L-selectin-like: MaveDB's offset fits except a run at the end; accepted, but every
        row is a UCSC placement."""
        rows = rowsForFragment(self.seq, 100, 30, mavedb=('NP_1.1', 100),
                               construct={p: self.otherThan(99 + p) for p in (27, 28, 29, 30)})
        pls, choices, _ = self.place(rows)
        self.assertEqual((choices[None][0].route, choices[None][0].kind), ('mavedb', 'tagEnd'))
        self.assertEqual(placedLabels(pls), {mp.PLACED_MAVEDB_OFFSET: 26})

    def testExactIsoformBeatsTagOnMavedbIsoform(self):
        """MaveDB's isoform differs at the last residues; another isoform fits exactly."""
        tail = ''.join(self.otherThan(i) for i in range(126, 130))
        self.ref.peptides['NP_2.1'] = self.seq[:126] + tail + self.seq[130:]
        self.ref.geneNps['GENEA'] = ['NP_1.1', 'NP_2.1']
        rows = rowsForFragment(self.seq, 100, 30, mavedb=('NP_2.1', 100))
        pls, choices, _ = self.place(rows)
        c, _ = choices[None]
        self.assertEqual((c.prot, c.kind, c.route), ('NP_1.1', 'exact', 'label'))

    def testPointOnMavedbBeatsExactOldVersion(self):
        """TP53 Arg72-like: an old version of MaveDB's protein carries the construct's
        residue; MaveDB's current protein with one differing position still wins."""
        pos = 12
        alt = self.otherThan(100 + pos - 1)
        self.ref.peptides['NP_1.0'] = self.seq[:111] + alt + self.seq[112:]
        rows = rowsForFragment(self.seq, 100, 40, mavedb=('NP_1.1', 100), construct={pos: alt})
        _, choices, _ = self.place(rows)
        c, _ = choices[None]
        self.assertEqual((c.prot, c.route, c.kind), ('NP_1.1', 'mavedb', 'point'))

    def testLongRunIsNotATag(self):
        rows = self.tagged(250, 30, list(range(1, 16)))
        _, choices, _ = self.place(rows)
        self.assertIsNone(choices[None][0])

    def testTagPlusInteriorMismatchRejected(self):
        rows = self.tagged(250, 30, [1, 2, 15])
        _, choices, _ = self.place(rows)
        self.assertIsNone(choices[None][0])

    def testTagLeavingTooFewMatchesRejected(self):
        rows = self.tagged(250, 14, [1, 2, 3, 4, 5])
        _, choices, _ = self.place(rows)
        self.assertIsNone(choices[None][0])

    def testNucleotideOnlyIndelIsGenomicOnly(self):
        """An in-frame deletion's protein term is shifted by the HGVS 3' rule and a
        frameshift's names where the stop appears: placed by the genomic term only."""
        rows = [{'accession': 'd1', 'hgvs_pro': 'NA', 'hgvs_nt': 'NM_1.1:c.31_33del',
                 'mavedb.post_mapped_hgvs_p': term(self.seq[11], 12, 'del', 'NP_1.1'),
                 'mavedb.post_mapped_hgvs_g': 'NC_000001.11:g.1030_1032del'},
                {'accession': 'd2', 'hgvs_pro': 'NA', 'hgvs_nt': 'NM_1.1:c.30dup',
                 'mavedb.post_mapped_hgvs_p': term(self.seq[10], 11, 'Ter', 'NP_1.1'),
                 'mavedb.post_mapped_hgvs_g': 'NC_000001.11:g.1030dup'},
                {'accession': 'd3', 'hgvs_pro': 'NA', 'hgvs_nt': 'NM_1.1:c.31_32delinsGT',
                 'mavedb.post_mapped_hgvs_p': term(self.seq[10], 11, 'Val', 'NP_1.1'),
                 'mavedb.post_mapped_hgvs_g': 'NC_000001.11:g.1031_1032delinsGT'}]
        pls, _, _ = self.place(rows)
        self.assertEqual([(p.status, p.prot) for p in pls],
                         [('placed', None), ('placed', None), ('placed', 'NP_1.1')])

    def testReferenceTermPending(self):
        """No protein term and no MaveDB mapping, but an NM_ term: left for hgvsToVcf."""
        rows = [{'accession': 'r1', 'hgvs_pro': 'NA', 'hgvs_nt': 'NM_007294.3:c.80+4A>T',
                 'mavedb.post_mapped_hgvs_p': 'NA', 'mavedb.post_mapped_hgvs_g': 'NA'},
                {'accession': 'r2', 'hgvs_pro': 'NA', 'hgvs_nt': 'c.80+4A>T',
                 'mavedb.post_mapped_hgvs_p': 'NA', 'mavedb.post_mapped_hgvs_g': 'NA'}]
        pls, _, _ = self.place(rows)
        self.assertEqual([(p.status, p.reason) for p in pls],
                         [('pending', 'referenceTerm'), ('rejected', 'notMapped')])

    def testVcfToGenomic(self):
        g = mp.vcfToGenomic('NC_1.1', 100, 'A', 'G')
        self.assertEqual((g.start, g.end, g.rest, g.bedRange()), (100, 100, 'A>G', (99, 100)))
        g = mp.vcfToGenomic('NC_1.1', 100, 'AGC', 'A')
        self.assertEqual((g.start, g.end, g.rest, g.bedRange()), (101, 102, 'del', (100, 102)))
        g = mp.vcfToGenomic('NC_1.1', 100, 'A', 'AT')
        self.assertEqual((g.rest, g.bedRange()), ('insT', (100, 100)))

    def otherThan(self, i):
        return 'W' if self.seq[i] != 'W' else 'C'


if __name__ == '__main__':
    unittest.main()
