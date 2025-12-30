#pragma once
#include <backend/def.h>
#include <type_traits>


r2_begin_

#define v_count_of(_v) std::extent< decltype( _v ) >::value

r2_end_