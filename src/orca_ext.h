#pragma once

#include "util/lists.h"

#define oc_str8_list_for(_list, elt)                                           \
  oc_list_for(_list.list, elt, oc_str8_elt, listElt)

#define oc_str8_printf(s8) s8.len, s8.ptr