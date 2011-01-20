#!/usr/bin/env ruby

# NOTE this version does not support running multiple instances 
#  of the main runner now.  Only one should be started per pipeline instance.

require File.dirname(__FILE__) + '/config/boot'

require RAILS_ROOT + '/config/environment'

#require "pp"  # debug remove

#pp User.find(:all)

include PipelineBackground

def myIsInteger?(object)
  true if Integer(object) rescue false
end

if (ARGV.length < 1)
  print "1st parameter is required. Use the instance name.\n"
  exit 0
end

# Actually if this is a copy of the runner itself, 
# then the 2nd param will be the queueId
# of the job to be run.

targetQId = -1
pipelineInstanceName = ARGV[0]
if (ARGV.length >= 2)
  param1 = ARGV[1]
  if (myIsInteger?(param1))
    targetQId = param1.to_i

    # debug
    #print "working on queueId #{targetQId}\n"
    STDOUT.flush

    job = QueuedJob.find_by_id(targetQId)
    unless job
      print "child runner failed to find job to run #{targetQId} in queued_jobs!\n"
      exit 1
    end

    #pp job
    project_id = job.project_id
    source = job.source_code
    queued_at = job.queued_at
    job.destroy
    unless job.save
      print "child runner failed to remove queued_jobs message #{targetQId}!\n"
      exit 1
    end

    # set process priority low to 19 and should be inherited by children
    Process.setpriority(Process::PRIO_USER, 0, 19)     
    Process.setpriority(Process::PRIO_PROCESS, 0, 19)

    # do the work
    set_run_stat(project_id)
    eval source
    clear_run_stat(project_id)

    # send the parent main runner a message that we are done
    queue_job project_id, "done"
    
    exit 0
    
  end
end

# main background runner program!
#  it will fork off children 
#  up to the limits allowed.

print "--- Pipeline Main Background Runner Starting -----------\n"
print "#{Time.now.strftime("%a %b %e %H:%M:%S %Y")}\n"

# We are currently only throttling the background instances
# for expanding, validating, and loading:
maxExpanders = ActiveRecord::Base.configurations[RAILS_ENV]['maxExpanders']
if maxExpanders
  maxExpanders = maxExpanders.to_i
else  
  maxExpanders = 4 # default
end
print "maxExpanders = #{maxExpanders}\n"

maxValidators = ActiveRecord::Base.configurations[RAILS_ENV]['maxValidators']
if maxValidators
  maxValidators = maxValidators.to_i
else  
  maxValidators = 3 # default
end
print "maxValidators = #{maxValidators}\n"

maxLoaders = ActiveRecord::Base.configurations[RAILS_ENV]['maxLoaders']
if maxLoaders
  maxLoaders = maxLoaders.to_i
else
  maxLoaders = 2  # default
end
print "maxLoaders = #{maxLoaders}\n"

maxServerLoad = ActiveRecord::Base.configurations[RAILS_ENV]['maxServerLoad']
if maxServerLoad
  maxServerLoad = maxServerLoad.to_f
else
  maxServerLoad = 20.0  # default
end
print "maxServerLoad = #{maxServerLoad}\n"

MAXPROCESSES = 50
print "MAXPROCESSES = #{MAXPROCESSES}\n"

alljobs = {}
expanders = {}
validators = {}
loaders = {}

paraFetchRunCount = {}
paraFetchRunProtoSite = {}
paraFetchConfig = get_paraFetch_config

