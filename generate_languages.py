#!/usr/bin/python

import os
import sys
import plistlib
import generate_en

def validate(en, language_name, language):
    if language_name == 'aliases':
        return False

    missing = False
    for key in en.keys():
        if key not in language:
            sys.stderr.write('MISSING:' + language_name + ':' + key + '\n')
            missing = True
    return missing


def process(d, en, language_name, file_name):
    language = plistlib.readPlist(file_name)
    d[language_name] = language
    return validate(en, language_name, language)

script_path = os.path.dirname(__file__)
languages_path = os.path.join(script_path, 'res', 'languages')

generate_en.generate_en()
en = plistlib.readPlist(os.path.join(languages_path, 'en.plist'))

d = dict()
missing = False
for file_name in os.listdir(languages_path):
    if file_name.endswith('.plist'):
        language_name = file_name[:file_name.index('.plist')]
        if process(d, en, language_name, os.path.join(languages_path, file_name)):
            missing = True

plistlib.writePlist(d, os.path.join(script_path, 'res', 'languages.plist'))
