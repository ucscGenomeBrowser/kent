#!/usr/bin/env python3
"""Unit tests for mavemdBuild.py: the trackDb filter menu comparison."""

import json
import os
import shutil
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
import mavemdBuild as build          # noqa: E402

# A doubled comma is a literal one, which is how trackDb carries the ClinVar term that
# has a comma in it (slNameListFromCommaEscaped, lib/common.c).
STANZA = """track mavemdVar
    filterValues.gene BRCA1,TP53
    filterLabel.gene Gene
    filterValues.clinvarSig Benign,Pathogenic/Likely pathogenic/Pathogenic,, low penetrance
    filterLabel.clinvarSig ClinVar significance
"""


class TestParseFilterValues(unittest.TestCase):
    def testPlainList(self):
        self.assertEqual(build.parseFilterValues(STANZA)['gene'], ['BRCA1', 'TP53'])

    def testDoubledCommaIsOneValue(self):
        self.assertEqual(build.parseFilterValues(STANZA)['clinvarSig'],
                         ['Benign', 'Pathogenic/Likely pathogenic/Pathogenic, '
                          'low penetrance'])

    def testNonFilterLinesIgnored(self):
        self.assertEqual(sorted(build.parseFilterValues(STANZA)), ['clinvarSig', 'gene'])


class TestFilterDrift(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.mkdtemp()
        self.ra = os.path.join(self.dir, 'mavemd.ra')
        with open(self.ra, 'w') as fh:
            fh.write(STANZA)

    def tearDown(self):
        shutil.rmtree(self.dir)

    def stats(self, values):
        path = os.path.join(self.dir, 'stats.json')
        with open(path, 'w') as fh:
            json.dump({'filterValues': values}, fh)
        return path

    def testMenusCoverTheDataSoNoDrift(self):
        path = self.stats({'gene': ['BRCA1'],
                           'clinvarSig': ['Pathogenic/Likely pathogenic/Pathogenic, '
                                          'low penetrance']})
        self.assertEqual(build.filterDrift(path, self.ra), [])

    def testANewGeneIsReported(self):
        path = self.stats({'gene': ['BRCA1', 'PTEN']})
        self.assertEqual(build.filterDrift(path, self.ra), ['gene: PTEN'])

    def testAValueTheMenuHasButTheDataLostIsNotReported(self):
        path = self.stats({'gene': ['BRCA1']})
        self.assertEqual(build.filterDrift(path, self.ra), [])

    def testTheCommaValueDoesNotLookLikeDrift(self):
        # Splitting the menu on plain commas would make this look like two missing
        # values on every single run.
        path = self.stats({'clinvarSig': ['Pathogenic/Likely pathogenic/Pathogenic, '
                                          'low penetrance']})
        self.assertEqual(build.filterDrift(path, self.ra), [])

    def testAMissingStanzaIsReportedRatherThanPassingQuietly(self):
        path = self.stats({'gene': ['BRCA1']})
        self.assertTrue(build.filterDrift(path, os.path.join(self.dir, 'nope.ra')))


if __name__ == '__main__':
    unittest.main()
