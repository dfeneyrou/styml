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
    # Get the defaults depending on the OS
    if sys.platform == "win32":
        defaultBuildType = "Release"
        defaultPythonCall = "python "
    else:
        defaultBuildType = ""
        defaultPythonCall = ""

    # Get all the input for the task
    if [1 for arg in sys.argv if [1 for h in ["-h", "-help", "--help"] if h in arg]]:
        print("Syntax: %s [options]" % sys.argv[0])
        print("  The expected location of the binaries is <git root>/<build folder>/bin/<type folder>/")
        print("  Options:")
        print("   -h / --help            This help message")
        print("   -b=<sub-build folder>  Name of the build folder. Default is ''")
        print("   -t=<type folder>       Name of the build type folder. Default is 'Release' on Windows, '' on Linux")
        print("   -ut                    Only the unit tests. Default is both unit tests and pattern-based test suite")
        print("   -ts                    Only the test suite. Default is both unit tests and pattern-based test suite")
        sys.exit(1)
    subBuildFolder = ([arg[3:] for arg in sys.argv if arg[:3] == "-b="] + [""])[0]
    buildType = ([arg[3:] for arg in sys.argv if arg[:3] == "-t="] + [defaultBuildType])[0]
    doUnitTests = not not [1 for arg in sys.argv if arg == "-ut"]
    doTestSuite = not not [1 for arg in sys.argv if arg == "-ts"]
    if not doUnitTests and not doTestSuite:
        doUnitTests, doTestSuite = True, True
    sourceDir = shell("git rev-parse --show-toplevel", check=True).stdout.split('\n')[0]
    testBinPath = os.path.join(sourceDir, "test", "")
    buildBinPath = os.path.join(sourceDir, "build", subBuildFolder, "bin", buildType, "")
    os.chdir(sourceDir)

    # Execute the unit tests
    if doUnitTests:
        ret = shell('%sstyml_unittest sanity benchmark' % buildBinPath, stdout=None)
        if ret.stderr:
            print("*** Error while executing the unit tests:\n%s" % ret.stderr)
        if ret.returncode != 0:
            sys.exit(ret.returncode)

    # Execute the test suite
    if doTestSuite:
        ret = shell('%s%stestsuite.py "%sstyml_encoder -" ./test/patterns -v' %
                    (defaultPythonCall, testBinPath, buildBinPath), stdout=None)
        if ret.stderr:
            print("*** Error while executing the test suite:\n%s" % ret.stderr)
        if ret.returncode != 0:
            sys.exit(ret.returncode)

    print("Tests are ok!")
    sys.exit(0)


# Bootstrap
if __name__ == "__main__":
    main()
