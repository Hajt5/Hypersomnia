#pragma once
#include "game/enums/entity_name.h"
#include "game/entity_id.h"
#include "game/entity_handle_declaration.h"

namespace components {
	struct name {
		entity_name id;

		bool custom_nickname = false;
		std::wstring nickname;
	};
}

entity_id get_first_named_ancestor(const_entity_handle);
