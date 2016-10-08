// Module: gtexTrackSettings

var gtexTrackSettings = (function() {

    // Data
    
    // SVG has it's own DOM
    var _svgDoc;
    var _svgRoot;

    var tissues = [
        'adiposeSubcut', 'adiposeVisceral', 'adrenalGland', 'arteryAorta', 'arteryCoronary', 
        'arteryTibial', 'bladder', 'brainAmygdala', 'brainAnCinCortex', 'brainCaudate', 
        'brainCerebelHemi', 'brainCerebellum', 'brainCortex', 'brainFrontCortex', 
        'brainHippocampus', 'brainHypothalamus', 'brainNucAccumbens', 'brainPutamen', 
        'brainSpinalcord', 'brainSubstanNigra', 'breastMamTissue', 'xformedlymphocytes',
        'xformedfibroblasts', 'ectocervix', 'endocervix', 'colonSigmoid', 'colonTransverse',
        'esophagusJunction', 'esophagusMucosa', 'esophagusMuscular', 'fallopianTube', 
        'heartAtrialAppend', 'heartLeftVentricl', 'kidneyCortex', 'liver', 'lung', 
        'minorSalivGland', 'muscleSkeletal', 'nerveTibial', 'ovary', 'pancreas', 'pituitary', 
        'prostate', 'skinNotExposed', 'skinExposed', 'smallIntestine', 'spleen', 'stomach', 
        'testis', 'thyroid', 'uterus', 'vagina', 'wholeBlood'
    ];

    // Convenience functions
    
    function initBodyMap(svg, doc) {
        // Set organs to unhighlighted state
        var line = $("#LL_kidneyCortex", svg)[0];
        $(line).hide();
        var white = $("#WHITE_kidneyCortex", svg);
        $(white).hide();
        var organ = $("#RO_kidneyCortex", svg);
        $(organ).hide();
        /*
        line = $("#LL_arteryAorta", svg);
        $(line).hide();
        white = $("#WHITE_arteryAorta", svg);
        $(white).hide();
        */
        // jQuery not working here, maybe figure this out later
        // var label = $("arteryAorta", svg);
        // $(label).css("fill", "blue");

        organ = doc.getElementById('arteryAorta');
        organ.style.fill = "blue";
    }

    function onClickToggleTissue(tis) {
        // mark selected in tissue table
        $(this).toggleClass('tableTissueSelected');

        // jQuery addClass doesn't work on SVG elements, using classList
        // May need a shim so this works on IE9 and Opera mini (as if Dec 2014)
        // https://martinwolf.org/blog/2014/12/adding-and-removing-classes-from-svg-elements-with-jquery
        var el = _svgDoc.getElementById(this.id);
        if (el !== null) {
            el.classList.toggle('tableTissueSelected');
        }
    }

    function onClickSetTissue(tis) {
        // mark selected in tissue table
        $(tis).addClass('tableTissueSelected');
        var el = _svgDoc.getElementById(tis);
        if (el !== null) {
            el.classList.add('tableTissueSelected');
        }
    }

    function onClickClearTissue(tis) {
        // mark selected in tissue table
        $(tis).removeClass('tableTissueSelected');
        var el = _svgDoc.getElementById(tis);
        if (el !== null) {
            el.classList.remove('tableTissueSelected');
        }
    }

    function onHoverTissue() {
        // HTML
        $(this).toggleClass('tissueHovered');

        // SVG
        var el = _svgDoc.getElementById(this.id);
        if (el !== null) {
            el.classList.toggle('tissueHovered');
        }
    }

    //function animateTissue(tis, i, ignore) {
    function animateTissue(tis) {
        //console.log(tis);
        // add handlers to tissue table
        $('#' + tis).click(tis, onClickToggleTissue);
        $('#' + tis).hover(onHoverTissue, onHoverTissue);
    }

    function animateTissues() {
        // Add event handlers to tissue table and body map SVG
        tissues.forEach(animateTissue);

        $('#setAll').click(onClickSetAll);
        $('#clearAll').click(onClickClearAll);
    }

    // UI event handlers

    function onClickSetAll() {
        // set font-weight: bold on table, set text fill: black on svg
        // NOTE: this shouldn't be needed (needs debugging in onClickSet)
        tissues.forEach(onClickSetTissue);
        $('.tissueLabel').addClass('tableTissueSelected');
    }

    function onClickClearAll() {
        tissues.forEach(onClickClearTissue);
        $('.tissueLabel').removeClass("tableTissueSelected");
    }

    function onClickMapTissue() {
        this.style.fill = "black";
        //document.getElementById("bodyMapSvg").contentDocument.getElementById('endocervix').style.fill = "black";
    }

    // Initialization

    function init() {
        // cart.setCgi('gtexTrackSettings');

        $(function() {
            // after page load, tweak layout and initialize event handlers
            var bodyMapSvg = document.getElementById("bodyMapSvg");
            globalSvg = bodyMapSvg;


            // wait for SVG to load
            bodyMapSvg.addEventListener("load", function() {
                var svg = bodyMapSvg.contentDocument.documentElement;
                _svgRoot = svg;
                _svgDoc = bodyMapSvg.contentDocument;
                
                initBodyMap(svg, bodyMapSvg.contentDocument);
                animateTissues();
            }, false);

        });
    }

    return {
            init: init
           };
    
}()); // gtexTrackSettings

gtexTrackSettings.init();
