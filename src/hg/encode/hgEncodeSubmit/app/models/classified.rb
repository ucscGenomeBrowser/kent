class Classified < ActiveRecord::Base
  
  belongs_to :category
  
  validates_presence_of :title, :message => "cannot be blank. Make your title descriptive"
  validates_presence_of :price
  validates_presence_of :location
  validates_presence_of :description
  validates_presence_of :email
  validates_numericality_of :price, :message => "must be a numeric value (do not include a dollar sign)"
  validates_format_of :email, :with => /^([^\s]+)@((?:[-a-z0-9]+\.)+[a-z]{2,})$/i
  
  protected
    def validate
      errors.add(:price, "should be a positive value") if price.nil? || price < 0.01
    end
    
end
