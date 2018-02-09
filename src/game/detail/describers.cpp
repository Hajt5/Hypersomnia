#include "augs/log.h"

#include "describers.h"

#include "game/transcendental/entity_handle.h"

#include "game/detail/inventory/inventory_slot_handle.h"
#include "game/components/melee_component.h"
#include "game/components/gun_component.h"
#include "game/components/missile_component.h"
#include "game/components/container_component.h"
#include "game/components/sentience_component.h"
#include "game/components/item_component.h"
#include "game/components/catridge_component.h"
#include "game/detail/inventory/perform_transfer.h"
#include "game/detail/inventory/inventory_slot.h"
#include "game/detail/inventory/inventory_slot_id.h"
#include "game/transcendental/cosmos.h"

entity_name_type get_bbcoded_entity_name(const const_entity_handle maybe_overridden_by_nickname) {
	return maybe_overridden_by_nickname.get_name();
}

const entity_description_type& get_bbcoded_entity_description(const const_entity_handle handle) {
	return handle.get_description();
}

std::wstring get_bbcoded_entity_properties(const const_entity_handle id) {
	std::wostringstream result;

	const auto* const gun = id.find<components::gun>();
	const auto* const gun_def = id.find<invariants::gun>();
	const auto* const missile = id.find<invariants::missile>();
	const auto* const container = id.find<invariants::container>();

	const auto* const item_def = id.find<invariants::item>();
	const auto item = id.find<components::item>();

	if (item && item_def) {
		result << L"[color=vsblue]" << get_bbcoded_item_categories(item_def->categories_for_slot_compatibility) << L"[/color]\n";
		
		const auto total_occupied = format_space_units(calculate_space_occupied_with_children(id));
		const auto per_charge = format_space_units(item_def->space_occupied_per_charge);

		result << "Occupies: [color=vscyan]" << total_occupied << " [/color]";
		
		if (item->get_charges() > 1) {
			result << "[color=vsdarkgray](" << per_charge << L" each)[/color]";
		}
		else if (container && total_occupied != per_charge) {
			result << "[color=vsdarkgray](" << per_charge << L" if empty)[/color]";
		}

		result << L"\n";
	}

	if (gun) {
		if (const auto flavour = gun_def->magic_missile_flavour) {
			result << typesafe_sprintf(L"Muzzle velocity: [color=vscyan]%x[/color]\nAmplification multiplier: [color=vscyan]%x[/color]\n", 
				(gun_def->muzzle_velocity.first + gun_def->muzzle_velocity.second) / 2, gun_def->damage_multiplier);
		}
		else {
			result << typesafe_sprintf(L"Muzzle velocity: [color=vscyan]%x[/color]\nDamage multiplier: [color=vscyan]%x[/color]\n",
				(gun_def->muzzle_velocity.first + gun_def->muzzle_velocity.second) / 2, gun_def->damage_multiplier);
		}
	}

	if (missile) {
		if (missile->damage_amount > 0) {
			result << L"Base missile: [color=vscyan]" << missile->damage_amount << L"[/color]\n";
		}
		else if (missile->damage_amount < 0) {
			result << L"Restores health: [color=vscyan]" << -missile->damage_amount << L"[/color]\n";
		}

		if (missile->constrain_lifetime) {
			result << L"Max lifetime: [color=vscyan]" << missile->max_lifetime_ms << L" ms[/color]\n";
		}
	}

	//if (melee) {
	//	result << L"Swing duration: [color=vscyan]" << melee->swings[0].duration_ms << L" ms[/color]\n";
	//	result << L"Swing cooldown: [color=vscyan]" << melee->swings[0].cooldown_ms << L" ms[/color]\n";
	//}

	const auto& depo = id[slot_function::ITEM_DEPOSIT];

	if (depo.alive()) {
		const auto children_space = format_space_units(depo.calculate_local_space_available());
		const auto with_parents_space = format_space_units(depo.calculate_real_space_available());

		result << L"Deposit space: [color=vsgreen]" << format_space_units(depo.calculate_real_space_available()) << L"[/color]/";

		if (children_space != with_parents_space)
			result << L"[color=vsyellow]" << format_space_units(depo.calculate_local_space_available()) << L"[/color]/";
			
		result << L"[color=vscyan]" << format_space_units(depo->space_available) << L"[/color]\n";
	}

	std::wstring out;

	// TODO: describe catridge invariant and describe types inside
	
	out = result.str();
	return out.substr(0, out.length() - 1);
}

std::wstring get_bbcoded_slot_details(const const_inventory_slot_handle id) {
	const auto name = get_bbcoded_slot_function_name(id.get_id().type);
	const auto description = get_bbcoded_slot_function_description(id.get_id().type);

	const auto catcolor = id->category_allowed == item_category::GENERAL ? L"vsblue" : L"violet";

	return name + L"\n[color=vslightgray]Allows: [/color][color=" + catcolor + L"]" + get_bbcoded_item_categories(id->get_allowed_categories()) + L"[/color][color=vsdarkgray]\n" +
		description + L"[/color]";
}

std::wstring get_bbcoded_entity_details(const const_entity_handle id) {
	const auto name = get_bbcoded_entity_name(id);
	const auto description = get_bbcoded_entity_description(id);

	auto properties = get_bbcoded_entity_properties(id);
	
	if (!properties.empty()) {
		properties += L"\n";
	}

	return L"[color=white]" + name + L"[/color]\n" + properties + L"[color=vsdarkgray]" + description + L"[/color]";
}