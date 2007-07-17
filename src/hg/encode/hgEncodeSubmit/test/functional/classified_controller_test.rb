require File.dirname(__FILE__) + '/../test_helper'
require 'classified_controller'

# Re-raise errors caught by the controller.
class ClassifiedController; def rescue_action(e) raise e end; end

class ClassifiedControllerTest < Test::Unit::TestCase
  def setup
    @controller = ClassifiedController.new
    @request    = ActionController::TestRequest.new
    @response   = ActionController::TestResponse.new
  end

  # Replace this with your real tests.
  def test_truth
    assert true
  end
end
