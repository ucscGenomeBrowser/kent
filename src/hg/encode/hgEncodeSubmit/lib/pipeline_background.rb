module PipelineBackground

# TODO maybe all of these can be private ?

#  protected
  def background_test
    #require 'pp'   # debug
    #logger.info(projectType.pretty_inspect)
    #logger.info "called PipelineBackground::background_test"
    yell "called PipelineBackground::background_test"
    #return 5
  end

  def get_deadline()
    x = open("#{RAILS_ROOT}/config/deadline.yml") { |f| YAML.load(f.read) }
    return Time.local(x["year"], x["month"], x["day"], x["hour"], x["minute"], 0)
  rescue
    return nil
  end

  def get_paraFetch_config()
    paraFetchConfig = open("#{RAILS_ROOT}/config/paraFetch.yml") { |f| YAML.load(f.read) }
    if paraFetchConfig == nil
      print "unable to load config/paraFetch.yml\n"
      exit 1
    end
    if paraFetchConfig["default"] == nil
      print "error: default setting missing in config/paraFetch.yml\n" 
      exit 1
    end
    if paraFetchConfig["default"]["conns"] == nil
      print "error: conns setting missing from default setting in config/paraFetch.yml\n" 
      exit 1
    end
    if paraFetchConfig["default"]["instances"] == nil
      print "error: instances setting missing from default setting in config/paraFetch.yml\n"  
      exit 1
    end
    return paraFetchConfig
  rescue
    print "unable to load config/paraFetch.yml\n" 
    exit 1
  end

  def yell(msg)
    # stupid simple logging:
    f = File.open(File.expand_path(File.dirname(__FILE__) + "/../log/yell.log"),"a")
    f.puts msg
    f.close
  end

  def clear_run_stat(project_id)
    project = Project.find(project_id)
    if  project.run_stat != "waiting"  # helps keep auto-chain-through blocked
      project.run_stat = nil
    end
    saver project
  rescue ActiveRecord::RecordNotFound
  end

  def set_run_stat(project_id)
    unless project_id == 0
      project = Project.find(project_id)
      project.run_stat = "running"
      saver project
    end
  end

  def validate_background(project_id, validate_option)

    project = Project.find(project_id)
    new_status project, "validating"
    projectType = getProjectType(project)
    projectDir = path_to_project_dir(project_id)
    if (validate_option != nil)
      cmd = "#{projectType['validator']} #{validate_option} #{projectType['type_params']} #{projectDir} &> #{projectDir}/validate_error"
    else 
      cmd = "#{projectType['validator']} #{projectType['type_params']} #{projectDir} &> #{projectDir}/validate_error"
    end
    timeout = projectType['time_out']

    exitCode = run_with_timeout(cmd, timeout)

    if exitCode == -1
      ecmd = "echo 'Timeout #{timeout} exceeded running [#{cmd}]' >> #{projectDir}/validate_error"
      run_with_timeout(ecmd, 60)
    end

    # Try to avoid error "Lost connection to MySQL server during query"
    ActiveRecord::Base.verify_active_connections!

    # get a fresh project instance so we don't overwrite changes by validator
    project = Project.find(project_id)

    if exitCode == 0
      #old way:
      #new_status project, "validated"
      #load_background(project_id)
      new_status project, "load requested"
      # do not chain through immediately, instead request the next step.
      unless queue_job project.id, "load_background(#{project.id})"
        print "System error - queued_jobs save failed."
        return
      end
    else
      new_status project, "validate failed"
      # send email notification
      user = User.find(project.user_id)
      UserNotifier.deliver_failure_notification(user, project, "a validation")
    end

  end 

    
  def load_background(project_id)

    project = Project.find(project_id)
    new_status project, "loading"

    projectType = getProjectType(project)
 
    projectDir = path_to_project_dir(project_id)
    cmd = "#{projectType['loader']} #{projectType['load_params']} #{projectDir} &> #{projectDir}/load_error"
    timeout = projectType['load_time_out']

    exitCode = run_with_timeout(cmd, timeout)

    if exitCode == -1
      ecmd = "echo 'Timeout #{timeout} exceeded running [#{cmd}]' >> #{projectDir}/load_error"
      run_with_timeout(ecmd, 60)
    end

    # Try to avoid error "Lost connection to MySQL server during query"
    ActiveRecord::Base.verify_active_connections!

    # get a fresh project instance so we don't overwrite changes by loader
    project = Project.find(project_id)

    if exitCode == 0
      project.status = "loaded"
      # send email notification
      user = User.find(project.user_id)
      UserNotifier.deliver_load_notification(user.email, user, project)
      # also notify the emailOnLoad person
      emailOnLoad = ActiveRecord::Base.configurations[RAILS_ENV]['emailOnLoad']
      if (emailOnLoad)
        UserNotifier.deliver_load_notification(emailOnLoad, user, project)
      end
    else
      project.status = "load failed"
      # send email notification
      user = User.find(project.user_id)
      UserNotifier.deliver_failure_notification(user, project, "a database load")
    end
    new_status project, project.status

  end 

  def unload_background(project_id)
    # call an unload cleanup routine 
    #  (e.g. that can remove .wib symlinks from /gbdb/ to the submission dir)
    project = Project.find(project_id)
    projectDir = path_to_project_dir(project_id)
    msg = ""
    if File.exists?(projectDir)
      unless new_status project, "unloading"
        return
      end
    
      projectType = getProjectType(project)
 
      projectDir = path_to_project_dir(project_id)
      cmd = "#{projectType['unloader']} #{projectType['unload_params']} #{projectDir} &> #{projectDir}/unload_error"
      timeout = projectType['unload_time_out']

      #logger.info "GALT! cmd=#{cmd} timeout=#{timeout}"

      exitCode = run_with_timeout(cmd, timeout)

      if exitCode == -1
        ecmd = "echo 'Timeout #{timeout} exceeded running [#{cmd}]' >> #{projectDir}/unload_error"
        run_with_timeout(ecmd, 60)
      end

      if exitCode == 0
        if project.project_archives.length == 0
          new_status project, "new"
        else
          new_status project, "expanded"
        end
      else
        yell "Project unload failed."
        new_status project, "unload failed"
        return
      end

    end    

  end
 
  def delete_background(project_id)
    project = Project.find(project_id)
    unload_background(project.id)
    delete_completion(project.id)
    new_status project, "deleted"
    destroyer project
  end
 

  def upload_background(project_id, upurl, upftp, upload, local_path, allowReloads)

    project = Project.find(project_id)

    #puts "got to start of upload method"  # debug DEBUG remove

    # handle FTP stuff
    user = project.user
    fullPath = ActiveRecord::Base.configurations[RAILS_ENV]['ftpMount']+'/'+user.login

    unless upurl
      upurl = ""
    end
    unless upftp
      upftp = ""
    end
    unless upload
      upload = ""
    end
    unless local_path
      local_path = ""
    end

    unless upurl.blank?
      filename = sanitize_filename(upurl)
    else
      unless upftp.blank?
        filename = sanitize_filename(upftp)
      else
        filename = sanitize_filename(upload)
      end
    end

    #msg = ""

    # make sure parent paths exist
    projectDir = path_to_project_dir(project_id)
    Dir.mkdir(projectDir,0775) unless File.exists?(projectDir)

    nextArchiveNo = project.archive_count+1

    plainName = filename
    filename = "#{"%03d" % nextArchiveNo}_#{filename}"

    pf = path_to_file(project_id, filename)

    #debugging
    #msg += "sanitized filename=#{filename}<br>"
    #msg += "RAILS_ROOT=#{RAILS_ROOT}<br>"
    #msg += "upload path=#{ActiveRecord::Base.configurations[RAILS_ENV]['upload']}<br>"
    #msg += "path_to_file=#{pf}<br>"
    #msg += "nextArchiveNo=#{nextArchiveNo}<br>"

    #msg += "Uploading/expanding #{plainName}.<br>"

    unless new_status project, "uploading"
      return
    end

    #  cases where file will "pre-exist":
    #    if local file upload the rails foreground creates it before upload_background is called
    #    if url with resume, keep it.
    
    unless upurl.blank?

      #OLD WGET: cmd = "wget -v -O #{pf} #{autoResume} '#{upurl}' &> #{projectDir}/upload_error" 
      # Use new paraFetch multiple parallel connection downloader
      #  Speeds up downloads from distant sites.
      #  Tries 30 simultaneous connections.
      #  Performs up to 10 retries with sleeps in between of currently 30 seconds between retries.
      protoSite = get_proto_site(upurl)
      paraFetchConfig = get_paraFetch_config
      if paraFetchConfig[protoSite] == nil
        protoSite = "default"
      end
      conns = paraFetchConfig[protoSite]["conns"]
      if conns == nil  # not supposed to happen
        conns = "10"
      end
      cmd = "paraFetch #{conns} 10 '#{upurl}' #{pf} &> #{projectDir}/upload_error" 

      #yell "\n\nGALT! cmd=[#{cmd}]\n\n"   # DEBUG remove

      timeout = 72000  # 20 hours
      exitCode = run_with_timeout(cmd, timeout)

      if exitCode == -1
        ecmd = "echo 'Timeout #{timeout} exceeded running [#{cmd}]' >> #{projectDir}/upload_error"
        run_with_timeout(ecmd, 60)
      end

      if exitCode != 0
        new_status project, "upload failed"
        # send email notification
        user = User.find(project.user_id)
        UserNotifier.deliver_failure_notification(user, project, "an upload")
        return
      end

    else 
      unless upftp.blank?
        ftpPath = File.join(fullPath,upftp)
        unless File.exists?(ftpPath)
          cmd = "echo 'FTP file #{upftp} is missing' &> #{projectDir}/upload_error" 
          system(cmd)
          new_status project, "upload failed"
          return
        end
        #FileUtils.copy(ftpPath, pf)
        File.delete(pf) if File.exists?(pf)
        File.rename(ftpPath, pf)
        File.delete(ftpPath) if File.exists?(ftpPath) and File.exists(pf)  # just in case rename doesn't cleanup
      else
        unless local_path.blank?
          unless File.exists?(local_path)
            cmd = "echo 'local_path file #{local_path} is missing' &> #{projectDir}/upload_error" 
            system(cmd)
            new_status project, "upload failed"
            return
          end
          #FileUtils.copy(local_path, pf)
          File.rename(local_path, pf)
          File.delete(local_path) if File.exists?(local_path) and File.exists(pf)  # just in case rename doesn't cleanup
        end
      end
    end

    unless process_uploaded_archive(project, filename)
      new_status project, "upload failed"
      return
    end

    unless new_status project, "uploaded"
      return
    end

    #old way: 
    #expand_background(project_id, filename, allowReloads)
    # do not chain through immediately, instead request the next step.
    new_status project, "expand requested"
    unless queue_job project.id, "expand_background(#{project.id}, \"#{filename}\", \"#{allowReloads}\")"
      print "System error - queued_jobs save failed.\n"
      return
    end

  end

  def expand_background(project_id, filename, allowReloads)
  
    project = Project.find(project_id)

    unless new_status project, "expanding"
      return
    end

    #new_status project, project.status
    unless prep_one_archive(project, filename, project.archive_count)
      new_status project, "expand failed"
      return
    end

    unless expand_archive(project, project.project_archives[project.archive_count-1])
      new_status project, "expand failed"
      return
    end

    unless new_status project, "expanded"
      return
    end

    #old way: 
    #validate_background(project_id, allowReloads)
    # do not chain through immediately, instead request the next step.
    new_status project, "validate requested"
    unless queue_job project.id, "validate_background(#{project.id}, \"#{allowReloads}\")"
      print "System error - queued_jobs save failed.\n"
      return
    end

  end

 def process_uploaded_archive(project, filename)
    
    # make sure parent paths exist
    projectDir = path_to_project_dir(project.id)

    nextArchiveNo = project.archive_count+1

    pf = path_to_file(project.id, filename)

    # need to test for and delete any with same archive_no (just in case?)
    # moved this up here just to get the @project_archive.id set
    old = ProjectArchive.find(:first, :conditions => ['project_id = ? and archive_no = ?', project.id, nextArchiveNo])
    old.destroy if old
    # add new projectArchive record
    project_archive = ProjectArchive.new
    project_archive.project_id = project.id 
    project_archive.archive_no = nextArchiveNo
    project_archive.file_name = filename
    project_archive.file_size = File.size(pf)
    project_archive.file_date = Time.now    # TODO: add .utc to make UTC time?
    project_archive.status = "see current"
    project_archive.archives_active = ""
    unless saver project_archive
      ecmd = "echo 'saving project_archive record failed for #{filename}' > #{projectDir}/upload_error"
      run_with_timeout(ecmd, 60)
      return false
    end

    project.archive_count = nextArchiveNo
    project.archives_active += "1"

    unless saver project
      ecmd = "echo 'saving project record failed for #{filename}' > #{projectDir}/upload_error"
      run_with_timeout(ecmd, 60)
      return false
    end

    return true

  end


  def expand_archive(project, archive)
    # expand archive belonging to archive record

    projectDir = path_to_project_dir(project.id)
    uploadDir = projectDir+"/upload_#{archive.archive_no}"

    unless process_archive(project, archive.id, projectDir, uploadDir, "")
      return false
    end

    # cleanup: delete temporary upload subdirectory
    clean_out_dir uploadDir

    return true

  end

  def process_archive(project, archive_id, projectDir, uploadDir, relativePath)
    # process the archive files
    fullPath = my_join(uploadDir,relativePath)
    Dir.entries(fullPath).each do
      |f| 
      if (f == ".") or (f == "..") or f.starts_with?("._")  # ignore . .. and mac resource forks (._)
        next
      end
      fullName = my_join(fullPath,f)
      FileUtils.chmod 0775, fullName  # avoid problems from weird perms inside the archive
      if (File.ftype(fullName) == "directory")
        newRelativePath = my_join(relativePath,f)
        newDir = my_join(projectDir, newRelativePath)
	unless File.exists?(newDir)
	  Dir.mkdir(newDir,0775)
	end
        unless process_archive(project, archive_id, projectDir, uploadDir, newRelativePath)
          return false
        end
      else 
        if File.ftype(fullName) == "file"
   
	  relName = my_join(relativePath,f)
 
          # delete any equivalent projectFile records
  	  project.project_archives.each do |c|
            old = ProjectFile.find(:first, :conditions => ['project_archive_id = ? and file_name = ?', c.id, relName])
            old.destroy if old
          end

          project_file = ProjectFile.new
          project_file.file_name = relName
          project_file.file_size = File.size(fullName)
          project_file.file_date = File.ctime(fullName)
          project_file.project_archive_id = archive_id 
          unless project_file.save
            ecmd = "echo 'saving project_file record failed for project #{project.id} archive #{archive_id} file #{filename}' > #{projectDir}/expand_error"
            run_with_timeout(ecmd, 60)
            return false
          end
    
          parentDir = my_join(projectDir, relativePath)
          toName = my_join(parentDir, f)    
          # move file from temporary upload dir into parent dir
          File.rename(fullName,toName);
          FileUtils.chmod 0664, toName

        end
      end
    end
    return true
  end

  def prep_one_archive(project, filename, archive_no)

    # make sure parent paths exist
    projectDir = path_to_project_dir(project.id)

    # make a temporary upload directory to unpack and merge from
    uploadDir = projectDir+"/upload_#{archive_no}"
    clean_out_dir uploadDir
    Dir.mkdir(uploadDir,0775)

    cmd = makeUnarchiveCommand(project, uploadDir, filename)
    timeout = 36000
    exitCode = run_with_timeout(cmd, timeout)

    if exitCode == -1
      ecmd = "echo 'Timeout #{timeout} exceeded running [#{cmd}]' >> #{projectDir}/expand_error"
      run_with_timeout(ecmd, 60)
    end

    if exitCode != 0
      return false
    end
    return true

  end

  def reexpand_all_background(project_id)

    project = Project.find(project_id)
    new_status project, "re-expanding all"
    # make a hash of things to keep
    keepers = {}
    project.project_archives.each do |a|
      keepers[a.file_name] = "keep"
    end
    # keep other special files
    keepers["validate_error"] = "keep"
    keepers["load_error"] = "keep"
    keepers["expand_error"] = "keep"
    keepers["upload_error"] = "keep"
    keepers["out"] = "keep"

    msg = ""
    # make sure parent paths exist
    projectDir = path_to_project_dir(project.id)

    # clean out directory
    Dir.entries(projectDir).each do 
      |f| 
      fullName = File.join(projectDir,f)
      unless keepers[f] or (f == ".") or (f == "..")
        cmd = "rm -fr #{fullName}"
        unless system(cmd)
          yell "System error cleaning up subdirectory: <br>command=[#{cmd}].<br>"  
          ecmd = "echo 'expansion failed: system error for command=[#{cmd}].}' > #{projectDir}/expand_error"
          run_with_timeout(ecmd, 60)
          project.status = "expand failed"
          new_status project, project.status
          return
        end
      end
    end

    project.project_archives.each do |a|
      a.project_files.each do |f|
        f.destroy
      end
    end

    found = false 
    project.project_archives.each do |a|
      n = a.archive_no-1
      c = project.archives_active[n..n]
      if c == "1"
        found = true
      end
    end

    if found
      project.project_archives.each do |a|
        n = a.archive_no-1
        c = project.archives_active[n..n]
        if c == "1"
          unless prep_one_archive project, a.file_name, a.archive_no
            ecmd = "echo 'expansion failed for #{a.file_name}' > #{projectDir}/expand_error"
            run_with_timeout(ecmd, 60)
            project.status = "expand failed"
            new_status project, project.status
            return            
          end
        end
      end
      reexpand_all_completion project
    else
      if project.project_archives.length == 0
        project.status = "new"
      else
        project.status = "expanded"
      end
    end
    new_status project, project.status

  end

 
  # --- run process with timeout ---- (probably should move this to an application helper location)
  def run_with_timeout(cmd, myTimeout)
    # run process, kill it if exceeds specified timeout in seconds
    sleepInterval = 0.5  #seconds
    if ( (cpid = fork) == nil)
      exec(cmd)
    else
      before = Time.now
      while (true)
	pid, status = Process.wait2(cpid,Process::WNOHANG)
        if pid == cpid
          return status.exitstatus
        end
        if ( (Time.now - before) > myTimeout)
          killAllDescendantsOf(cpid)   # kill any descendents of this child first.
          Process.kill("ABRT",cpid)    # kill this child
	  pid, status = Process.wait2(cpid) # clean up zombies
          return -1
        end
        sleep(sleepInterval)
      end
    end
  end


  def sanitize_filename(file_name)
    # get only the filename, not the whole path (from IE)
    just_filename = File.basename(file_name) 
    # replace all non-alphanumeric, underscore or periods with underscore
    just_filename.gsub(/[^\w\.\_]/,'_') 
  end

  def my_join(path,name)
    if (path == "") 
      return name
    end
    if (name == "") 
      return path
    end
    return File.join(path,name)
  end


  # --------- PRIVATE ---------
