// Small helpers for the Unreal side reading the plain C++ network (std::vector) in Unreal's idiom.
#pragma once

#include <algorithm>
#include <vector>

template <class T> inline int32 SimNum(const std::vector<T>& V) { return (int32)V.size(); }
template <class T> inline bool SimValid(const std::vector<T>& V, int32 I) { return I >= 0 && I < (int32)V.size(); }
