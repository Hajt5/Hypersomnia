#include "augs/misc/marks.hpp"
#include "game/detail/inventory/inventory_slot_handle.h"
#include "augs/string/string_templates.h"
#include "augs/templates/algorithm_templates.h"
#include "augs/misc/imgui/imgui_utils.h"
#include "augs/filesystem/directory.h"
#include "augs/templates/thread_templates.h"
#include "augs/templates/algorithm_templates.h"
#include "augs/window_framework/window.h"
#include "augs/window_framework/platform_utils.h"
#include "augs/window_framework/shell.h"

#include "game/detail/visible_entities.h"
#include "view/viewables/images_in_atlas_map.h"

#include "application/config_lua_table.h"
#include "application/setups/editor/editor_setup.hpp"
#include "application/setups/editor/detail/on_mode_with_input.hpp"
#include "application/setups/editor/editor_paths.h"
#include "application/setups/editor/editor_camera.h"
#include "application/setups/editor/editor_history.hpp"

#include "application/setups/editor/gui/editor_tab_gui.h"
#include "application/setups/draw_setup_gui_input.h"
#include "view/rendering_scripts/draw_area_indicator.h"
#include "augs/gui/text/printer.h"
#include "application/setups/editor/editor_selection_groups.hpp"

#include "game/detail/weapon_like.h"

#include "augs/readwrite/byte_file.h"
#include "augs/readwrite/lua_file.h"

std::optional<ltrb> editor_setup::find_screen_space_rect_selection(
	const vec2i screen_size,
	const vec2i mouse_pos
) const {
	if (const auto eye = find_current_camera_eye()) {
		return selector.find_screen_space_rect_selection(camera_cone(*eye, screen_size), mouse_pos);
	}

	return std::nullopt;
}

void editor_setup::open_last_folders(sol::state& lua) {
	catch_popup([&]() { ::open_last_folders(lua, signi); });
}

double editor_setup::get_audiovisual_speed() const {
	return anything_opened() ? player().get_speed() : 1.0;
}

const cosmos& editor_setup::get_viewed_cosmos() const {
	return anything_opened() ? work().world : cosmos::zero; 
}

real32 editor_setup::get_interpolation_ratio() const {
	return anything_opened() ? player().get_timer().fraction_of_step_until_next_step(get_viewed_cosmos().get_fixed_delta()) : 1.0;
}

entity_id editor_setup::get_viewed_character_id() const {
	if (anything_opened()) {
		return folder().get_viewed_character_id();
	}

	return {};
}

const_entity_handle editor_setup::get_viewed_character() const {
	return get_viewed_cosmos()[get_viewed_character_id()];
}

const all_viewables_defs& editor_setup::get_viewable_defs() const {
	return anything_opened() ? work().viewables : all_viewables_defs::empty;
}

void editor_setup::unhover() {
	selector.unhover();
}

bool editor_setup::is_editing_mode() const {
	return anything_opened() ? player().is_editing_mode() : false;
}

bool editor_setup::is_gameplay_on() const {
	return anything_opened() && !is_editing_mode();
}

std::optional<camera_eye> editor_setup::find_current_camera_eye() const {
	if (anything_opened()) {
		return editor_detail::calculate_camera(player(), view(), get_matching_go_to_entity(), get_viewed_character());
	}

	return std::nullopt;
}

const_entity_handle editor_setup::get_matching_go_to_entity() const {
	return go_to_entity_gui.get_matching_go_to_entity(work().world);
}

void editor_setup::on_folder_changed() {
	if (anything_opened()) {
		player().pause();
	}

	selector.clear();
}

void editor_setup::set_popup(const editor_popup p) {
	ok_only_popup = p;

	const auto logged = typesafe_sprintf(
		"%x\n%x\n%x", 
		p.title, p.message, p.details
	);

	LOG(logged);

	augs::save_as_text(LOG_FILES_DIR "/last_editor_message.txt", augs::date_time().get_readable() + '\n' + logged);
}

void editor_setup::override_viewed_entity(const entity_id overridden_id) {
	if (anything_opened()) {
		on_mode_with_input(
			[&](const auto& typed_mode, const auto&) {
				if (const auto id = typed_mode.lookup(work().world[overridden_id].get_guid()); id.is_set()) {
					view().local_player = id;
					view().overridden_viewed = {};
				}
				else {
					view().local_player = {};
					view().overridden_viewed = overridden_id;
				}
			}
		);
	}
}

editor_setup::editor_setup(
	sol::state& lua
) : 
	recent(lua),
	destructor_input{ lua }
{
	augs::create_directories(get_untitled_dir());

	open_last_folders(lua);
	load_gui_state();
}

editor_setup::editor_setup(
	sol::state& lua, 
	const augs::path_type& intercosm_path
) : 
	recent(lua),
	destructor_input{ lua }
{
	augs::create_directories(get_untitled_dir());

	open_last_folders(lua);
	open_folder_in_new_tab({ lua, intercosm_path });
	load_gui_state();
}

void editor_setup::load_gui_state() {
	try {
		augs::load_from_bytes(*this, get_editor_gui_state_path());
	}
	catch (const augs::file_open_error&) {
		// We don't care if it does not exist
	}
}

void editor_setup::save_gui_state() {
	augs::save_as_bytes(*this, get_editor_gui_state_path());
}

editor_setup::~editor_setup() {
	save_gui_state();
	force_autosave_now();
}

void editor_setup::force_autosave_now() const {
	autosave.save(destructor_input.lua, signi);
}

void editor_setup::accept_game_gui_events(const cosmic_entropy& entropy) {
	control(entropy);
}

void editor_setup::customize_for_viewing(config_lua_table& config) const {
	if (anything_opened()) {
		config.window.name = "Editor - " + folder().get_display_path();
	}
	else {
		config.window.name = "Editor";
	}

	if (!anything_opened() || is_editing_mode()) {
		config.drawing.draw_aabb_highlighter = false;
		config.interpolation.enabled = false;
		config.drawing.draw_area_markers = {};
	}

	if (is_gameplay_on()) {
		on_mode_with_input(
			[&](const auto& typed_mode, const auto& mode_input) {
				using T = remove_cref<decltype(typed_mode)>;

				if constexpr(!std::is_same_v<T, test_scene_mode>) {
					mode_input.vars.view.adjust(config.drawing);
				}
			}
		);
	}

	return;
}

void editor_setup::apply(const config_lua_table& cfg) {
	settings = cfg.editor;
	return;
}

std::size_t editor_setup::find_folder_by_path(const augs::path_type& current_path) const {
	for (std::size_t i = 0; i < signi.folders.size(); ++i) {
		if (signi.folders[i].current_path == current_path) {
			return static_cast<folder_index>(i);
		}
	}

	return dead_folder_v;
}

