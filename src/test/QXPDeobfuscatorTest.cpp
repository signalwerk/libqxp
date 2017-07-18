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
  QXP33Deobfuscator deobfuscator(0x1dee, 0x1eb5);

  uint8_t type = 0xed;
  uint16_t content = 0x1dee;
  CPPUNIT_ASSERT_EQUAL(uint8_t(3), deobfuscator.deobfuscate(type));
  CPPUNIT_ASSERT_EQUAL(uint16_t(0), deobfuscator.deobfuscate(content));
  deobfuscator.next();

  type = 0xa0;
  content = 0x3cb3;
  CPPUNIT_ASSERT_EQUAL(uint8_t(3), deobfuscator.deobfuscate(type));
  CPPUNIT_ASSERT_EQUAL(uint16_t(0x10), deobfuscator.deobfuscate(content));
  deobfuscator.next();

  type = 0x55;
  content = 0x5b58;
  CPPUNIT_ASSERT_EQUAL(uint8_t(13), deobfuscator.deobfuscate(type));
  CPPUNIT_ASSERT_EQUAL(uint16_t(0), deobfuscator.deobfuscate(content));
  deobfuscator.next();
}

void QXPDeobfuscatorTest::test4Deobfuscation()
{
}

CPPUNIT_TEST_SUITE_REGISTRATION(QXPDeobfuscatorTest);

}

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