private

  def reexpand_all_completion(project) 
    # after background expansion of all archives not marked as deleted

    project.project_archives.each do |a|
      n = a.archive_no-1
      c = project.archives_active[n..n]
      if c == "1"
        unless expand_archive(project, a)
          new_status project, "expand failed"
          return
        end
      end
    end

    if project.project_archives.length == 0
      project.status = "new"
    else
      project.status = "expanded"
    end

  end

  def delete_completion(project_id)

    projectDir = path_to_project_dir(project_id)
    if File.exists?(projectDir)
      Dir.entries(projectDir).each { 
        |f| 
        unless (f == ".") or (f == "..")
          fullName = File.join(projectDir,f)
          cmd = "rm -fr #{fullName}"
          unless system(cmd)
            yell "System error cleaning out project subdirectory: <br>command=[#{cmd}].<br>" 
            # TODO maybe make better way to handle feedback to user?	
          end
        end
      }
      Dir.delete(projectDir)
    end
  end


  def path_to_project_dir(project_id)
    # the expand_path method resolves this relative path to full absolute path
    File.expand_path("#{ActiveRecord::Base.configurations[RAILS_ENV]['upload']}/#{project_id}")
  end


  def path_to_file(project_id, filename)
    # the expand_path method resolves this relative path to full absolute path
    path_to_project_dir(project_id)+"/#{filename}"
  end


  #TODO : turn projectTypes back into a yaml file in config dir

  # --- read project types from config file into hash -------
  def getProjectTypes
    #open("#{RAILS_ROOT}/config/projectTypes.yml") { |f| YAML.load(f.read) }
    ProjectType.find(:all, :conditions => ['display_order != 0'], :order => "display_order")
  end

  # --- read one project type from config file into hash -------
  def getProjectType(project)
    projectTypes = getProjectTypes
    projectTypes.each do |x|
      if x['id'] == project.project_type_id
        return x
      end
    end
  end


  def log_project_status(project)
    # add new projectArchive record
    project_status_log = ProjectStatusLog.new
    project_status_log.project_id = project.id
    project_status_log.status = project.status
    return saver(project_status_log)
  end

  def saver(record)
    unless record.save
      msg = "System error - #{record.table_name} record save failed.\n"
      record.errors.each_full { |x| msg += x + "\n" }
      yell msg
      return false
    end
    return true
  end
 
  def destroyer(record)
    unless record.destroy
      msg = "System error - #{record.table_name} record destroy failed.\n"
      record.errors.each_full { |x| msg += x + "\n" }
      yell msg
      return false
    end
    return true
  end

  def new_status(project, status)
    project.status = status
    if status.ends_with?(" requested")
      project.run_stat = "waiting"
    end
    unless saver project
      return false
    end
    return log_project_status(project)
  end
 
  def makeUnarchiveCommand(project, uploadDir, filename)
    # handle unzipping the archive
    pf = path_to_file(project.id, filename)
    if ["zip", "ZIP"].any? {|ext| filename.ends_with?("." + ext) }
      cmd = "unzip -o  #{pf} -d #{uploadDir} &> #{File.dirname(uploadDir)}/expand_error"   # .zip 
    else
      if ["gz", "GZ", "tgz", "TGZ"].any? {|ext| filename.ends_with?("." + ext) }
        cmd = "tar -xzf #{pf} -C #{uploadDir} &> #{File.dirname(uploadDir)}/expand_error"  # .gz .tgz gzip 
      else  
        cmd = "tar -xjf #{pf} -C #{uploadDir} &> #{File.dirname(uploadDir)}/expand_error"  # .bz2 bzip2
      end
    end
  end

  def clean_out_dir(path)
    if File.exists?(path)
      cmd = "rm -fr #{path}"
      unless system(cmd)
        flash[:error] = "System error cleaning out subdirectory: <br>command=[#{cmd}].<br>"  
        return false
      end
    end
    return true
  end

  def getErrText(project, filename)
    # get error output file
    errFile = path_to_file(project.id, filename)
    return File.open(errFile, "rb") { |f| f.read }
  rescue
    return ""
  end

  def getUploadErrText(project)
    return getErrText(project, "upload_error")
  end

  def getExpandErrText(project)
    return getErrText(project, "expand_error")
  end

  def getValidateErrText(project)
    return getErrText(project, "validate_error")
  end

  def getLoadErrText(project)
    return getErrText(project, "load_error")
  end

  def getUnloadErrText(project)
    return getErrText(project, "unload_error")
  end

  def getNewestFileByExtensionIgnoringCase(project_id, ext)
    # return the body of the newest file with the given extension
    newest = ""
    newestDate = nil
    projectDir = path_to_project_dir(project_id)
    if File.exists?(projectDir)
      Dir.entries(projectDir).each { 
        |f| 
        if f.ends_with?("." + ext) or
	   f.ends_with?("." + ext.downcase) or
           f.ends_with?("." + ext.upcase)
          ftime = File.mtime(path_to_file(project_id, f))
	  if newest == "" or (ftime > newestDate)
	    newest = f
            newestDate = ftime
	  end
        end
      }
    end
    unless newest == ""
      theFile = path_to_file(project_id, newest)
      return newest, File.open(theFile, "rb") { |f| f.read }
    else
      return "",""
    end
  rescue
    return "",""
  end

  def getDafText(project)
    return getNewestFileByExtensionIgnoringCase(project.id, "daf")
  end

  def getDdfText(project)
    return getNewestFileByExtensionIgnoringCase(project.id, "ddf")
  end

  def goGreek(size)
    newsize = size * 1.0
    if size > 1024 * 1024 * 1024
	newsize /= 1024 * 1024 * 1024
	suffix = "G"
    elsif size > 1024 * 1024
	newsize /= 1024 * 1024
	suffix = "M"
    elsif size > 1024
	newsize /= 1024
	suffix = "K"
    else 
	suffix = ""
    end
    return ("%.2f" % newsize) + suffix
  end


  def queue_job(project_id, source)
    job = QueuedJob.new
    job.project_id = project_id
    job.queued_at = Time.now.utc
    job.source_code = source
    unless job.save
      return false
    end
    return true
  end

  def killAllDescendantsOf(pid)
    f = open("|pgrep -P #{pid}")
    lines = f.readlines
    f.close
    lines.each do |line|
      dpid = Integer(line)
      killAllDescendantsOf(dpid)   # Recurse
      Process.kill("ABRT",dpid)
    end

  end

  def get_proto_site(url)
    pastProto = url.index("://")
    if pastProto == nil
	return nil
    end
    pastProto += 3
    pastSite = url.index("/", pastProto)
    if pastProto == nil
	return nil
    end
    pastSite += 1
    return url[0,pastSite]
  end

end
