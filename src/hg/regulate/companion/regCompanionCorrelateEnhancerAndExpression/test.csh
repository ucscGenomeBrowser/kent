#!/bin/tcsh -efx
regCompanionCorrelateEnhancerAndExpression rc/enhPicks/enh7.bed rc/enhPicks/ave/H3k27acAll.tab rc/genePicks/canonical.bed rc/genePicks/canonicalExp.work/canonical.tab enhToProm.tab outPairBed=enhToProm.bed -outEnhBed=enh.bed -outProBed=pro.bed
