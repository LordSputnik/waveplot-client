WavePlot Client
===============

This repository contains two pieces of software.

The first is the WavePlot Imager. This is an executable file written in C++11,
which interprets the audio data in audio files and uses it to create the data
needed for a WavePlot image.

The second is an example of a scanning application, which recursively scans
directories for audio files, runs the WavePlot Imager and reads the file
metadata. It then makes a submission to the WavePlot website. All of this code
is contained in the "waveplot" python script.

Setting Up
----------

You'll need to get libav-codec, libav-format and libav-utils in order to compile
the Imager executable. Then you should be able to use the included Makefile to
compile and link the required source files.

You'll need to install Python 2.7 and the "mutagen" and "requests" Python
packages.

Running
-------

After building, the Imager executable should be placed in the ./imager directory
relative to the waveplot script. To execute, simply navigate to the folder
containing files you'd like to scan, then type:


```
<path_to_waveplot_installation>/waveplot
```

It'll find the executable, download any updates, and then find files to scan.
