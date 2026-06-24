// Copyright 2015 Open Source Robotics Foundation, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Timing Implementation on DJGPP/FreeDOS Platforms
// Using DJGPP's uclock(), in microseconds (UCLOCKS_PER_SEC)
// Corresponding to the three function interfaces in time_unix.c:
// rcutils_system_time_now → wall clock (same as steady, FreeDOS has no RTC API)
// rcutils_steady_time_now → monotonic clock (uclock starts timing from power-on)
// rcutils_raw_steady_time_now → raw monotonic (same as steady)

#ifndef __DJGPP__
# error time_djgpp.c is only intended for DJGPP (FreeDOS/DOS) targets
#endif

#ifdef __cplusplus
extern "C"
{
#endif

#include <time.h>     // uclock(), UCLOCKS_PER_SEC, uclock_t

#include "rcutils/error_handling.h"
#include "rcutils/time.h"

extern unsigned long CLOCKS_PER_MICROSEC;
extern unsigned long long _86rdtsc(void);

static rcutils_ret_t
djgpp_time_now(rcutils_time_point_value_t * now)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(now, RCUTILS_RET_INVALID_ARGUMENT);

  uclock_t ticks = _86rdtsc();
  
  int64_t ns = (_86rdtsc() / CLOCKS_PER_MICROSEC) * 1000LL;

  *now = ns;
  return RCUTILS_RET_OK;
}

rcutils_ret_t
rcutils_system_time_now(rcutils_time_point_value_t * now)
{
  // FreeDOS does not have a simplified RTC nanosecond API; uclock is used instead.
  return djgpp_time_now(now);
}

rcutils_ret_t
rcutils_steady_time_now(rcutils_time_point_value_t * now)
{
  return djgpp_time_now(now);
}

rcutils_ret_t
rcutils_raw_steady_time_now(rcutils_time_point_value_t * now)
{
  return djgpp_time_now(now);
}

#ifdef __cplusplus
}
#endif
