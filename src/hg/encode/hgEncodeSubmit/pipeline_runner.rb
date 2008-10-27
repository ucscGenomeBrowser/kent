#!/usr/bin/env ruby

require File.dirname(__FILE__) + '/config/boot'

require RAILS_ROOT + '/config/environment'

#require "pp"

#pp User.find(:all)

include PipelineBackground

while true

  source = ""
  project_id = 0
  queued_at = ""

  QueuedJob.transaction do
    job = QueuedJob.find(:first, :lock => true)
    if job
      #pp job
      project_id = job.project_id
      source = job.source_code
      queued_at = job.queued_at
      job.destroy
      job.save!
    end
  end

  if source == "quit"
    break
  end

  if source.blank?
    sleep 3  
  else
    set_run_stat(project_id)
    eval source
    clear_run_stat(project_id)
  end


end

#puts "wow!"

