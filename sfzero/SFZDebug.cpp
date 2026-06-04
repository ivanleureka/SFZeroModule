/*************************************************************************************
 * Original code copyright (C) 2012 Steve Folta
 * Converted to Juce module (C) 2016 Leo Olivers
 * Forked from https://github.com/stevefolta/SFZero
 * For license info please see the LICENSE file distributed with this source code
 *************************************************************************************/
#include "SFZDebug.h"
#include <stdarg.h>

#ifdef JUCE_DEBUG

// Debug-only printf shim. The C-style varargs (C26826) and the array->pointer
// decay into vsnprintf (C26485) are intrinsic to a printf-style logger and
// cannot be expressed with typed args / std::span without changing the public
// signature used across the module; suppressed locally. The body only calls
// va_start / vsnprintf / va_end, none of which throw -> noexcept.
#pragma warning(suppress : 26826)
void sfzero::dbgprintf(const char *msg, ...) noexcept
{
  va_list args;

  va_start(args, msg);

  char output[256];
#pragma warning(suppress : 26485)
  vsnprintf(output, 256, msg, args);

  va_end(args);
}

#endif // JUCE_DEBUG
