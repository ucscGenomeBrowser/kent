require File.dirname(__FILE__) + '/../test_helper'

class ClassifiedTest < Test::Unit::TestCase
  fixtures :classifieds

  # Replace this with your real tests.
  def test_truth
    assert true
  end

  def test_create
    c = Classified.new
    c.title = "Test Title"
    c.price = 566.00
    c.location = "Chicago"
    c.description = "This is a sample description that is not very interesting"
    c.email = "justin@secondgearllc.com"
    assert c.save
    
    # This will pass
    d = Classified.find(c.id)
    assert_equal(c,d)

    # This will fail
    e = Classified.find(1)
    assert_not_equal(d,e)

  end

end
