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
  *  Extra light framework for unit testing in C++.
  *
  *  Typical use is to define your tests as in the following example:
  *
  *  #include <CppUnitXLite/CppUnitXLite.cpp>
  *
  *  TEST(testclassname, testname)
  *  {
  *     CHECK(condition);
  *     CHECK_EQUAL(expected, actual);
  *     if (condition) FAIL("Arbitrary error message");
  *  }
  *
  *  TESTMAIN
  *
  *  Liberally adapted from Michael Feather's CppUnitLite (downloaded from
  *  http://www.objectmentor.com/resources/bin/CppUnitLite.zip )
  *
  *  Some of the changes from CppUnitLite include the following:
  *     *  Reversed order of arguments of TEST macro to match other frameworks
  *        such as googletest and unittest.
  *     *  Use C++ templates to reduce the need for lots of CHECK macros.
  *
  *   CHECK_EQUAL should work with any data type that has an operator==().
  *   CHECK_EQUAL instantiates to a templated method, which does have an
  *   explicit instantiate for comparing const char * style strings.
  */
#ifndef CPPUNITXLITE_H_
#define CPPUNITXLITE_H_
#include <cmath>
#include <iostream>
#include <sstream>
#include <string>

struct Failure;
class Test;
class TestRegistry;
class TestResult;


#define TEST(testGroup, testName)\
class testGroup##testName##Test : public Test \
{ public: testGroup##testName##Test () : Test (#testName "Test") {} \
  void run (TestResult& theResult); } \
testGroup##testName##Instance; \
void testGroup##testName##Test::run (TestResult& theResult)

#define CHECK(condition) check(theResult, (condition), #condition, __FILE__, __LINE__)

#define CHECK_EQUAL(expected,actual) checkEqual((expected), (actual), theResult, __FILE__, __LINE__)
#define CHECK_LE(expected,actual) checkLE((expected), (actual), theResult, __FILE__, __LINE__)
#define CHECK_LT(expected,actual) checkLT((expected), (actual), theResult, __FILE__, __LINE__)
#define CHECK_GT(expected,actual) checkGT((expected), (actual), theResult, __FILE__, __LINE__)
#define CHECK_GE(expected,actual) checkGE((expected), (actual), theResult, __FILE__, __LINE__)

#define CHECK_APPROX_EQUAL(expected,actual,threshold) checkApproxEqual((expected), (actual), (threshold), theResult, __FILE__, __LINE__)

#define FAIL(text) fail(theResult, (text), __FILE__, __LINE__)

#define TESTMAIN int main(int, char **) { TestResult tr; TestRegistry::runAll(tr); return 0; }


/**
 * Constructor of Test registers the test instance with the
 * TestRegistry during the static initialization phase.
 */
class TestRegistry
{
public:
  static void addTest(Test *test) { instance().add(test); }

  static void runAll(TestResult &result) { instance().run(result); }

private:
  static TestRegistry& instance()
  {
    static TestRegistry registry;
    return registry;
  }

  void add(Test *test);

  void run(TestResult &result);

  Test *tests;
};


/**
 *  Inherit from Test to define your own unit test.
 */
class Test
{
public:
  Test(const char *theTestName);

  /**
   * Override the run() method to execute your own tests.
   *
   * @param result Pass the result argument to the check methods
   *               to record results.
   */
  virtual void run(TestResult &result) = 0;

  inline void next(Test *test) { nextTest = test; }
  inline Test* next() const { return nextTest; }

protected:
  bool check(TestResult &result,
             bool condition,
             const char *conditionString,
             const char *fileName = __FILE__,
             unsigned int lineNumber = __LINE__)
  {
    if (!condition) fail(result, conditionString, fileName, lineNumber);
    return condition;
  }


  bool fail(TestResult &result,
            const char *conditionString,
            const char *fileName = __FILE__,
            unsigned int lineNumber = __LINE__);

  template<typename SubjectType>
  bool checkEqual(SubjectType expected,
                  SubjectType actual,
                  TestResult &result,
                  const char *fileName = __FILE__,
                  unsigned int lineNumber = __LINE__)
  {
    bool successful = expected == actual;
    if (!successful)
    {
      std::ostringstream message;
      message << "expected: " << expected << " but received: " << actual;
      fail(result, message.str().c_str(), fileName, lineNumber);
    }
    return successful;
  }

