#!/usr/bin/env python3
# Reports user agent captcha exceptions as well as API key captcha bypasses on the RR/euro/dev. Refs #36797

import os
import re
import subprocess
from datetime import datetime
from collections import defaultdict
from pathlib import Path

def get_current_year_and_month():
    """Get current year and determine which year's logs to check."""
    now = datetime.now()
    current_month = now.month
    current_year = now.year
    
    # If running in January, look at previous year's logs
    if current_month == 1:
        log_year = current_year - 1
    else:
        log_year = current_year
    
    return log_year

def get_last_n_error_logs(directory, n=5):
    """Get the last n error log files from a directory."""
    if not os.path.exists(directory):
        return []
    
    # Find all error_log.*.gz files
    log_files = []
    for file in os.listdir(directory):
        if file.startswith('error_log.') and file.endswith('.gz'):
            full_path = os.path.join(directory, file)
            log_files.append(full_path)
    
    # Sort by filename (which includes date) and get last n
    log_files.sort()
    return log_files[-n:]

def extract_api_keys_from_log(log_file):
    """Extract API keys and their counts from a gzipped log file."""
    api_key_counts = defaultdict(int)
    
    try:
        # Use zcat and grep to extract API keys
        cmd = f"zcat {log_file} | grep -oP 'CAPTCHAPASS_APIKEY \\K\\S+'"
        result = subprocess.run(cmd, shell=True, capture_output=True, text=True)
        
        if result.returncode == 0:
            for api_key in result.stdout.strip().split('\n'):
                if api_key:  # Skip empty lines
                    api_key_counts[api_key] += 1
    except Exception as e:
        print(f"Error processing {log_file}: {e}")
    
    return api_key_counts

def extract_bot_agents_from_log(log_file):
    """Extract bot agents and their counts from a gzipped log file."""
    bot_agent_counts = defaultdict(int)
    
    try:
        # Use zcat and grep to extract lines with CAPTCHAPASS (not CAPTCHAPASS_APIKEY)
        cmd = f"zcat {log_file} | grep 'CAPTCHAPASS' | grep -v 'CAPTCHAPASS_APIKEY'"
        result = subprocess.run(cmd, shell=True, capture_output=True, text=True)
        
        if result.returncode == 0:
            for line in result.stdout.strip().split('\n'):
                if line:
                    # Extract the agent name after "matches"
                    match = re.search(r'matches\s+(\S+)', line)
                    if match:
                        agent = match.group(1)
                        # Strip trailing comma and other punctuation
                        agent = agent.rstrip('.,;:')
                        bot_agent_counts[agent] += 1
    except Exception as e:
        print(f"Error processing {log_file}: {e}")
    
    return bot_agent_counts

def get_api_key_mappings_rr():
    """Query the RR database to get userName to apiKey mappings."""
    mappings = {}
    
    try:
        cmd = 'hgsql -h genome-centdb -e "select userName, apiKey from apiKeys" hgcentral'
        result = subprocess.run(cmd, shell=True, capture_output=True, text=True)
        
        if result.returncode == 0:
            lines = result.stdout.strip().split('\n')
            # Skip header line
            for line in lines[1:]:
                parts = line.strip().split('\t')
                if len(parts) == 2:
                    username, api_key = parts
                    mappings[api_key] = username
    except Exception as e:
        print(f"Error querying RR database: {e}")
    
    return mappings

def get_api_key_mappings_euro():
    """Query the Euro database to get userName to apiKey mappings."""
    mappings = {}
    
    try:
        cmd = 'ssh qateam@genome-euro "hgsql -e \'select userName, apiKey from apiKeys\' hgcentral"'
        result = subprocess.run(cmd, shell=True, capture_output=True, text=True)
        
        if result.returncode == 0:
            lines = result.stdout.strip().split('\n')
            # Skip header line
            for line in lines[1:]:
                parts = line.strip().split('\t')
                if len(parts) == 2:
                    username, api_key = parts
                    mappings[api_key] = username
    except Exception as e:
        print(f"Error querying Euro database: {e}")
    
    return mappings

