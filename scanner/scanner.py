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
import requests
import mutagen
import os
import sys
import time
import datetime
import json
import sqlite3 as db


VERSION = u"CITRUS"
LAST_RUN = u"2013-05-17 00:04"

_console_encoding = sys.stdout.encoding
def safeprint(str_to_print):
    """Encode unicode strings to filesystem encoding."""
    if isinstance(str_to_print, unicode):
        print str_to_print.encode(_console_encoding, 'replace')
    else:
        print unicode(str_to_print).encode(_console_encoding, "replace")  # Assume string is ascii.


def set_waveplot_paths():
    global config_path, data_path

    if sys.platform == "linux2":
        config_path = os.path.realpath(os.path.expanduser(u"~/.config"))
        data_path = os.path.realpath(os.path.expanduser(u"~/.local/share"))
    elif sys.platform == "win32":
        config_path = data_path = os.path.realpath(unicode(os.environ['APPDATA']))

    if (config_path == None) or (not os.path.exists(config_path)):
        config_path = os.path.realpath(os.path.dirname(__file__))
    else:
        config_path = os.path.join(config_path, u"waveplot")

    if (data_path == None) or (not os.path.exists(data_path)):
        data_path = os.path.realpath(os.path.dirname(__file__))
    else:
        data_path = os.path.join(data_path, u"waveplot")

    try:
        os.makedirs(config_path)
    except OSError:
        pass

    try:
        os.makedirs(data_path)
    except OSError:
        pass

    config_path = os.path.join(config_path, u"waveplot.conf")
    data_path = os.path.join(data_path, u"waveplot.db")


def check_and_read_config():
    global config
    if not os.path.exists(config_path):
        # First time running
        safeprint ("It looks like this is the first time you've run the script. Please follow the following instructions to get started!\n")
        safeprint ("1. Firstly, you'll need to enter your editor key. This is the number at the end of the link in your activation email. Please enter it below.")

        config['editor_key'] = raw_input("Activation Key: ")
        config['server'] = "http://pi.ockmore.net:19048"
        config['max_http_attempts'] = 3
        config['updates'] = { 'repo_url' : "https://raw.github.com/LordSputnik/waveplot-client",
                              'update_branch' : "alpha" }

        safeprint ("\nThat's it for now! WavePlot will now scan the current working directory, and let you know how it's getting on from time to time. Simple set-ups are so good, right?")

        with open(config_path, "wb") as config_file:
            json.dump(config, config_file, sort_keys = True, indent = 4, separators = (',', ': '))

    else:
        with open(config_path, "rb") as config_file:
            config = json.load(config_file)


def check_and_open_db():
    global db_conn
    db_conn = db.connect(data_path)
    db_conn.row_factory = db.Row

    c = db_conn.cursor()
    try:
        c.execute('SELECT file_path, waveplot_id, version, date FROM scanned_files LIMIT 1')
    except db.OperationalError:
        # Drop the table, create new one.
        c.execute("DROP TABLE IF EXISTS scanned_files")
        c.execute("CREATE TABLE scanned_files (file_path TEXT PRIMARY KEY, waveplot_id TEXT, version TEXT, date INTEGER)")
        db_conn.commit()

def GetLastRunValue(script_str):
    find_str = "LAST_RUN" + " = u\""
    start = script_str.find(find_str)

    if start == -1:
        return None

    start += len(find_str)
    end = script_str.find("\"", start)

    return script_str[start:end]


def auto_update():
    remote_script = requests.get(u"{}/{}/{}".format(config['updates']['repo_url'], config['updates']['update_branch'], u"scanner/scanner.py")).content

    remote_last_run = GetLastRunValue(remote_script)

    if remote_last_run is None:
        return

    remote_last_run = datetime.datetime.strptime(remote_last_run[0:16], "%Y-%m-%d %H:%M")

    local_last_run = datetime.datetime.strptime(LAST_RUN[0:16], "%Y-%m-%d %H:%M")

    if local_last_run < remote_last_run:
        update_now = ""
        while (update_now != "y") and (update_now != "n"):
            update_now = raw_input("Update ready! ({}) Apply? (y/n) ".format(remote_last_run))

        if update_now.lower() == "y":
            print "Updating! Please wait..."

            with open(__file__, "wb") as local_script:
                local_script.write(remote_script)

            os.execl(__file__)


