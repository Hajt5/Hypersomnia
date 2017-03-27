#pragma once
#include "augs/window_framework/window.h"
#include "augs/misc/machine_entropy.h"

class game_window {
public:
	augs::window::glwindow window;

	vec2i get_screen_size() const;
	void swap_buffers();

	bool clear_window_inputs_once = true;

	decltype(augs::machine_entropy::local) collect_entropy(const bool enable_cursor_clipping);
};