void editor_setup::open_folder_in_new_tab(const path_operation op) {
	if (const auto existing = find_folder_by_path(op.path); existing != dead_folder_v) {
		set_current(existing);
		return;
	}

	try_to_open_new_folder(
		[this, op](editor_folder& f) {
			f.set_folder_path(op.path);

			if (const auto warning = f.open_most_relevant_content(op.lua)) {
				set_popup(*warning);
			}

			recent.add(op.lua, f.current_path);
			/* f.mark_as_just_saved(); */
		}
	);
}

bool editor_setup::save_current_folder() {
	if (!anything_opened()) {
		return false;
	}

	auto& f = folder();
	const auto& p = f.current_path;

	auto make_error = [&](const auto& what) {
		const auto content = typesafe_sprintf(
			"Unknown problem occured when saving the project files to:\n%x.\nTry to save the files to a safe location immediately!",
			p
		);

		set_popup({ "Error", content, what });
	};

	try {
		std::as_const(f).save_folder();
		f.mark_as_just_saved();

		recent_message.set("Written the current project files to %x", p);

		return true;
	}
	catch (const std::exception& e) {
		make_error(e.what());
	}
	catch (...) {
		make_error("Unknown exception");
	}

	return false;
}

bool editor_setup::save_current_folder_to(const path_operation op) {
	if (!anything_opened()) {
		return false;
	}

	auto& f = folder();

	const auto previous_path = f.current_path;
	const bool was_untitled = f.is_untitled();
	const auto& target_path = op.path;

	f.set_folder_path(target_path);

	if (save_current_folder()) {
		if (was_untitled) {
			augs::remove_directory(previous_path);
		}

		recent.add(op.lua, target_path);
		return true;
	}
	else {
		f.set_folder_path(previous_path);
	}

	return false;
}

void editor_setup::export_current_folder_to(const path_operation op) {
	if (!anything_opened()) {
		return;
	}

	const auto& p = op.path;

	auto make_error = [&](const auto& what) {
		const auto content = typesafe_sprintf(
			"Unknown problem occured when exporting the .int and .modes files to %x.",
			p
		);

		set_popup({ "Error", content, what });
	};

	try {
		const auto& f = folder();
		f.export_folder(op.lua, p);
		recent_message.set("Exported the .int and .modes files to %x.", p);
	}
	catch (const std::exception& e) {
		make_error(e.what());
	}
	catch (...) {
		make_error("Unknown exception");
	}
}

void editor_setup::fill_with_minimal_scene() {
	if (anything_opened()) {
		clear_id_caches();

		if (!player().has_testing_started()) {
			folder().history.execute_new(fill_with_test_scene_command(true), make_command_input());
		}
		else {
			recent_message.set("Cannot fill with a test scene when a playtesting session is in progress.");
		}
	}
}

void editor_setup::fill_with_test_scene() {
	if (anything_opened()) {
		clear_id_caches();
		if (!player().has_testing_started()) {
			folder().history.execute_new(fill_with_test_scene_command(false), make_command_input());
		}
		else {
			recent_message.set("Cannot fill with a minimal scene when a playtesting session is in progress.");
		}
	}
}

float editor_setup::get_menu_bar_height() const {
	const auto& g = *ImGui::GetCurrentContext();
	return g.FontBaseSize + g.Style.FramePadding.y * 2.0f;
}

float editor_setup::get_game_screen_top() const {
	if (is_editing_mode()) {
		return get_menu_bar_height() * 2;
	}

	return 0.f;
}

