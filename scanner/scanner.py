#!/usr/bin/env python
# Copyright 2013 Ben Ockmore

# This file is part of WavePlot.

# WavePlot is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

# WavePlot is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with WavePlot. If not, see <http://www.gnu.org/licenses/>.

import subprocess
import base64
import urllib
import urllib2
import mutagen.flac
import mutagen.id3
import mutagen.oggvorbis
import os
import sys
import tempfile
import time
import datetime

MAX_HTTP_ATTEMPTS = 3

VERSION = u"BANNANA"
EDITOR_KEY = u""
SERVER = u"http://pi.ockmore.net:19048"

LAST_RUN = u"2013-03-08 12:35:00"
REMOTE_URL = u"https://raw.github.com/LordSputnik/waveplot-client"
UPDATE_BRANCH = u"alpha"

update_applied = False

def GetScriptConfigValue(script_str, key):
    find_str = key + " = u\""
    start = script_str.find(find_str)

    if start == -1:
        return None

    start += len(find_str)
    end = script_str.find("\"",start)

    return script_str[start:end]

def SetScriptConfigValue(script_str, key, value):
    find_str = key + " = u\""
    start = script_str.find(find_str)

    if start == -1:
        return None

    start += len(find_str)
    end = script_str.find("\"",start)

    return script_str[:start]+value+script_str[end:]


def AutoUpdate():
    remote_script = urllib2.urlopen(u"{}/{}/{}".format(REMOTE_URL,UPDATE_BRANCH,u"scanner/scanner.py")).read()

    remote_last_run = GetScriptConfigValue(remote_script,"LAST_RUN")

    if remote_last_run is None:
        return

    remote_last_run = datetime.datetime.strptime(remote_last_run,"%Y-%m-%d %H:%M:%S")

    local_last_run = datetime.datetime.strptime(LAST_RUN,"%Y-%m-%d %H:%M:%S")

    if local_last_run < remote_last_run:
        update_now = ""
        while (update_now != "y") and (update_now != "n"):
            update_now = raw_input("Update ready! ({}) Apply? (y/n) ".format(remote_last_run))

        if update_now == "y":
            print "Updating! Please wait..."
            update_applied = True

            remote_script = SetScriptConfigValue(remote_script,"EDITOR_KEY",EDITOR_KEY)

            if remote_script is None:
                return

            with open(__file__, "w") as local_script:
                local_script.write(remote_script)

    return

def WriteEditorKey():
    with open(__file__,"r+") as script_file:
        script_str = script_file.read()
        script_str = SetScriptConfigValue(script_str,"EDITOR_KEY",EDITOR_KEY)
        script_file.seek(0,0)
        script_file.write(script_str)

def WriteLastRun():
    with open(__file__,"r+") as script_file:
        LAST_RUN = datetime.datetime.utcnow().strftime("%Y-%m-%d %H:%M:%S")

        script_str = script_file.read()
        script_str = SetScriptConfigValue(script_str,"LAST_RUN",LAST_RUN)
        script_file.seek(0,0)
        script_file.write(script_str)

def FindExe():
    global exe_file

    # Get the directory containing the scanner script
    # Imager should be in the same directory.
    scanner_dir = os.path.realpath(os.path.dirname(__file__))

    for filename in os.listdir(scanner_dir):
        if filename == exe_file:
            exe_file = os.path.realpath(os.path.join(scanner_dir,filename))
            return

    print "Unable to locate WavePlotImager in script directory - assuming it's in PATH."
    return

def WavePlotPost(url, values):
    attempts = 0
    while attempts < MAX_HTTP_ATTEMPTS:
        post_data = urllib.urlencode(values)
        req = urllib2.Request(url, post_data)
        try:
            response = urllib2.urlopen(req)
        except urllib2.URLError:
            attempts += 1
            time.sleep(0.5)
        else:
            return response.read()

    return None

# --- Main Script --- #

exe_file = "WavePlotImager"
if sys.platform == "win32":
    exe_file += ".exe"

AutoUpdate()

FindExe()

print "Using executable: " + exe_file

if EDITOR_KEY == "":
    print ("\nYou can obtain an editor key by registering at {}/register. ".format(SERVER) + "Since this is the first time you've run the script on this computer, please enter your activation key below.\n")
    EDITOR_KEY = str(input("Activation Key: "))

    # This updates the stored script when the editor key is entered
    WriteEditorKey()

print "\nFinding files to scan...\n"

for directory, directories, filenames in os.walk(u"."):
    for filename in filenames:
        recording_id = ""
        release_id = ""
        track_num = ""
        disc_num = ""
        file_ext = os.path.splitext(filename)[1][1:]
        in_path = os.path.realpath(os.path.join(directory,filename))
        audio = mutagen.File(os.path.join(directory,filename),easy=True)
        if audio:
            if "musicbrainz_trackid" in audio:
                recording_id = audio["musicbrainz_trackid"][0]
            if "musicbrainz_albumid" in audio:
                release_id = audio["musicbrainz_albumid"][0]
            if "tracknumber" in audio:
                track_num = audio["tracknumber"][0].split('/')[0].strip()
            if "discnumber" in audio:
                disc_num = audio["discnumber"][0].split('/')[0].strip()

        if (recording_id != "") and (release_id != "") and (track_num != "") and (disc_num != ""):
            url = SERVER+"/check"

            values = {'recording' : recording_id,
                      'release' : release_id,
                      'track' : track_num,
                      'disc' : disc_num
                      }

            the_page = WavePlotPost(url,values)

            if the_page == "0":
                try:
                    in_path_enc = in_path.encode('UTF-8','strict')
                except UnicodeError:
                    print "Filename couldn't be encoded to UTF-8! You have a really strange collection!"
                    pass
                else:
                    print "File:" + in_path.encode('ascii','replace')

                    try:
                        output = subprocess.check_output([exe_file,in_path_enc,VERSION])
                    except subprocess.CalledProcessError:
                        print "Imager Error - Skipped File."
                    else:
                        output = output.partition("WAVEPLOT_START")[2]

                        image_data, sep, output = output.partition("WAVEPLOT_LARGE_THUMB")
                        if sep == "":
                          raise ValueError

                        large_thumbnail, sep, output = output.partition("WAVEPLOT_SMALL_THUMB")
                        if sep == "":
                          raise ValueError

                        small_thumbnail, sep, output = output.partition("WAVEPLOT_INFO")
                        if sep == "":
                          raise ValueError

                        info, sep, output = output.partition("WAVEPLOT_END")
                        if sep == "":
                          raise ValueError

                        image_data = base64.b64encode(image_data)

                        large_thumbnail = base64.b64encode(large_thumbnail)

                        small_thumbnail = base64.b64encode(small_thumbnail)

                        length, trimmed, sourcetype, num_channels = info.split("|")

                        url = SERVER+'/submit'

                        values = {'recording' : recording_id,
                                  'release' : release_id,
                                  'track' : track_num,
                                  'disc' : disc_num,
                                  'image' : image_data,
                                  'large_thumb' : large_thumbnail,
                                  'small thumb' : small_thumbnail,
                                  'editor' : EDITOR_KEY,
                                  'length' : length,
                                  'trimmed' : trimmed,
                                  'source' : sourcetype,
                                  'num_channels': num_channels,
                                  'version' : VERSION }

                        the_page = WavePlotPost(url,values)

                        print the_page

if update_applied:
    WriteLastRun()
