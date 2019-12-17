// Copyright (c) 2009-2009 Satoshi Nakamoto
// Copyright (c) 2009-2014 Bitcoin developers
// Copyright (c) 2015-2019 PIVX developers
// Copyright (c) 2019-2019 Chronon developers

// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_SUPPORT_CLEANSE_H
#define BITCOIN_SUPPORT_CLEANSE_H

#include <stdlib.h>

void memory_cleanse(void *ptr, size_t len);

#endif // BITCOIN_SUPPORT_CLEANSE_H
