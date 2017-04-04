#pragma once
#include "augs/math/vec2.h"

namespace assets {
	enum class game_image_id {
		BLANK,

		CRATE,
		CRATE_DESTROYED,
		CAR_INSIDE,
		CAR_FRONT,

		TRUCK_INSIDE,
		TRUCK_FRONT,

		JMIX114,
		
		DEAD_TORSO,

		TEST_CROSSHAIR,

		TORSO_MOVING_FIRST,
		TORSO_MOVING_LAST = TORSO_MOVING_FIRST + 5,

		VIOLET_TORSO_MOVING_FIRST,
		VIOLET_TORSO_MOVING_LAST = VIOLET_TORSO_MOVING_FIRST + 5,

		BLUE_TORSO_MOVING_FIRST,
		BLUE_TORSO_MOVING_LAST = BLUE_TORSO_MOVING_FIRST + 5,

		SMOKE_PARTICLE_FIRST,
		SMOKE_PARTICLE_LAST = SMOKE_PARTICLE_FIRST + 6,

		PIXEL_THUNDER_FIRST,
		PIXEL_THUNDER_LAST = PIXEL_THUNDER_FIRST + 5,

		ASSAULT_RIFLE,
		BILMER2000,
		KEK9,
		SUBMACHINE,
		PISTOL,
		SHOTGUN,
		URBAN_CYAN_MACHETE,

		TEST_BACKGROUND,
		TEST_BACKGROUND2,

		SAMPLE_MAGAZINE,
		SMALL_MAGAZINE,
		SAMPLE_SUPPRESSOR,
		ROUND_TRACE,
		ENERGY_BALL,
		PINK_CHARGE,
		PINK_SHELL,
		CYAN_CHARGE,
		CYAN_SHELL,
		RED_CHARGE,
		RED_SHELL,
		GREEN_CHARGE,
		GREEN_SHELL,
		BACKPACK,
		RIGHT_HAND_ICON,
		LEFT_HAND_ICON,
		DROP_HAND_ICON,

		GUI_CURSOR,
		GUI_CURSOR_HOVER,
		GUI_CURSOR_ADD,
		GUI_CURSOR_MINUS,
		GUI_CURSOR_ERROR,
		
		HUD_CIRCULAR_BAR_MEDIUM,
		HUD_CIRCULAR_BAR_SMALL,

		CONTAINER_OPEN_ICON,
		CONTAINER_CLOSED_ICON,

		BUTTON_WITH_CUTS,

		ATTACHMENT_CIRCLE_FILLED,
		ATTACHMENT_CIRCLE_BORDER,
		PRIMARY_HAND_ICON,
		SECONDARY_HAND_ICON,
		SHOULDER_SLOT_ICON,
		ARMOR_SLOT_ICON,
		CHAMBER_SLOT_ICON,
		DETACHABLE_MAGAZINE_ICON,
		GUN_MUZZLE_SLOT_ICON,

		METROPOLIS_TILE_FIRST,
		METROPOLIS_TILE_LAST = METROPOLIS_TILE_FIRST + 49,

		HAVE_A_PLEASANT,
		AWAKENING,
		METROPOLIS,

		BRICK_WALL,
		ROAD,
		ROAD_FRONT_DIRT,

		BLINK_FIRST,
		BLINK_LAST = BLINK_FIRST + 7,

		CAST_BLINK_FIRST,
		CAST_BLINK_LAST = CAST_BLINK_FIRST + 19,

		WANDERING_SQUARE,
		WANDERING_CROSS,

		TRUCK_ENGINE,

		MENU_GAME_LOGO,

		MENU_BUTTON_INSIDE,

		MENU_BUTTON_LT,
		MENU_BUTTON_RT,
		MENU_BUTTON_RB,
		MENU_BUTTON_LB,

		MENU_BUTTON_L,
		MENU_BUTTON_T,
		MENU_BUTTON_R,
		MENU_BUTTON_B,

		MENU_BUTTON_LB_COMPLEMENT,

		MENU_BUTTON_LT_BORDER,
		MENU_BUTTON_RT_BORDER,
		MENU_BUTTON_RB_BORDER,
		MENU_BUTTON_LB_BORDER,

		MENU_BUTTON_L_BORDER,
		MENU_BUTTON_T_BORDER,
		MENU_BUTTON_R_BORDER,
		MENU_BUTTON_B_BORDER,

		MENU_BUTTON_LB_COMPLEMENT_BORDER,

		MENU_BUTTON_LT_INTERNAL_BORDER,
		MENU_BUTTON_RT_INTERNAL_BORDER,
		MENU_BUTTON_RB_INTERNAL_BORDER,
		MENU_BUTTON_LB_INTERNAL_BORDER,
		
		HOTBAR_BUTTON_INSIDE,

		HOTBAR_BUTTON_LT,
		HOTBAR_BUTTON_RT,
		HOTBAR_BUTTON_RB,
		HOTBAR_BUTTON_LB,

		HOTBAR_BUTTON_L,
		HOTBAR_BUTTON_T,
		HOTBAR_BUTTON_R,
		HOTBAR_BUTTON_B,

		HOTBAR_BUTTON_LB_COMPLEMENT,

		HOTBAR_BUTTON_LT_BORDER,
		HOTBAR_BUTTON_RT_BORDER,
		HOTBAR_BUTTON_RB_BORDER,
		HOTBAR_BUTTON_LB_BORDER,

		HOTBAR_BUTTON_L_BORDER,
		HOTBAR_BUTTON_T_BORDER,
		HOTBAR_BUTTON_R_BORDER,
		HOTBAR_BUTTON_B_BORDER,

		HOTBAR_BUTTON_LB_COMPLEMENT_BORDER,

		HOTBAR_BUTTON_LT_INTERNAL_BORDER,
		HOTBAR_BUTTON_RT_INTERNAL_BORDER,
		HOTBAR_BUTTON_RB_INTERNAL_BORDER,
		HOTBAR_BUTTON_LB_INTERNAL_BORDER,

		ACTION_BUTTON_FILLED,
		ACTION_BUTTON_BORDER,
			
		LASER,
		LASER_GLOW_EDGE,

		HEALTH_ICON,
		PERSONAL_ELECTRICITY_ICON,
		CONSCIOUSNESS_ICON,

		AMPLIFIER_ARM,

		SPELL_HASTE_ICON,
		SPELL_GREATER_HASTE_ICON,
		SPELL_ELECTRIC_SHIELD_ICON,
		SPELL_ELECTRIC_MISSILE_ICON,
		SPELL_ELECTRIC_TRIAD_ICON,
		SPELL_FURY_OF_THE_AEONS_ICON,
		SPELL_ULTIMATE_WRATH_OF_THE_AEONS_ICON,

		SPELL_BORDER,

		PERK_HASTE_ICON,
		PERK_ELECTRIC_SHIELD_ICON,

		CAST_HIGHLIGHT,

		GRENADE_SPOON,

		FORCE_GRENADE,
		PED_GRENADE,
		INTERFERENCE_GRENADE,

		FORCE_GRENADE_RELEASED,
		PED_GRENADE_RELEASED,
		INTERFERENCE_GRENADE_RELEASED,

		COUNT
	};
	
	vec2u get_size(game_image_id);
}

namespace augs {
	struct texture_atlas_entry;
}

augs::texture_atlas_entry& operator*(const assets::game_image_id& id);
bool operator!(const assets::game_image_id& id);