while true

  # debug
  #print ".\n"  

  STDOUT.flush

  source = ""
  project_id = 0
  queued_at = ""

  jobs = QueuedJob.find(:all, :order => 'id')

  doSleep = true
    
  jobs.each do |job|
    #pp job
    project_id = job.project_id
    source = job.source_code
    queued_at = job.queued_at
    if source == "quit"
      # remove this simple quit message immediately
      job.destroy
      job.save!
      exit 0
    end
    if source == "resetLists"
      # remove this simple message immediately
      job.destroy
      job.save!
      # presumably the counts have gotten out of whack
      alljobs = {}
      expanders = {}
      validators = {}
      loaders = {}
      print "all lists reset!\n"
      next
    end
    if source == "showListSizes"
      # remove this simple message immediately
      job.destroy
      job.save!
      # presumably the counts have gotten out of whack
      print "alljobs(#{alljobs.length}) expanders(#{expanders.length}) validators(#{validators.length}) loaders(#{loaders.length})\n"
      next 
    end
    if source == "done"
      # remove the project id from running lists
      expanders.delete(project_id)
      validators.delete(project_id)
      loaders.delete(project_id)
      alljobs.delete(project_id)
      protoSite = paraFetchRunProtoSite[project_id]
      if protoSite != nil
        paraFetchRunCount[protoSite] = paraFetchRunCount[protoSite] - 1
        paraFetchRunProtoSite.delete(project_id)
      end

      # remove this simple done message immediately
      job.destroy
      job.save!
      # if one is done, we probably want 
      # to re-scan without sleeping so we can start
      # anything startable right away.
      doSleep = false

    else  
      doRun = true
      # check to remove the project id from running lists
      if source.starts_with? "expand_background("
        if expanders.length >= maxExpanders
          doRun = false
        end
      elsif source.starts_with? "validate_background("
        if validators.length >= maxValidators
          doRun = false
        end
      elsif source.starts_with? "load_background("
        if loaders.length >= maxLoaders
          doRun = false
        end
      elsif source.starts_with? "upload_background("
        # parse out the url parameter from the source command
        #  the parameters are comma-separated, and literal strings are surrounded by quotes.
        pastFirstComma = source.index(',"')+2
        beforeSecondComma = source.index('",', pastFirstComma) - 1
        upurl = source[pastFirstComma..beforeSecondComma]
        if upurl != ""
          # load-balance paraFetch in a site-specific manner 
          # determined by the config/paraFetch.yml settings.
          protoSite = get_proto_site(upurl)  # can return nil for invalid url
          maxParaFetches = paraFetchConfig["default"]["instances"]
          if paraFetchConfig[protoSite] != nil
            if paraFetchConfig[protoSite]["instances"] != nil
              maxParaFetches = paraFetchConfig[protoSite]["instances"]
            end
          end
          paraRunCount = paraFetchRunCount[protoSite]
          if paraRunCount == nil
            paraRunCount = 0
          end
          if paraRunCount >= maxParaFetches
            doRun = false
          end
        end
      end


      # is it already running, or trying to start?
      if alljobs[project_id] 
        doRun = false  
      end

      # check the load
      if doRun
        myLoad = (`cat /proc/loadavg | gawk '{print $2}'`).to_f
        if (myLoad > maxServerLoad)
	  # DO NOT FILL THE LOG WITH MSGS
  	  unless defined?(old_timeHighLoad)
    	    old_timeHighLoad = Time.now
	  end
	  if Time.now - old_timeHighLoad > (5 * 60)
	    print "Load too high? #{myLoad}\n"
	    print "#{Time.now.strftime("%a %b %e %H:%M:%S %Y")}\n"
	    old_timeHighLoad = Time.now
	  end
          doRun = false  
        end
      end
	
      if alljobs.length > MAXPROCESSES
	# DO NOT FILL THE LOG WITH MSGS
	unless old_timeTooMany
    	  old_timeTooMany = Time.now
        end
        if Time.now - old_timeTooMany > (5 * 60)
    	  print "Too many processes running? #{alljobs.length}\n"
	  print "#{Time.now.strftime("%a %b %e %H:%M:%S %Y")}\n"
	  old_timeTooMany = Time.now
	end
	doRun = false  
      end

      if doRun
        # add the project id to running lists
        if source.starts_with? "expand_background("
          expanders[project_id] = true
        elsif source.starts_with? "validate_background("
          validators[project_id] = true
        elsif source.starts_with? "load_background("
          loaders[project_id] = true
        elsif source.starts_with? "upload_background("
          if upurl != ""
            if protoSite != nil
              paraFetchRunCount[protoSite] = paraRunCount + 1
              paraFetchRunProtoSite[project_id] = protoSite
            end
          end
        end
        alljobs[project_id] = true

        # works as long as the convention remains of naming things op_background() 
        bgr = source.rindex("_background")
        if bgr
	  op = source[0,bgr]
        else
          op = ""
        end

        pid = fork do
          exec("./pipeline_runner.rb #{pipelineInstanceName} #{job.id} #{project_id} #{op}")
        end

        Process.detach(pid)

      end

    end
    
  end

  if doSleep
    sleep 3  
  end

end

#puts "wow!"

exit 0
