require File.dirname(__FILE__) + '/../test_helper'

class CategoryTest < Test::Unit::TestCase
  fixtures :categories

  # Replace this with your real tests.
  def test_truth
    assert true
  end

  def test_valid
    c = Category.new(:name => "Furniture")
    assert_valid c
  end

  def test_unique_name
    c = Category.new(:name => "Sample Category")
    assert !c.valid?
  end

end
