#pragma once

#include "SDK/Basic.hpp"

#define SDK_DISABLE_STATIC_ASSERTS

#ifdef SDK_DISABLE_STATIC_ASSERTS
#define SDK_STATIC_ASSERT_CONCAT_INNER(a, b) a##b
#define SDK_STATIC_ASSERT_CONCAT(a, b) SDK_STATIC_ASSERT_CONCAT_INNER(a, b)
#define SDK_STATIC_ASSERT(...) \
	typedef int SDK_STATIC_ASSERT_CONCAT(sdk_static_assert_disabled_, __COUNTER__)[1]
#define static_assert(...) SDK_STATIC_ASSERT(__VA_ARGS__)
#endif

#include "SDK.hpp"

#ifdef SDK_DISABLE_STATIC_ASSERTS
#undef static_assert
#undef SDK_STATIC_ASSERT
#undef SDK_STATIC_ASSERT_CONCAT
#undef SDK_STATIC_ASSERT_CONCAT_INNER
#endif