void editor_setup::perform_custom_imgui(
	sol::state& lua,
	augs::window& owner,
	const images_in_atlas_map& game_atlas,
	const config_lua_table& config
) {
	using namespace augs::imgui;

	autosave.advance(lua, signi, settings.autosave);

	auto path_op = [&](const auto& path) {
		return path_operation{ lua, path };
	};

	auto item_if_tabs_and = [this](const bool condition, const char* label, const char* shortcut = nullptr) {
		return ImGui::MenuItem(label, shortcut, nullptr, condition && anything_opened());
	};

	auto item_if_tabs = [&](const char* label, const char* shortcut = nullptr) {
		return item_if_tabs_and(true, label, shortcut);
	};

	const auto mouse_pos = vec2i(ImGui::GetIO().MousePos);
	const auto screen_size = vec2i(ImGui::GetIO().DisplaySize);

	const auto menu_bar_height = get_menu_bar_height();

	const bool has_ctrl = ImGui::GetIO().KeyCtrl;

	if (!is_gameplay_on()) {
		{
			/* We don't want ugly borders in our menu bar */
			auto window_border_size = scoped_style_var(ImGuiStyleVar_WindowBorderSize, 0.0f);
			auto frame_border_size = scoped_style_var(ImGuiStyleVar_FrameBorderSize, 0.0f);

			if (auto main_menu = scoped_main_menu_bar()) {
				if (auto menu = scoped_menu("File")) {
					if (ImGui::MenuItem("New", "CTRL+N")) {
						new_tab();
					}

					if (ImGui::MenuItem("Open", "CTRL+O")) {
						open(owner);
					}

					if (auto menu = scoped_menu("Recent projects", !recent.empty())) {
						/*	
							IMPORTANT! recent.paths can be altered in the loop by loading a intercosm,
							thus we need to copy its contents.
						*/

						const auto recent_paths = recent.paths;

						for (const auto& target_path : recent_paths) {
							const auto str = augs::filename_first(target_path);

							if (ImGui::MenuItem(str.c_str())) {
								LOG_NVPS(target_path);
								open_folder_in_new_tab(path_op(target_path));
							}
						}

						ImGui::Separator();

						if (ImGui::MenuItem("Clear Recent Items")) {
							recent.clear(lua);
						}
					}

					ImGui::Separator();

					if (item_if_tabs("Save", "CTRL+S")) {
						save(owner);
					}

					if (item_if_tabs("Save as", "F12")) {
						save_as(owner);
					}

					if (item_if_tabs("(Experimental) Export for compatibility (lua)", "")) {
						export_for_compatibility(owner);
					}

					ImGui::Separator();

					const auto close_str = [&]() -> std::string {
						if (anything_opened()) {
							return std::string("Close ") + folder().get_display_path();
						}

						return "Close";
					}();

					if (item_if_tabs(close_str.c_str(), "CTRL+W")) {
						close_folder();
					}

					if (item_if_tabs("Close all")) {
						while (anything_opened()) {
							if (!close_folder()) {
								break;
							}
						}
					}
				}

				if (auto menu = scoped_menu("Edit")) {
					{
						const bool enable_undo = anything_opened() && !folder().history.is_revision_oldest();
						const bool enable_redo = anything_opened() && !folder().history.is_revision_newest();

						if (item_if_tabs_and(enable_redo, "Undo", "CTRL+Z")) { undo(); }
						if (item_if_tabs_and(enable_undo, "Redo", "CTRL+SHIFT+Z")) { redo(); }
					}

					ImGui::Separator();
#if 0
					if (item_if_tabs("Cut", "CTRL+X")) {}
					if (item_if_tabs("Copy", "CTRL+C")) {}
					if (item_if_tabs("Paste", "CTRL+V")) {}
					ImGui::Separator();

#endif

#if BUILD_TEST_SCENES
					if (item_if_tabs("Fill with test scene", "SHIFT+F5")) {
						fill_with_test_scene();
					}

					if (item_if_tabs("Fill with minimal scene", "CTRL+SHIFT+F5")) {
						fill_with_minimal_scene();
					}
#else
					if (item_if_tabs_and(false, "Fill with test scene", "SHIFT+F5")) {}
					if (item_if_tabs_and(false, "Fill with minimal scene", "CTRL+SHIFT+F5")) {}
#endif
				}
				if (auto menu = scoped_menu("View")) {
					auto do_window_entry = [&](auto& win, const auto shortcut) {
						const auto s = std::string("ALT+") + shortcut;
						if (item_if_tabs(win.get_title().c_str(), s.c_str())) {
							win.open();
						}
					};

					ImGui::MenuItem("(Info)", NULL, false, false);

					do_window_entry(summary_gui, "U");
					do_window_entry(coordinates_gui, "O");

					ImGui::Separator();
					ImGui::MenuItem("(Editing)", NULL, false, false);

					do_window_entry(layers_gui, "L");
					do_window_entry(history_gui, "H");

					if (item_if_tabs("Player", "ALT+P")) {
						player_gui.show = true;
					}

					do_window_entry(selection_groups_gui, "G");
					do_window_entry(modes_gui, "D");

					ImGui::Separator();
					ImGui::MenuItem("(State)", NULL, false, false);

					do_window_entry(common_state_gui, "C");
					do_window_entry(fae_gui, "A");
					do_window_entry(selected_fae_gui, "S");

					ImGui::Separator();
					ImGui::MenuItem("(Assets)", NULL, false, false);

					do_window_entry(images_gui, "I");
					do_window_entry(sounds_gui, "S");
					do_window_entry(particle_effects_gui, "R");
					do_window_entry(plain_animations_gui, "M");

					ImGui::Separator();

					if (ImGui::MenuItem("Tutorial", "ALT+X", nullptr, true)) {
						tutorial_gui.show = true;
					}
				}
			}
		}

		if (anything_opened()) {
			perform_editor_tab_gui(
				[&](const auto index_to_close){ close_folder(index_to_close); },
				[&](const auto index_to_set){ set_current(index_to_set); },
				signi,
				menu_bar_height
			);
		}
	}

	tutorial_gui.perform(*this);

	if (anything_opened()) {
		history_gui.perform(make_command_input());
	}

	const bool on_empty_revision = 
		anything_opened() 
		&& !player().has_testing_started()
		&& folder().history.on_first_revision()
	;

	if (anything_opened() && !on_empty_revision) {
		common_state_gui.perform(settings, make_command_input());

		{
			const auto output = fae_gui.perform(make_fae_gui_input(), view_ids().selected_entities);

			if (const auto id = output.instantiate_id) {
				if (is_gameplay_on()) {
					/* Go back to editing */
					player().pause();
				}

				instantiate_flavour_command cmd;
				cmd.instantiated_id = *id;
				cmd.where.pos = *find_world_cursor_pos();
				const auto cmd_in = make_command_input();
				const auto& executed = post_editor_command(cmd_in, std::move(cmd));

				auto& cosm = work().world;

				if (const auto created = cosm[executed.get_created_id()]) {
					mover.start_moving_selection(make_mover_input());
					make_last_command_a_child();
				}
			}
		}

		selection_groups_gui.perform(has_ctrl, make_command_input());
		modes_gui.perform(settings, make_command_input());

		summary_gui.perform(*this);
		layers_gui.perform(
			settings.property_editor,
			view().viewing_filter,
			view().selecting_filter
		);

		images_gui.perform(owner, settings.property_editor, game_atlas, make_command_input());
		sounds_gui.perform(owner, settings.property_editor, game_atlas, make_command_input());

		particle_effects_gui.perform(settings.property_editor, game_atlas, make_command_input());
		plain_animations_gui.perform(settings.property_editor, game_atlas, make_command_input());

		const auto all_selected = [&]() -> decltype(get_all_selected_entities()) {
			auto selections = get_all_selected_entities();

			if (const auto held = selector.get_held(); held.is_set() && work().world[held]) {
				selections.emplace(held);

				if (!view().ignore_groups) {
					view_ids().selection_groups.for_each_sibling(held, [&](const auto id){ selections.emplace(id); });
				}
			}

			if (const auto matching = get_matching_go_to_entity()) {
				selections.emplace(matching);
			}

			return selections;
		}();

		{
			const auto in = make_fae_gui_input();
			const auto output = selected_fae_gui.perform(in, all_selected);

			const auto& cosm = work().world;
			output.filter.perform(cosm, view_ids().selected_entities);
		}

		coordinates_gui.perform(*this, screen_size, mouse_pos, all_selected);

		player_gui.perform(make_command_input());

		const auto go_to_dialog_pos = vec2 { static_cast<float>(screen_size.x / 2), menu_bar_height * 2 + 1 };

		if (const auto confirmation = 
			go_to_entity_gui.perform(settings.go_to, work().world, go_to_dialog_pos)
		) {
			::standard_confirm_go_to(*confirmation, has_ctrl, view(), view_ids());
		}

		if (const auto rot = mover.current_mover_rot_delta(make_mover_input())) {
			text_tooltip("%x*", *rot);
		}
		else if (const auto pos = mover.current_mover_pos_delta(make_mover_input())) {
			text_tooltip("x: %x\ny: %x", pos->x, pos->y);
		}

		on_mode_with_input(
			[&](const auto& typed_mode, const auto& mode_input) {
				const auto draw_mode_in = draw_mode_gui_input { 
					get_game_screen_top(), 
					view().local_player, 
					game_atlas,
					config
				};

				const auto new_entropy = arena_gui.perform_imgui(
					draw_mode_in, 
					typed_mode, 
					mode_input
				);

				control(new_entropy);
			}
		);
	}

	if (open_folder_dialog.valid() && is_ready(open_folder_dialog)) {
		const auto result_path = open_folder_dialog.get();

		if (result_path) {
			open_folder_in_new_tab(path_op(*result_path));
		}
	}

	if (anything_opened()) {
		if (save_folder_dialog.valid() && is_ready(save_folder_dialog)) {
			if (const auto result_path = save_folder_dialog.get()) {
				const auto& p = *result_path;

				if (::is_untitled_path(p)) {
					set_popup({"Error", "Can't save to a directory with untitled projects.", ""});
				}
				else {
					save_current_folder_to(path_op(p));
				}
			}
		}

		if (export_folder_dialog.valid() && is_ready(export_folder_dialog)) {
			if (const auto result_path = export_folder_dialog.get()) {
				const auto& p = *result_path;

				if (::is_untitled_path(p)) {
					set_popup({"Error", "Can't export to a directory with untitled projects.", ""});
				}
				else {
					export_current_folder_to(path_op(p));
				}
			}
		}
	}

	if (ok_only_popup && ok_only_popup->perform()) {
		ok_only_popup = std::nullopt;
	}
}

