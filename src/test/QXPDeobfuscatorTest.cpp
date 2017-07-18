/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libqxp project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include "libqxp_utils.h"
#include "QXP33Deobfuscator.h"

namespace test
{

using libqxp::QXP33Deobfuscator;

class QXPDeobfuscatorTest : public CPPUNIT_NS::TestFixture
{
public:
  virtual void setUp() override;
  virtual void tearDown() override;

private:
  CPPUNIT_TEST_SUITE(QXPDeobfuscatorTest);
  CPPUNIT_TEST(test33Deobfuscation);
  CPPUNIT_TEST(test4Deobfuscation);
  CPPUNIT_TEST_SUITE_END();

private:
  void test33Deobfuscation();
  void test4Deobfuscation();
};

void QXPDeobfuscatorTest::setUp()
{
}

void QXPDeobfuscatorTest::tearDown()
{
}

void QXPDeobfuscatorTest::test33Deobfuscation()
{
  QXP33Deobfuscator deobfuscator(0x337c, 0x3797);

  uint8_t type = 0x7f;
  uint16_t content = 0x337c;
  CPPUNIT_ASSERT_EQUAL(uint8_t(3), deobfuscator.deobfuscate(type));
  CPPUNIT_ASSERT_EQUAL(uint16_t(0), deobfuscator.deobfuscate(content));
  deobfuscator.next();

  type = 0x10;
  content = 0x6b03;
  CPPUNIT_ASSERT_EQUAL(uint8_t(3), deobfuscator.deobfuscate(type));
  CPPUNIT_ASSERT_EQUAL(uint16_t(0x10), deobfuscator.deobfuscate(content));
  deobfuscator.next();

  type = 0xa7;
  content = 0xa2aa;
  CPPUNIT_ASSERT_EQUAL(uint8_t(13), deobfuscator.deobfuscate(type));
  CPPUNIT_ASSERT_EQUAL(uint16_t(0), deobfuscator.deobfuscate(content));
  deobfuscator.next();

  type = 0x42;
  content = 0xda41;
  CPPUNIT_ASSERT_EQUAL(uint8_t(3), deobfuscator.deobfuscate(type));
  CPPUNIT_ASSERT_EQUAL(uint16_t(0), deobfuscator.deobfuscate(content));
  deobfuscator.next();

  type = 0xdb;
  content = 0x11ca;
  CPPUNIT_ASSERT_EQUAL(uint8_t(3), deobfuscator.deobfuscate(type));
  CPPUNIT_ASSERT_EQUAL(uint16_t(0x12), deobfuscator.deobfuscate(content));
  deobfuscator.next();

  type = 0x6c;
  content = 0x496f;
  CPPUNIT_ASSERT_EQUAL(uint8_t(3), deobfuscator.deobfuscate(type));
  CPPUNIT_ASSERT_EQUAL(uint16_t(0), deobfuscator.deobfuscate(content));
  deobfuscator.next();
}

void QXPDeobfuscatorTest::test4Deobfuscation()
{
}

CPPUNIT_TEST_SUITE_REGISTRATION(QXPDeobfuscatorTest);

}

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
