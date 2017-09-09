#!/usr/bin/python

import os
import plistlib
import re

def process_escapes(s):
    return s.decode('string_escape').decode('UTF-8')

def process(s, file_name):
    s |= set(map(process_escapes, re.findall(r'localize\("(.*?)"\)', open(file_name, 'r').read())))

def generate_en():
    script_path = os.path.dirname(__file__)

    s = set()
    src_path = os.path.join(script_path, 'src')
    for file_name in os.listdir(src_path):
        if file_name.endswith('.m') or file_name.endswith('.c') or file_name.endswith('.cpp') or file_name.endswith('.h'):
            process(s, os.path.join(src_path, file_name))

    src_path = os.path.join(script_path, 'kernel', 'ios')
    for file_name in os.listdir(src_path):
        if file_name.endswith('.m') or file_name.endswith('.c') or file_name.endswith('.cpp') or file_name.endswith('.h'):
            process(s, os.path.join(src_path, file_name))

    d = dict()
    for string in s:
        d[string] = string

    languages_path = os.path.join(script_path, 'res', 'languages')
    if not os.path.exists(languages_path):
        os.makedirs(languages_path)
    plistlib.writePlist(d, os.path.join(languages_path, 'en.plist'))

if __name__ == '__main__':
    generate_en()
