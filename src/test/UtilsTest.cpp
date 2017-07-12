/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libqxp project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <memory>

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <librevenge-stream/librevenge-stream.h>

#include "libqxp_utils.h"

namespace test
{

using libqxp::readFloat16;

using librevenge::RVNGStringStream;
using std::make_shared;

class UtilsTest : public CPPUNIT_NS::TestFixture
{
public:
  virtual void setUp() override;
  virtual void tearDown() override;

private:
  CPPUNIT_TEST_SUITE(UtilsTest);
  CPPUNIT_TEST(testFloat16);
  CPPUNIT_TEST_SUITE_END();

private:
  void testFloat16();
};

void UtilsTest::setUp()
{
}

void UtilsTest::tearDown()
{
}

void UtilsTest::testFloat16()
{
  uint8_t data[] =
  {
    0x0, 0x0,
    0xff, 0xff,
    0x88, 0xc6,
    0xc6, 0x88
  };
  auto stream = make_shared<RVNGStringStream>(data, sizeof(data));

  CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, readFloat16(stream), 0.001);
  CPPUNIT_ASSERT_DOUBLES_EQUAL(1.0, readFloat16(stream), 0.001);
  CPPUNIT_ASSERT_DOUBLES_EQUAL(0.776, readFloat16(stream, false), 0.001);
  CPPUNIT_ASSERT_DOUBLES_EQUAL(0.776, readFloat16(stream, true), 0.001);
}

CPPUNIT_TEST_SUITE_REGISTRATION(UtilsTest);

}

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