def find_imager():
    global imager_exe

    # First, search script directory.
    scanner_dir = os.path.realpath(os.path.dirname(__file__))

    for filename in os.listdir(scanner_dir):
        if filename == imager_exe:
            imager_exe = os.path.realpath(os.path.join(scanner_dir, filename))
            return

    # Otherwise, assume it can just be called as "WaveplotImager" or "WaveplotImager.exe"
    safeprint("Unable to locate WavePlotImager in script directory - assuming it's in PATH.")
    return


def WavePlotPost(url, values):
    attempts = 0
    while attempts < config['max_http_attempts']:
        try:
            r = requests.post(url, data = values)
        except requests.ConnectionError:
            attempts += 1
            time.sleep(0.5)
        else:
            return r.content

    return None


# --- Main Script --- #
if __name__ == "__main__":

    config_path = None
    data_path = None

    config = {}

    db_conn = None

    dir_has_audio = False

    imager_exe = u"WavePlotImager"
    if sys.platform == "win32":
        imager_exe += u".exe"

    set_waveplot_paths()
    check_and_read_config()
    check_and_open_db()

    auto_update()

    find_imager()

    safeprint(u"Using executable: {}".format(imager_exe))

    safeprint(u"\nFinding files to scan...")

    c = db_conn.cursor()

    for directory, directories, filenames in os.walk(u"."):

        if dir_has_audio:
            safeprint("")

        safeprint(directory)
        dir_has_audio = False

        for filename in filenames:
            recording_id = ""
            release_id = ""
            track_num = ""
            disc_num = ""
            file_ext = os.path.splitext(filename)[1][1:]
            in_path = os.path.realpath(os.path.join(directory, filename))

            audio = mutagen.File(os.path.join(directory, filename), easy = True)
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
                sys.stdout.write(u"#".encode(sys.stdout.encoding, "replace"))
                sys.stdout.flush()
                dir_has_audio = True

                c.execute("SELECT * FROM scanned_files WHERE file_path=?", (in_path,))
                r = c.fetchone()

                if r != None:
                    if r['version'] == VERSION:
                        continue

                try:
                    in_path_enc = in_path.encode('UTF-8', 'strict')
                except UnicodeError:
                    safeprint("Filename couldn't be encoded to UTF-8! You have a really strange collection!")
                else:
                    safeprint(u"File:" + in_path)

                    try:
                        output = subprocess.check_output([imager_exe.encode("UTF-8"), in_path_enc, VERSION.encode("UTF-8")])
                    except subprocess.CalledProcessError:
                        safeprint("Imager Error - Skipped File.")
                    else:
                        output = output.partition("WAVEPLOT_START")[2]

                        image_data, sep, output = output.partition("WAVEPLOT_DR")
                        if sep == "":
                          raise ValueError

                        dr, sep, output = output.partition("WAVEPLOT_INFO")
                        if sep == "":
                          raise ValueError

                        info, sep, output = output.partition("WAVEPLOT_END")
                        if sep == "":
                          raise ValueError

                        image_data = base64.b64encode(image_data)

                        print dr

                        length, trimmed, sourcetype, num_channels = info.split("|")

                        url = config['server'] + '/submit'

                        values = {'recording' : recording_id,
                                  'release' : release_id,
                                  'track' : track_num,
                                  'dr_level' : dr,
                                  'disc' : disc_num,
                                  'image' : image_data,
                                  'editor' : config['editor_key'],
                                  'length' : length,
                                  'trimmed' : trimmed,
                                  'source' : sourcetype,
                                  'num_channels': num_channels,
                                  'version' : VERSION }

                        assigned_waveplot_id = WavePlotPost(url, values)

                        safeprint(assigned_waveplot_id)

                        c.execute("INSERT INTO scanned_files VALUES (?,?,?,?)", (in_path, assigned_waveplot_id, VERSION, datetime.datetime.utcnow().strftime("%Y-%m-%d %H:%M")))
                        db_conn.commit()


    db_conn.close()
