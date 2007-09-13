class SubmissionArchive < ActiveRecord::Base

  belongs_to :submission
  has_many :submission_files, :dependent => :destroy

  validates_presence_of :file_name
  validates_presence_of :file_size
  validates_presence_of :file_date
  validates_presence_of :submission_id

  protected
    def validate
      #errors.add(:price, "should be a positive value") if price.nil? || price < 0.01
    end

end
