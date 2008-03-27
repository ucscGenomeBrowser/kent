class AccountController < ApplicationController

  layout 'account'
 
  # If you want "remember me" functionality, add this before_filter to Application Controller
  before_filter :login_from_cookie

  def index
    redirect_to(:action => 'signup') unless logged_in? || User.count > 0
  end

  def login
    return unless request.post?
    self.current_user = User.authenticate(params[:login], params[:password])
    if logged_in?
      if params[:remember_me] == "1"
        self.current_user.remember_me
        cookies[:auth_token] = { :value => self.current_user.remember_token , :expires => self.current_user.remember_token_expires_at }
      end
      redirect_to(:controller => '/pipeline', :action => 'show_user')
    else
      flash[:error] = "Unknown user or password."
    end
  end

  def change_profile
    @user = self.current_user
    return unless request.post?
    unless params[:commit] == "Cancel"
      @user.update_attributes(params[:user])
      @user.save!
      flash[:notice] = "Profile has been successfully changed."
      redirect_to(:controller => '/pipeline', :action => 'show_user')
    else
      redirect_to(:controller => '/pipeline', :action => 'show_user')
    end
  rescue ActiveRecord::RecordInvalid
    render :action => 'change_profile'
  end

  def signup
    @user = User.new(params[:user])
    @user.host = request.host
    @user.port = request.port
    return unless request.post?
    @user.save!
    # the next few lines are equivalent to instant autologin,
    # so they have been commented out and the welcome render added.
    # user's shouldn't get in until activated through email
    #self.current_user = @user
    # redirect_to(:controller => '/account', :action => 'index')
    #flash[:notice] = "Thanks for signing up!"
    render :action => 'welcome'
  rescue ActiveRecord::RecordInvalid
    render :action => 'signup'
  end
  
  def logout
    self.current_user.forget_me if logged_in?
    cookies.delete :auth_token
    reset_session
    flash[:notice] = "You have been logged out."
     redirect_to(:controller => '/welcome', :action => 'index')
  end

  def activate
    flash.clear  
    return if params[:id] == nil and params[:activation_code] == nil
    activator = params[:id] || params[:activation_code]
    @user = User.find_by_activation_code(activator) 
    if @user
      @user.host = request.host
      @user.port = request.port
    end
    if @user and @user.activate
      flash[:notice] = 'Your account has been activated.  Please log in.'
       redirect_to(:controller => '/account', :action => 'login')
    else
      flash[:error] = 'Unable to activate the account.  Please check or enter manually.' 
       redirect_to(:controller => '/account', :action => 'login')
    end
  end


  # reset lost password section
  def forgot_password
    return unless request.post?
    if @user = User.find_by_email(params[:email])
      @user.host = request.host
      @user.port = request.port
      @user.forgot_password
      @user.save
      redirect_to(:controller => '/account', :action => 'index')
      flash[:notice] = "A password reset link has been sent to your email address." 
    else
      flash[:error] = "Could not find a user with that email address." 
    end
  end

  def reset_password
    @user = User.find_by_password_reset_code(params[:id]) if params[:id]
    raise if @user.nil?
    return if @user unless params[:password]
      if (params[:password] == params[:password_confirmation])
        @user.password_confirmation = params[:password_confirmation]
        @user.password = params[:password]
        @user.reset_password
        flash[:notice] = @user.save ? "Password reset." : "Password not reset." 
      else
        flash[:error] = "Password mismatch." 
      end  
      redirect_to(:controller => '/account', :action => 'index') 
  rescue
    logger.error "Invalid reset code entered." 
    flash[:error] = "Sorry - that is an invalid password reset code. Please check your code and try again. Perhaps your email client inserted a carriage return." 
     redirect_to(:controller => '/account', :action => 'index')
  end


  # change password section
  def change_password
    return unless request.post?
    unless params[:commit] == "Cancel"
      @user = self.current_user
      @user.update_attributes(params[:user])
      @user.new_email = ""  # prevent it from thinking it's new email which triggers email notify
      # until we have a place to redirect back to, just log them out  
      @user.forget_me if logged_in?  # part of log out, it must be before reset_password flag is set
      @user.reset_password
      @user.save!  # put this after all other change to @user to prevent multiple email notices
      @user.update_ftp_password
      cookies.delete :auth_token
      reset_session
      flash[:notice] = "Password has been successfully changed."
      redirect_to(:action => 'index')
    else # cancel
      redirect_to(:controller => '/pipeline', :action => 'show_user')
    end
  rescue ActiveRecord::RecordInvalid
    render :action => 'change_password'
  end

  # change email section
  def change_email
    @user = self.current_user
    return unless request.post?
    unless params[:commit] == "Cancel"
      unless params[:user][:email].blank?
        @user.host = request.host
        @user.port = request.port
        @user.change_email_address(params[:user][:email])
        if @user.save
          @changed = true
          flash.clear  
        end
      else # email blank
        #flash[:warning] = "Please enter an email address." 
      end
    else # cancel
      redirect_to(:controller => '/pipeline', :action => 'show_user')
    end
  rescue Net::SMTPFatalError
    flash[:error] = "Invalid email address." 
    @changed = false
    render :action => 'change_email'
  end

  def activate_new_email
    flash.clear  
    return unless params[:id].nil? or params[:email_activation_code].nil?
    activator = params[:id] || params[:email_activation_code]
    @user = User.find_by_email_activation_code(activator) 
    if @user and @user.activate_new_email
      redirect_to(:controller => '/pipeline', :action => 'show_user')
      flash[:notice] = "The email address for your account has been updated." 
    else
      flash[:error] = "Unable to update the email address." 
    end
  end

end
