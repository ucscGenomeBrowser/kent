#!/usr/bin/env python3
"""Unit tests for mavemdFetch.py: fingerprints, namespace choice, failure accounting."""

import os
import shutil
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
import urllib.parse                  # noqa: E402
import mavemdFetch as fetch          # noqa: E402


class FakeApi(object):
    def __init__(self, namespaces):
        self.namespaces = namespaces

    def json(self, url, delay=None):
        return self.namespaces


class TestNamespacesFor(unittest.TestCase):
    def namespaces(self):
        return [{'namespace': 'scores', 'group': 'data'},
                {'namespace': 'clinvar.2024_01', 'group': 'annotation'},
                {'namespace': 'clinvar.2026_01', 'group': 'annotation'},
                {'namespace': 'clinvar.2015_02', 'group': 'annotation'},
                {'namespace': 'calibration.urn:mavedb:calibration-b', 'group': 'calibration'},
                {'namespace': 'calibration.urn:mavedb:calibration-a', 'group': 'calibration'}]

    def testOnlyTheNewestClinvarIsRequested(self):
        wanted, _ = fetch.namespacesFor(FakeApi(self.namespaces()), 'urn:mavedb:x')
        clinvars = [n for n in wanted if n.startswith('clinvar.')]
        self.assertEqual(clinvars, ['clinvar.2026_01'])

    def testEveryCalibrationIsRequested(self):
        wanted, _ = fetch.namespacesFor(FakeApi(self.namespaces()), 'urn:mavedb:x')
        self.assertEqual(sorted(n for n in wanted if n.startswith('calibration.')),
                         ['calibration.urn:mavedb:calibration-a',
                          'calibration.urn:mavedb:calibration-b'])

    def testOfferedListHoldsEveryNamespace(self):
        _, offered = fetch.namespacesFor(FakeApi(self.namespaces()), 'urn:mavedb:x')
        self.assertEqual(len(offered), 6)

    def testNoClinvarAtAllIsFine(self):
        wanted, _ = fetch.namespacesFor(
            FakeApi([{'namespace': 'scores', 'group': 'data'}]), 'urn:mavedb:x')
        self.assertEqual([n for n in wanted if n.startswith('clinvar.')], [])


class TestFingerprint(unittest.TestCase):
    def record(self):
        return {'urn': 'urn:mavedb:00000001-a-1', 'modificationDate': '2026-06-10',
                'numVariants': 3180, 'experiment': {'modificationDate': '2019-08-08'}}

    def testStableForTheSameInput(self):
        self.assertEqual(fetch.fingerprint(self.record(), ['mavedb']),
                         fetch.fingerprint(self.record(), ['mavedb']))

    def testANewNamespaceMovesIt(self):
        # A new ClinVar release arrives as a namespace on a record whose modification
        # date never changes; without this the build would never notice.
        self.assertNotEqual(fetch.fingerprint(self.record(), ['mavedb']),
                            fetch.fingerprint(self.record(),
                                              ['mavedb', 'clinvar.2027_01']))

    def testAChangedVariantCountMovesIt(self):
        other = self.record()
        other['numVariants'] = 3181
        self.assertNotEqual(fetch.fingerprint(self.record(), ['mavedb']),
                            fetch.fingerprint(other, ['mavedb']))

    def testAChangedExperimentMovesIt(self):
        other = self.record()
        other['experiment'] = {'modificationDate': '2026-10-01'}
        self.assertNotEqual(fetch.fingerprint(self.record(), ['mavedb']),
                            fetch.fingerprint(other, ['mavedb']))

    def testMissingFieldsDoNotRaise(self):
        self.assertEqual(fetch.fingerprint({'urn': 'urn:mavedb:x'}, []),
                         ('urn:mavedb:x', '', '', '', ''))


