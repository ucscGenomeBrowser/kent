class Submission < ActiveRecord::Base

  belongs_to :user
  has_many :submission_files, :dependent => :destroy

  validates_presence_of :name
  validates_presence_of :s_type
  validates_presence_of :status
  validates_presence_of :user_id

end