  template<typename SubjectType>
  bool checkLE(SubjectType expected,
               SubjectType actual,
               TestResult &result,
               const char *fileName = __FILE__,
               unsigned int lineNumber = __LINE__)
  {
    bool successful = expected <=  actual;
    if (!successful)
    {
      std::ostringstream message;
      message << "expected " << expected << " not <= actual " << actual;
      fail(result, message.str().c_str(), fileName, lineNumber);
    }
    return successful;
  }

  template<typename SubjectType>
  bool checkLT(SubjectType expected,
               SubjectType actual,
               TestResult &result,
               const char *fileName = __FILE__,
               unsigned int lineNumber = __LINE__)
  {
    bool successful = expected <  actual;
    if (!successful)
    {
      std::ostringstream message;
      message << "expected " << expected << " not < actual " << actual;
      fail(result, message.str().c_str(), fileName, lineNumber);
    }
    return successful;
  }

  template<typename SubjectType>
  bool checkGT(SubjectType expected,
               SubjectType actual,
               TestResult &result,
               const char *fileName = __FILE__,
               unsigned int lineNumber = __LINE__)
  {
    bool successful = expected >  actual;
    if (!successful)
    {
      std::ostringstream message;
      message << "expected " << expected << " not > actual " << actual;
      fail(result, message.str().c_str(), fileName, lineNumber);
    }
    return successful;
  }

  template<typename SubjectType>
  bool checkGE(SubjectType expected,
               SubjectType actual,
               TestResult &result,
               const char *fileName = __FILE__,
               unsigned int lineNumber = __LINE__)
  {
    bool successful = expected >=  actual;
    if (!successful)
    {
      std::ostringstream message;
      message << "expected " << expected << " not >= actual " << actual;
      fail(result, message.str().c_str(), fileName, lineNumber);
    }
    return successful;
  }

  template<typename SubjectType>
  bool checkApproxEqual(SubjectType expected,
                        SubjectType actual,
                        SubjectType threshold,
                        TestResult &result,
                        const char *fileName = __FILE__,
                        unsigned int lineNumber = __LINE__);

private:
  template<typename SubjectType>
  SubjectType abs(SubjectType x) { return x < 0 ? -x : x; }

  std::string testName;
  Test *nextTest;
};


/**
 *  Record everything knowable about the circumstance and
 *  location of a failure.
 */
struct Failure
{
  Failure(const std::string &theTestName,
          const std::string &theFileName,
          unsigned int theLineNumber,
          const std::string &theCondition)
  : message(theCondition),
    testName(theTestName),
    fileName(theFileName),
    lineNumber(theLineNumber)
  {}

  std::string message;
  std::string testName;
  std::string fileName;
  unsigned int lineNumber;
};


/**
 *  Collect all of the results of tests and checks.
 */
class TestResult
{
public:
  TestResult() : failureCount(0) {}

  virtual void addFailure(const Failure &failure)
  {
    std::cout << "Failure: \"" << failure.message << "\" @" << failure.fileName << ":" << failure.lineNumber << std::endl;
    ++failureCount;
  }

  virtual void testsEnded()
  {
    if (failureCount > 0)
    {
      std::cout << "There were " << failureCount << " failures" << std::endl;
    }
    else
    {
      std::cout << "There were no test failures" << std::endl;
    }
  }

private:
  int failureCount;
};


inline
bool
Test::fail(TestResult &result,
          const char *conditionString,
          const char *fileName,
          unsigned int lineNumber)
{
  result.addFailure(Failure(testName.c_str(), fileName, lineNumber, conditionString));
  return false;
}


template<typename SubjectType>
bool
Test::checkApproxEqual(SubjectType expected,
                       SubjectType actual,
                       SubjectType threshold,
                       TestResult &result,
                       const char *fileName,
                       unsigned int lineNumber)
{
  bool successful = abs(expected - actual) <= threshold;
  if (!successful)
  {
    std::ostringstream message;
    message << "expected: " << expected << " but received: " << actual;
    fail(result, message.str().c_str(), fileName, lineNumber);
  }
  return successful;
}
#endif // CPPUNITXLITE_H_

