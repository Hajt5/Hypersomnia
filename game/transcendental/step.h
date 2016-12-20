#pragma once
#include <array>
#include "augs/misc/delta.h"
#include "game/transcendental/cosmic_entropy.h"
#include "game/transcendental/entity_handle_declaration.h"
#include "game/detail/camera_cone.h"
#include "game/enums/render_layer.h"

#include "augs/templates/maybe_const.h"
#include "game/transcendental/data_living_one_step.h"
#include "game/transcendental/game_drawing_settings.h"

class cosmos;
struct immediate_hud;
struct aabb_highlighter;
struct data_living_one_step;

namespace augs {
	class renderer;
}

template<bool is_const>
class basic_cosmic_step {
protected:
	typedef maybe_const_ref_t<is_const, cosmos> cosmos_ref;
public:
	cosmos_ref cosm;
	
	basic_cosmic_step(cosmos_ref cosm) : cosm(cosm) {}
	
	cosmos_ref get_cosmos() const {
		return cosm;
	}

	operator basic_cosmic_step<true>() const {
		return{ cosm };
	}
};

typedef basic_cosmic_step<false> cosmic_step;
typedef basic_cosmic_step<true> const_cosmic_step;

template <bool is_const>
class basic_logic_step : public basic_cosmic_step<is_const> {
	friend class cosmos;
	typedef maybe_const_ref_t<is_const, data_living_one_step> data_living_one_step_ref;
public:
	data_living_one_step_ref transient;
	const cosmic_entropy& entropy;

	basic_logic_step(cosmos_ref cosm, const cosmic_entropy& entropy, data_living_one_step_ref transient)
		: basic_cosmic_step(cosm), entropy(entropy), transient(transient) {

	}

	augs::fixed_delta get_delta() const {
		return cosm.get_fixed_delta();
	}

	operator basic_logic_step<true>() const {
		return{ cosm, entropy, transient };
	}
};

typedef basic_logic_step<false> logic_step;
typedef basic_logic_step<true> const_logic_step;

class viewing_session;

class viewing_step : public const_cosmic_step {
public:
	viewing_step(const cosmos&, viewing_session&, const augs::variable_delta&, augs::renderer&, const camera_cone camera_state, const entity_id viewed_character);

	camera_cone camera;
	entity_id viewed_character;

	game_drawing_settings settings;
	viewing_session& session;
	augs::variable_delta delta;
	augs::renderer& renderer;

	augs::variable_delta get_delta() const;
	double get_interpolated_total_time_passed_in_seconds() const;

	vec2 get_screen_space(const vec2 pos) const;

	std::vector<const_entity_handle> visible_entities;
	std::array<std::vector<const_entity_handle>, render_layer::COUNT> visible_per_layer;
};