void editor_setup::clear_id_caches() {
	selector.clear();

	if (anything_opened()) {
		view_ids().selected_entities.clear();
	}
}

void editor_setup::finish_rectangular_selection() {
	if (anything_opened()) {
		selector.finish_rectangular(view_ids().selected_entities);
	}
}

setup_escape_result editor_setup::escape() {
	if (ok_only_popup) {
		ok_only_popup = std::nullopt;
		return setup_escape_result::JUST_FETCH;
	}
	else if (anything_opened() && is_gameplay_on()) {
		player().pause();
		return setup_escape_result::SWITCH_TO_GAME_GUI;
	}
	else if (anything_opened() && view().marks.state != augs::marks_state::NONE) {
		view().marks.close();
		return setup_escape_result::JUST_FETCH;
	}
	else if (mover.escape()) {
		undo();
		return setup_escape_result::JUST_FETCH;
	}

	return setup_escape_result::LAUNCH_INGAME_MENU;
}

bool editor_setup::confirm_modal_popup() {
	if (ok_only_popup) {
		ok_only_popup = std::nullopt;
		return true;
	}

	return false;
}

void editor_setup::open(const augs::window& owner) {
	if (ok_only_popup) {
		return;
	}

	open_folder_dialog = std::async(
		std::launch::async,
		[&](){
			return owner.choose_directory_dialog("Open folder with project files");
		}
	);
}

void editor_setup::save(const augs::window& owner) {
	if (!anything_opened()) {
		return;
	}

	if (folder().is_untitled()) {
		save_as(owner);
	}
	else {
		save_current_folder();
	}
}

void editor_setup::export_for_compatibility(const augs::window& owner) {
	if (!anything_opened() || ok_only_popup) {
		return;
	}

	export_folder_dialog = std::async(
		std::launch::async,
		[&](){
			return owner.choose_directory_dialog("Choose folder for the exported project files");
		}
	);
}

void editor_setup::save_as(const augs::window& owner) {
	if (!anything_opened() || ok_only_popup) {
		return;
	}

	save_folder_dialog = std::async(
		std::launch::async,
		[&](){
			return owner.choose_directory_dialog("Choose folder for project files");
		}
	);
}

void editor_setup::undo() {
	if (anything_opened()) {
		folder().history.undo(make_command_input());
	}
}

void editor_setup::redo() {
	if (anything_opened()) {
		folder().history.redo(make_command_input());
	}
}

void editor_setup::copy() {

}

void editor_setup::cut() {

}

void editor_setup::paste() {

}

std::unordered_set<entity_id> editor_setup::get_all_selected_entities() const {
	std::unordered_set<entity_id> all;

	for_each_selected_entity(
		[&](const auto e) {
			all.insert(e);
		}
	);

	return all;
}

void editor_setup::cut_selection() {
	delete_selection();
}

void editor_setup::delete_selection() {
	if (anything_opened()) {
		auto command = make_command_from_selections<delete_entities_command>("Deleted ");

		if (!command.empty()) {
			folder().history.execute_new(std::move(command), make_command_input());
			clear_id_caches();
		}
	}
}

void editor_setup::mirror_selection(const vec2i direction) {
	if (anything_opened()) {
		finish_rectangular_selection();

		const bool only_duplicating = direction.is_zero();

		auto command = make_command_from_selections<duplicate_entities_command>(only_duplicating ? "Duplicated " : "Mirrored ");

		if (!command.empty()) {
			command.mirror_direction = direction;
			folder().history.execute_new(std::move(command), make_command_input());
		}

		if (only_duplicating) {
			mover.start_moving_selection(make_mover_input());
			make_last_command_a_child();
		}
	}
}

void editor_setup::duplicate_selection() {
	mirror_selection(vec2i(0, 0));
}

void editor_setup::group_selection() {
	if (anything_opened()) {
		auto command = make_command_from_selections<change_grouping_command>("Grouped ");

		if (!command.empty()) {
			command.all_to_new_group = true;
			folder().history.execute_new(std::move(command), make_command_input());
		}
	}
}

void editor_setup::ungroup_selection() {
	if (anything_opened()) {
		auto command = make_command_from_selections<change_grouping_command>("Ungrouped ");

		if (!command.empty()) {
			folder().history.execute_new(std::move(command), make_command_input());
		}
	}
}

void editor_setup::make_last_command_a_child() {
	if (anything_opened()) {
		auto set_has_parent = [](auto& command) { 
			command.common.has_parent = true; 
		};

		std::visit(set_has_parent, folder().history.last_command());
	}
}

void editor_setup::center_view_at_selection() {
	if (anything_opened()) {
		if (const auto aabb = find_selection_aabb()) {
			view().center_at(aabb->get_center());
		}
	}
}

void editor_setup::go_to_all() {

}

void editor_setup::go_to_entity() {
	go_to_entity_gui.open();
}

void editor_setup::reveal_in_explorer(const augs::window& owner) {
	owner.reveal_in_explorer(folder().get_paths().int_file);
}

void editor_setup::new_tab() {
	try_to_open_new_folder([&](editor_folder& t) {
		t.current_path = get_first_free_untitled_path(
			"Project%x", 
			[&](const auto& candidate) {
				return find_folder_by_path(candidate) == dead_folder_v;
			}
		);
		augs::create_directories(t.current_path);
	});
}

void editor_setup::next_tab() {
	if (anything_opened()) {
		set_current((signi.current_index + 1) % signi.folders.size());
	}
}

void editor_setup::prev_tab() {
	if (anything_opened()) {
		set_current(signi.current_index == 0 ? static_cast<folder_index>(signi.folders.size() - 1) : signi.current_index - 1);
	}
}

bool editor_setup::close_folder(const folder_index i) {
   	auto& folder_to_close = signi.folders[i];

	if (!folder_to_close.allow_close()) {
		set_popup({ "Error", "Save your work before closing the tab.", "" });
		return false;
	}
		
	if (folder_to_close.is_untitled()) {
		augs::remove_directory(folder_to_close.current_path);
	}

	signi.folders.erase(signi.folders.begin() + i);

	if (signi.folders.empty()) {
		signi.current_index = -1;
	}
	else {
		signi.current_index = std::min(signi.current_index, static_cast<folder_index>(signi.folders.size() - 1));
	}

	return true;
}

bool editor_setup::close_folder() {
	if (anything_opened()) {
		return close_folder(signi.current_index);
	}

	return false;
}


editor_command_input editor_setup::make_command_input() {
	return { destructor_input.lua, settings, folder(), selector, fae_gui, selected_fae_gui, mover };
}

grouped_selector_op_input editor_setup::make_grouped_selector_op_input() const {
	return { view_ids().selected_entities, view_ids().selection_groups, view().ignore_groups };
}

editor_fae_gui_input editor_setup::make_fae_gui_input() {
	return { settings.property_editor, make_command_input() };
}

entity_mover_input editor_setup::make_mover_input() {
	return { *this };
}

