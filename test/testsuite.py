#!/usr/bin/env python3

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

import sys
import argparse
import glob
import os.path
import subprocess
import pprint


# ===================
# Constants
# ===================
RED, GREEN, CYAN, YELLOW, PURPLE, DWHITE, NORMAL = "\033[91m", "\033[92m", "\033[96m", "\033[93m", "\033[95m", "\033[37m", "\033[0m"


# ===================
# Helpers
# ===================

def addMultilinePrefix(s, linePrefix):
    return "\n".join([linePrefix+l for l in s.split('\n')])


# ===================
# Core
# ===================

def loadTestSuite(testdir):
    suite = []
    for filename in glob.glob("%s/*.yaml" % testdir):
        testName = os.path.basename(filename)[:-5]
        baseFilename = filename[:-5]

        # Get the input
        with open(baseFilename+".yaml", "r") as fh:
            testTextInput = fh.read()

        # Get the output
        try:
            with open(baseFilename+".txt", "r") as fh:
                pythonLikeExpectedOutput = fh.read()
                try:
                    testStructOutput = eval(pythonLikeExpectedOutput)
                except:
                    print(">>> ERROR: unable to evaluate the expected Python output test pattern:\n%s%s%s" %
                          (RED, pythonLikeExpectedOutput, NORMAL))
                    sys.exit(1)
        except Exception as e:
            print("> No expected output file found for %s, using empty one" % testName)
            testStructOutput = None

        # Get the error (optional)
        try:
            with open(baseFilename+".error", "r") as fh:
                testTextError = fh.read()
        except:
            testTextError = None

        # Store
        suite.append((testName, testTextInput, testStructOutput, testTextError))

    # Return the alphabetically sorted test suite
    suite.sort(key=lambda x: x[0])
    return suite


