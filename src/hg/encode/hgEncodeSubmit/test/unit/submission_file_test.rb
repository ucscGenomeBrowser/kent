require File.dirname(__FILE__) + '/../test_helper'

class SubmissionFileTest < Test::Unit::TestCase
  fixtures :users
  fixtures :submissions
  fixtures :submission_files

  # Replace this with your real tests.
  def test_truth
    assert true
  end

  def test_create
    f = SubmissionFile.new
    f.file_name = "Test1.adr"
    f.file_size = 32158
    f.file_date = Time.now
    f.submission_id = submissions('one').id
    assert f.save
  end

  def test_create_fail_no_name
    f = SubmissionFile.new
    f.file_size = 32158
    f.file_date = Time.now
    f.submission_id = submissions('one').id
    assert !f.save
  end

  def test_create_fail_no_size
    f = SubmissionFile.new
    f.file_name = "Test1.adr"
    f.file_date = Time.now
    f.submission_id = submissions('one').id
    assert !f.save
  end

  def test_create_fail_no_date
    f = SubmissionFile.new
    f.file_name = "Test1.adr"
    f.file_size = 32158
    f.submission_id = submissions('one').id
    assert !f.save
  end

  def test_create_fail_no_submissionid
    f = SubmissionFile.new
    f.file_name = "Test1.adr"
    f.file_size = 32158
    f.file_date = Time.now
    assert !f.save
  end

end
