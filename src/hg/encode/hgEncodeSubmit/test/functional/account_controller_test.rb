require File.dirname(__FILE__) + '/../test_helper'
require 'account_controller'

# Re-raise errors caught by the controller.
class AccountController; def rescue_action(e) raise e end; end

class AccountControllerTest < Test::Unit::TestCase
  # Be sure to include AuthenticatedTestHelper in test/test_helper.rb instead
  # Then, you can remove it from this and the units test.
  include AuthenticatedTestHelper

  fixtures :users

  def setup
    @controller = AccountController.new
    @request    = ActionController::TestRequest.new
    @response   = ActionController::TestResponse.new

    # for testing action mailer
    ActionMailer::Base.delivery_method = :test
    ActionMailer::Base.perform_deliveries = true
    ActionMailer::Base.deliveries = []
    @emails = ActionMailer::Base.deliveries 
    @emails.clear
  end

  def test_should_activate_user_and_send_activation_email
    get :activate, :id => users(:aaron).activation_code
    assert_equal 1, @emails.length
    assert(@emails.first.subject =~ /Your account has been activated/)
    assert(@emails.first.body    =~ /#{assigns(:user).login}, your account has been activated/)
  end

  def test_should_activate_user
    assert_nil User.authenticate('aaron', 'test')
    get :activate, :id => users(:aaron).activation_code
    assert_equal users(:aaron), User.authenticate('aaron', 'test')
  end

  def test_should_send_activation_email_after_signup
    create_user
    assert_equal 1, @emails.length
    assert(@emails.first.subject =~ /Please activate your new account/)
    assert(@emails.first.body    =~ /Username: quire/)
    #assert(@emails.first.body    =~ /Password: quire/)  # this no longer sent in email
    assert(@emails.first.body    =~ /account\/activate\/#{assigns(:user).activation_code}/)
  end


  def test_should_login_and_redirect
    post :login, :login => 'quentin', :password => 'test'
    assert session[:user]
    assert_response :redirect
  end

  def test_should_fail_login_and_not_redirect
    post :login, :login => 'quentin', :password => 'bad password'
    assert_nil session[:user]
    assert_response :success
  end

  def test_should_allow_signup
    assert_difference User, :count do
      create_user
      assert_response :success
    end
  end

  def test_should_require_login_on_signup
    assert_no_difference User, :count do
      create_user(:login => nil)
      assert assigns(:user).errors.on(:login)
      assert_response :success
    end
  end

  def test_should_require_password_on_signup
    assert_no_difference User, :count do
      create_user(:password => nil)
      assert assigns(:user).errors.on(:password)
      assert_response :success
    end
  end

  def test_should_require_password_confirmation_on_signup
    assert_no_difference User, :count do
      create_user(:password_confirmation => nil)
      assert assigns(:user).errors.on(:password_confirmation)
      assert_response :success
    end
  end

  def test_should_require_email_on_signup
    assert_no_difference User, :count do
      create_user(:email => nil)
      assert assigns(:user).errors.on(:email)
      assert_response :success
    end
  end

  def test_should_logout
    login_as :quentin
    get :logout
    assert_nil session[:user]
    assert_response :redirect
  end

  def test_should_remember_me
    post :login, :login => 'quentin', :password => 'test', :remember_me => "1"
    assert_not_nil @response.cookies["auth_token"]
  end

  def test_should_not_remember_me
    post :login, :login => 'quentin', :password => 'test', :remember_me => "0"
    assert_nil @response.cookies["auth_token"]
  end
  
  def test_should_delete_token_on_logout
    login_as :quentin
    get :logout
    assert_equal @response.cookies["auth_token"], []
  end

  def test_should_login_with_cookie
    users(:quentin).remember_me
    @request.cookies["auth_token"] = cookie_for(:quentin)
    get :index
    assert @controller.send(:logged_in?)
  end

  def test_should_fail_expired_cookie_login
    users(:quentin).remember_me
    users(:quentin).update_attribute :remember_token_expires_at, 5.minutes.ago
    @request.cookies["auth_token"] = cookie_for(:quentin)
    get :index
    assert !@controller.send(:logged_in?)
  end

  def test_should_fail_cookie_login
    users(:quentin).remember_me
    @request.cookies["auth_token"] = auth_token('invalid_auth_token')
    get :index
    assert !@controller.send(:logged_in?)
  end

  # reset password section
  def test_should_forget_password
    post :forgot_password, :email => 'quentin@example.com'
    assert_response :redirect
    assert flash.has_key?(:notice), "Flash should contain notice message." 
    assert_equal 1, @emails.length
    assert(@emails.first.subject =~ /Request to change your password/)
  end

  def test_should_not_forget_password
    post :forgot_password, :email => 'invalid@email'
    assert_response :success
    assert flash.has_key?(:notice), "Flash should contain notice message." 
    assert_equal 0, @emails.length
  end

  def test__reset_password__valid_code_and_password__should_reset

    @user = users(:aaron)

    @user.host = "hgwdev.soe.ucsc.edu"
    @user.port = 3001
    @user.activated_at = Time.now

    @user.forgot_password && @user.save
    @emails.clear

    #assert false, "debug: @user.password_reset_code = #{@user.password_reset_code}"

    post :reset_password, :id => @user.password_reset_code, :password  => "new_password", :password_confirmation => "new_password" 
    assert_response :redirect

    #@user = User.find(2);
    #assert false, "debug: @user = #{@user.inspect}"

    assert_match("Password reset", flash[:notice])
    assert_equal 1, @emails.length # make sure that it e-mails the user notifying that their password was reset
    assert_equal(@user.email, @emails.first.to[0], "should have gone to user") 

    #assert false, "debug: @user = #{@user.inspect}"
    #assert false, "debug: @user.activated_at = #{@user.activated_at}"

    # Make sure that the user can login with this new password
    assert(User.authenticate(@user.login, "new_password"), "password should have been reset")
  end

  def test__reset_password__valid_code_but_not_matching_password__shouldnt_reset
    @user = users(:aaron)
    @user.host = "hgwdev.soe.ucsc.edu"
    @user.port = 3001

    @user.forgot_password && @user.save

    post :reset_password, :id => @user.password_reset_code, :password  => "new_password", :password_confirmation => "not matching password" 

    assert_equal(1, @emails.length)
    assert_match("Password mismatch", flash[:notice])

    assert(!User.authenticate(@user.login, "new_password"), "password should not have been reset")
  end

  def test__reset_password__invalid_code__should_show_error
    post :reset_password, :id => "Invalid Code", :password  => "new_password", :password_confirmation => "not matching password" 

    assert_match(/invalid password reset code/, flash[:notice])
  end


  # change_profile section
  def test_should_render_template_change_profile
    login_as :quentin
    get :change_profile
    assert_response :success
    assert_template "change_profile", "should have rendered change_profile template"
  end

  def test_should_save_valid_post_change_profile
    login_as :quentin
    post :change_profile, 
      :user => {:id => users('quentin').id, :login => "newLogin", :name => "New Name", :pi => "New PI"}
    assert_match( "Profile has been successfully changed.", flash[:notice])
    assert_response :redirect
  end

  def test_should_not_save_invalid_post_nonunique_login_change_profile
    login_as :quentin
    post :change_profile, 
      :user => {:id => users('quentin').id, :login => users('aaron').login, :name => "New Name", :pi => "New PI"}
    assert_response :success
    assert_template "change_profile", "should have rendered change_profile template"
    assert assigns(:user).errors.on(:login)
  end



  # change_password section
  def test_should_render_template_change_password
    login_as :quentin
    get :change_password
    assert_response :success
    assert_template "change_password", "should have rendered change_password template"
  end

  def test_should_save_valid_post_change_password
    login_as :quentin
    post :change_password, 
      :user => {:id => users('quentin').id, :password => "newPassword", :password_confirmation => "newPassword"}
    assert_match( "Password has been successfully changed.", flash[:notice])
    assert_response :redirect
  end

  def test_should_not_save_invalid_post_nonmatching_passwords_change_password
    login_as :quentin
    post :change_profile, 
      :user => {:id => users('quentin').id, :password => "newPassword", :password_confirmation => "newPassword2"}
    assert_response :success
    assert_template "change_profile", "should have rendered change_profile template"
    assert assigns(:user).errors.on(:password)
  end

  # change_email section
  def test_should_render_template_change_email
    login_as :quentin
    get :change_email
    assert_response :success
    assert_template "change_email", "should have rendered change_email template"
  end

  def test_should_save_valid_post_change_email
    login_as :quentin
    post :change_email, 
      :user => {:id => users('quentin').id, :email => "bubba@aol.com"}
    assert_response :success
    assert_equal 1, @emails.length
    assert(@emails.first.subject =~ /Request to change your email/)
    assert(@emails.first.body    =~ \
     /#{assigns(:user).login}, follow this link to activate the change to your new email address/)
  end

  def test_should_activate_new_email
    get :activate_new_email, :id => users(:aaron).email_activation_code
    @user = User.find(users(:aaron).id)
    assert_equal @user.email, users(:aaron).new_email
    assert_response :redirect
    assert_match( "The email address for your account has been updated.", flash[:notice])
  end

   


  protected
    def create_user(options = {})
      post :signup, :user => { :login => 'quire', :email => 'quire@example.com', 
       :name => 'Quire', :pi=> 'Mary Poppins', :institution => 'Disneyland',
        :password => 'quire', :password_confirmation => 'quire' }.merge(options)
    end
    
    def auth_token(token)
      CGI::Cookie.new('name' => 'auth_token', 'value' => token)
    end
    
    def cookie_for(user)
      auth_token users(user).remember_token
    end

end
