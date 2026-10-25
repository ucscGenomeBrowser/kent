#!/usr/bin/env python3
"""Unit tests of the catalogue rules in mavedbFetch.py (no network)."""

import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
import mavedbFetch as mf   # noqa: E402


def sequenceTarget(organism):
    return {'targetSequence': {'sequenceType': 'protein', 'sequence': 'MA',
                               'taxonomy': {'organismName': organism}}}


def accessionTarget(accession, assembly=None):
    return {'targetAccession': {'accession': accession, 'assembly': assembly}}


class HumanTargets(unittest.TestCase):

    def testSequenceTargets(self):
        self.assertTrue(mf.isHumanTarget(sequenceTarget('Homo sapiens')))
        self.assertFalse(mf.isHumanTarget(sequenceTarget('artificial sequences')))
        self.assertFalse(mf.isHumanTarget(sequenceTarget('Saccharomyces cerevisiae')))

    def testAccessionTargets(self):
        """Accession targets carry no organism; the search's organism filter drops them."""
        for acc in ('NC_000017.11', 'NM_007294.3', 'NP_000537.3', 'ENST00000460680.6',
                    'ENSG00000141510'):
            self.assertTrue(mf.isHumanTarget(accessionTarget(acc)), acc)
        self.assertTrue(mf.isHumanTarget(accessionTarget('chr17', 'GRCh38')))
        self.assertFalse(mf.isHumanTarget(accessionTarget('NC_001133.9')))   # yeast chrI

    def testAnyTargetCounts(self):
        sets = [{'urn': 'b', 'targetGenes': [sequenceTarget('Mus musculus'),
                                             accessionTarget('NM_1.1')]},
                {'urn': 'a', 'targetGenes': [sequenceTarget('Mus musculus')]}]
        self.assertEqual([s['urn'] for s in mf.humanScoreSets(sets)], ['b'])


class Fingerprints(unittest.TestCase):

    def testRoundTrip(self):
        s = {'urn': 'urn:mavedb:1-a-1', 'modificationDate': '2026-01-02', 'numVariants': 7,
             'experiment': {'modificationDate': '2025-12-01'}}
        fp = mf.fingerprint(s)
        with tempfile.TemporaryDirectory(dir=os.environ.get('TMPDIR', '/data/tmp')) as d:
            path = os.path.join(d, 'fetched.tsv')
            mf.writeFingerprints(path, {s['urn']: fp})
            self.assertEqual(mf.readFingerprints(path), {s['urn']: fp})
        self.assertEqual(mf.readFingerprints('/nonexistent/fetched.tsv'), {})


class FailureRate(unittest.TestCase):

    def testRepeatFailuresNotCounted(self):
        # a quiet month: only last month's failure is retried, and fails again
        self.assertIsNone(mf.tooManyFailures({'x'}, {'x'}, {'x'}))
        # a newly failing score set among a few tried is reported
        self.assertIsNotNone(mf.tooManyFailures({'x', 'y'}, {'x', 'y', 'z'}, {'x'}))
        # one new failure in a full download of 1,287 is not
        tried = {str(i) for i in range(1287)}
        self.assertIsNone(mf.tooManyFailures({'1'}, tried, set()))
        self.assertIsNotNone(mf.tooManyFailures({str(i) for i in range(65)}, tried, set()))


class Downloads(unittest.TestCase):
    """A download is only cached when it is what it claims to be."""

    class FakeApi(object):
        def __init__(self, body):
            self.body = body

        def request(self, url, data=None, delay=0, timeout=0):
            return self.body

    def fetch(self, body, expected):
        with tempfile.TemporaryDirectory(dir=os.environ.get('TMPDIR', '/data/tmp')) as d:
            path = os.path.join(d, 'x.csv')
            mf.fetchVariants(self.FakeApi(body), 'urn:mavedb:1-a-1', path, expected)
            return open(path, 'rb').read()

    def testVariantRowCount(self):
        body = b'accession,score\nurn:mavedb:1-a-1#1,0.1\nurn:mavedb:1-a-1#2,0.2\n'
        self.assertEqual(self.fetch(body, 2), body)
        with self.assertRaises(mf.FetchError):
            self.fetch(body, 3)
        with self.assertRaises(mf.FetchError):
            self.fetch(b'<html>busy</html>', None)

    def testRecordMustBeJson(self):
        with tempfile.TemporaryDirectory(dir=os.environ.get('TMPDIR', '/data/tmp')) as d:
            path = os.path.join(d, 'r.json')
            with self.assertRaises(mf.FetchError):
                mf.fetchScoreSetRecord(self.FakeApi(b'<html>'), 'urn:mavedb:1-a-1', path)
            self.assertFalse(os.path.exists(path))


if __name__ == '__main__':
    unittest.main()
