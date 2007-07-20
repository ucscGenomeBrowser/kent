class Category < ActiveRecord::Base
  has_many :classifieds
  validates_presence_of :name
  validates_uniqueness_of :name
end
