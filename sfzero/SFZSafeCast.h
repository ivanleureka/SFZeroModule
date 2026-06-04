/*************************************************************************************
 * SFZSafeCast.h
 *
 * Standard-library narrowing-cast helper local to the SFZero module, used to
 * satisfy the MSVC C++ Core Guidelines checker (type.1 / C26472, C26467) WITHOUT
 * a dependency on the Guidelines Support Library (GSL).
 *
 * Prefer brace-initialisation `T{value}` for non-narrowing conversions; use
 * `sfzero::narrowCast<T>(value)` for an intentional narrowing conversion. The
 * single unavoidable static_cast is contained here behind one localised
 * suppression so call sites stay cast-free.
 *
 * Part of the SFZero module - see the LICENSE file distributed with this source.
 *************************************************************************************/
#ifndef SFZSAFECAST_H_INCLUDED
#define SFZSAFECAST_H_INCLUDED

#include <utility>

namespace sfzero
{

/** Explicit, intentional narrowing conversion (GSL-free equivalent of
    gsl::narrow_cast). Performs no runtime check - it documents intent only. */
template <typename To, typename From>
constexpr To narrowCast(From &&value) noexcept
{
#pragma warning(suppress : 26472)
  return static_cast<To>(std::forward<From>(value));
}

} // namespace sfzero

#endif // SFZSAFECAST_H_INCLUDED
