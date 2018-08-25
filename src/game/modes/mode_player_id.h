#pragma once

struct mode_player_id {
	using id_value_type = unsigned;
	// GEN INTROSPECTOR struct mode_player_id
	id_value_type value = 0u;
	// END GEN INTROSPECTOR

	static auto dead() {
		return mode_player_id(0);
	}

	static auto first() {
		return mode_player_id(1);
	}

	mode_player_id() = default;
	explicit mode_player_id(const id_value_type b) : value(b) {}
	
	mode_player_id& operator=(const mode_player_id& b) = default;

	bool operator==(const mode_player_id& b) const {
		return value == b.value;
	}

	bool operator!=(const mode_player_id& b) const {
		return value != b.value;
	}

	auto& operator++() {
		++value;
		return *this;
	}

	operator id_value_type() const {
		return value;
	}

	bool is_set() const {
		return *this != mode_player_id();
	}

	void unset() {
		*this = {};
	}
};

namespace std {
	template <>
	struct hash<mode_player_id> {
		std::size_t operator()(const mode_player_id v) const {
			return hash<mode_player_id::id_value_type>()(v.value);
		}
	};
}
