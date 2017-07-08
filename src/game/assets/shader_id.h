#pragma once
#include "game/container_sizes.h"

namespace assets {
	enum class shader_id {
		INVALID,
		DEFAULT_VERTEX,
		DEFAULT_FRAGMENT,
		DEFAULT_ILLUMINATED_VERTEX,
		DEFAULT_ILLUMINATED_FRAGMENT,
		LIGHT_VERTEX,
		LIGHT_FRAGMENT,
		PURE_COLOR_HIGHLIGHT_VERTEX,
		PURE_COLOR_HIGHLIGHT_FRAGMENT,
		CIRCULAR_BARS_VERTEX,
		CIRCULAR_BARS_FRAGMENT,
		EXPLODING_RING_VERTEX,
		EXPLODING_RING_FRAGMENT,
		FULLSCREEN_VERTEX,
		FULLSCREEN_FRAGMENT,
		SMOKE_VERTEX,
		SMOKE_FRAGMENT,
		ILLUMINATING_SMOKE_VERTEX,
		ILLUMINATING_SMOKE_FRAGMENT,
		SPECULAR_HIGHLIGHTS_VERTEX,
		SPECULAR_HIGHLIGHTS_FRAGMENT,
		REQUISITE_COUNT,
		COUNT = MAX_SHADER_COUNT + 1
	};
}