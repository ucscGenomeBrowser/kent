var pt = React.PropTypes;

var LoadingImage = React.createClass({

    // When enabled, show an animated loading gif and a message

    propTypes: { loading: pt.bool.isRequired,  // If true, show the image and message.
                 // Optional:
                 message: pt.string            // Override default message
               },

    getDefaultProps: function () {
        return { message: 'Executing your query may take some time.  ' +
                          'Please leave this window open during processing.'
               };
    },

    render: function() {
        if (this.props.loading) {
            return (
              <div>
                <img id='loadingImg' src='../images/loading.gif' />
	        <p className='loadingMessage'>
		  {this.props.message}
                </p>
              </div>
            );
        } else {
            return null;
        }
    }
});

// Without this, jshint complains that LoadingImage is not used.  Module system would help.
LoadingImage = LoadingImage;