void editor_setup::select_all_entities(const bool has_ctrl) {
	if (anything_opened()) {
		selector.select_all(
			work().world,
			view().rect_select_mode,
		   	has_ctrl,
			view_ids().selected_entities,
			view().get_effective_selecting_filter()
		);
	}
}

bool editor_setup::handle_input_before_imgui(
	const augs::event::state& common_input_state,
	const augs::event::change e,

	augs::window& window
) {
	using namespace augs::event;
	using namespace keys;

	if (settings.autosave.on_lost_focus && e.msg == message::deactivate) {
		force_autosave_now();
	}

	if (e.was_any_key_pressed()) {
		const auto k = e.data.key.key;
		
		auto move_currently_viewed = [&](const int n) {
			images_gui.move_currently_viewed_by += n;
			sounds_gui.move_currently_viewed_by += n;
		};

		switch (k) {
			case key::PAGEUP: move_currently_viewed(-1); return true;
			case key::PAGEDOWN: move_currently_viewed(1); return true;

			default: break;
		}

		const bool has_alt{ common_input_state[key::LALT] };
		const bool has_ctrl{ common_input_state[key::LCTRL] };
		const bool has_shift{ common_input_state[key::LSHIFT] };

		if (!has_ctrl && k == key::ENTER) {
			if (confirm_modal_popup()) {
				return true;
			}

			if (mover.escape()) {
				return true;
			}
		}	

		if (has_alt) {
			switch (k) {
				case key::A: fae_gui.open(); return true;
				case key::H: history_gui.open(); return true;
				case key::S: selected_fae_gui.open(); return true;
				case key::C: common_state_gui.open(); return true;
				case key::G: selection_groups_gui.open(); return true;
				case key::P: player_gui.show = true; return true;
				case key::U: summary_gui.open(); return true;
				case key::O: coordinates_gui.open(); return true;
				case key::L: layers_gui.open(); return true;
				case key::I: images_gui.open(); return true;
				case key::N: sounds_gui.open(); return true;
				case key::R: particle_effects_gui.open(); return true;
				case key::M: plain_animations_gui.open(); return true;
				case key::D: modes_gui.open(); return true;
				case key::X: tutorial_gui.open(); return true;
				default: break;
			}
		}

		if (has_ctrl) {
			switch (k) {
				case key::N: new_tab(); return true;
				case key::O: open(window); return true;
				default: break;
			}
		}

		if (is_editing_mode()) {
			if (has_ctrl) {
				if (has_shift) {
					switch (k) {
						case key::E: reveal_in_explorer(window); return true;
						case key::F5: fill_with_minimal_scene(); return true;
						case key::TAB: prev_tab(); return true;
						default: break;
					}
				}

				switch (k) {
					case key::S: save(window); return true;
					case key::COMMA: go_to_all(); return true;
					case key::TAB: next_tab(); return true;
					default: break;
				}
			}

			switch (k) {
				case key::F12: save_as(window); return true;
				default: break;
			}

			if (has_shift) {
				switch (k) {
					case key::F5: fill_with_test_scene(); return true;
					default: break;
				}
			}
		}
	}

	return false;
}

bool editor_setup::handle_input_before_game(
	const app_ingame_intent_map& app_controls,
	const necessary_images_in_atlas_map& sizes_for_icons,

	const augs::event::state& common_input_state,
	const augs::event::change e,

	augs::window&
) {
	using namespace augs::event;
	using namespace augs::event::keys;

	if (!anything_opened()) {
		return false;
	}

	const bool has_ctrl{ common_input_state[key::LCTRL] };
	const bool has_shift{ common_input_state[key::LSHIFT] };

	if (e.was_any_key_pressed()) {
		switch (e.data.key.key) {
			case key::NUMPAD0: player().set_speed(1.0); return true;
			case key::NUMPAD1: player().set_speed(0.01); return true;
			case key::NUMPAD2: player().set_speed(0.05); return true;
			case key::NUMPAD3: player().set_speed(0.1); return true;
			default: break;
		}
	}

	if (arena_gui.control({ app_controls, common_input_state, e })) { 
		return true;
	}

	if (is_editing_mode()) {
		auto& cosm = work().world;

		if (e.was_any_key_pressed()) {
			const auto k = e.data.key.key;

			if (has_ctrl) {
				switch(k) {
					case key::BACKSPACE: finish_and_discard(); return true; 
					case key::ENTER: finish_and_reapply(); return true;

					case key::LEFT: mirror_selection(vec2i(-1, 0)); return true;
					case key::RIGHT: mirror_selection(vec2i(1, 0)); return true;
					case key::UP: mirror_selection(vec2i(0, -1)); return true;
					case key::DOWN: mirror_selection(vec2i(0, 1)); return true;
					default: break;
				}
			}
		}

		if (const auto maybe_eye = find_current_camera_eye()) {
			const auto current_eye = *maybe_eye;

			if (const auto result = view().marks.control(e, current_eye); 
				result.result != augs::marks_result_type::NONE
			) {
				if (result.result == augs::marks_result_type::JUMPED) {
					view().panned_camera = result.chosen_value;
				}

				return true;
			}

			const auto world_cursor_pos = get_world_cursor_pos(current_eye);

			const auto screen_size = vec2i(ImGui::GetIO().DisplaySize);
			const auto current_cone = camera_cone(current_eye, screen_size);

			if (editor_detail::handle_camera_input(
				settings.camera,
				current_cone,
				common_input_state,
				e,
				world_cursor_pos,
				view().panned_camera
			)) {
				return true;
			}

			if (e.msg == message::mousemotion) {
				if (mover.do_mousemotion(make_mover_input(), world_cursor_pos)) {
					return true;
				}

				selector.do_mousemotion(
					sizes_for_icons,
					cosm,
					view().rect_select_mode,
					world_cursor_pos,
					current_eye,
					common_input_state[key::LMOUSE],
					view().get_effective_selecting_filter()
				);

				return true;
			}

			if (e.was_pressed(key::LMOUSE)) {
				if (mover.do_left_press(make_mover_input())) {
					return true;	
				}
			}

			{
				auto& selections = view_ids().selected_entities;

				if (e.was_pressed(key::LMOUSE)) {
					selector.do_left_press(cosm, has_ctrl, world_cursor_pos, selections);
					return true;
				}
				else if (e.was_released(key::LMOUSE)) {
					selections = selector.do_left_release(has_ctrl, make_grouped_selector_op_input());
				}
			}
		}

		if (e.was_any_key_pressed()) {
			const auto k = e.data.key.key;

			if (has_ctrl) {
				if (has_shift) {
					switch (k) {
						case key::Z: redo(); return true;
						default: break;
					}
				}

				switch (k) {
					case key::W: close_folder(); return true;
					case key::A: select_all_entities(has_ctrl); return true;
					case key::_0: view().reset_zoom(); return true;
					case key::Z: undo(); return true;
					case key::C: copy(); return true;
					case key::X: cut(); return true;
					case key::V: paste(); return true;

					case key::G: group_selection(); return true;
					case key::U: ungroup_selection(); return true;
					case key::R: mover.rotate_selection_once_by(make_mover_input(), -90); return true;
					default: break;
				}
			}

			auto reperform_selector = [&]() {
				if (const auto maybe_eye = find_current_camera_eye()) {
					const auto current_eye = *maybe_eye;
					const auto world_cursor_pos = get_world_cursor_pos(current_eye);

					selector.do_mousemotion(
						sizes_for_icons,
						cosm,
						view().rect_select_mode,
						world_cursor_pos,
						current_eye,
						false,
						view().get_effective_selecting_filter()
					);
				}
			};

			if (has_shift) {
				switch (k) {
					case key::Z: center_view_at_selection(); view().reset_zoom(); return true;
					case key::O: override_viewed_entity({}); view().reset_panning(); return true;
					case key::R: mover.rotate_selection_once_by(make_mover_input(), 90); return true;
					case key::E: mover.start_resizing_selection(make_mover_input(), true); return true;

					case key::H: mover.flip_selection(make_mover_input(), flip_flags::make_horizontally()); return true;
					case key::V: mover.flip_selection(make_mover_input(), flip_flags::make_vertically()); return true;

					case key::_1: view().rect_select_mode = editor_rect_select_type::EVERYTHING; return true;
					case key::_2: view().rect_select_mode = editor_rect_select_type::SAME_LAYER; return true;
					case key::_3: view().rect_select_mode = editor_rect_select_type::SAME_FLAVOUR; return true;
					default: break;
				}
			}

			auto clamp_units = [&]() { view().grid.clamp_units(8, settings.grid.render.get_maximum_unit()); };

			if (!has_shift && !has_ctrl) {
				switch (k) {
					case key::O: 
						if (view_ids().selected_entities.size() == 1) { 
							override_viewed_entity(*view_ids().selected_entities.begin()); 
						}
						return true;
					case key::A: view().toggle_ignore_groups(); return true;
					case key::Z: center_view_at_selection(); return true;
					case key::I: player().begin_recording(folder()); return true;
					case key::L: player().begin_replaying(folder()); return true;
					case key::G: view().toggle_grid(); return true;
					case key::S: view().toggle_snapping(); return true;
					case key::OPEN_SQUARE_BRACKET: view().grid.decrease_grid_size(); clamp_units(); return true;
					case key::CLOSE_SQUARE_BRACKET: view().grid.increase_grid_size(); clamp_units(); return true;
					case key::C: duplicate_selection(); return true;
					case key::D: cut_selection(); return true;
					case key::DEL: delete_selection(); return true;
					case key::W: mover.reset_rotation(make_mover_input()); return true;
					case key::E: mover.start_resizing_selection(make_mover_input(), false); return true;
					case key::R: mover.start_rotating_selection(make_mover_input()); return true;
					case key::T: mover.start_moving_selection(make_mover_input()); return true;
					case key::ADD: player().request_steps(1); return true;
					case key::SUBTRACT: player().seek_backward(1, make_command_input()); return true;
					case key::H: hide_layers_of_selected_entities(); reperform_selector(); return true;
					case key::U: unhide_all_layers(); reperform_selector(); return true;
					case key::SLASH: go_to_entity(); return true;
					case key::PAUSE: make_command_input().purge_selections(); return true;
					default: break;
				}
			}
		}
	}

	return false;
}

