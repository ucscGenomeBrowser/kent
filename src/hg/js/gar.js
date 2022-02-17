
     ///////////////////////////////////////////////////////////////////////
    //// gar - genArkRequest 2022-02                                    ///
   ////     this 'var gar' is the 'namespace' for these functions      ///
  ////     everything can be referenced via gar.name for functions or ///
 ////    variables                                                   ///
///////////////////////////////////////////////////////////////////////
var gar = {

    modalWrapper: document.getElementById("modalWrapper"),
    modalWindow: document.getElementById("modalWindow"),
    modalForm: document.getElementById("modalFeedback"),
    queryString: '',
    // and a query object with keys arg name and value the paired tag
    urlParams: null,
    submitButton: document.getElementById("submitButton"),
    garStatus: document.getElementById("garStatus"),
    asmIdText: document.getElementById("formAsmId"),
    commonName: document.getElementById("commonName"),
    betterCommonName: document.getElementById("betterCommonName"),
    comment: document.getElementById("comment"),
    sciName: document.getElementById("formSciName"),
    onLoadTime: new Date(),
    garEndTime: new Date(),
    // recent improvement has reduced this to a single table, no longer
    // split up by clades
    cladeTableList: document.getElementsByClassName("cladeTable"),
    columnCheckBoxItems: document.getElementById('columnCheckBoxItems'),
    // text box that says: visible rows: 123,456
    counterDisplay: document.getElementById("counterDisplay"),
//    perfDisplay: document.getElementById("perfDisplay"),
    completedAsmId: new Map(),
    columnNames: new Map(),	// key is name, value is column number [0..n)
    checkBoxNames: new Map(),	// key is name, value is the checkbox object
    // going to record the original button labels so they can be augmented
    // with count information here
    checkBoxLabels: new Map(),	// key is name, value is the original button label
    measureTiming: false,       // will turn on with URL argument measureTiming

    millis: function() {
      var d = new Date();
      return d.getTime();
    },

    tableLegend: function(e) {
      alert(e.innerText);
    },

    // Given a table cell c, set it to visible/hidden based on 'tf'
    //   where tf is 'true' == turn on, or 'false' == turn off
    setCellVis: function(c,tf) {
      if (tf) {
        c.style.display = "table-cell";
      } else {
        c.style.display = "none";
      }
    },

    // find out the names of the columns, and get all the checkBox
    //  hideShow button names so they can be managed later
    discoverColumnsCheckboxes: function() {
      var colGroup = document.getElementById('colDefinitions');
      var i = 0;
      for (i = 0; i < colGroup.children.length; i++) {
        gar.columnNames.set(colGroup.children[i].id, i);
      }
      var hideShowList = document.getElementsByClassName('hideShow');
      for (i = 0; i < hideShowList.length; i++) {
        var checkBoxId = hideShowList[i].id;
        var cbId = checkBoxId.replace("CheckBox", "");
        gar.checkBoxNames.set(cbId, hideShowList[i]);
        var labelId = cbId + "Label";
        var labelText = document.getElementById(labelId).innerText;
        gar.checkBoxLabels.set(cbId, labelText);
      }
//      gar.columnNames.forEach(function(value, key) {
//         alert(key + " : " + value);
//      });
//      for (var [key, value] of gar.columnNames.entries()) {
//         alert(key + " : " + value);
//      }
      gar.measureTiming = gar.urlParams.has('measureTiming');
      // If there are any URL arguments to the page see if any of them
      // are column names
      var hasAll = gar.urlParams.has('all');
      if (hasAll) {
        var columnList = document.getElementsByClassName('columnCheckBox');
        for ( i = 0; i < columnList.length; i++) { 
           var id = columnList[i].value + "CheckBox";
           var checkBox = document.getElementById(id);
           if (checkBox) {
             checkBox.checked = true;
             var n = gar.columnNames.get(columnList[i].value);
             gar.setColumnNvis(n, true);
           }
        }
      } else {
        gar.urlParams.forEach(function(val, arg) {
          // beware, get('comName') returns zero, fails this if() statement
          if (gar.columnNames.has(arg)) {
            var checkBoxId = arg + "CheckBox";
            var checkBox = document.getElementById(checkBoxId);
            if (checkBox) {
              checkBox.checked = true;
              var n = gar.columnNames.get(arg);
              gar.setColumnNvis(n, true);
            }
/*  could look at val here when present to turn on/off
            if (val) {
              alert(checkBoxId + " yes val col: '" + arg + "' val: '" + val + "' n: '" + n + "'");
            } else {
              alert(checkBoxId + " no val col: '" + arg + "' val: '" + val + "' n: '" + n + "'");
            }
*/
          }
        });
      }
    },	//	discoverColumnsCheckboxes: function()

/* column names:
<col id='comName' span='1' class=colGComName>
<col id='sciName' span='1' class=colGSciName>
<col id='asmId' span='1' class=colGAsmId>
<col id='asmSize' span='1' class=colGAsmSize>
<col id='seqCount' span='1' class=colGAsmSeqCount>
<col id='scafN50' span='1' class=colGScafN50>
<col id='ctgN50' span='1' class=colGContigN50>
<col id='IUCN' span='1' class=colGIUCN>
<col id='taxId' span='1' class=colGTaxId>
<col id='asmDate' span='1' class=colGAsmDate>
<col id='submitter' span='1' class=colGSubmitter>
*/

    // given a category and a counts Map, increment the count for that category
    incrementCount: function(category, counts) {
      if (counts.get(category)) {
        counts.set(category, counts.get(category) + 1);
      } else {
        counts.set(category, 1);
      }
    },

    // given a category and a counts Map, and some flags
    countVisHidden: function(category, counts, gca, gcf, canBeReq, ucscDb) {
       gar.incrementCount(category, counts);
       if (gca) { gar.incrementCount('gca', counts); }
       if (gcf) { gar.incrementCount('gcf', counts); }
       if (canBeReq) {
          gar.incrementCount('gar', counts);
       } else {
          gar.incrementCount('gak', counts);
       }
    },

    // foreach table, for each row in the table, count visible rows
    countVisibleRows: function(et) {
//      var t0 = gar.millis();
      var comNameRow = gar.columnNames.get('comName');
      var asmIdRow = gar.columnNames.get('asmId');
      var cladeRow = gar.columnNames.get('clade');
      var visRows = 0;
      var totalRows = 0;
      // key is category name, value is count visible
      var categoryVisible = new Map();
      // key is category name, value is count hidden
      var categoryHidden = new Map();
      var i = 0;
      for (i = 0; i < gar.cladeTableList.length; i++) {
        for (var j = 0; j < gar.cladeTableList[i].rows.length; j++) {
          var rowId = gar.cladeTableList[i].rows[j];
          var tagN = rowId.parentNode.tagName.toLowerCase();
          // ignore thead and tfoot rows
          if (tagN === "thead" || tagN === "tfoot") { continue; }
          ++totalRows;
          var thisClade = rowId.cells[cladeRow].innerHTML;
          var asmId = rowId.cells[asmIdRow].innerHTML;
          var isGCA = asmId.includes("GCA");
          var isGCF = asmId.includes("GCF");
          var comName = rowId.cells[comNameRow].innerHTML;
          var canBeRequested = comName.includes("button");
          var ucscDb = comName.includes("cgi-bin/hgTracks");
          if ( rowId.style.display !== "none") {
            gar.countVisHidden(thisClade, categoryVisible, isGCA, isGCF, canBeRequested, ucscDb);
            ++visRows;
          } else {
            gar.countVisHidden(thisClade, categoryHidden, isGCA, isGCF, canBeRequested, ucscDb);
          }
        }
      }
      var notVis = totalRows - visRows;
      /* fixup the hideAll checkbox status, if there are any rows visible,
       * those hideAll checkboxes should indicate off so they are
       * thereby ready to do the function of 'hideAll'.  Or if all rows
       * are not visible, they should be on to indicate 'hideAll' is in effect
       */
      /* reset the checked status on all the other show/hide check boxes */
      gar.checkBoxNames.forEach(function(checkBox, name) {
         if (categoryVisible.get(name)) {
           checkBox.checked = true;
         } else {
           checkBox.checked = false;
         }
         // add the counts (visible/hidden) to the checkBox label text
         var visibleCount = 0;
         var hiddenCount = 0;
         if ( categoryVisible.get(name) ) {
           visibleCount = categoryVisible.get(name);
         }
         if ( categoryHidden.get(name) ) {
           hiddenCount = categoryHidden.get(name);
         }
         var labelId = name + "Label";
         var labelEl = document.getElementById(labelId);
         var labelText = gar.checkBoxLabels.get(name);
         if (labelEl) {
            labelEl.innerText = labelText + " (" + visibleCount.toLocaleString() + "/" + hiddenCount.toLocaleString() + ")";
         } else {
alert("no element for label '" + labelId + "'");
         }
      });
      var hideAllList = document.getElementsByClassName('hideAll');
//      var thisEt = gar.millis() - t0;
      var thisEt = et;
      if (visRows > 0) {
//        var pageEt = gar.garEndTime.getTime() - window.garStartTime.getTime();
//        perfDisplay.innerHTML = "countRows  " + gar.garEndTime.getTime() + " - " + window.garStartTime.getTime() + " = page load time " + pageEt + " millis : DOMContentLoaded: " + gar.onLoadTime.getTime();
        if (gar.measureTiming) {
          counterDisplay.innerHTML = "showing " + visRows.toLocaleString() + " assemblies, " + notVis.toLocaleString() + " hidden : process time: " + thisEt + " millis";
        } else {
          counterDisplay.innerHTML = "showing " + visRows.toLocaleString() + " assemblies, " + notVis.toLocaleString() + " hidden";
        }
        for (i = 0; i < hideAllList.length; i++) {
          hideAllList[i].checked = false;
        }
        hideAllLabelList = document.getElementsByClassName('hideAllLabel');
        for (i = 0; i < hideAllLabelList.length; i++) {
          hideAllLabelList[i].innerHTML = "hide all (" + visRows.toLocaleString() + "/" + notVis.toLocaleString() + ")"; 
        }

      } else {
        if (gar.measureTiming) {
          counterDisplay.innerHTML = totalRows.toLocaleString() + " total ssemblies : use the selection menus to select subsets : process time: " + thisEt + " millis";
        } else {
          counterDisplay.innerHTML = totalRows.toLocaleString() + " total ssemblies : use the selection menus to select subsets";
        }
        for (i = 0; i < hideAllList.length; i++) {
          hideAllList[i].checked = true;
        }
        hideAllLabelList = document.getElementsByClassName('hideAllLabel');
        for (i = 0; i < hideAllLabelList.length; i++) {
          hideAllLabelList[i].innerHTML = "hide all (" + visRows.toLocaleString() + "/" + notVis.toLocaleString() + ")"; 
        }
      }
    },

    // given a column number n, and true/false in tf
    // set that column visibility
    setColumnNvis: function(n, tf) {
      for (var i = 0; i < gar.cladeTableList.length; i++) {
        for (var j = 0; j < gar.cladeTableList[i].rows.length; j++) {
          var c = gar.cladeTableList[i].rows[j].cells[n];
          if (c) {
            gar.setCellVis(c, tf);
          }
        }
      }
    },

    // the 'e' is the event object from the checkbox
    // the 'e.value' is the column name.
    // can find the column number from columnNames Map object
    resetColumnVis: function(e) {
      var t0 = gar.millis();
      var n = gar.columnNames.get(e.value);
      var tf = e.checked;       // true - turn column on, false - turn off
// alert("column '" + e.value + "' n '" + n + " : checked: '" + tf + "'");
      gar.setColumnNvis(n, tf);
      var et = gar.millis() - t0;
      gar.countVisibleRows(et);
    },

    hideTable: function(tableName) {
      tId = document.getElementById(tableName);
      var buttonName = tableName + "HideShow";
      bId = document.getElementById(buttonName);
      if ( tId.style.display === "none") {
        tId.style.display = "block";
        bId.innerHTML = "[hide]";
      } else {
        tId.style.display = "none";
        bId.innerHTML = "[show]";
      }
    },

    // foreach table, for each row in the table, hide row
    hideAll: function(offOn) {
      var t0 = gar.millis();
      for (var i = 0; i < gar.cladeTableList.length; i++) {
        for (var j = 0; j < gar.cladeTableList[i].rows.length; j++) {
          var rowId = gar.cladeTableList[i].rows[j];
          var tagN = rowId.parentNode.tagName.toLowerCase();
          if (tagN === 'thead' || tagN === 'tfoot') {
            continue;
          }
          if (offOn) {   // true, turn *off* the row == this is 'hideAll'
            rowId.style.display = "none";
          } else {	// false, turn *on* the row == this is ! 'hideAll'
            rowId.style.display = "table-row";
          }
        }
      }
      /* reset the checked status on all the other show/hide check boxes */
//      var checkBoxList = document.getElementsByClassName('hideShow');
//      for (var i = 0; i < checkBoxList.length; i++) {
//        checkBoxList[i].checked = ! offOn;
//      }
      /* the countVisibleRows will take care of the status on all 'hideAll'
       * checkboxes - AND the checkBox 'hideShow' check boxes depending upon
       * the counts for that category.
       */
      var et = gar.millis() - t0;
      gar.countVisibleRows(et);
    },

    // given one of: garList gakList gcaList gcfList, work through
    //   all the elements to change vis
    // list can be any of the class lists for rows in the tables
    // offOn - false turn off, true turn on
    resetListVis: function(list, offOn) {
      var t0 = gar.millis();
      for (var i = 0; i < list.length; i++) {
         var rowId = list[i];
         if (offOn) {   // true, turn on the row
           rowId.style.display = "table-row";
         } else {	// false, turn off the row
           rowId.style.display = "none";
         }
      }
      var et = gar.millis() - t0;
      gar.countVisibleRows(et);
    },

    // a check box function is working, do not allow any of
    // them to be available for a second click while in progress
    //  tf is 'true' or 'false' to disable (true), or reenable (false)
    disableCheckBoxes: function(tf) {
      var hideAllList = document.getElementsByClassName('hideAll');
      var i = 0;
      for (i = 0; i < hideAllList.length; i++) {
        hideAllList[i].disabled = tf;
      }
      var hideShowList = document.getElementsByClassName('hideShow');
      for (i = 0; i < hideShowList.length; i++) {
        hideShowList[i].disabled = tf;
      }
    },

    // checked is false when off, true when on
    // value comes from the value='string' in the <input value='string'> element
    visCheckBox: function(e) {
      var offOn = e.checked;    // false to turn off, true to turn on
      gar.disableCheckBoxes(true);  // disable while processing
      if (e.value === "all") {
        gar.hideAll(offOn);
      } else {
        var thisList = document.getElementsByClassName(e.value);
        gar.resetListVis(thisList, offOn);
        if (e.value === "gak") {	// implies also ucscDb list
          thisList = document.getElementsByClassName("ucscDb");
          gar.resetListVis(thisList, offOn);
        }
      }
      gar.disableCheckBoxes(false);  // re-enable as processing done
    },

    columnPullDownClick: function(e) {
      if (gar.columnCheckBoxItems.classList.contains('visible')) {
        gar.columnCheckBoxItems.classList.remove('visible');
        gar.columnCheckBoxItems.style.display = "none";
      } else {
        gar.columnCheckBoxItems.classList.add('visible');
        gar.columnCheckBoxItems.style.display = "block";
      }
    },

    columnPullDownOnblur: function(e) {
      gar.columnCheckBoxItems.classList.remove('visible');
    },

    failedRequest: function(url) {
      gar.submitButton.value = "request failed";
      gar.submitButton.disabled = true;
      garStatus.innerHTML = "FAILED: '" + url + "'";
    },

    sendRequest: function(name, email, asmId, betterName, comment) {
    var urlComponents = encodeURIComponent(name) + "&email=" + encodeURIComponent(email) + "&asmId=" + encodeURIComponent(asmId) + "&betterName=" + encodeURIComponent(betterName) + "&comment=" + encodeURIComponent(comment);

    var url = "/cgi-bin/gar?name=" + urlComponents;
// information about escaping characters:
// https://stackoverflow.com/questions/10772066/escaping-special-character-in-a-url/10772079
// encodeURI() will not encode: ~!@#$&*()=:/,;?+'
// encodeURIComponent() will not encode: ~!*()'

//    var encoded = encodeURIComponent(url);
//    encoded = encoded.replace("'","&rsquo;");
//    var encoded = encodeURI(cleaner);
    var xmlhttp = new XMLHttpRequest();
    xmlhttp.onreadystatechange = function() {
         if (4 === this.readyState && 200 === this.status) {
            gar.submitButton.value = "request completed";
//            gar.submitButton.disabled = true;
            garStatus.innerHTML = "OK: <a href='" + url + "' target=_blank>" + url + "</a>";
//            alert("SUCCESS: '" + url + "'");
         } else {
            if (4 === this.readyState && 404 === this.status) {
               gar.failedRequest(url);
            }
         }
       };
    xmlhttp.open("GET", url, true);
    xmlhttp.send();
//    alert("SENT: '" + url + "'");

    },  //      sendRequest: function(name, email. asmId)

    // work up the chain to find the row this element is in
    whichRow: function (e) {
    while (e) {
      if (e.rowIndex) {
        return e.rowIndex;
      }
      e = e.parentNode;
    }
    return undefined;
    },

// obtain table name from
// https://stackoverflow.com/questions/9066792/get-table-parent-of-an-element
    parentTable: function (e) {
    while (e) {
      e = e.parentNode;
      if (e.tagName.toLowerCase() === 'table') {
          return e;
      }
    }
    return undefined;
    },

  // Original JavaScript code by Chirp Internet: chirpinternet.eu
  // Please acknowledge use of this code by including this header.
//      const form = document.getElementById("modalFeedback");

    openModal: function(e) {
     if (e.name && e.name !== "specific") {
       var pTable = gar.parentTable(e);
       var thisRow = gar.whichRow(e);
//       var thisCell = pTable.closest('td').cellIndex;
       var comName = pTable.rows[thisRow].cells[0].innerText;
       var sciName = pTable.rows[thisRow].cells[1].innerText;
       gar.commonName.textContent = comName;
       gar.betterCommonName.value = comName;
       gar.sciName.textContent = sciName;
       gar.asmIdText.textContent = e.name;
//       gar.modalForm.name.value = "some name";
//       gar.modalForm.email.value = "some@email.com";
       garStatus.innerHTML = "&nbsp;";
       // check if this asmId already submitted
       if (gar.completedAsmId.get(e.name)) {
          gar.submitButton.value = "request completed";
          gar.submitButton.disabled = false;
          gar.modalWrapper.className = "overlay";
          return;
       }
       gar.completedAsmId.set(e.name, true);
     } else {
       gar.commonName.textContent = "enter information about desired assembly in the 'Other comments'";
       gar.sciName.textContent = "including the scientific name";
       gar.asmIdText.textContent = "and the GCx accession identifer";
     }
     gar.submitButton.value = "Submit request";
     gar.submitButton.disabled = false;
     gar.modalWrapper.className = "overlay";
      var overflow = gar.modalWindow.offsetHeight - document.documentElement.clientHeight;
      if(overflow > 0) {
        gar.modalWindow.style.maxHeight = (parseInt(window.getComputedStyle(gar.modalWindow).height) - overflow) + "px";
      }
      gar.modalWindow.style.marginTop = (-gar.modalWindow.offsetHeight)/2 + "px";
      gar.modalWindow.style.marginLeft = (-gar.modalWindow.offsetWidth)/2 + "px";
      if (e.preventDefault) {
        e.preventDefault();
      } else {
        e.returnValue = false;
      }
    },

    closeModal: function(e)
    {
      gar.modalWrapper.className = "";
      if (e.preventDefault) {
        e.preventDefault();
      } else {
        e.returnValue = false;
      }
    },

    clickHandler: function(e) {
      if(!e.target) e.target = e.srcElement;
      if(e.target.tagName === "DIV") {
        if(e.target.id != "modalWindow") gar.closeModal(e);
      }
    },

    keyHandler: function(e) {
      if(e.keyCode === 27) gar.closeModal(e);
    },

  modalInit: function() {
    gar.onLoadTime = new Date();
//    var pageEt = gar.garEndTime.getTime() - window.garStartTime.getTime();
    var domReady = gar.onLoadTime.getTime() - gar.garEndTime.getTime();
//    perfDisplay.innerHTML = "modalInit " + gar.garEndTime.getTime() + " - " + window.garStartTime.getTime() + " = page load time " + pageEt + " millis : DOMContentLoaded: " + gar.onLoadTime.getTime() + " - " + gar.garEndTime.getTime() + " = " + domReady + " domReady millis";
    if(document.addEventListener) {
//      document.getElementById("modalOpen").addEventListener("click", gar.openModal, false);
      document.getElementById("modalClose").addEventListener("click", gar.closeModal, false);
      document.addEventListener("click", gar.clickHandler, false);
      document.addEventListener("keydown", gar.keyHandler, false);
    } else {
//      document.getElementById("modalOpen").attachEvent("onclick", gar.openModal);
      document.getElementById("modalClose").attachEvent("onclick", gar.closeModal);
      document.attachEvent("onclick", gar.clickHandler);
      document.attachEvent("onkeydown", gar.keyHandler);
    }
  },    //      modalInit: function()

  // Original JavaScript code by Chirp Internet: chirpinternet.eu
  // Please acknowledge use of this code by including this header.

  checkForm: function(e) {
 // alert("button.value: '" + gar.submitButton.value + "'");
    if (gar.submitButton.value === "request completed") {
       if (e.preventDefault) {
         e.preventDefault();
       } else {
         e.returnValue = false;
       }
       gar.closeModal(e);
       return;
    }
    var form = (e.target) ? e.target : e.srcElement;
    if(form.name.value === "") {
      alert("Please enter your Name");
      form.name.focus();
      if (e.preventDefault) {
        e.preventDefault();
      } else {
        e.returnValue = false;
      }
      return;
    }
    if(form.email.value === "") {
      alert("Please enter a valid Email address");
      form.email.focus();
      if (e.preventDefault) {
        e.preventDefault();
      } else {
        e.returnValue = false;
      }
      return;
    }
// validation regex from:
//	https://www.w3resource.com/javascript/form/email-validation.php
// another example from
//	https://www.simplilearn.com/tutorials/javascript-tutorial/email-validation-in-javascript
//   var validRegex = /^[a-zA-Z0-9.!#$%&'*+/=?^_`{|}~-]+@[a-zA-Z0-9-]+(?:\.[a-zA-Z0-9-]+)*$/;
// another example from
//	https://ui.dev/validate-email-address-javascript/
//	return /\S+@\S+\.\S+/.test(email)
//	return /^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(email)
//	var re = /^[^\s@]+@[^\s@]+$/;
//  if (re.test(email)) { OK }
        
//    var validEmail = /^\w+([\.-]?\w+)*@\w+([\.-]?\w+)*(\.\w{2,3})+$/;
      var validEmail = /^[a-zA-Z0-9.!#$%&'*+/=?^_`{|}~-]+@[a-zA-Z0-9-]+(?:\.[a-zA-Z0-9-]+)*$/;
    if(! validEmail.test(form.email.value)) {
      alert("You have entered an invalid email address!");
      form.email.focus();
      if (e.preventDefault) {
        e.preventDefault();
      } else {
        e.returnValue = false;
      }
      return;
    }
    gar.sendRequest(form.name.value, form.email.value, gar.asmIdText.textContent, gar.betterCommonName.value, gar.comment.value);
    if (e.preventDefault) {
      e.preventDefault();
    } else {
      e.returnValue = false;
    }
  },    //	checkForm: function(e)

  unhideTables: function() {
      // foreach table, set display to turn on the table
      for (var i = 0; i < gar.cladeTableList.length; i++) {
          gar.cladeTableList[i].style.display = "table";
      }
      document.getElementById('counterDisplay').style.display = "block";
      document.getElementById('loadingStripes').style.display = "none";
  },	// unhide tables

};      //      var gar

  if(document.addEventListener) {
    var t0 = gar.millis();
    // allow semi colon separators as well as ampersand
    var queryString = window.location.search.replaceAll(";", "&");
    gar.urlParams = new URLSearchParams(queryString);
    document.getElementById("modalFeedback").addEventListener("submit", gar.checkForm, false);
    document.addEventListener("DOMContentLoaded", gar.modalInit, false);
    gar.discoverColumnsCheckboxes();
    gar.unhideTables();
    var et = gar.millis() - t0;
    gar.garEndTime = new Date();
    gar.countVisibleRows(et);
  } else {
    var t0 = gar.millis();
    document.getElementById("modalFeedback").attachEvent("onsubmit", gar.checkForm);
    window.attachEvent("onload", gar.modalInit);
    gar.discoverColumnsCheckboxes();
    gar.unhideTables();
    var et = gar.millis() - t0;
    gar.countVisibleRows(et);
  }
