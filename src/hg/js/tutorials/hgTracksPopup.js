

// funtion to create the pop-up on hgTracks

window.openTutorialPopup = function() {
  // Create the pop-up container
  const tutorialDiv = document.createElement("div");
  tutorialDiv.id = "tutorialContainer";
 
  // Create the contents for the popup
  tutorialDiv.innerHTML = `
    <p>
    These interactive tutorials will provide step-by-step guides to help
    navigate through various tools and pages on the UCSC Genome Browser.</p>
    <h4 id="hgTracksTutorials">Genome Browser tutorials</h4>
    <table style="width:600px; border-color:#666666; border-collapse:collapse; margin: auto;">
      <tr><td style="padding: 8px;width: 200px;">
          <a href="#" id="basicTutorial">Basic tutorial</a></td>
          <td style="width: 350px; word-wrap: break-word;">
          Some description about the first tutorial
          </td></tr>
      <tr><td style="width: 200px; padding: 8px;">
          <a href="#" id="clinicalTutorial">Advanced tutorial for clinicians</a></td>
          <td style="width: 350px; word-wrap: break-word;">
          Some explaination about why you would use this tutorial over another
          (only available on hg19 & hg38)
          </td></tr>
    </table>
  `;

  document.body.appendChild(tutorialDiv);

  $("#tutorialContainer").dialog({
    modal: true,
    title: "All Interactive Tutorials Available",
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

  // Function to control the basic tutorial link
  document.getElementById('basicTutorial').addEventListener('click', function(event) {
    event.preventDefault();
    $("#tutorialContainer").dialog("close");
    clinicalTour.start();
  });

  // Function to control the clincial tutorial link
  document.getElementById('clinicalTutorial').addEventListener('click', function(event) {
    event.preventDefault();
    $("#tutorialContainer").dialog("close");
    clinicalTour.start();
  });
};
