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

VERSION = "BANNANA"
EDITOR_KEY = ""
SERVER = "http://pi.ockmore.net:19048"

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



# --- Main Script --- #

exe_file = "WavePlotImager"
if sys.platform == "win32":
    exe_file += ".exe"

FindExe()

print "Using executable: " + exe_file

if EDITOR_KEY == "":
    print ("\nYou can obtain an editor key by registering at {}/register. ".format(SERVER) + "Since this is the first time you've run the script on this computer, please enter your activation key below.\n")
    EDITOR_KEY = str(input("Activation Key: "))

    # This updates the stored script when the editor key is entered
    with open(__file__,"r+") as script_file:
        script_str = script_file.read()
        editor_key_pos = script_str.rfind("EDITOR_KEY = \"") + len("EDITOR_KEY = \"")
        script_str = script_str[:editor_key_pos] + EDITOR_KEY + script_str[editor_key_pos:]
        script_file.seek(0,0)
        script_file.write(script_str)

for directory, directories, filenames in os.walk("."):
    for filename in filenames:
        recording_id = ""
        release_id = ""
        track_num = ""
        disc_num = ""
        file_ext = os.path.splitext(filename)[1][1:]
        in_path = os.path.realpath(os.path.join(directory,filename))
        if file_ext == "mp3":
            try:
                audio = mutagen.id3.ID3(in_path)
            except mutagen.id3.ID3NoHeaderError:
                pass
            else:
                if "UFID:http://musicbrainz.org" in audio:
                    recording_id = str(audio["UFID:http://musicbrainz.org"].data)
                if "TXXX:MusicBrainz Album Id" in audio:
                    release_id = str(audio["TXXX:MusicBrainz Album Id"])
                if "TRCK" in audio:
                    track_num = str(audio["TRCK"]).split("/")[0].strip()
                if "TPOS" in audio:
                    disc_num = str(audio["TPOS"]).split("/")[0].strip()

        elif file_ext == "flac":
            try:
                audio = mutagen.flac.FLAC(in_path)
            except mutagen.flac.FLACNoHeaderError:
                pass
            else:
                if "MUSICBRAINZ_TRACKID" in audio:
                    recording_id = str(audio["MUSICBRAINZ_TRACKID"][0])
                if "MUSICBRAINZ_ALBUMID" in audio:
                    release_id = str(audio["MUSICBRAINZ_ALBUMID"][0])
                if "TRACKNUMBER" in audio:
                    track_num = str(audio["TRACKNUMBER"][0])
                if "DISCNUMBER" in audio:
                    disc_num = str(audio["DISCNUMBER"][0])

        elif file_ext == "ogg":
            try:
                audio = mutagen.oggvorbis.OggVorbis(in_path)
            except mutagen.oggvorbis.OggVorbisHeaderError:
                pass
            else:
                if "MUSICBRAINZ_TRACKID" in audio:
                    recording_id = str(audio["MUSICBRAINZ_TRACKID"][0])
                if "MUSICBRAINZ_ALBUMID" in audio:
                    release_id = str(audio["MUSICBRAINZ_ALBUMID"][0])
                if "TRACKNUMBER" in audio:
                    track_num = str(audio["TRACKNUMBER"][0])
                if "DISCNUMBER" in audio:
                    disc_num = str(audio["DISCNUMBER"][0])

        if (recording_id != "") and (release_id != "") and (track_num != "") and (disc_num != ""):
            output = subprocess.check_output([exe_file,in_path,VERSION])

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

            data = urllib.urlencode(values)
            req = urllib2.Request(url, data)
            response = urllib2.urlopen(req)
            the_page = response.read()

            print the_page
