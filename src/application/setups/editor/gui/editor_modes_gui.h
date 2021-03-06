#pragma once
#include "application/setups/editor/commands/editor_command_structs.h"
#include "application/setups/editor/property_editor/property_editor_structs.h"
#include "augs/misc/imgui/standard_window_mixin.h"

struct editor_settings;
struct editor_command_input;

struct editor_modes_gui : standard_window_mixin<editor_modes_gui> {
	using base = standard_window_mixin<editor_modes_gui>;
	using base::base;
	using introspect_base = base;

	mode_entropy_general perform(const editor_settings&, editor_command_input);

private:
	property_editor_state property_editor_data;
};
