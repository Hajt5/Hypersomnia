#include "augs/gui/text/printer.h"
#include "view/mode_gui/arena/arena_scoreboard_gui.h"
#include "game/cosmos/entity_handle.h"
#include "game/modes/bomb_mode.hpp"
#include "application/setups/draw_setup_gui_input.h"
#include "application/config_lua_table.h"

bool arena_scoreboard_gui::control(
	const augs::event::state& common_input_state,
	const augs::event::change e
) {
	using namespace augs::event;
	using namespace augs::event::keys;

	(void)common_input_state;
	/* const bool has_ctrl{ common_input_state[key::LCTRL] }; */
	/* const bool has_shift{ common_input_state[key::LSHIFT] }; */

	if (e.was_pressed(key::TAB)) {
		show = true;
		return true;
	}

	if (e.was_released(key::TAB)) {
		show = false;
		return true;
	}

	return false;
}

template <class M>
void arena_scoreboard_gui::draw_gui(
	const draw_setup_gui_input& in,
	const draw_mode_gui_input& mode_in,

	const M& typed_mode, 
	const typename M::input& mode_input
) const {
	(void)mode_in;
	(void)typed_mode;
	(void)mode_input;

	using namespace augs::gui::text;

	const auto& cfg = in.config.arena_mode_gui.scoreboard_settings;

	const auto content_pad = vec2i(12, 8);
	const auto cell_pad = vec2i(4, 4);

	const auto& o = in.drawer;

	const auto window_bg_rect = ltrbi(vec2i::zero, in.screen_size).expand_from_center_mult(0.6f);
	o.aabb_with_border(window_bg_rect, cfg.background_color, cfg.border_color);

	const auto sz = window_bg_rect.get_size() - content_pad * 2;
	const auto lt = window_bg_rect.left_top() + content_pad;

	auto make_style = [&](const auto of_col = white) {
		return style(in.gui_font, of_col);
	};

	auto fmt = [&](const auto& text, const rgba col = white) {
		return formatted_string(text, make_style(col));
	};

	auto calc_size = [&](const auto& text) {
		return get_text_bbox(fmt(text));
	};

	vec2i pen = lt;

	auto text = [&](const auto& ss, const rgba col, vec2i where, auto... args) {
		where += pen;

		print(
			o,
			where,
			fmt(ss, col),
			args...
		);
	};
	(void)text;

	auto text_stroked = [&](const auto& ss, const rgba col, vec2i where, auto... args) {
		where += pen;

		print_stroked(
			o,
			where,
			fmt(ss, col),
			args...
		);
	};

	const auto window_name = "Scoreboard";
	const auto window_name_size = calc_size(window_name);

	{
		text_stroked(window_name, white, { sz.x / 2 - window_name_size.x / 2, 0 });
	}

	pen.y += window_name_size.y;

	struct column {
		int w;
		std::string label;
		bool align_right = false;

		column(
			int w, 
			std::string s,
			bool align_right = false
		) : w(w), label(s), align_right(align_right) {}

		int l;
		int r;
	};

	std::vector<column> columns = {
		{ calc_size("9999").x, "Ping" },
		{ 22, " " },
		{ sz.x, "Player" },
		{ calc_size("999999$").x, "Money", true },

		{ calc_size("999").x, "K", true },
		{ calc_size("999").x, "A", true },
		{ calc_size("999").x, "D", true },

		{ calc_size("999999").x, "Score", true },
	};

	const auto font_h = in.gui_font.metrics.get_height();
	const auto cell_h = font_h + cell_pad.y * 2;

	pen.y += cell_h;

	auto& player_col = columns[2];

	for (auto& c : columns) {
		c.w += cell_pad.x * 2;
	}

	for (const auto& c : columns) {
		if (&c != &player_col) {
			player_col.w -= c.w;
		}
	}

	{
		int ll = 0;

		for (auto& c : columns) {
			c.l = ll;
			c.r = c.l + c.w;

			ll = c.r;
		}
	}

	for (const auto& c : columns) {
		if (c.align_right) {
			text_stroked(c.label, gray, vec2i(c.r - calc_size(c.label).x, 0));
		}
		else {
			text_stroked(c.label, gray, vec2i(c.l, 0));
		}
	}

	pen.y += cell_h;

	auto aabb = [&](auto orig, rgba col) {
		orig.l += pen.x;
		orig.t += pen.y;
		orig.r += pen.x;
		orig.b += pen.y;
		col.a = cfg.elements_alpha;

		o.aabb(orig, col);
	};

	auto print_faction = [&](const faction_type faction, const bool on_top) {
		(void)on_top;
		auto& cols = in.config.faction_view.colors[faction];

		const auto& cosm = mode_input.cosm;
		(void)cosm;

		std::vector<std::pair<bomb_mode_player, mode_player_id>> sorted_players;

		typed_mode.for_each_player_in(faction, [&](
			const auto& id, 
			const auto& player
		) {
			sorted_players.emplace_back(player, id);
		});

		sort_range(sorted_players);

		const auto h = sorted_players.size() * cell_h;

		for (const auto& c : columns) {
			aabb(
				ltrbi(c.l, 0, c.l + 1, h - 1),
				cols.background_darker
			);
		}

		for (const auto& p : sorted_players) {
			const auto i = index_in(sorted_players, p);
			const auto y = i * cell_h;

			const auto cell_top = y;

			auto draw_cell_bg = [&](const rgba col) {
				for (const auto& c : columns) {
					const auto orig = ltrbi(c.l + 1, cell_top + 1, c.r, cell_top + cell_h);

					aabb(orig, col);

					aabb(
						ltrbi(c.l + 1, cell_top, c.r, cell_top + 1),
						cols.background_darker
					);
				}
			};

			draw_cell_bg(cols.background.normal);
		}
	};

	const auto participants = typed_mode.calc_participating_factions(mode_input);

	print_faction(participants.defusing, true);
	//print_faction(participants.bombing, false);

#if GGWP
	if (!show) {
		//return;
	}

	using namespace augs::imgui;

	ImGui::SetNextWindowPosCenter();

	ImGui::SetNextWindowSize((vec2(ImGui::GetIO().DisplaySize) * 0.6f).operator ImVec2(), ImGuiCond_FirstUseEver);

	const auto window_name = "Scoreboard";
	auto window = scoped_window(window_name, nullptr, ImGuiWindowFlags_NoTitleBar);

	{
		const auto s = ImGui::CalcTextSize(window_name, nullptr, true);
		const auto w = ImGui::GetWindowWidth();
		ImGui::SetCursorPosX(w / 2 - s.x / 2);
		text(window_name);
	}

	auto spacing = ImGui::GetStyle().ItemSpacing;
	spacing.y *= 1.9;
	auto spacing_scope = scoped_style_var(ImGuiStyleVar_ItemSpacing, spacing);

	const auto p = typed_mode.calc_participating_factions(mode_input);

	const auto num_columns = 8;

	const auto ping_col_w = ImGui::CalcTextSize("9999", nullptr, true).x;
	const auto point_col_w = ImGui::CalcTextSize("999", nullptr, true).x;
	const auto score_col_w = ImGui::CalcTextSize("999999", nullptr, true).x;
	const auto money_col_w = ImGui::CalcTextSize("999999$", nullptr, true).x;

	ImGui::Columns(num_columns, nullptr, false);

	const auto w_off = 12;

	ImGui::SetColumnWidth(0, ping_col_w + w_off);
	ImGui::SetColumnWidth(2, money_col_w + w_off);
	ImGui::SetColumnWidth(3, point_col_w + w_off);
	ImGui::SetColumnWidth(4, point_col_w + w_off);
	ImGui::SetColumnWidth(5, point_col_w + w_off);
	ImGui::SetColumnWidth(6, score_col_w + w_off);

	text_disabled("Ping");
	ImGui::NextColumn();
	text_disabled("Player");
	ImGui::NextColumn();
	text_disabled("Money");
	ImGui::NextColumn();
	text_disabled("K");
	ImGui::NextColumn();
	text_disabled("A");
	ImGui::NextColumn();
	text_disabled("D");
	ImGui::NextColumn();
	text_disabled("Score");
	ImGui::NextColumn();
	ImGui::NextColumn();

	ImGui::Columns(1);

	auto print_faction = [&](const faction_type faction, const bool on_top) {
		const auto orig_faction_col = white;
		const auto faction_color = rgba(orig_faction_col).multiply_rgb(1.2f);
		const auto disabled_faction_color = rgba(orig_faction_col).multiply_rgb(.6f);

		const auto faction_bg = rgba(orig_faction_col).multiply_rgb(.25f);
		const auto disabled_faction_bg = rgba(orig_faction_col).multiply_rgb(.085f);

		auto scope_separator = scoped_style_color(ImGuiCol_Separator, disabled_faction_bg);

		ImGui::Columns(num_columns, nullptr, false);

		const auto& cosm = mode_input.cosm;

		std::vector<std::pair<bomb_mode_player, mode_player_id>> sorted_players;

		typed_mode.for_each_player_in(faction, [&](
			const auto& id, 
			const auto& player
		) {
			sorted_players.emplace_back(player, id);
		});

		sort_range(sorted_players);

		auto draw_headline = [&]() {
			ImGui::Columns(1);

			const auto& faction_state = typed_mode.factions[faction];
			const auto faction_name = format_enum(faction);
			const auto headline = [&]() {
				const auto& score = faction_state.score;

				auto score_number = typesafe_sprintf("%x", score);

				if (score < 10) {
					score_number += " ";
				}

				const auto n_players = sorted_players.size();
				const auto n_players_conscious = typed_mode.num_conscious_players_in(cosm, faction);

				const auto l1 = typesafe_sprintf("%x for %x", score_number, faction_name);
				const auto l2 = typesafe_sprintf("Players conscious: %x/%x", n_players_conscious, n_players); 

				return std::make_pair(l1, l2);
			}();

			auto scope_header = scoped_style_color(ImGuiCol_Header, disabled_faction_bg);
			auto scope_header_hovered = scoped_style_color(ImGuiCol_HeaderHovered, disabled_faction_bg);
			auto scope_header_active = scoped_style_color(ImGuiCol_HeaderActive, disabled_faction_bg);

			auto scope_text = scoped_text_color(faction_color);

			if (on_top) {
				ImGui::Selectable(" ", true);
			}

			ImGui::Selectable(headline.first.c_str(), true);

			{
				const auto w = ImGui::CalcTextSize(headline.second.c_str(), nullptr, true);
				ImGui::SameLine(ImGui::GetWindowWidth() - w.x - 10);
			}

			text_color(headline.second, faction_color);

			if (!on_top) {
				ImGui::Selectable(" ", true);
			}
		};

		if (!on_top) {
			draw_headline();
			ImGui::Columns(num_columns, nullptr, false);
		}

		for (const auto& s : sorted_players) {
			const auto& id = s.second;

			const auto& player = s.first;

			const auto player_handle = cosm[player.guid];
			const auto is_conscious = player_handle.alive() && player_handle.template get<components::sentience>().is_conscious();

			auto bg_color = is_conscious ? faction_bg : disabled_faction_bg;
			auto color = is_conscious ? faction_color : disabled_faction_color;

			if (id == draw_in.local_player) {
				bg_color.multiply_rgb(1.6f);
				color.multiply_rgb(1.9f);
			}

			auto scope = scoped_style_color(ImGuiCol_Button, bg_color);
			auto scope_text = scoped_text_color(color);
 
			const auto ping = 0;
			const auto ping_str = typesafe_sprintf("%x", ping);

			auto scope_header = scoped_style_color(ImGuiCol_Header, bg_color);
			auto scope_header_hovered = scoped_style_color(ImGuiCol_HeaderHovered, rgba(bg_color).multiply_rgb(1.4f));
			auto scope_header_active = scoped_style_color(ImGuiCol_HeaderActive, rgba(bg_color).multiply_rgb(1.6f));

			ImGui::Selectable(ping_str.c_str(), true, ImGuiSelectableFlags_SpanAllColumns);

			ImGui::NextColumn();
			text(player.chosen_name);
			ImGui::NextColumn();
			text(typesafe_sprintf("%x$", player.money));
			ImGui::NextColumn();
			text(typesafe_sprintf("%x", player.knockouts));
			ImGui::NextColumn();
			text(typesafe_sprintf("%x", player.assists));
			ImGui::NextColumn();
			text(typesafe_sprintf("%x", player.deaths));
			ImGui::NextColumn();

			text(typesafe_sprintf("%x", player.calc_score()));
			ImGui::NextColumn();
			ImGui::NextColumn();

			/* auto& sepcol = ImGui::GetStyle().Colors[ImGuiCol_Separator]; */
			/* auto prev = sepcol; */
			/* sepcol = white; */

			/* { */
			{
				auto spacing_sep_scope = scoped_style_var(ImGuiStyleVar_ItemSpacing, ImVec2(spacing.x, 0));
				auto b_scope_header = scoped_style_color(ImGuiCol_Header, disabled_faction_bg);
				auto b_scope_header_hovered = scoped_style_color(ImGuiCol_HeaderHovered, disabled_faction_bg);
				auto b_scope_header_active = scoped_style_color(ImGuiCol_HeaderActive, disabled_faction_bg);
				auto b_sep = scoped_style_color(ImGuiCol_Separator, disabled_faction_bg);

			/* 	auto idd = scoped_id(id.value); */
			//ImGui::Selectable("##dummy", true, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0, 0));
				ImGui::Separator();
			}

				next_columns(num_columns);
			/* } */
			/* auto bss = ImVec2(ImGui::GetWindowContentRegionMax().x, 2); */
			/* ImGui::Button("", bss); */

			/* auto spacing_sep_scope = scoped_style_var(ImGuiStyleVar_ItemSpacing, ImVec2(spacing.x, 1)); */
			/* ImGui::Separator(); */
			/* ImGui::Separator(); */
			/* sepcol = prev; */
		}

		if (on_top) {
			draw_headline();
		}

		ImGui::Columns(1);
	};

	print_faction(p.defusing, true);
	print_faction(p.bombing, false);
#endif
}


template void arena_scoreboard_gui::draw_gui(
	const draw_setup_gui_input&,
	const draw_mode_gui_input&, 

	const bomb_mode& mode, 
	const typename bomb_mode::input&
) const;
