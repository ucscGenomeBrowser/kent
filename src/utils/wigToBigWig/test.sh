#!/bin/tcsh -exf
mkdir -p expected
mkdir -p xyz

wigToBigWig testFixed.wig test.sizes xyz/testFixed.bigWig
wigToBigWig testVar.wig test.sizes xyz/testVar.bigWig
wigToBigWig testBedGraph.wig test.sizes xyz/testBedGraph.bigWig

bigWigToBedGraph xyz/testFixed.bigWig expected/testFixed.bedGraph
bigWigToBedGraph xyz/testVar.bigWig expected/testVar.bedGraph
bigWigToBedGraph xyz/testBedGraph.bigWig expected/testBedGraph.bedGraph

bigWigSummary xyz/testFixed.bigWig chr7 115597761 115598761 1 > expected/tf1.sum
bigWigSummary xyz/testFixed.bigWig chr7 115597761 115598761 3 > expected/tf3.sum
bigWigSummary xyz/testFixed.bigWig chr7 115597761 115598761 10 > expected/tf10.sum
bigWigSummary xyz/testFixed.bigWig chr7 115597761 115598761 30 > expected/tf30.sum
bigWigSummary xyz/testFixed.bigWig chr7 115597761 115598761 100 > expected/tf100.sum
bigWigSummary xyz/testFixed.bigWig chr7 115597761 115598761 300 > expected/tf300.sum
bigWigSummary xyz/testFixed.bigWig chr7 115597761 115598761 1000 > expected/tf1000.sum
bigWigSummary xyz/testFixed.bigWig chr7 115597761 115598761 3000 > expected/tf3000.sum
bigWigSummary xyz/testFixed.bigWig chr7 115597761 115598761 10000 > expected/tf10000.sum

bigWigSummary xyz/testVar.bigWig chrY 81000 91000 1 > expected/tv1.sum
bigWigSummary xyz/testVar.bigWig chrY 81000 91000 10 > expected/tv10.sum
bigWigSummary xyz/testVar.bigWig chrY 81000 91000 100 > expected/tv100.sum
bigWigSummary xyz/testVar.bigWig chrY 81000 91000 1000 > expected/tv1000.sum
bigWigSummary xyz/testVar.bigWig chrY 81000 91000 10000 > expected/tv10000.sum

bigWigSummary xyz/testBedGraph.bigWig chr5 131308806 132230652 1 > tbg1.sum
bigWigSummary xyz/testBedGraph.bigWig chr5 131308806 132230652 10 > tbg10.sum
bigWigSummary xyz/testBedGraph.bigWig chr5 131308806 132230652 100 > tbg100.sum
bigWigSummary xyz/testBedGraph.bigWig chr5 131308806 132230652 1000 > tbg1000.sum
bigWigSummary xyz/testBedGraph.bigWig chr5 131308806 132230652 10000 > tbg10000.sum
