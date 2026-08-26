// SPDX-FileCopyrightText: Copyright DB InfraGO AG
// SPDX-License-Identifier: Apache-2.0

#ifndef AUX_H
#define AUX_H

#include <limits>
#include <type_traits>

template <class T, class U>
constexpr bool try_add_unsigned(T& value, U increment, T const max_value = std::numeric_limits<T>::max())
{
    static_assert(std::is_integral_v<T> && std::is_integral_v<U>);
    static_assert(std::is_unsigned_v<T> && std::is_unsigned_v<U>);
    static_assert(sizeof(U) <= sizeof(T));

    const T addend = static_cast<T>(increment);
    if (addend > max_value)
    {
        // The increment is already too large to be added to any value of type T without exceeding max_value.
        return false;
    }
    else if (value > (max_value - addend))
    {
        // Adding the increment would exceed max_value.
        return false;
    }
    else
    {
        // We're within bounds, so we can safely add the increment to value.
        value += addend;
        return true;
    }
}

#endif // AUX_H
