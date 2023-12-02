#! /usr/bin/env python3

# STYML - an efficient C++ single-header STrictYaML parser and emitter
#
# The MIT License (MIT)
#
# Copyright(c) 2023, Damien Feneyrou <dfeneyrou@gmail.com>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files(the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions :
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

import os
import sys
import subprocess


def shell(command, check=False, stdout=subprocess.PIPE, stderr=subprocess.PIPE):
    return subprocess.run(command, stdout=stdout, stderr=stderr, shell=True, universal_newlines=True, check=check)


def main():
    # Get all the input for the task
    if [1 for arg in sys.argv if [1 for h in ["-h", "-help", "--help"] if h in arg]]:
        print("Syntax: %s [-h/--help] [fix]" % sys.argv[0])
        sys.exit(1)
    doFix = not not [1 for arg in sys.argv if arg == "fix"]

    sourceDir = shell("git rev-parse --show-toplevel", check=True).stdout.split('\n')[0]
    os.chdir(sourceDir)

    for cmd in ["check.py", "format.py%s" % ("" if doFix else " nofix"), "tidy.py%s" % (" fix" if doFix else "")]:
        print("--- Launching %s" % cmd)
        ret = shell('./ci/%s' % cmd, stdout=None)
        if ret.returncode != 0:
            print("ERROR: %s failed" % cmd)
            if ret.stderr:
                print(ret.stderr)
            sys.exit(ret.returncode)

    print("Ready for commit!")
    sys.exit(0)


# Bootstrap
if __name__ == "__main__":
    main()
