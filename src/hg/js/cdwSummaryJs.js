
function cellIdMouseOver(d) {
    /* show a mouse over for objects that have a cellId attribute */
    var ptIdx = d.pointIndex;
    cellId = d.series[0].values[ptIdx].cellId;
    lines = [];

    dataMap = cellInfo[cellId];
    for (var key in dataMap) {
    if (dataMap.hasOwnProperty(key)) {
        lines.push(("<b>"+ (cellTagLabels[key] || key) + "</b>: " + dataMap[key]));
      }
    }
    return lines.join("<br>\n");
}

function addPcaGraph() {
    /* add PCA graph to DOM */
    nv.addGraph(function() {

        console.log("updating PCA start");
        xTag = $("#xAxisSelect").val();
        yTag = $("#yAxisSelect").val();
        groupBy = $("#pcaGroupSelect").val();
        console.log("PCA update: "+xTag+" "+yTag+" "+groupBy);
        var pcsByGroup = d3.nest()
          .key(function(d) { return cellInfo[d.cellId][groupBy];})
          .entries(pcaData);

        pcaChart = nv.models.scatterChart()
                    .showDistX(false)    //showDist, when true, will display those little distribution lines on the axis.
                    .showDistY(false)
                    .showLegend(true)
                    .x(function(d) { return d[xTag]; })
                    .y(function(d) { return d[yTag]; });

        pcaChart.xAxis.axisLabel(xTag);
        pcaChart.yAxis.axisLabel(yTag);

        pcaChart.tooltip.contentGenerator(cellIdMouseOver);
        pcaChart.tooltip.headerEnabled(false);
        //pcaChart.tooltip.fixedTop(50);

        pcaChart.xAxis.tickFormat(d3.format(',f'));

        pcaChart.yAxis.tickFormat(d3.format(',f'));

        pcaDataElement = d3.select("#pcaChartDiv svg")
            .datum(pcsByGroup);
        pcaDataElement.call(pcaChart);

        nv.utils.windowResize(pcaChart.update);

      return pcaChart;
    });
}

var tsneColors = {};

function seuratMouseOver(d) {
    /* show a mouse over for objects that have a cellId attribute */
    console.log("MouseOver");
    var ptIdx = d.pointIndex;
    cellId = d.series[0].values[ptIdx].cellId;
    lines = [];
    lines.push("<b>T-SNE Cluster:</b> Cluster" + cellToCluster[cellId]);

    dataMap = cellInfo[cellId];
    for (var key in dataMap) {
    if (dataMap.hasOwnProperty(key)) {
        lines.push(("<b>"+ (cellTagLabels[key] || key) + "</b>: " + dataMap[key]));
      }
    }
    return lines.join("<br>\n");
}


var tsnePointsByGroup; // easier debugging

function addSeuratGraph() {
    /* add Seurat T-SNE graph to DOM */
	nv.addGraph(function() {
        var groupBy = $("#tsneGroupSelect").val();
        console.log("grouping tsne by|"+groupBy+"|");
        if (groupBy=="tsneCluster")
            // cluster info is stored in a separate map
            {
            console.log("2nd try: grouping tsne by "+groupBy);
            tsnePointsByGroup = d3.nest()
              .key(function(d) { return "Cluster "+cellToCluster[d.cellId] ;}).sortKeys(d3.ascending)
              .entries(seuratTsnePoints);
            console.log('group by cluster');
            }
        else 
            // all other meta info is in the cellInfo map
            tsnePointsByGroup = d3.nest()
              .key(function(d) { cellId = d.cellId; return cellInfo[cellId][groupBy]; }).sortKeys(d3.ascending)
              .entries(seuratTsnePoints);

        // if we have color info specified for this attribute, color dots 
        if (groupBy in tsneColors) {
            var groupColors = tsneColors[groupBy];
            for(var groupIdx in tsnePointsByGroup) {
                groupId = tsnePointsByGroup[groupIdx].key;
                if (groupId in groupColors) {
                    tsnePointsByGroup[groupIdx].color = groupColors[groupId];
                }
            }
        }

        seuratChart = nv.models.scatterChart()
                    .showDistX(false)    //showDist, when true, will display those little distribution lines on the axis.
                    .showDistY(false)
                    .showLegend(true)
                    .x(function(d) { return d.x; })
                    .y(function(d) { return d.y; });

        seuratChart.xAxis.axisLabel("T-SNE 1");
        seuratChart.yAxis.axisLabel("T-SNE 2");
        seuratChart.xAxis.tickFormat(d3.format('.2f'));
        seuratChart.yAxis.tickFormat(d3.format('.2f'));

        seuratChart.tooltip.contentGenerator(seuratMouseOver);
        seuratChart.scatter.dispatch.elementClick(function(e) {
          console.log(e);
          });
        console.log("Added generator");
        seuratChart.tooltip.headerEnabled(false);
        seuratChart.tooltip.fixedTop(50);

        seuratChartElement = d3.select("#seuratChartDiv svg")
            .datum(tsnePointsByGroup);
        seuratChartElement.call(seuratChart);

        nv.utils.windowResize(seuratChart.update);
        return seuratChart;
    });
}
