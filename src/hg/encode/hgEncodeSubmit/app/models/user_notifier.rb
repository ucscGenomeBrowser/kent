class UserNotifier < ActionMailer::Base

  def load_notification(user, project)
    setup_email(user)  
    # also notify the emailOnLoad person
    emailOnLoad = ActiveRecord::Base.configurations[RAILS_ENV]['emailOnLoad']
    if (emailOnLoad)    
      @bcc = emailOnLoad
    end
    db = ActiveRecord::Base.configurations[RAILS_ENV]['database']
    @subject    += "Submission #{db} #{project.id} #{project.name} loaded"
    @body[:project] = project
    @body[:database] = db
  end
  
  def failure_notification(user, project, failureType)
    setup_email(user)
    db = ActiveRecord::Base.configurations[RAILS_ENV]['database']
    @subject    += "Submission \"#{project.name}\": #{failureType} error"
    @body[:project] = project
    @body[:database] = db
    @body[:failure_type] = failureType
    errText = ""
    case project.status
      when "upload failed"
        errText = getUploadErrText(project)
      when "validate failed"
        errText = getValidateErrText(project)
      when "load failed"
        errText = getLoadErrText(project)
      when "unload failed"
        errText = getUnloadErrText(project)
    end 
    @body[:error_text] = errText
  end
  
  def signup_notification(user)
    setup_email(user)
    @subject    += 'Please activate your new account'
    @body[:url]  = url_for(:host => user.host, :port => user.port, :controller => "account", \
	:action => "activate", :id => user.activation_code)
  end
  
  def activation(user)
    setup_email(user)
    @subject    += 'Your account has been activated'
    @body[:url]  = url_for(:host => user.host, :port => user.port, :controller => "account") 
  end

  def test_dummy_test
    #do nothing
  end
 
  def forgot_password(user)
    setup_email(user)
    @subject    += 'Request to change your password'
    @body[:url]  = url_for(:host => user.host, :port => user.port, :controller => "account", \
	:action => "reset_password", :id => user.password_reset_code)
  end

  def reset_password(user)
    setup_email(user)
    @subject    += 'Your password has been reset'
  end

  def change_email(user)
    setup_email(user)
    @recipients  = "#{user.new_email}" 
    @subject    += 'Request to change your email'
    @body[:url]  = url_for(:host => user.host, :port => user.port, :controller => "account", \
	:action => "activate_new_email", :id => user.email_activation_code)
  end
 
  protected
  def setup_email(user)
    @recipients  = "#{user.email}"
    @from        = "#{ActiveRecord::Base.configurations[RAILS_ENV]['from']}"
    @subject     = "ENCODE DCC: "
    @sent_on     = Time.now
    @body[:user] = user
  end
end
