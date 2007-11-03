class SubmissionType < ActiveRecord::Base

  #has_many :submission_archives

  validates_presence_of :name
  validates_presence_of :short_label
  validates_presence_of :long_label
  validates_presence_of :validator
  validates_presence_of :type_params
  validates_presence_of :description
  validates_presence_of :display_order
  validates_presence_of :time_out

  protected
    def validate
      #errors.add(:price, "should be a positive value") if price.nil? || price < 0.01
    end

end