void editor_setup::hide_layers_of_selected_entities() {
	if (anything_opened()) {
		auto& vf = view().viewing_filter;

		if (vf.is_enabled) {
			selector.clear();

			auto& cosm = work().world;

			for_each_selected_entity(
				[&](const auto e) {
					const auto l = calc_render_layer(cosm[e]);
					vf.value.layers[l] = false;
				}
			);
		}
	}
}

void editor_setup::unhide_all_layers() {
	if (anything_opened()) {
		auto& vf = view().viewing_filter;

		if (vf.is_enabled) {
			selector.clear();

			for (auto& f : vf.value.layers) {
				f = true;
			}
		}
	}
}

std::optional<vec2> editor_setup::find_world_cursor_pos() const {
	if (const auto camera = find_current_camera_eye()) {
		return get_world_cursor_pos(camera.value());
	}

	return std::nullopt;
}

vec2 editor_setup::get_world_cursor_pos(const camera_eye eye) const {
	const auto mouse_pos = vec2i(ImGui::GetIO().MousePos);
	const auto screen_size = vec2i(ImGui::GetIO().DisplaySize);

	return camera_cone(eye, screen_size).to_world_space(mouse_pos);
}

const editor_view* editor_setup::find_view() const {
	if (anything_opened()) {
		return std::addressof(view());
	}

	return nullptr;
}

std::optional<ltrb> editor_setup::find_selection_aabb() const {
	if (is_editing_mode()) {
		return selector.find_selection_aabb(work().world, make_grouped_selector_op_input());
	}

	return std::nullopt;
}

std::optional<rgba> editor_setup::find_highlight_color_of(const entity_id id) const {
	if (is_editing_mode()) {
		return selector.find_highlight_color_of(
			settings.entity_selector, id, make_grouped_selector_op_input()
		);
	}

	return std::nullopt;
}

augs::path_type editor_setup::get_unofficial_content_dir() const {
	if (anything_opened()) {
		return folder().current_path;
	}

	return {};
}

augs::maybe<render_layer_filter> editor_setup::get_render_layer_filter() const {
	if (const auto v = find_view()) {
		return v->viewing_filter;
	}

	return render_layer_filter::disabled();
}

