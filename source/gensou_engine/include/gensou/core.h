#pragma once

#include "core/core.h"
#include "core/uuid.h"
#include "core/log.h"
#include "core/system.h"
#include "core/runtime.h"
#include "core/input.h"
#include "core/event.h"
#include "core/time.h"
#include "core/misc.h"
#include "scene/game_instance.h"

#define EVENT(returnType, ...) gs::event<std::function<returnType(__VA_ARGS__)>>