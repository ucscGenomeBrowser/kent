function submitClick(ele)
{
// Tell the user we are processing the upload when the user clicks on the submit button.
    loadingImage.run();
    return true;
}

$(document).ready(function()
{
    loadingImage.init($("#loadingImg"), $("#loadingMsg"), "<p style='color: red; font-style: italic;'>Uploading and processing your data may take some time. Please leave this window open while your custom track is loading.</p>");
});
