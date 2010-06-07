# Filters added to this controller apply to all controllers in the application.
# Likewise, all the methods added will be available for all controllers.

class ApplicationController < ActionController::Base

  include AuthenticatedSystem

  # Pick a unique cookie name to distinguish our session data from others'
  #session :session_key => '_encPipeline_session_id'
  session :session_key => "_#{ActiveRecord::Base.configurations[RAILS_ENV]['database']}_session_id"

end
