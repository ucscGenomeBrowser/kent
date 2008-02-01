class ProjectStatusLog < ActiveRecord::Base

  #belongs_to :project

  validates_presence_of :project_id
  validates_presence_of :status

  protected
    def validate
      #errors.add(:price, "should be a positive value") if price.nil? || price < 0.01
    end

end
