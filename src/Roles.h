#pragma once
#include <Qt>

namespace Roles {
enum : int {
    Node = Qt::UserRole + 1, // FileNode* as void*
    Fraction,                // double 0..1 used by PercentBarDelegate
    Depth,                   // int, used to colour percentage bars
    Sort,                    // raw value for sorting proxies
    ExtId                    // int index into ExtensionTable
};
}
