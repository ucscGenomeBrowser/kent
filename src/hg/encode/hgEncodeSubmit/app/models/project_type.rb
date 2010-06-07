class ProjectType < ActiveRecord::Base

  #has_many :Project_archives

  validates_presence_of :name
  validates_presence_of :short_label
  validates_presence_of :long_label
  validates_presence_of :validator
  validates_presence_of :type_params
  validates_presence_of :description
  validates_presence_of :display_order
  validates_presence_of :time_out
  validates_presence_of :loader
  validates_presence_of :load_params
  validates_presence_of :load_time_out
  validates_presence_of :unloader
  validates_presence_of :unload_params
  validates_presence_of :unload_time_out

  protected
    def validate
      #errors.add(:price, "should be a positive value") if price.nil? || price < 0.01
    end

end
