require File.dirname(__FILE__) + '/../test_helper'

class SubmissionTest < Test::Unit::TestCase
  fixtures :users
  fixtures :submissions

  # Replace this with your real tests.
  def test_truth
    assert true
  end

  def test_create
    s = Submission.new
    s.name = "Test1"
    s.s_type = "chip-chip"
    s.status = "new"
    s.user_id = users('aaron').id
    assert s.save
  end

  def test_create_fail_no_name
    s = Submission.new
    s.s_type = "chip-chip"
    s.status = "new"
    s.user_id = users('aaron').id
    assert !s.save
  end

  def test_create_fail_no_stype
    s = Submission.new
    s.name = "Test1"
    s.status = "new"
    s.user_id = users('aaron').id
    assert !s.save
  end

  def test_create_fail_no_status
    s = Submission.new
    s.name = "Test1"
    s.s_type = "chip-chip"
    s.user_id = users('aaron').id
    assert !s.save
  end

  def test_create_fail_no_userid
    s = Submission.new
    s.name = "Test1"
    s.s_type = "chip-chip"
    s.status = "new"
    assert !s.save
  end

end
