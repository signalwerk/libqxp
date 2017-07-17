/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libqxp project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef QXPTEXTCOLLECTOR_H_INCLUDED
#define QXPTEXTCOLLECTOR_H_INCLUDED

#include "libqxp_utils.h"
#include "QXPCollector.h"

namespace libqxp
{

class QXPTextCollector : public QXPCollector
{
  // disable copying
  QXPTextCollector(const QXPTextCollector &other) = delete;
  QXPTextCollector &operator=(const QXPTextCollector &other) = delete;

public:
  QXPTextCollector();

private:
};

}

#endif // QXPTEXTCOLLECTOR_H_INCLUDED

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
