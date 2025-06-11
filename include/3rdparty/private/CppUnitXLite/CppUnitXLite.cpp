// -*- mode:C++; c-basic-offset:2; indent-tabs-mode:nil -*-
/* 
Copyright (c) 2015 Glen S. Dayton

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE. 
*/

/** 
 *  One-time linker definitions.
 *  
 * Include this file in the same place where you define your 
 * main(). 
 */


#include <cmath>
#include <iostream>
#include <sstream>
#include <string>
#include <CppUnitXLite/CppUnitXLite.hpp>

Test::Test(const char *theTestName)
: testName(theTestName),
  nextTest(NULL)
{
  TestRegistry::addTest(this);
}


template<>
bool
Test::checkEqual<const char *>(const char *expected,
                 const char * actual,
                 TestResult &result,
                 const char *fileName,
                 unsigned lineNumber)
{
  expected = expected != NULL ? expected : "<null>";
  actual = actual != NULL ? actual : "<null>";
  bool successful = std::string(expected) == std::string(actual);
  if (!successful)
  {
    std::ostringstream message;
    message << "expected: " << expected << " but received: " << actual;
    fail(result, message.str().c_str(), fileName, lineNumber);
  }
  return successful;
}


void TestRegistry::add(Test *test)
{
  if (tests != NULL) test->next(tests);
  tests = test;
}


void TestRegistry::run(TestResult &result)
{
  for (Test *test = tests; test != NULL; test = test->next()) test->run(result);
  result.testsEnded();
}


