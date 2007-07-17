require File.dirname(__FILE__) + '/../test_helper'
require 'pipeline_controller'

# Re-raise errors caught by the controller.
class PipelineController; def rescue_action(e) raise e end; end

class PipelineControllerTest < Test::Unit::TestCase
  def setup
    @controller = PipelineController.new
    @request    = ActionController::TestRequest.new
    @response   = ActionController::TestResponse.new
  end

  # Replace this with your real tests.
  def test_truth
    assert true
  end
end