class TestFingerprintFile(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.mkdtemp()

    def tearDown(self):
        shutil.rmtree(self.dir)

    def testRoundTrip(self):
        path = os.path.join(self.dir, 'fetched.tsv')
        fps = {'urn:mavedb:x': ('urn:mavedb:x', '2026-01-01', '10', '2026-01-01', 'a,b')}
        fetch.writeFingerprints(path, fps)
        self.assertEqual(fetch.readFingerprints(path), fps)

    def testMissingFileIsEmpty(self):
        self.assertEqual(fetch.readFingerprints(os.path.join(self.dir, 'nope.tsv')), {})


class TestSupersededBy(unittest.TestCase):
    def testDictForm(self):
        self.assertEqual(fetch.supersededBy({'supersedingScoreSet':
                                             {'urn': 'urn:mavedb:00001250-a-2'}}),
                         'urn:mavedb:00001250-a-2')

    def testStringForm(self):
        self.assertEqual(fetch.supersededBy({'supersedingScoreSet': 'urn:mavedb:x'}),
                         'urn:mavedb:x')

    def testAbsent(self):
        self.assertIsNone(fetch.supersededBy({}))
        self.assertIsNone(fetch.supersededBy({'supersedingScoreSet': None}))


class TestTooManyFailures(unittest.TestCase):
    def testOneFailureOutOfManyIsFine(self):
        tried = {'a', 'b', 'c', 'd', 'e'} | {'x%d' % i for i in range(40)}
        self.assertIsNone(fetch.tooManyFailures({'a'}, tried, set()))

    def testHalfFailingIsReported(self):
        self.assertIsNotNone(fetch.tooManyFailures({'a', 'b'}, {'a', 'b', 'c', 'd'}, set()))

    def testAScoreSetThatAlsoFailedLastRunDoesNotCount(self):
        # One score set MaveDB times out on every week would otherwise be the only one
        # tried in a quiet week and fail the run every time.
        self.assertIsNone(fetch.tooManyFailures({'a'}, {'a'}, {'a'}))


class FakeWholeApi(object):
    """Enough of Api for fetch(): a canned collection, records and variant pages."""

    def __init__(self, urns, failRecord=(), rows=3):
        self.urns = list(urns)
        self.failRecord = set(failRecord)
        self.rows = rows
        self.requests = []

    def json(self, url, delay=None):
        self.requests.append(url)
        if url.endswith('/api/version'):
            return {'version': '2026.3.2'}
        if '/collections/' in url:
            return {'name': 'MaveMD', 'modificationDate': '2026-10-21',
                    'scoreSetUrns': self.urns}
        if url.endswith('/csv-namespaces'):
            return [{'namespace': 'mavedb', 'group': 'data'}]
        urn = urllib.parse.unquote(url.rsplit('/', 1)[-1])
        if urn in self.failRecord:
            raise fetch.FetchError('%s: HTTP 500' % urn)
        return {'urn': urn, 'modificationDate': '2026-01-01', 'numVariants': self.rows,
                'experiment': {'modificationDate': '2026-01-01'}}

    def request(self, url, delay=None, timeout=None):
        self.requests.append(url)
        body = [b'accession,hgvs_nt']
        body += [b'urn:x#%d,c.1A>G' % i for i in range(self.rows)]
        return b'\n'.join(body) + b'\n'


class TestFetchResilience(unittest.TestCase):
    """A score set whose record cannot be read must not look like one that was removed."""

    def setUp(self):
        self.dir = tempfile.mkdtemp()
        self.realApi = fetch.Api
        # Big enough that one failure is under MAX_FAILED_FRACTION, as it is with the
        # real collection of 84; a two-score-set collection cannot lose one quietly.
        self.urns = ['urn:mavedb:%08d-a-1' % i for i in range(1, 26)]

    def tearDown(self):
        fetch.Api = self.realApi
        shutil.rmtree(self.dir)

    def run_(self, failRecord=()):
        api = FakeWholeApi(self.urns, failRecord)
        fetch.Api = lambda log=None: api
        log = open(os.devnull, 'w')
        try:
            return fetch.fetch(self.dir, log=log)
        finally:
            log.close()

    def testCleanRunCachesEverything(self):
        summary = self.run_()
        self.assertEqual(sorted(summary['fetched']), sorted(self.urns))
        for urn in self.urns[:3]:
            self.assertTrue(os.path.exists(os.path.join(
                self.dir, 'variants', fetch.fileStem(urn) + '.csv')))

    def testOneBadRecordDoesNotAbortTheRun(self):
        self.run_()
        summary = self.run_(failRecord=[self.urns[1]])
        self.assertIn(self.urns[1], summary['failed'])

    def testOneBadRecordDoesNotEvictItsCachedCopy(self):
        # The collection still names it, so a transient 500 must not look like a removal.
        self.run_()
        summary = self.run_(failRecord=[self.urns[1]])
        self.assertEqual(summary['dropped'], [])
        self.assertTrue(os.path.exists(os.path.join(
            self.dir, 'variants', fetch.fileStem(self.urns[1]) + '.csv')))

    def testOneBadRecordLeavesItsFingerprintAlone(self):
        self.run_()
        before = fetch.readFingerprints(os.path.join(self.dir, 'catalogue.tsv'))
        self.run_(failRecord=[self.urns[1]])
        after = fetch.readFingerprints(os.path.join(self.dir, 'catalogue.tsv'))
        self.assertEqual(before, after)

    def testAScoreSetLeavingTheCollectionIsStillPruned(self):
        self.run_()
        gone = self.urns[-1]
        self.urns = self.urns[:-1]
        summary = self.run_()
        self.assertEqual(summary['dropped'], [gone])
        self.assertFalse(os.path.exists(os.path.join(
            self.dir, 'variants', fetch.fileStem(gone) + '.csv')))

    def testManyBadRecordsDoStopTheRun(self):
        self.run_()
        with self.assertRaises(fetch.FetchError):
            self.run_(failRecord=self.urns[:5])


class TestVariantPaging(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.mkdtemp()
        self.page = fetch.PAGE
        fetch.PAGE = 3

    def tearDown(self):
        fetch.PAGE = self.page
        shutil.rmtree(self.dir)

    def testExactPageMultipleDoesNotAskPastTheEnd(self):
        # Without the count guard this asks for start=3 as well, and what the API does
        # with a start past the last row is not something request() knows how to retry.
        api = FakeWholeApi([], rows=3)
        path = os.path.join(self.dir, 'v.csv')
        fetch.fetchVariants(api, 'urn:mavedb:x', path, ['mavedb'], expected=3)
        self.assertEqual(len(api.requests), 1)
        self.assertIn('start=0', api.requests[0])


if __name__ == '__main__':
    unittest.main()
