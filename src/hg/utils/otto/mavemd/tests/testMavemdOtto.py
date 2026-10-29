#!/usr/bin/env python3
"""Unit tests for mavemdOtto.py: the parts that decide whether to release."""

import datetime
import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
import mavemdOtto as otto            # noqa: E402


class TestFullBuildDue(unittest.TestCase):
    def testNeverBuilt(self):
        self.assertTrue(otto.fullBuildDue(datetime.date(2026, 11, 4), None))

    def testFirstRunOfAQuarterMonth(self):
        self.assertTrue(otto.fullBuildDue(datetime.date(2027, 1, 6),
                                          datetime.date(2026, 12, 30)))

    def testSecondRunOfTheSameQuarterMonth(self):
        self.assertFalse(otto.fullBuildDue(datetime.date(2027, 1, 20),
                                           datetime.date(2027, 1, 6)))

    def testOrdinaryMonth(self):
        self.assertFalse(otto.fullBuildDue(datetime.date(2026, 11, 4),
                                           datetime.date(2026, 10, 7)))

    def testTooOldEvenInAnOrdinaryMonth(self):
        self.assertTrue(otto.fullBuildDue(datetime.date(2026, 11, 4),
                                          datetime.date(2026, 7, 1)))


class TestReleaseSortKey(unittest.TestCase):
    def testNumberedReleasesSortAfterTheirDay(self):
        names = ['2026-11-02.10', '2026-12-01', '2026-11-02', '2026-11-02.2']
        self.assertEqual(sorted(names, key=otto.releaseSortKey),
                         ['2026-11-02', '2026-11-02.2', '2026-11-02.10', '2026-12-01'])


class TestCheckRelease(unittest.TestCase):
    good = {'variantItems': 462486, 'placedByMavedb': 430133, 'cells': 391758,
            'mapItems': 84, 'overlaps': 0, 'accessionsDropped': 0}

    def testCleanBuildPasses(self):
        self.assertEqual(otto.checkRelease(self.good, self.good,
                                           {'mavemdVar.bb': 1, 'mavemdMap.bb': 1}), [])

    def testEmptyBigBedIsCaught(self):
        problems = otto.checkRelease(self.good, self.good,
                                     {'mavemdVar.bb': 0, 'mavemdMap.bb': 1})
        self.assertIn('mavemdVar.bb has no items', problems)

    def testDroppedAccessionIsCaught(self):
        stats = dict(self.good, accessionsDropped=1)
        self.assertTrue(any('disagreed with the genome' in p for p in
                            otto.checkRelease(stats, self.good, {'a.bb': 1})))

    def testBigItemDropIsCaught(self):
        stats = dict(self.good, variantItems=300000)
        self.assertTrue(any('variant items went from' in p for p in
                            otto.checkRelease(stats, self.good, {'a.bb': 1})))

    def testSmallItemDropPasses(self):
        stats = dict(self.good, variantItems=440000)
        self.assertEqual(otto.checkRelease(stats, self.good, {'a.bb': 1}), [])

    def testMavedbMappingsWithdrawnIsCaught(self):
        # What their staging API looked like in October 2026: the terms are gone, so we
        # place everything ourselves and the cross-check has nothing left to compare.
        stats = dict(self.good, placedByMavedb=1000)
        self.assertTrue(any('withdrawn mappings' in p for p in
                            otto.checkRelease(stats, self.good, {'a.bb': 1})))

    def testNoPreviousReleaseSkipsTheDropChecks(self):
        self.assertEqual(otto.checkRelease(self.good, None, {'a.bb': 1}), [])


class TestCatalogueChanges(unittest.TestCase):
    def testAddedChangedRemoved(self):
        prev = {'a': ('a', '1'), 'b': ('b', '1'), 'c': ('c', '1')}
        cur = {'a': ('a', '1'), 'b': ('b', '2'), 'd': ('d', '1')}
        self.assertEqual(otto.catalogueChanges(prev, cur), (['d'], ['b'], ['c']))


class TestReleaseKey(unittest.TestCase):
    def setUp(self):
        import tempfile
        self.dir = tempfile.mkdtemp()
        os.makedirs(os.path.join(self.dir, 'variants'))
        for stem in ('urn_mavedb_00000001-a-1', 'urn_mavedb_00000002-a-1'):
            open(os.path.join(self.dir, 'variants', stem + '.csv'), 'w').close()

    def tearDown(self):
        import shutil
        shutil.rmtree(self.dir)

    def fetched(self):
        return {'urn:mavedb:00000001-a-1': ('urn:mavedb:00000001-a-1', '2026-01-01', '10',
                                            '2026-01-01', 'mavedb'),
                'urn:mavedb:00000002-a-1': ('urn:mavedb:00000002-a-1', '2026-01-01', '10',
                                            '2026-01-01', 'mavedb')}

    def testSameInputsSameKey(self):
        self.assertEqual(otto.releaseKey(self.fetched(), self.fetched(), self.dir, 'x'),
                         otto.releaseKey(self.fetched(), self.fetched(), self.dir, 'x'))

    def testAnnotationChangeMovesTheKey(self):
        self.assertNotEqual(otto.releaseKey(self.fetched(), self.fetched(), self.dir, 'x'),
                            otto.releaseKey(self.fetched(), self.fetched(), self.dir, 'y'))

    def testAChangedScoreSetMovesTheKey(self):
        other = self.fetched()
        other['urn:mavedb:00000002-a-1'] = ('urn:mavedb:00000002-a-1', '2026-06-01', '10',
                                            '2026-01-01', 'mavedb')
        self.assertNotEqual(otto.releaseKey(self.fetched(), self.fetched(), self.dir, 'x'),
                            otto.releaseKey(other, other, self.dir, 'x'))

    def testAScoreSetDroppedFromTheCollectionLeavesTheKey(self):
        # The converters read the cache by globbing, so a score set the collection no
        # longer names must not keep contributing; mavemdFetch removes its files.
        self.assertNotEqual(otto.releaseKey(self.fetched(), self.fetched(), self.dir, 'x'),
                            otto.releaseKey(self.fetched(),
                                            {'urn:mavedb:00000001-a-1': None},
                                            self.dir, 'x'))

    def testAScoreSetWithNoCachedFileIsLeftOut(self):
        extra = self.fetched()
        extra['urn:mavedb:00000003-a-1'] = ('urn:mavedb:00000003-a-1', '2026-01-01', '10',
                                            '2026-01-01', 'mavedb')
        self.assertEqual(otto.releaseKey(self.fetched(), self.fetched(), self.dir, 'x'),
                         otto.releaseKey(extra, extra, self.dir, 'x'))


if __name__ == '__main__':
    unittest.main()
