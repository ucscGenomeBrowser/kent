#!/usr/bin/env python3
"""
Generate allTips.html and tipOfDay.html from a raw tips file.
Tracks used tips and cycles through them randomly without repetition.
"""
import os
import sys
import hashlib
import random
import argparse
from pathlib import Path

# Output paths
OUTPUT_DIR = "/usr/local/apache/htdocs"
ALL_TIPS_FILE = os.path.join(OUTPUT_DIR, "allTips.html")
TIP_OF_DAY_FILE = os.path.join(OUTPUT_DIR, "tipOfDay.html")

# Tracking directory
USER = os.environ.get('USER', 'default')
TRACKING_DIR = f"/cluster/home/{USER}/"
TIPS_DONE_FILE = os.path.join(TRACKING_DIR, "tipsDone.txt")

# HTML header
HEADER = """<!DOCTYPE html>
<!--#set var="TITLE" value="All Genome Browser Tips" -->
<!--#set var="ROOT" value="." -->
<!-- Relative paths to support mirror sites with non-standard GB docs install -->
<!--#include virtual="$ROOT/inc/gbPageStart.html" -->
"""

# HTML footer
FOOTER = """<!--#include virtual="$ROOT/inc/gbPageEnd.html" -->
"""

# Tip box template
TIP_TEMPLATE = """<div style="background-color: #f0f8ff; border-left: 4px solid #4a90e2; padding: 12px 15px; margin-top: 15px; margin-left: auto; margin-right: auto; max-width: 70%; border-radius: 4px; display: flex; align-items: center; gap: 12px;">
  <img src="/images/didYouKnow.png" alt="Did you know?" style="height: 50px; width: auto; flex-shrink: 0;">
  <p style="margin: 0; font-size: 14px; line-height: 1.5; color: #333;">
  {line}
  </p>
</div>
"""


def log(message, silent=False):
    """Print message unless in silent mode"""
    if not silent:
        print(message)


def hash_line(line):
    """Generate a hash for a tip line"""
    return hashlib.md5(line.encode('utf-8')).hexdigest()


def ensure_tracking_dir():
    """Create tracking directory if it doesn't exist"""
    Path(TRACKING_DIR).mkdir(parents=True, exist_ok=True)


def load_used_tips():
    """Load the set of already used tip hashes"""
    if not os.path.exists(TIPS_DONE_FILE):
        return set()
    
    with open(TIPS_DONE_FILE, 'r', encoding='utf-8') as f:
        return set(line.strip() for line in f if line.strip())


def save_used_tip(tip_hash):
    """Append a tip hash to the used tips file"""
    with open(TIPS_DONE_FILE, 'a', encoding='utf-8') as f:
        f.write(f"{tip_hash}\n")


def reset_used_tips():
    """Clear the used tips file to start a new cycle"""
    if os.path.exists(TIPS_DONE_FILE):
        os.remove(TIPS_DONE_FILE)


def read_tips(input_file):
    """Read and return non-empty tips from input file"""
    if not os.path.exists(input_file):
        raise FileNotFoundError(f"Input file not found: {input_file}")
    
    with open(input_file, 'r', encoding='utf-8') as f:
        lines = f.readlines()
    
    # Return non-empty lines with trailing newlines removed
    return [line.rstrip('\n') for line in lines if line.strip()]


def select_tip_of_day(tips, used_hashes):
    """Select a random unused tip, or reset and start over if all used"""
    # Create mapping of hash to tip
    tip_map = {hash_line(tip): tip for tip in tips}
    
    # Find unused tips
    unused_hashes = set(tip_map.keys()) - used_hashes
    
    # If all tips have been used, reset and start over
    if not unused_hashes:
        reset_used_tips()
        unused_hashes = set(tip_map.keys())
    
    # Select random unused tip
    selected_hash = random.choice(list(unused_hashes))
    return selected_hash, tip_map[selected_hash]


def generate_tip_of_day(tip_text):
    """Generate tipOfDay.html with just the raw tip text"""
    with open(TIP_OF_DAY_FILE, 'w', encoding='utf-8') as f:
        f.write(tip_text)


def generate_all_tips(tips):
    """Generate allTips.html with all tips (includes header/footer)"""
    with open(ALL_TIPS_FILE, 'w', encoding='utf-8') as f:
        f.write(HEADER)
        
        for tip in tips:
            tip_html = TIP_TEMPLATE.format(line=tip)
            f.write(tip_html)
            f.write('\n')
        
        f.write(FOOTER)


def main():
    parser = argparse.ArgumentParser(
        description='Generate daily tip and all tips pages from raw tips file'
    )
    parser.add_argument(
        'input_file',
        help='Path to the raw tips input file'
    )
    parser.add_argument(
        '-s', '--silent',
        action='store_true',
        help='Silent mode - only output errors'
    )
    
    args = parser.parse_args()
    
    try:
        # Ensure tracking directory exists
        ensure_tracking_dir()
        
        # Read all tips
        tips = read_tips(args.input_file)
        log(f"Loaded {len(tips)} tips from {args.input_file}", args.silent)
        
        if not tips:
            raise ValueError("No tips found in input file")
        
        # Load used tips
        used_hashes = load_used_tips()
        log(f"Found {len(used_hashes)} previously used tips", args.silent)
        
        # Select tip of the day
        selected_hash, selected_tip = select_tip_of_day(tips, used_hashes)
        log(f"Selected tip: {selected_tip[:50]}...", args.silent)
        
        # Generate tip of day file (just raw text)
        generate_tip_of_day(selected_tip)
        log(f"Generated {TIP_OF_DAY_FILE}", args.silent)
        
        # Mark tip as used
        save_used_tip(selected_hash)
        
        # Generate all tips file (with header/footer)
        generate_all_tips(tips)
        log(f"Generated {ALL_TIPS_FILE}", args.silent)
        
        # Check if we've completed a cycle
        if len(used_hashes) + 1 >= len(tips):
            log("Completed full cycle of tips - will reset on next run", args.silent)
        
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