def main(argv):

    # Parse CLI parameters
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "encoder", help="STYML encoder program. stdin: StrictYAML, stdout: Python-struct evaluable")
    parser.add_argument("testdir", help="Path of the test files directory")
    parser.add_argument("-k", nargs='?', help="Pattern matching on the test name")
    parser.add_argument("-o", nargs='?', help="Optional repository to output Pattern matching on the test name")
    parser.add_argument('-f', action='store_true', help="Stops at first error")
    parser.add_argument("-u", action='store_true',
                        help="Do not check double conversion (back to YAML then back to Python-struct")
    parser.add_argument('-v', action='store_true', help="Increased verbosity when a test fails")
    args = parser.parse_args()

    # Load the tests
    testSuite = loadTestSuite(args.testdir)
    if not testSuite:
        print(">>> ERROR: no test found in directory '%s'" % args.testdir)
        sys.exit(1)

    # Display the header
    headerWidth = 61
    formatName = "STYML"
    print(YELLOW, end='')
    print("%s %s tests %s" % ("="*((headerWidth-len(formatName)-8)//2),
                              formatName,
                              "="*(headerWidth-len(formatName)-8-(headerWidth-len(formatName)-8)//2)))
    print(NORMAL, end='')

    # Helper
    def checkYamlToPyStructParsing(textInput, testStructOutput):
        # Get the output
        execResult = subprocess.run(args.encoder.split(), input=textInput, text=True, capture_output=True)
        # Get the status
        pyStructOutput = None
        errorMsg = ""
        if execResult.returncode == 0:
            if testTextError != None:
                errorMsg = "An error was expected but none seen"
            else:
                try:
                    pyStructOutput = eval(execResult.stdout)
                    if pyStructOutput != testStructOutput:
                        errorMsg = "Parsing result differs from the expected one"
                except:
                    pyStructOutput = None
                    errorMsg = "Unable to evaluate the execution output"
        else:
            if testTextError != None:
                if testTextError not in execResult.stdout:
                    errorMsg = "Expected parsing failure but with another error"
            else:
                errorMsg = "Unexpected failure of parsing"
        # Return
        return execResult, pyStructOutput, errorMsg

    # Execute the tests
    okCount = 0
    runCount = 0
    for testName, testTextInput, testStructOutput, testTextError in testSuite:
        # Filter
        if args.k != None and args.k not in testName:
            continue
        runCount += 1

        # Get the output
        execResult, pyStructOutput, errorMsg = checkYamlToPyStructParsing(testTextInput, testStructOutput)

        # Do the back translation
        # =======================
        backExecResult, backTextInput, idemExecResult = None, None, None
        if not args.u and not testTextError and not errorMsg:

            # Parse the YAML and dump into YAML
            backExecResult = subprocess.run(
                args.encoder.split() + ["-d"],
                input=testTextInput, text=True, capture_output=True)

            if backExecResult.returncode != 0:
                errorMsg = "Unexpected failure of the looped dump YAML -> YAML"
            else:
                execResult, pyStructOutput, errorMsg = checkYamlToPyStructParsing(
                    backExecResult.stdout, testStructOutput)
                if errorMsg:
                    errorMsg = "[LOOP] " + errorMsg

                else:
                    # Parse the back YAML and dump into YAML: strict idempotence expected (useful for "normalization")
                    idemExecResult = subprocess.run(
                        args.encoder.split() + ["-d"],
                        input=backExecResult.stdout, text=True, capture_output=True)

                    if backExecResult.returncode != 0:
                        errorMsg = "Unexpected failure of the idempotence (YAML ->) YAML => YAML"
                    elif idemExecResult.stdout != backExecResult.stdout:
                        errorMsg = "[IDEMPOTENCE] Results differ"

        # Display
        print("%s%-40s%s " % (YELLOW, testName, NORMAL), end='')
        if errorMsg:
            prefix = " "
            print("%sFAIL - %s%s" % (RED, errorMsg, NORMAL))
            if args.v:
                print(" Input:\n%s%s%s" % (RED, addMultilinePrefix(testTextInput, prefix), NORMAL))
                if backExecResult:
                    print(" Looped input:\n%s%s%s" % (RED, addMultilinePrefix(backExecResult.stdout, prefix), NORMAL))
                if execResult.stderr.strip():
                    print(" stderr:\n%s%s%s" % (RED, addMultilinePrefix(execResult.stderr.strip(), prefix), NORMAL))
                elif backExecResult and backExecResult.stderr.strip():
                    print(" looped stderr:\n%s%s%s" %
                          (RED, addMultilinePrefix(backExecResult.stderr.strip(), prefix), NORMAL))
                elif idemExecResult:
                    if idemExecResult.stderr.strip():
                        print(" double looped stderr:\n%s%s%s" %
                              (RED, addMultilinePrefix(idemExecResult.stderr.strip(), prefix), NORMAL))
                else:
                    print(" Expected:\n%s%s%s" % (RED, addMultilinePrefix(
                        pprint.pformat(testStructOutput), prefix), NORMAL))
                    if pyStructOutput != None:
                        print(" Output:\n%s%s%s" % (RED, addMultilinePrefix(
                            pprint.pformat(pyStructOutput), prefix), NORMAL))
                    elif execResult.stdout.strip():
                        print(" Output text:\n%s%s%s" % (RED, addMultilinePrefix(execResult.stdout, prefix), NORMAL))
                print()
            if args.f:
                print("Stopping at first error...")
                break

        else:
            print("%sOK%s" % (GREEN, NORMAL))
            okCount += 1

    # Overall status
    isGlobalOk = (okCount == runCount)
    print(GREEN if isGlobalOk else RED, end='')
    print("="*headerWidth)
    print("%-40s %s / %s => %s" % ("TOTAL", okCount, runCount, "OK" if isGlobalOk else "FAIL"))
    print("="*headerWidth)
    print(NORMAL, end='')
    sys.exit(0 if isGlobalOk else 1)


# Bootstrap
if __name__ == "__main__":
    main(sys.argv)