void editor_setup::draw_custom_gui(const draw_setup_gui_input& in) {
	auto on_screen = [in](const auto p) {
		return in.cone.to_screen_space(p);
	};

	auto& triangles = in.drawer;
	auto& lines = in.line_drawer;
	const auto screen_size = in.screen_size;
	auto& editor_cfg = in.config.editor;

	for_each_icon(
		in.all_visible,
		in.config.faction_view,
		[&](const auto typed_handle, const auto image_id, const transformr world_transform, const rgba color) {
			const auto screen_space = transformr(vec2i(on_screen(world_transform.pos)), world_transform.rotation);

			const auto image_size = in.necessary_images[image_id].get_original_size();

			const auto blank_tex = triangles.default_texture;

			if (auto active_color = find_highlight_color_of(typed_handle.get_id())) {
				active_color->a = static_cast<rgba_channel>(std::min(1.8 * active_color->a, 255.0));

				augs::detail_sprite(
					triangles.output_buffer,
					blank_tex,
					image_size + vec2i(10, 10),
					screen_space.pos,
					screen_space.rotation,
					*active_color
				);

				active_color->a = static_cast<rgba_channel>(std::min(2.2 * active_color->a, 255.0));

				lines.border(
					image_size,
					screen_space.pos,
					screen_space.rotation,
					*active_color,
					border_input { 1, 0 }
				);
			}

			augs::detail_sprite(
				triangles.output_buffer,
				in.necessary_images.at(image_id),
				screen_space.pos,
				screen_space.rotation,
				color
			);

			lines.border(
				image_size,
				screen_space.pos,
				screen_space.rotation,
				color,
				border_input { 1, 2 }
			);

			::draw_area_indicator(typed_handle, lines, screen_space, in.cone.eye.zoom, 1.f, drawn_indicator_type::EDITOR, color);
		}	
	);

	if (auto eye = find_current_camera_eye()) {
		eye->transform.pos.discard_fract();

		if (const auto view = find_view()) {
			if (view->show_grid && is_editing_mode()) {
				triangles.grid(
					screen_size,
					view->grid.unit_pixels,
					*eye,
					editor_cfg.grid.render
				);
			}
		}

		if (const auto selection_aabb = find_selection_aabb()) {
			auto col = white;

			if (is_mover_active()) {
				col.a = 120;
			}

			triangles.border(
				camera_cone(*eye, screen_size).to_screen_space(*selection_aabb),
				col,
				border_input { 1, -1 }
			);
		}
	}

	for_each_dashed_line(
		[&](vec2 from, vec2 to, const rgba color, const double secs = 0.0, bool fatten = false) {
			const auto a = on_screen(from.round_fract());
			const auto b = on_screen(to.round_fract());

			lines.dashed_line(a, b, color, 5.f, 5.f, secs);

			if (fatten) {
				const auto ba = b - a;
				const auto perp = ba.perpendicular_cw().normalize();
				lines.dashed_line(a + perp, b + perp, color, 5.f, 5.f, secs);
				lines.dashed_line(a + perp * 2, b + perp * 2, color, 5.f, 5.f, secs);
			}
		}	
	);

	if (const auto r = find_screen_space_rect_selection(screen_size, in.mouse_pos)) {
		triangles.aabb_with_border(
			*r,
			editor_cfg.rectangular_selection_color,
			editor_cfg.rectangular_selection_border_color
		);
	}

	draw_mode_gui(in);
	draw_status_bar(in);
	draw_recent_message(in);
	draw_marks_gui(in);
}

void editor_setup::draw_status_bar(const draw_setup_gui_input& in) {
	using namespace augs::gui::text;

	const auto ss = in.screen_size;
	const auto padding = vec2i(2, 2);

	if (const auto current_eye = find_current_camera_eye()) {
		const auto st = style(in.gui_fonts.gui);

		{
			const auto zoom = current_eye->zoom * 100.f;
			const auto zoom_text = typesafe_sprintf("%x%", zoom);

			print_stroked(
				in.drawer,
				ss - padding,
				formatted_string(zoom_text, st),
				augs::ralign::RB
			);
		}

#if 0
		if (const auto wp = find_world_cursor_pos()) {
			const auto world_cursor_pos_text = typesafe_sprintf("X: %x Y: %x", wp->x, wp->y);

			print_stroked(
				in.drawer,
				vec2i(padding.x, ss.y - padding.y),
				formatted_string(world_cursor_pos_text, st),
				augs::ralign::LB
			);
		}
#endif

		{
			const auto& v = view();

			std::string total_text;
			std::string separator = "  ";

			total_text += "Groups: ";

			if (v.ignore_groups) {
				total_text += "No ";
			}
			else {
				total_text += "Yes";
			}

			total_text += separator;
			total_text += "Snap: ";

			if (v.snapping_enabled) {
				total_text += "Yes";
			}
			else {
				total_text += "No ";
			}

			total_text += separator;
			total_text += "Grid: ";

			{
				const auto& g = v.grid;
				total_text += typesafe_sprintf("%x/%x", g.unit_pixels, settings.grid.render.get_maximum_unit());
			}

			total_text += separator;
			total_text += "RSM: ";

			{
				switch (v.rect_select_mode) {
					case editor_rect_select_type::EVERYTHING: total_text += "E"; break;
					case editor_rect_select_type::SAME_LAYER: total_text += "L"; break;
					case editor_rect_select_type::SAME_FLAVOUR: total_text += "F"; break;
					default: break;
				}
			}

			if (const auto viewed = get_viewed_character()) {
				total_text += separator;
				total_text += "Controlling: ";
				total_text += viewed.get_name();
			}

			auto formatted_status_bar_text = formatted_string(total_text, st);

			if (player().has_testing_started() && is_editing_mode()) {
				const auto st = style(in.gui_fonts.gui, yellow);

				formatted_status_bar_text = formatted_string("Paused the playtesting\n", st) + formatted_status_bar_text;
			}

			print_stroked(
				in.drawer,
				vec2i(padding.x, ss.y - padding.y),
				formatted_status_bar_text,
				augs::ralign::LB
			);
		}
	}

}

#include "3rdparty/sol2/sol/unicode.hpp"

void editor_setup::draw_marks_gui(const draw_setup_gui_input& in) {
	if (anything_opened() && is_editing_mode()) {
		const auto& marks = view().marks;

		if (marks.state != augs::marks_state::NONE) {
			using namespace augs::gui::text;

			const auto& fnt = in.gui_fonts.gui;

			auto colored = [&](const auto& s, const rgba col = white) {
				const auto st = style(fnt, col);
				return formatted_string(s, st);
			};

			auto marks_text = colored("Marks ");

			if (marks.state == augs::marks_state::REMOVING) {
				marks_text += colored("(Removing - Backspace to stop)\n\n", red);
			}
			else {
				marks_text += colored("(Backspace to start removing, Del clears)\n\n");
			}

			auto add_mark = [&](const auto& char_text, const auto& value) {
				marks_text += colored(char_text, yellow);
				marks_text += colored(typesafe_sprintf("     x: %x, y: %x, zoom: %x\n", value.transform.pos.x, value.transform.pos.y, value.zoom));
			};

			add_mark("'", marks.previous);

			for (const auto& m : marks.marks) {
				const auto& code_point = m.first;
				const auto utf8 = sol::unicode::code_point_to_utf8(code_point);

				auto char_text = std::string();

				for (std::size_t i = 0; i < utf8.code_units_size; ++i) {
					char_text += utf8.code_units[i];
				}

				add_mark(char_text, m.second);
			}

			const auto bbox = get_text_bbox(marks_text);

			const auto ss = in.screen_size;
			const auto& cfg = settings.action_indicator;

			const auto text_padding = cfg.text_padding;

			const auto& out = in.drawer;

			const auto rect_pos = ss / 2 - bbox / 2 - text_padding * 2;
			const auto text_pos = rect_pos + text_padding;
			const auto rect_size = bbox + text_padding * 2;
			const auto rect = xywh(rect_pos, rect_size);

			out.aabb_with_border(rect, cfg.bg_color, cfg.bg_border_color);

			print_stroked(
				out,
				text_pos,
				marks_text
			);
		}
	}
}

