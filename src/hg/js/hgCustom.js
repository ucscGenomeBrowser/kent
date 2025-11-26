function submitClick(ele)
{
// Tell the user we are processing the upload when the user clicks on the submit button.
    loadingImage.run();
    return true;
}

$(document).ready(function()
{
    loadingImage.init($("#loadingImg"), $("#loadingMsg"), "<p style='color: red; font-style: italic;'>Uploading and processing your data may take some time. Please leave this window open while your custom track is loading.</p>");
    if (typeof customTrackTour !== 'undefined') {
        if (typeof startCustomTutorialOnLoad !== 'undefined' && startCustomTutorialOnLoad) {
            customTrackTour.start();
        }
    }
    // allow the user to bring the tutorials popup via a new help menu button
    var tutorialLinks = document.createElement("li");
    tutorialLinks.id = "hgTracksHelpTutorialLinks";
    tutorialLinks.innerHTML = "<a id='hgCustomHelpTutorialLinks' href='#showTutorialPopup'>" +
       "Interactive Tutorials</a>";
    $("#help > ul")[0].appendChild(tutorialLinks);
    $("#hgCustomHelpTutorialLinks").on("click", function () {
        // Check to see if the tutorial popup has been generated already
        var tutorialPopupExists = document.getElementById ("tutorialContainer");
        if (!tutorialPopupExists) {
            // Create the tutorial popup if it doesn't exist
            createTutorialPopup();
        } else {
            //otherwise use jquery-ui to open the popup
            $("#tutorialContainer").dialog("open");
        }
    });
});
