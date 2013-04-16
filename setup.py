# -*- coding: utf-8 -*-
from distutils.core import setup, Extension

setup(
    name = 'pyminizip',
    version = '0.0.1',
    description = 'A minizip wrapper: to create a password encrypted zip file in python.',
    author='Shin Aoyama',
    author_email = "smihica@gmail.com",
    url = "https://github.com/smihica/pyminizip",
    download_url = "",
    keywords = ["zip", "file", "compress", "password", "encryption"],
    classifiers = [
        "Programming Language :: Python",
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
                  sources = [
                'src/py_minizip.c',
                'src/zip.c',
                'src/ioapi.c',
                ],
                  include_dirs=['src'],
                  libraries=['z']
                  )
        ],
    long_description = """\
A minizip wrapper: to create a password encrypted zip file in python.
""",
    )
