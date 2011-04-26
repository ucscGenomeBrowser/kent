import re
from RaFile import *

class CvFile(RaFile):

    def __init__(self, filePath=''):
        RaFile.__init__(self)
        if filePath != '':
            self.read(filePath)


    def readStanza(self, stanza):
        e = RaStanza()
        e.readStanza(stanza)
        type = e['type']
        print type

        # this will become a 30 case if-else block if I do it this way,
        # should I maybe use a dictionary or something?
        if type == 'Cell Line':
            entry = HumanStanza()
        else:
            entry = CvStanza()

        key, val = entry.readStanza(stanza)
        return key, val, entry


    def validate(self):
        for stanza in self.itervalues():
            stanza.validate(self)


class CvStanza(RaStanza):

    def __init__(self):
        RaStanza.__init__(self)


    def validate(self, ra):
        print 'CvStanza.validate(' + self.name + ')'

        for key in ('term', 'tag', 'type', 'description'):
            if not key in self.keys() or self[key] == '':
                # I'm still not quite sure what to do with errors. Do you
                # still want to raise exceptions and then catch them into
                # some list-type structure?
                print 'bad ' + key


class TemplateCvStanza(CvStanza):

    # this class is just the template I'm using to copy new Stanzas off

    def __init__(self):
        CvStanza.__init__(self)

    def validate(self, ra):
        CvStanza.validate(self, ra)
        print 'TemplateCvStanza.validate(' + self.name + ')'


class HumanStanza(CvStanza):
 
    def __init__(self):
        CvStanza.__init__(self)

    def validate(self, ra):
        CvStanza.validate(self, ra)
        print 'HumanStanza.validate(' + self.name + ')'

        # I had to copy term, tag, type, desc into the necessary fields
        # to make the check for extraneous keys work. This means that the
        # base CvStanza.validate() is redundant. Should I remove it?
        necessary = ('term', 'tag', 'type', 'description', 'organism', 'vendorName', 'orderUrl', 'sex', 'tier')
        optional = ('tissue', 'vendorId', 'karyotype', 'lineage', 'termId', 'termUrl', 'color', 'protocol', 'category')

        # the following 3 codeblocks I can see being reused a lot.
        # Should I make them subs in CvStanza, perhaps?

        for key in necessary:
            if not key in self.keys() or self[key] == '':
                print 'bad ' + key
 
        for key in self.iterkeys():
            if key not in necessary and key not in optional:
                print 'extra ' + key

        p = 0
        for key in ra.iterkeys():
            if self['organism'] == key and ra[key]['type'] == 'organism':
                p = 1
        if p == 0:
            print 'organism doesnt match'
