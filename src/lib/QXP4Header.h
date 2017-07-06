/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libqxp project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef QXP4HEADER_H_INCLUDED
#define QXP4HEADER_H_INCLUDED

#include "libqxp_utils.h"
#include "QXPHeader.h"

namespace libqxp
{

class QXP4Header : public QXPHeader
{
public:
  QXP4Header();

  bool load(const std::shared_ptr<librevenge::RVNGInputStream> &input) override;

  uint16_t masterPagesCount() const;
  uint16_t seed() const;
  uint16_t increment() const;

private:
  uint16_t m_masterPagesCount;
  uint16_t m_seed;
  uint16_t m_increment;
};

}

#endif // QXP4HEADER_H_INCLUDED

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
