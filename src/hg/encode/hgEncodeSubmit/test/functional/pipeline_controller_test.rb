require File.dirname(__FILE__) + '/../test_helper'
require 'pipeline_controller'
require 'account_controller'

# Re-raise errors caught by the controller.
class PipelineController; def rescue_action(e) raise e end; end

class PipelineControllerTest < Test::Unit::TestCase

  # Be sure to include AuthenticatedTestHelper in test/test_helper.rb instead
  # Then, you can remove it from this and the units test.
  include AuthenticatedTestHelper

  fixtures :users
  fixtures :submissions

  def login(username, password)
    controller = @controller
    @controller = AccountController.new
    post :login, :login => username, :password => password
    assert session[:user]
    assert_response :redirect
    @controller = controller
  end

  def setup
    @controller = PipelineController.new
    @request    = ActionController::TestRequest.new
    @response   = ActionController::TestResponse.new
    login('quentin', 'test')
  end

  # Replace this with your real tests.
  def test_truth
    assert true
  end

  def test_list
    get :list
    assert_response :success
    assert_template "list", "expected list template as response"
  end

  def test_show_user
    get :show_user
    assert_response :success
    assert_template "list", "expected list template as response"
  end


  # submissions tests

  def test_new
    get :new
    assert_response :success
    assert_template "new"
  end

  def test_create
    post :create, :submission => {:name => "test3" , :s_type => "chip=chip"}
    assert_response :redirect
  end

  def test_show
    get :show, :id => submissions("one").id 
    assert_response :success
    assert_template "show"
  end

  def test_edit
    get :edit, :id => submissions("one").id 
    assert_response :success
    assert_template "edit"
  end

  def test_update
    post :update, :id => submissions("one").id,  :submission => {:name => "test3" , :s_type => "chip=seq"}
    assert_response :redirect
  end

  def test_delete
    get :delete, :id => submissions("one").id 
    assert_response :redirect
  end

  # TODO: add tests that fail, add tests for upload
 

end
