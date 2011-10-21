function refreshLoadingImg ()
{
    // hack to make sure animation continues in IE after form submission
    // See: http://stackoverflow.com/questions/774515/keep-an-animated-gif-going-after-form-submits
    // and http://stackoverflow.com/questions/780560/animated-gif-in-ie-stopping
    $("#loadingImg").attr('src', $("#loadingImg").attr('src'));
}

function submitClick(ele)
{
// Tell the user we are processing the upload when the user clicks on the submit button.

    $("#loadingMsg").append("<p style='color: red; font-style: italic;'>Uploading and processing your data may take some time. Please leave this window open while your custom track is loading.</p>");
    if(navigator.userAgent.indexOf("Chrome") != -1) {
        // In Chrome, gif animation and setTimeout's are stopped when the browser receives the first blank line/comment of the next page
        // (basically, the current page is unloaded). I have found no way around this problem, so we just show a 
        // simple "Processing..." message (we can't make that blink, b/c Chrome doesn't support blinking text).
        // 
        // (Surprisingly, this is NOT true for Safari, so this is apparently not a WebKit issue).

        $("#loadingImg").replaceWith("<span id='loadingBlinker'>&nbsp;&nbsp;<b>Processing...</b></span>");
    } else {
        $("#loadingImg").show();
        setTimeout(refreshLoadingImg, 1000);
    }
    return true;
}

$(document).ready(function()
{
    // To make the loadingImg visible on FF, we have to make sure it's visible during page load (otherwise it doesn't get shown by the submitClick code).
    $("#loadingImg").hide();
});
