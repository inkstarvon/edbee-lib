/**
 * Copyright 2011-2013 - Reliable Bits Software by Blommers IT. All Rights Reserved.
 * Author Rick Blommers
 */

#pragma once

#include "edbee/models/textchange.h"

namespace edbee {

/// This is an abstract class for ranged text-changes
/// This are changes (text changes and line changes) that span a range in an array.
/// These ranges share a common alogrithm for performing merges, detecting overlaps etc.
class AbstractRangedTextChange : public TextChange
{
public:
    virtual ~AbstractRangedTextChange();

    /// this method should return the offset of the change
    virtual int offset() const = 0;

    /// this method should return the old length of the change
    virtual int oldLength() const = 0;

    /// this method should return the new length of the change
    virtual int newLength() const = 0;

    bool isOverlappedBy( AbstractRangedTextChange* secondChange );
    bool isTouchedBy( AbstractRangedTextChange* secondChange );

};


} // edbee