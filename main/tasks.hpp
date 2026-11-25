#pragma once

#include "core/settings.hpp"

// settings: what this boot runs with (loaded, or the factory defaults).
void start_tasks(const core::Settings& settings);
void log_stack_high_water();
