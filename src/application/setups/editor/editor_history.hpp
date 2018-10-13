#pragma once
#include "augs/templates/history.hpp"
#include "application/setups/editor/editor_history.h"

template <class T>
const T& editor_history::execute_new(T&& command, const editor_command_input in) {
	if (!in.allow_execution()) {
		return command;
	}

	command.common.reset_timestamp();

	if (has_last_command()) {
		const auto should_make_child = std::visit(
			[](auto& cmd) -> bool {
				return is_create_asset_id_command_v<remove_cref<decltype(cmd)>>;
			},
			last_command()
		);

		if (should_make_child) {
			command.common.has_parent = true;
		}
	}

	command.common.when_happened = in.get_current_step();

	return editor_history_base::execute_new(
		std::forward<T>(command),
		in
	);
}
