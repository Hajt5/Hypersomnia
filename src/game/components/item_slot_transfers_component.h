#pragma once
#include "game/container_sizes.h"
#include "augs/misc/constant_size_vector.h"

#include "game/transcendental/entity_id.h"
#include "game/detail/inventory/inventory_slot_id.h"

#include "augs/math/physics_structs.h"
#include "game/transcendental/entity_handle_declaration.h"
#include "augs/misc/timing/stepped_timing.h"
#include "augs/pad_bytes.h"

struct item_slot_mounting_operation {
	// GEN INTROSPECTOR struct item_slot_mounting_operation
	entity_id current_item;
	inventory_slot_id intented_mounting_slot;
	// END GEN INTROSPECTOR
};

namespace components {
	struct item_slot_transfers {
		// GEN INTROSPECTOR struct components::item_slot_transfers
		augs::stepped_cooldown pickup_timeout = augs::stepped_cooldown(200);

		augs::constant_size_vector<entity_id, ONLY_PICK_THESE_ITEMS_COUNT> only_pick_these_items;
		bool pick_all_touched_items_if_list_to_pick_empty = true;
		bool picking_up_touching_items_enabled = false;
		pad_bytes<2> pad;
		// END GEN INTROSPECTOR

#if TODO_MOUNTING
		item_slot_mounting_operation mounting;
		
		static item_slot_mounting_operation find_suitable_montage_operation(const_entity_handle parent_container);
		void interrupt_mounting();
#endif
	};
}

namespace invariants {
	struct item_slot_transfers {
		// GEN INTROSPECTOR struct invariants::item_slot_transfers
		impulse_info standard_throw_impulse = { 7000.f, 0.f };
		impulse_info standard_drop_impulse = { 2000.f, 1.5f };
		float disable_collision_on_drop_for_ms = 300.f;
		// END GEN INTROSPECTOR
	};
}