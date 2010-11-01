// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_url_util.h"

#include "ppapi/c/dev/ppb_url_util_dev.h"
#include "ppapi/cpp/dev/url_util_dev.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(UrlUtil);

static bool ComponentEquals(const PP_UrlComponent_Dev& component,
                            int begin, int len) {
  return component.begin == begin && component.len == len;
}

bool TestUrlUtil::Init() {
  util_ = pp::UrlUtil_Dev::Get();
  return !!util_;
}

void TestUrlUtil::RunTest() {
  RUN_TEST(Canonicalize);
  RUN_TEST(ResolveRelative);
  RUN_TEST(IsSameSecurityOrigin);
  RUN_TEST(DocumentCanRequest);
  RUN_TEST(DocumentCanAccessDocument);
}

std::string TestUrlUtil::TestCanonicalize() {
  // Test no canonicalize output.
  pp::Var result = util_->Canonicalize("http://Google.com");
  ASSERT_TRUE(result.AsString() == "http://google.com/");

  // Test all the components
  PP_UrlComponents_Dev c;
  result = util_->Canonicalize(
      "http://me:pw@Google.com:1234/path?query#ref ",
      &c);
  ASSERT_TRUE(result.AsString() ==
  //          0         1         2         3         4
  //          0123456789012345678901234567890123456789012
              "http://me:pw@google.com:1234/path?query#ref");
  ASSERT_TRUE(ComponentEquals(c.scheme, 0, 4));
  ASSERT_TRUE(ComponentEquals(c.username, 7, 2));
  ASSERT_TRUE(ComponentEquals(c.password, 10, 2));
  ASSERT_TRUE(ComponentEquals(c.host, 13, 10));
  ASSERT_TRUE(ComponentEquals(c.port, 24, 4));
  ASSERT_TRUE(ComponentEquals(c.path, 28, 5));
  ASSERT_TRUE(ComponentEquals(c.query, 34, 5));
  ASSERT_TRUE(ComponentEquals(c.ref, 40, 3));

  // Test minimal components.
  result = util_->Canonicalize("http://google.com/", &c);
  //                                0         1
  //                                0123456789012345678
  ASSERT_TRUE(result.AsString() == "http://google.com/");
  ASSERT_TRUE(ComponentEquals(c.scheme, 0, 4));
  ASSERT_TRUE(ComponentEquals(c.username, 0, -1));
  ASSERT_TRUE(ComponentEquals(c.password, 0, -1));
  ASSERT_TRUE(ComponentEquals(c.host, 7, 10));
  ASSERT_TRUE(ComponentEquals(c.port, 0, -1));
  ASSERT_TRUE(ComponentEquals(c.path, 17, 1));
  ASSERT_TRUE(ComponentEquals(c.query, 0, -1));
  ASSERT_TRUE(ComponentEquals(c.ref, 0, -1));

  return std::string();
}

std::string TestUrlUtil::TestResolveRelative() {
  const int kTestCount = 6;
  struct TestCase {
    const char* base;
    const char* relative;
    const char* expected;  // NULL if
  } test_cases[kTestCount] = {
    {"http://google.com/", "foo", "http://google.com/foo"},
    {"http://google.com/foo", "/bar", "http://google.com/bar"},
    {"http://foo/", "http://bar", "http://bar/"},
    {"data:foo", "/bar", NULL},
    {"data:foo", "http://foo/", "http://foo/"},
    {"http://foo/", "", "http://foo/"},
  };

  for (int i = 0; i < kTestCount; i++) {
    pp::Var result = util_->ResolveRelativeToUrl(test_cases[i].base,
                                                 test_cases[i].relative);
    if (test_cases[i].expected == NULL) {
      ASSERT_TRUE(result.is_null());
    } else {
      ASSERT_TRUE(result.AsString() == test_cases[i].expected);
    }
  }
  return std::string();
}

std::string TestUrlUtil::TestIsSameSecurityOrigin() {
  ASSERT_FALSE(util_->IsSameSecurityOrigin("http://google.com/",
                                           "http://example.com/"));
  ASSERT_TRUE(util_->IsSameSecurityOrigin("http://google.com/foo",
                                          "http://google.com/bar"));
  return std::string();
}

std::string TestUrlUtil::TestDocumentCanRequest() {
  // This is hard to test, but we can at least verify we can't request
  // some random domain.
  ASSERT_FALSE(util_->DocumentCanRequest(*instance_, "http://evil.com/"));
  return std::string();
}

std::string TestUrlUtil::TestDocumentCanAccessDocument() {
  // This is hard to test, but we can at least verify we can access ourselves.
  ASSERT_TRUE(util_->DocumentCanAccessDocument(*instance_, *instance_));
  return std::string();
}

