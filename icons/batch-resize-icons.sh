#!/bin/bash
sips -z 144 144 source.png --out Icon-72\@2x.png
sips -z 72 72 source.png --out Icon-72.png
sips -z 50 50 source.png --out Icon-Small-50.png
sips -z 100 100 source.png --out Icon-Small-50\@2x.png
sips -z 29 29 source.png --out Icon-Small.png
sips -z 58 58 source.png --out Icon-Small\@2x.png
sips -z 57 57 source.png --out Icon.png
sips -z 114 114 source.png --out Icon\@2x.png
sips -z 512 512 source.png --out iTunesArtwork.png

