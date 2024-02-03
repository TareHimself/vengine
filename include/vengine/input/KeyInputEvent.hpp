﻿#pragma once
#include "InputEvent.hpp"
#include "vengine/containers/String.hpp"

#include <SDL2/SDL_events.h>

namespace vengine::input {
class KeyInputEvent : public InputEvent {
  SDL_KeyboardEvent _key;
public:

  
  KeyInputEvent(const SDL_KeyboardEvent &event);

  String GetName() const override;
};
}