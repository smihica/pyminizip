# -*- coding: utf-8 -*-
import sys
import os
from setuptools import setup, Extension

SOURCES = ['src/py_minizip.c', 'src/zip.c', 'src/py_miniunz.c', 'src/unzip.c', 'src/ioapi.c',
           'zlib123/adler32.c', 'zlib123/compress.c', 'zlib123/crc32.c', 'zlib123/deflate.c', 
           'zlib123/gzio.c', 'zlib123/infback.c', 'zlib123/inffast.c', 'zlib123/inflate.c', 
           'zlib123/inftrees.c', 'zlib123/trees.c', 'zlib123/uncompr.c', 'zlib123/zutil.c']

if 'win32' in sys.platform:
    SOURCES.append('src/iowin32.c')

def read(fname):
    return open(os.path.join(os.path.dirname(__file__), fname)).read()

setup(
    name = 'pyminizip',
    version = '0.2.3',
    description = 'A minizip wrapper - To create a password encrypted zip file in python.',
    author='Shin Aoyama',
    author_email = "smihica@gmail.com",
    url = "https://github.com/smihica/pyminizip",
    download_url = "",
    keywords = ["zip", "file", "compress", "password", "encryption"],
    classifiers = [
        "Programming Language :: Python",
        "Programming Language :: Python :: 2",
        "Development Status :: 3 - Alpha",
        "Environment :: Other Environment",
        "Intended Audience :: Developers",
        "License :: OSI Approved :: zlib/libpng License",
        "Operating System :: OS Independent",
        "Topic :: System :: Archiving :: Compression",
        "Topic :: Software Development :: Libraries :: Python Modules",
        ],
    ext_modules=[
        Extension(name="pyminizip",
                  sources=SOURCES,
                  include_dirs=['src','zlib123'],
                  )
        ],
    long_description = read('README.txt'),
)
