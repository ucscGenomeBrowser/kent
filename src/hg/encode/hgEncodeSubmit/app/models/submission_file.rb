class SubmissionFile < ActiveRecord::Base

  belongs_to :submission

  validates_presence_of :file_name
  validates_presence_of :file_size
  validates_presence_of :file_date
  validates_presence_of :submission_id
  validates_presence_of :sf_type
  validates_presence_of :status

  protected
    def validate
      #errors.add(:price, "should be a positive value") if price.nil? || price < 0.01
    end

end
