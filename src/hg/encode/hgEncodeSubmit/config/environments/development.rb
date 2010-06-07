# Settings specified here will take precedence over those in config/environment.rb

# In the development environment your application's code is reloaded on
# every request.  This slows down response time but is perfect for development
# since you don't have to restart the webserver when you make code changes.
config.cache_classes = false

# Log error messages when you accidentally call methods on nil.
config.whiny_nils = true

# Enable the breakpoint server that script/breakpointer connects to
config.breakpoint_server = true

# Show full error reports and disable caching
config.action_controller.consider_all_requests_local = true
config.action_controller.perform_caching             = false
config.action_view.cache_template_extensions         = false
config.action_view.debug_rjs                         = true

# deliver the mail for real
config.action_mailer.delivery_method = :smtp
#note: moved the setting to here because when
# it was in the ../environments.rb, this setting
# in the test.rb did not get over-ridden,
# therefore had to move setting for this
# out of there, otherwise emails still
# attempt to send for real when just running
# the test harness

# care if the mailer can't send
config.action_mailer.raise_delivery_errors = true

