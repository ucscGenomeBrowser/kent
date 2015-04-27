/** @jsx React.DOM */
var pt = React.PropTypes;

var Section = React.createClass({
    // Wrap children in a bordered box with a light blue title bar -- just formatting, no action

    propTypes: { title: pt.node.isRequired,  // section title to appear in light blue bar
                 // Optional
                 style: pt.object            // Extra CSS for enclosing div
               },

    render: function() {
        return (
              <div className='sectionBorder sectionPanel' style={this.props.style}>
                <div className='sectionHeader'>
                  {this.props.title}
                </div>
                <div className='sectionContents'>
                  {this.props.children}
                </div>
              </div>
        );
    }
});

// Without this, jshint complains that Section is not used.  Module system would help.
Section = Section;
