
// funtion to create the pop-up on hgTracks
window.createTutorialPopup = function() {
  // Create the pop-up container
  const tutorialDiv = document.createElement("div");
  tutorialDiv.id = "tutorialContainer";
  tutorialDiv.style.display = "none";
  // Create the contents for the popup
  tutorialDiv.innerHTML = `
    <p>
    These interactive tutorials will provide step-by-step guides to help
    navigate through various tools and pages on the UCSC Genome Browser.</p>
    <table style="width:600px; border-color:#666666; border-collapse:collapse; margin: auto;">
      <tr><td style="padding: 8px;width: 200px; text-align: center; border: 1px solid #666666;">
          <a href="#" id="basicTutorial">Basic tutorial</a></td>
          <td style="padding: 8px; width: 450px; word-wrap: break-word; border: 1px solid #666666; text-align:center">
          <small>
          An introductory tutorial to help users navigate the UCSC Genome Browser tracks
          display. Learn how to configure display settings, search for tracks, and view the
          negative strand (3' to 5').</small>
          </td></tr>
      <tr><td style="width: 250px; padding: 8px; text-align: center; border: 1px solid #666666;">
          <a href="#" id="clinicalTutorial">Advanced tutorial for clinicians</a><br>
          <em style="font-size: 11px">(only available on hg19 & hg38)</em></td>
          <td style="padding: 8px; width: 450px; word-wrap: break-word; border: 1px solid #666666; text-align:center">
          <small>
          A tutorial focused on clinical genetics to showcase resources that
          may be useful in variant interpretation.
          <br>
          Learn how to search for variants,
          view recommended track sets, and save your configuration settings to share with others.
          </small>
          </td></tr>
      <tr><td style="padding: 8px;width: 200px; text-align: center; border: 1px solid #666666;">
          <a href="#" id="tableBrowserTutorial">Table Browser tutorial</a></td>
          <td style="padding: 8px; width: 450px; word-wrap: break-word; border: 1px solid #666666; text-align:center">
          <small>
          An introductory tutorial to navigate and configure the UCSC Table Browser.
          Learn how to switch between assemblies, select primary or related tables for a track,
          and select your output format.</small>
          </td></tr>
      <tr><td style="padding: 8px;width: 200px; text-align: center; border: 1px solid #666666;">
          <a href="#" id="gatewayTutorial">Gateway tutorial</a></td>
          <td style="padding: 8px; width: 450px; word-wrap: break-word; border: 1px solid #666666; text-align:center">
          <small>
          An introductory tutorial to navigate the UCSC Gateway page.
          Learn how to switch between assemblies, search for a GenArk hub, view chromosome
          sequences, and more.</small>
          </td></tr>
    </table>`;

  document.body.appendChild(tutorialDiv); // Add tutorial popup <div> to the page

  $("#tutorialContainer").dialog({
    modal: false,
    title: "Interactive Tutorials",
    draggable: true,
    resizable: false,
    width: 650,
    /*
    buttons: [{
      text: "Close",
      click: function() {
        $(this).dialog("close");
      }
    }],*/
    position: {my: "center top", at: "center top+100", of: window}
  });
  
  const url = new URL (window.location.href); // Get which CGI you are viewing
  const pathParts = url.pathname.split('/');
  const cgi = pathParts[pathParts.length -1];
  const db = getDb();                             // Get which database you are viewing

  // Function to control the basic tutorial link
  document.getElementById('basicTutorial').addEventListener('click', function(event) {
    event.preventDefault();
    $("#tutorialContainer").dialog("close");
    if (cgi == "hgTracks") {
        basicTour.start();
    } else {
        window.location.href = "/cgi-bin/hgTracks?startTutorial=true";
    }
  });

  // Function to control the clincial tutorial link
  document.getElementById('clinicalTutorial').addEventListener('click', function(event) {
    event.preventDefault();
    $("#tutorialContainer").dialog("close");
    if (cgi == "hgTracks" && (db == "hg38" || db =="hg19")) {
        clinicalTour.start(); // If you are on hg38 or hg19, then start the tutorial
    } else {
        // Otherwise go to hg38 and start the tutorial.
        window.location.href = "/cgi-bin/hgTracks?db=hg38&startClinical=true";
    }
  });

  // Function to control the Table Browser tutorial link
  document.getElementById('tableBrowserTutorial').addEventListener('click', function(event) {
    event.preventDefault();
    $("#tutorialContainer").dialog("close");
    if (cgi == "hgTables" && (typeof startTableBrowserOnLoad !== 'undefined' && startTableBrowserOnLoad)) {
        tableBrowserTour.start(); // If you are on hgTables, start the tutorial
    } else {
        // Go to hgTables and start the tutorial
        window.location.href = "/cgi-bin/hgTables?startTutorial=true";
    }
  });

  // Function to control the Gateway tutorial link
  document.getElementById('gatewayTutorial').addEventListener('click', function(event) {
    event.preventDefault();
    $("#tutorialContainer").dialog("close");
    if (cgi == "hgGateway" && (typeof startGatewayOnLoad !== 'undefined' && startGatewayOnLoad)) {
        gatewayTour.start(); // If you are on hgGateway, start the tutorial
    } else {
        // Go to the Gateway page and start the tutorial
        window.location.href = "/cgi-bin/hgGateway?startTutorial=true";
    }
  });
};
