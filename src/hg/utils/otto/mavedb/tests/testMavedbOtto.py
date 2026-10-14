#!/usr/bin/env python3
"""Unit tests of the release decisions in mavedbOtto.py (no network, no build)."""

import datetime
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
import mavedbOtto as mo   # noqa: E402

D = datetime.date


def stats(**kw):
    s = {'mapItems': 1000, 'variantItems': 100000, 'shownSets': 1000, 'badItems': 0}
    s.update(kw)
    return s


class FullBuild(unittest.TestCase):

    def testDue(self):
        self.assertTrue(mo.fullBuildDue(D(2026, 11, 2), None))
        self.assertTrue(mo.fullBuildDue(D(2027, 1, 3), D(2026, 12, 1)))    # quarter month
        self.assertTrue(mo.fullBuildDue(D(2026, 12, 1), D(2026, 9, 1)))    # 91 days
        self.assertFalse(mo.fullBuildDue(D(2026, 11, 2), D(2026, 10, 14)))
        self.assertFalse(mo.fullBuildDue(D(2026, 10, 20), D(2026, 10, 14)))  # same month


class Checks(unittest.TestCase):

    def testFirstRelease(self):
        counts = dict.fromkeys(mo.BIGBEDS, 5)
        self.assertEqual(mo.checkRelease(stats(), None, counts), [])

    def testEmptyAndBadItems(self):
        counts = dict.fromkeys(mo.BIGBEDS, 5)
        counts['mavedbAlignAa.bb'] = 0
        problems = mo.checkRelease(stats(badItems=2), None, counts)
        self.assertEqual(len(problems), 2)

    def testDrops(self):
        counts = dict.fromkeys(mo.BIGBEDS, 5)
        self.assertEqual(mo.checkRelease(stats(mapItems=901, shownSets=951), stats(), counts),
                         [])
        problems = mo.checkRelease(stats(mapItems=899, variantItems=50000, shownSets=949),
                                   stats(), counts)
        self.assertEqual([p.split(' went')[0] for p in problems],
                         ['heatmap items', 'variant items', 'score sets shown'])
        self.assertEqual(mo.checkRelease(stats(mapItems=5000), stats(), counts), [])


class Bookkeeping(unittest.TestCase):

    def testCatalogueChanges(self):
        prev = {'a': ('a', '1'), 'b': ('b', '1'), 'c': ('c', '1')}
        cur = {'a': ('a', '1'), 'b': ('b', '2'), 'd': ('d', '1')}
        self.assertEqual(mo.catalogueChanges(prev, cur), (['d'], ['b'], ['c']))

    def testReleaseNameAndKey(self):
        with tempfile.TemporaryDirectory(dir=os.environ.get('TMPDIR', '/data/tmp')) as d:
            self.assertEqual(mo.releaseName(d, D(2026, 11, 2)), '2026-11-02')
            os.mkdir(os.path.join(d, '2026-11-02'))
            self.assertEqual(mo.releaseName(d, D(2026, 11, 2)), '2026-11-02.2')
            open(os.path.join(d, '1-a-1.csv'), 'w').close()
            cat = [{'urn': 'urn:mavedb:1-a-1'}, {'urn': 'urn:mavedb:2-a-1'}]
            fetched = {'urn:mavedb:1-a-1': ('urn:mavedb:1-a-1', 'x'),
                       'urn:mavedb:2-a-1': ('urn:mavedb:2-a-1', 'x')}
            key = mo.releaseKey(fetched, cat, d)
            # a score set with no file (never downloaded) does not change the key
            self.assertEqual(key, mo.releaseKey(fetched, cat[:1], d))
            fetched['urn:mavedb:1-a-1'] = ('urn:mavedb:1-a-1', 'y')
            self.assertNotEqual(key, mo.releaseKey(fetched, cat, d))


if __name__ == '__main__':
    unittest.main()