def get_api_key_mappings_dev():
    """Query the Dev database to get userName to apiKey mappings."""
    mappings = {}
    
    try:
        cmd = 'hgsql -e "select userName, apiKey from apiKeys" hgcentraltest'
        result = subprocess.run(cmd, shell=True, capture_output=True, text=True)
        
        if result.returncode == 0:
            lines = result.stdout.strip().split('\n')
            # Skip header line
            for line in lines[1:]:
                parts = line.strip().split('\t')
                if len(parts) == 2:
                    username, api_key = parts
                    mappings[api_key] = username
    except Exception as e:
        print(f"Error querying Dev database: {e}")
    
    return mappings

def process_region(region_name, directories, get_mappings_func):
    """Process logs for a specific region and generate report."""
    # Collect all API key usage across all logs
    total_api_key_counts = defaultdict(int)
    total_bot_agent_counts = defaultdict(int)
    
    # Process each directory
    for base_dir in directories:
        log_files = get_last_n_error_logs(base_dir, n=5)
        
        for log_file in log_files:
            # Extract API keys
            api_key_counts = extract_api_keys_from_log(log_file)
            for api_key, count in api_key_counts.items():
                total_api_key_counts[api_key] += count
            
            # Extract bot agents
            bot_agent_counts = extract_bot_agents_from_log(log_file)
            for agent, count in bot_agent_counts.items():
                total_bot_agent_counts[agent] += count
    
    # Get username mappings
    mappings = get_mappings_func()
    
    # Print API Key Usage Report
    print("=" * 80)
    print(f"{region_name} API KEY USAGE REPORT")
    print("=" * 80)
    
    if total_api_key_counts:
        # Sort by usage count (descending)
        sorted_keys = sorted(total_api_key_counts.items(), key=lambda x: x[1], reverse=True)
        
        print(f"{'Count':<10} {'Username':<30} {'API Key':<44}")
        print("-" * 80)
        
        for api_key, count in sorted_keys:
            username = mappings.get(api_key, "UNKNOWN")
            print(f"{count:<10} {username:<30} {api_key:<44}")
        
        print("-" * 80)
        print(f"Total API calls: {sum(total_api_key_counts.values())}")
        print(f"Unique API keys: {len(total_api_key_counts)}")
        
        # Report unknown keys
        unknown_keys = [k for k in total_api_key_counts.keys() if k not in mappings]
        if unknown_keys:
            print(f"\nWarning: {len(unknown_keys)} API key(s) not found in database:")
            for key in unknown_keys:
                print(f"  {key} (used {total_api_key_counts[key]} times)")
    else:
        print("No API key usage found.")
    
    print("\n")
    
    # Print Bot Agent Report
    print("=" * 80)
    print(f"{region_name} BOT AGENT USAGE REPORT")
    print("=" * 80)
    
    if total_bot_agent_counts:
        # Sort by usage count (descending)
        sorted_agents = sorted(total_bot_agent_counts.items(), key=lambda x: x[1], reverse=True)
        
        print(f"{'Count':<10} {'Bot Agent':<70}")
        print("-" * 80)
        
        for agent, count in sorted_agents:
            print(f"{count:<10} {agent:<70}")
        
        print("-" * 80)
        print(f"Total bot agent matches: {sum(total_bot_agent_counts.values())}")
        print(f"Unique bot agents: {len(total_bot_agent_counts)}")
    else:
        print("No bot agent usage found.")
    
    print("\n")

def main():
    print("API CAPTCHA key and user agent exception usage over the last month.\n")
    
    # Determine which year to check
    log_year = get_current_year_and_month()
    
    # Process RR region
    rr_dirs = [
        f"/hive/data/inside/wwwstats/RR/{log_year}/hgw0",
        f"/hive/data/inside/wwwstats/RR/{log_year}/hgw1",
        f"/hive/data/inside/wwwstats/RR/{log_year}/hgw2"
    ]
    process_region("RR", rr_dirs, get_api_key_mappings_rr)
    
    # Process Euro region
    euro_dirs = [
        f"/hive/data/inside/wwwstats/euroNode/{log_year}"
    ]
    process_region("EURO", euro_dirs, get_api_key_mappings_euro)
    
    # Process Dev region
    dev_dirs = [
        f"/hive/data/inside/wwwstats/genome-test/{log_year}"
    ]
    process_region("DEV", dev_dirs, get_api_key_mappings_dev)

if __name__ == "__main__":
    main()