void editor_setup::draw_recent_message(const draw_setup_gui_input& in) {
	if (anything_opened() && is_editing_mode()) {
		const auto& h = folder().history;

		using namespace augs::gui::text;
		using O = augs::history_op_type;

		const auto& fnt = in.gui_fonts.gui;

		auto colored = [&](const auto& s, const rgba col = white) {
			const auto st = style(fnt, col);
			return formatted_string(s, st);
		};

		auto make_colorized = [&](auto dest) {
			formatted_string result;

			auto try_preffix = [&](const auto& preffix, const auto col) {
				if (begins_with(dest, preffix)) {
					cut_preffix(dest, preffix);
					result = colored(preffix, col) + colored(dest);
					return true;
				}

				return false;
			};

			if (try_preffix("Deleted", red)
				|| try_preffix("Cannot", red)
				|| try_preffix("Discarded", orange)
				|| try_preffix("Reapplied", pink)
				|| try_preffix("Successfully", green)
				|| try_preffix("Filled", green)
				|| try_preffix("Exported", green)
				|| try_preffix("Written", green)
				|| try_preffix("Saved", green)
				|| try_preffix("Altered", yellow)
				|| try_preffix("Grouped", yellow)
				|| try_preffix("Ungrouped", orange)
				|| try_preffix("Set", yellow)
				|| try_preffix("Moved", yellow)
				|| try_preffix("Rotated", yellow)
				|| try_preffix("Resized", yellow)
				|| try_preffix("Renamed", yellow)
				|| try_preffix("Changed", yellow)
				|| try_preffix("Created", green)
				|| try_preffix("Started", green)				
				|| try_preffix("Started tracking", green)				
				|| try_preffix("Duplicated", cyan)
				|| try_preffix("Mirrored", cyan)
			) {
				return result;
			}

			return colored(dest);
		};

		augs::date_time considered_stamp;
		formatted_string message_text;

		if (const auto op = h.get_last_op(); op.stamp > recent_message.stamp) {
			auto get_description = [&](const auto& from_command) -> decltype(auto) {
				return std::visit(
					[&](const auto& typed_command) -> decltype(auto) {
						{
							using T = remove_cref<decltype(typed_command)>;

							static constexpr bool show_only_if_undid_or_redid = 
								is_one_of_v<T, resize_entities_command, move_entities_command>
							;

							if constexpr(show_only_if_undid_or_redid) {
								if (op.type == O::EXECUTE_NEW) {
									return colored("");
								}
							}
						}

						return make_colorized(typed_command.describe());

					},
					from_command
				);
			};

			const auto description = [&]() {
				if (op.type == O::UNDO) {
					if (h.has_next_command()) {
						const auto preffix = colored("Undid ", orange);
						const auto& cmd = h.next_command();

						return preffix + get_description(cmd);
					}
				}
				else if (op.type == O::REDO) {
					if (h.has_last_command()) {
						const auto preffix = colored("Redid ", pink);
						const auto& cmd = h.last_command();

						return preffix + get_description(cmd);
					}
				}
				else if (op.type == O::EXECUTE_NEW) {
					if (h.has_last_command()) {
						const auto& cmd = h.last_command();

						return get_description(cmd);
					}

				}

				return colored("Unknown op type");
			}();

			if (description.size() > 0) {
				message_text = colored(typesafe_sprintf("#%x: ", 1 + h.get_current_revision())) + description;
			}
		}
		else {
			message_text = make_colorized(recent_message.content);
			considered_stamp = recent_message.stamp;
		}

		if (message_text.size() > 0) {
			const auto& cfg = settings.action_indicator;

			if (considered_stamp.seconds_ago() <= cfg.show_for_ms / 1000) {
				/* TODO: (LOW) Improve granularity to milliseconds */

				const auto ss = in.screen_size;
				const auto rb_space = cfg.offset;
				const auto text_padding = cfg.text_padding;
				const auto wrapping = cfg.max_width;

				const auto bbox = get_text_bbox(message_text, wrapping, false);
				//const auto line_h = static_cast<int>(fnt.metrics.get_height());
				//bbox.y = std::max(bbox.y, line_h * 2);

				const auto& out = in.drawer;

				const auto rect_pos = ss - rb_space - bbox - text_padding * 2;
				const auto text_pos = rect_pos + text_padding;
				const auto rect_size = bbox + text_padding * 2;
				const auto rect = xywh(rect_pos, rect_size);

				out.aabb_with_border(rect, cfg.bg_color, cfg.bg_border_color);

				print_stroked(
					out,
					text_pos,
					message_text,
					augs::ralign_flags {},
					black,
					wrapping
				);
			}
		}
	}
}

template <class F>
void editor_setup::on_mode_with_input(F&& callback) const {
	if (anything_opened()) {
		auto& f = folder();

		player().on_mode_with_input(
			f.commanded->mode_rules.vars,
			f.commanded->work.world,
			std::forward<F>(callback)
		);
	}
}

void editor_setup::draw_mode_gui(const draw_setup_gui_input& in) {
	if (anything_opened()) {
		on_mode_with_input(
			[&](const auto& typed_mode, const auto& mode_input) {
				const auto draw_mode_in = draw_mode_gui_input { 
					get_game_screen_top(), 
					view().local_player, 
					in.images_in_atlas,
					in.config
				};

				arena_gui.draw_mode_gui(in, draw_mode_in, typed_mode, mode_input);
			}
		);
	}
}

void editor_setup::ensure_handler() {
	if (anything_opened()) {
		player().ensure_handler(); 
		force_autosave_now();
	}
}

void editor_setup::set_current(const folder_index i) {
	if (signi.current_index != i) {
		on_folder_changed();
	}

	signi.current_index = i;
}

bool editor_setup::anything_opened() const {
	return 
		signi.folders.size() > 0 
		&& signi.current_index != static_cast<folder_index>(-1) 
		&& signi.current_index < signi.folders.size()
	;
}

void editor_setup::finish_and_discard() {
	if (anything_opened()) { 
		auto& p = player();

		if (p.has_testing_started()) {
			player().finish_testing(make_command_input(), finish_testing_type::DISCARD_CHANGES);

			recent_message.set("Discarded the playtesting session");
		}
	}
}

void editor_setup::finish_and_reapply() {
	if (anything_opened()) { 
		auto& p = player();

		if (p.has_testing_started()) {
			player().finish_testing(make_command_input(), finish_testing_type::REAPPLY_CHANGES);

			recent_message.set("Reapplied changes done during the playtesting session");
		}
	}
}

editor_folder& editor_setup::folder() {
	return signi.folders[signi.current_index];
}

const editor_folder& editor_setup::folder() const {
	return signi.folders[signi.current_index];
}

intercosm& editor_setup::work() {
	return folder().commanded->work;
}

const intercosm& editor_setup::work() const {
	return folder().commanded->work;
}

editor_player& editor_setup::player() {
	return folder().player;
}

const editor_player& editor_setup::player() const {
	return folder().player;
}

editor_view& editor_setup::view() {
	return folder().view;
}

const editor_view& editor_setup::view() const {
	return folder().view;
}

editor_view_ids& editor_setup::view_ids() {
	return folder().commanded->view_ids;
}

const editor_view_ids& editor_setup::view_ids() const {
	return folder().commanded->view_ids;
}

template struct augs::marks<camera_eye>;
