# -*- coding: utf-8 -*-
import sys
import os
from setuptools import setup, Extension

SOURCES = ['src/py_minizip.c', 'src/py_miniunz.c',
           'zlib-1.2.12/contrib/minizip/zip.c', 'zlib-1.2.12/contrib/minizip/unzip.c', 'zlib-1.2.12/contrib/minizip/ioapi.c',
           'zlib-1.2.12/adler32.c', 'zlib-1.2.12/compress.c', 'zlib-1.2.12/crc32.c', 'zlib-1.2.12/deflate.c',
           'zlib-1.2.12/infback.c', 'zlib-1.2.12/inffast.c', 'zlib-1.2.12/inflate.c',
           'zlib-1.2.12/inftrees.c', 'zlib-1.2.12/trees.c', 'zlib-1.2.12/uncompr.c', 'zlib-1.2.12/zutil.c']

if 'win32' in sys.platform:
    SOURCES.append('zlib-1.2.12/contrib/minizip/iowin32.c')

def read(fname):
    return open(os.path.join(os.path.dirname(__file__), fname)).read()

setup(
    name = 'pyminizip',
    version = '0.2.7',
    description = 'A minizip wrapper - To create a password encrypted zip file in python.',
    author='Shin Aoyama',
    author_email = "smihica@gmail.com",
    url = "https://github.com/smihica/pyminizip",
    download_url = "",
    keywords = ["zip", "file", "compress", "password", "encryption"],
    classifiers = [
        "Programming Language :: Python",
        "Programming Language :: Python :: 2",
        "Programming Language :: Python :: 3",
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
                  include_dirs=['src','zlib-1.2.12','zlib-1.2.12/contrib/minizip'],
                  )
        ],
    long_description = read('README.md'),
    long_description_content_type = 'text/markdown',
)
