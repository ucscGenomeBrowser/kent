#!/usr/bin/env python3
"""Unit tests for mavemdFetch.py: fingerprints, namespace choice, failure accounting."""

import os
import shutil
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
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


if __name__ == '__main__':
    unittest.main()
