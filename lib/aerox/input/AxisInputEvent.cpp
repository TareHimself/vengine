﻿#include <aerox/input/AxisInputEvent.hpp>

namespace aerox::input {
AxisInputEvent::AxisInputEvent(const EInputAxis axis, const float &value) :
  _value(value), _axis(axis) {
}

float AxisInputEvent::GetValue() const {
  return _value;
}

std::string AxisInputEvent::GetName() const {
  switch (_axis) {
  case EInputAxis::MouseX:
    return "Mouse X";

  case EInputAxis::MouseY:
    return "Mouse Y";
  case EInputAxis::MouseScroll:
    return "Mouse Scroll";
  }

  return "Unknown Axis";
}
}
