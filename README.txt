To create a password encrypted zip file in python.
And the zip file is able to extract in WINDOWS.

This is a simple Minizip wrapper of python.
(http://www.winimage.com/zLibDll/minizip.html)
This software uses zlib.
License: zlib/libpng License.

install zlib

    linux:
    $ sudo apt-get install zlib
    mac:
    $ sudo port install zlib

install pyminizip

    $ python ./setup.py install

----------------------------------------------------------------------------

Provides just one function.
==============================

pyminizip.compress("/srcfile/path.txt", "/distfile/path.zip", "password", int(compress_level))

  Args:
  1. src file path (string)
  2. dst file path (string)
  3. password (string) or None (to create no-password zip)
  4. compress_level(int) between 1 to 9, 1 (more fast) <---> 9 (more compress) or 0 (default)

  Return value:
  - always returns None

==============================
