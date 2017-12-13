#include <iostream>

#if PLATFORM_UNIX
#include <csignal>
#endif

#include "augs/unit_tests.h"
#include "augs/global_libraries.h"

#include "augs/templates/always_false.h"
#include "augs/templates/container_templates.h"

#include "augs/filesystem/file.h"
#include "augs/filesystem/directory.h"

#include "augs/math/matrix.h"

#include "augs/misc/imgui/imgui_utils.h"
#include "augs/misc/machine_entropy.h"
#include "augs/misc/lua/lua_utils.h"

#include "augs/graphics/renderer.h"

#include "augs/window_framework/shell.h"
#include "augs/window_framework/window.h"
#include "augs/window_framework/platform_utils.h"
#include "augs/audio/audio_context.h"

#include "game/organization/all_component_includes.h"
#include "game/organization/all_messages_includes.h"

#include "game/transcendental/data_living_one_step.h"
#include "game/transcendental/cosmos.h"

#include "view/viewables/loaded_sounds.h"
#include "view/viewables/atlas_distributions.h"

#include "view/game_gui/game_gui_system.h"

#include "view/audiovisual_state/world_camera.h"
#include "view/audiovisual_state/audiovisual_state.h"
#include "view/rendering_scripts/illuminated_rendering.h"

#include "application/session_profiler.h"
#include "application/config_lua_table.h"

#include "application/gui/settings_gui.h"
#include "application/gui/ingame_menu_gui.h"

#include "application/setups/main_menu_setup.h"
#include "application/setups/test_scene_setup.h"
#include "application/setups/editor/editor_setup.h"

#include "application/main/imgui_pass.h"
#include "application/main/draw_debug_details.h"
#include "application/main/release_flags.h"

#include "cmd_line_params.h"

extern std::string help_contents;

int work(const int argc, const char* const * const argv);

#if PLATFORM_WINDOWS
#if BUILD_IN_CONSOLE_MODE
int main(const int argc, const char* const * const argv) {
#else
int __stdcall WinMain(HINSTANCE, HINSTANCE, char*, int) {
	const auto argc = __argc;
	const auto argv = __argv;
#endif
#elif PLATFORM_UNIX
int main(const int argc, const char* const * const argv) {
#else
#error "Unsupported platform!"
#endif

	if (cmd_line_params(argc, argv).help_only) {
		std::cout << help_contents << std::endl;
		
		return EXIT_SUCCESS;
	}

	const auto exit_code = work(argc, argv);

	{
		const auto logs = program_log::get_current().get_complete(); 

		switch (exit_code) {
			case EXIT_SUCCESS: 
				augs::save_as_text(LOG_FILES_DIR "exit_success_debug_log.txt", logs); 
				break;
			case EXIT_FAILURE: 
				{
					const auto failure_log_path = augs::path_type(LOG_FILES_DIR "exit_failure_debug_log.txt");
					augs::save_as_text(failure_log_path, logs);
					
					augs::open_text_editor(failure_log_path.string());
				}

				break;
			default: break;
		}
	}

	return exit_code;
}

/*
	static is used for variables because some are huge, especially setups.
	On the other hand, to preserve the destruction in the order of definition,
	we must make all variables static, otherwise the huge resources marked static
	would get destructed last, possibly causing bugs.

	this function will also be called only once.
*/

int work(const int argc, const char* const * const argv) try {
	static const auto params = cmd_line_params(argc, argv);

	augs::create_directories(LOG_FILES_DIR);
	augs::create_directories(GENERATED_FILES_DIR);
	augs::create_directories(LOCAL_FILES_DIR);

	static const auto canon_config_path = augs::path_type("config.lua");
	static const auto local_config_path = augs::path_type(LOCAL_FILES_DIR "config.local.lua");

	static auto lua = augs::create_lua_state();

	static config_lua_table config { 
		lua, 
		augs::switch_path(
			canon_config_path,
			local_config_path
		)
	};

	augs::imgui::init(
		LOCAL_FILES_DIR "imgui.ini",
		LOG_FILES_DIR "imgui_log.txt",
		config.gui_style
	);

	static auto last_saved_config = config;

	static auto change_with_save = [](auto setter) {
		setter(config);
		setter(last_saved_config);

		last_saved_config.save(lua, local_config_path);
	};

	if (config.unit_tests.run) {
		augs::run_unit_tests(config.unit_tests);

		LOG("All unit tests have passed.");

		if (params.unit_tests_only) {
			return EXIT_SUCCESS;
		}
	}

	augs::generate_alsoft_ini(config.audio.max_number_of_sound_sources);

	static const augs::global_libraries libraries;
	
	static augs::window window(config.window);
	static augs::audio_context audio(config.audio);
	augs::log_all_audio_devices(LOG_FILES_DIR "audio_devices.txt");

	static augs::renderer renderer;

	static necessary_fbos fbos(
		window.get_screen_size(),
		config.drawing
	);

	static necessary_shaders shaders(
		"content/necessary/shaders",
		"content/necessary/shaders",
		config.drawing
	);

	static necessary_sound_buffers sounds(
		"content/necessary/sfx"
	);

	static necessary_image_loadables_map images(
		lua,
		"content/necessary/gfx",
		config.content_regeneration.regenerate_every_launch
	);
	
	static const auto imgui_atlas = augs::imgui::create_atlas();

	static const configuration_subscribers configurables {
		window,
		fbos,
		audio
	};

	/* 
		Main menu setup state may be preserved, 
		therefore it resides in a separate optional.
	*/

	using setup_variant = std::variant<
		test_scene_setup,
		editor_setup
	>;

	static std::optional<main_menu_setup> main_menu;
	static std::optional<setup_variant> current_setup;

	static settings_gui_state settings_gui;
	static ingame_menu_gui ingame_menu;

	static all_viewables_defs currently_loaded_defs;

	/*
		Runtime representations of viewables,
		loaded from the definitions provided by the current setup.
		The setup's chosen viewables_loading_type decides if they are 
		loaded just once or if they are for example continuously streamed.
	*/

	static loaded_sounds game_sounds;
	static game_images_in_atlas_map game_atlas_entries;
	static necessary_images_in_atlas necessary_atlas_entries;
	
#if STATICALLY_ALLOCATE_BAKED_FONTS
	static augs::baked_font gui_font;

	static auto get_gui_font = []() -> augs::baked_font& {
		return gui_font;
	};
#else
	static auto gui_font = std::make_unique<augs::baked_font>();

	static auto get_gui_font = []() -> augs::baked_font& {
		return *gui_font;
	};
#endif
	
	static std::optional<augs::graphics::texture> game_world_atlas;

	static world_camera gameplay_camera;
	static audiovisual_state audiovisuals;

	/*
		The lambdas that aid to make the main loop code more concise.
	*/	

	static auto visit_current_setup = [](auto callback) -> decltype(auto) {
		if (current_setup.has_value()) {
			return std::visit(
				[&](auto& setup) -> decltype(auto) {
					return callback(setup);
				}, 
				current_setup.value()
			);
		}
		else {
			return callback(main_menu.value());
		}
	};

	static auto on_specific_setup = [](auto callback) -> decltype(auto) {
		using T = std::decay_t<argument_of_t<decltype(callback), 0>>;

		if constexpr(std::is_same_v<T, main_menu_setup>) {
			if (main_menu.has_value()) {
				callback(*main_menu);
			}
		}
		else {
			if (current_setup.has_value()) {
				if (auto* setup = std::get_if<T>(&*current_setup)) {
					callback(*setup);
				}
			}
		}
	};

	/* TODO: We need to have one game gui per cosmos. */
	static game_gui_system game_gui;
	static bool game_gui_mode = false;

	static session_profiler profiler;

	static auto load_all = [](const all_viewables_defs& new_defs) {
		auto scope = measure_scope(profiler.reloading_viewables);

		auto equal = [](const auto& a, const auto& b) {
			return augs::equal_by_introspection(a, b);
		};
		
		/* Atlas pass */

		{
			bool new_atlas_required = false;
			
			if (!game_world_atlas.has_value()) {
				new_atlas_required = true;
			}

			/* Check for unloaded and changed resources */
			for (const auto& old : currently_loaded_defs.game_image_loadables) {
				if (const auto found = mapped_or_nullptr(new_defs.game_image_loadables, old.first)) {
					if (const bool reload = !equal(*found, old.second)) {
						/* Changed, reload */
						new_atlas_required = true;
					}
				}
				else {
					/* Missing, unload */
					new_atlas_required = true;
				}
			}
			
			/* Check for new resources */
			for (const auto& fresh : new_defs.game_image_loadables) {
				if (nullptr == mapped_or_nullptr(currently_loaded_defs.game_image_loadables, fresh.first)) {
					new_atlas_required = true;
				}
				/* Otherwise it's already taken care of */
			}

			if (new_atlas_required) {
				const auto settings = config.content_regeneration;

				game_world_atlas.emplace(standard_atlas_distribution({
					new_defs.game_image_loadables,
					images,
					config.gui_font,
					{
						static_cast<unsigned>(renderer.get_max_texture_size()),
						augs::path_type(GENERATED_FILES_DIR "atlases/game_world_atlas") 
							+= (settings.save_regenerated_atlases_as_binary ? ".bin" : ".png"),
						settings.regenerate_every_launch,
						settings.check_integrity_every_launch
					},
					game_atlas_entries,
					necessary_atlas_entries,
					get_gui_font()
				}));
			}
		}

		/* Sounds pass */

		{
			/* Check for unloaded and changed resources */
			for (const auto& old : currently_loaded_defs.sounds) {
				if (const auto found = mapped_or_nullptr(new_defs.sounds, old.first)) {
					if (const bool reload = !equal(*found, old.second)) {
						/* Changed, reload */
						game_sounds.at(old.first) = *found;
					}
				}
				else {
					/* Missing, unload */
					audiovisuals.get<sound_system>().clear_sources_playing(old.first);
					game_sounds.erase(old.first);
				}
			}

			/* Check for new resources */
			for (const auto& fresh : new_defs.sounds) {
				if (nullptr == mapped_or_nullptr(currently_loaded_defs.sounds, fresh.first)) {
					game_sounds.try_emplace(fresh.first, fresh.second);
				}
				/* Otherwise it's already taken care of */
			}
		}

		/* Done, overwrite */
		currently_loaded_defs = new_defs;
	};

	static auto setup_launcher = [](auto&& setup_init_callback) {
		audiovisuals.get<sound_system>().clear();

		current_setup = std::nullopt;
		ingame_menu.show = false;

		setup_init_callback();
		
		/* MSVC ICE workaround */
		auto& _load_all = load_all;

		visit_current_setup([&](const auto& setup) {
			using T = std::decay_t<decltype(setup)>;
			
			if constexpr(T::loading_strategy == viewables_loading_type::LOAD_ALL_ONLY_ONCE) {
				_load_all(setup.get_viewable_defs());
			}
		});
	};

	static auto launch_editor = [](auto&&... args) {
		setup_launcher([&]() {
			current_setup.emplace(std::in_place_type_t<editor_setup>(),
				std::forward<decltype(args)>(args)...
			);

			game_gui_mode = true;
		});
	};

	static auto launch_setup = [](const launch_type mode) {
		LOG("Launched mode: %x", augs::enum_to_string(mode));
		
		change_with_save([mode](config_lua_table& cfg) {
			cfg.launch_mode = mode;
		});

		switch (mode) {
			case launch_type::MAIN_MENU:
				setup_launcher([]() {
					if (!main_menu.has_value()) {
						main_menu.emplace(lua, config.main_menu);
					}
				});

				break;

			case launch_type::EDITOR:
				launch_editor(lua);

				break;

			case launch_type::TEST_SCENE:
				setup_launcher([]() {
					current_setup.emplace(std::in_place_type_t<test_scene_setup>(),
						lua,
						config.session.create_minimal_test_scene,
						config.get_input_recording_mode()
					);
				});

				break;

			default:
				ensure(false && "The launch_setup mode you have chosen is currently out of service.");
				break;
		}
	};

	static auto get_viewable_defs = []() -> const all_viewables_defs& {
		return visit_current_setup([](auto& setup) -> const all_viewables_defs& {
			return setup.get_viewable_defs();
		});
	};

	static auto create_game_gui_deps = []() {
		return game_gui_context_dependencies{
			get_viewable_defs().game_image_metas,
			game_atlas_entries,
			necessary_atlas_entries,
			get_gui_font()
		};
	};

	static auto create_menu_context_deps = [](const auto& viewing_config) {
		return menu_context_dependencies{
			necessary_atlas_entries,
			get_gui_font(),
			sounds,
			viewing_config.audio_volume
		};
	};

	static auto get_viewed_character = []() -> const_entity_handle {
		const auto& viewed_cosmos = visit_current_setup([](auto& setup) -> const cosmos& {
			return setup.get_viewed_cosmos();
		});

		const auto viewed_character_id = visit_current_setup([](auto& setup) {
			return setup.get_viewed_character_id();
		});

		return viewed_cosmos[viewed_character_id];
	};
		
	static auto should_draw_game_gui = []() {
		{
			bool should = true;

			on_specific_setup([&](editor_setup& setup) {
				if (setup.is_paused()) {
					should = false;
				}
			});

			if (main_menu.has_value() && !current_setup.has_value()) {
				should = false;
			}

			if (!should) {
				return false;
			}
		}

		const auto viewed = get_viewed_character();

		if (!viewed.alive()) {
			return false;
		}

		if (!get_viewed_character().has<components::item_slot_transfers>()) {
			return false;
		}

		return true;
	};

	static auto get_camera = []() {		
		if(const auto custom = visit_current_setup(
			[](const auto& setup) { 
				return setup.get_custom_camera(); 
			}
		)) {
			return *custom;
		}
		
		return gameplay_camera.get_current_cone();
	};

	static auto handle_app_intent = [](const app_intent_type intent) {
		using T = decltype(intent);

		switch (intent) {
			case T::SWITCH_DEVELOPER_CONSOLE: {
				change_with_save([](config_lua_table& cfg) {
					bool& f = cfg.session.show_developer_console;
					f = !f;
				});

				break;
			}

			default: break;
		}
	};
	
	static auto handle_app_ingame_intent = [](const app_ingame_intent_type intent) {
		using T = decltype(intent);

		switch (intent) {
			case T::CLEAR_DEBUG_LINES:
				DEBUG_PERSISTENT_LINES.clear();
				break;

			case T::SWITCH_WEAPON_LASER: {
				bool& f = config.drawing.draw_weapon_laser;
				f = !f;
				break;
			}

			case T::SWITCH_GAME_GUI_MODE: {
				bool& f = game_gui_mode;
				f = !f;
				break;
			}
			
			case T::SWITCH_CHARACTER: {

			}

			default: break;
		}
	};

#if PLATFORM_UNIX	
	static volatile std::sig_atomic_t signal_status = 0;
 
	static auto signal_handler = [](const int signal_type) {
   		signal_status = signal_type;
	};

#if IS_PRODUCTION_BUILD // If debugging, we use SIGINT for simulating a debugger break
	std::signal(SIGINT, signal_handler);
#endif
	std::signal(SIGTERM, signal_handler);
	std::signal(SIGSTOP, signal_handler);
#endif
 
	static bool should_quit = false;

	static auto do_main_menu_option = [](const main_menu_button_type t) {
		using T = decltype(t);

		switch (t) {
			case T::LOCAL_UNIVERSE:
				launch_setup(launch_type::TEST_SCENE);
				break;

			case T::EDITOR:
				launch_setup(launch_type::EDITOR);
				break;

			case T::SETTINGS:
				settings_gui.show = true;
				ImGui::SetWindowFocus("Settings");
				break;

			case T::CREATORS:
				main_menu.value().launch_creators_screen();
				break;

			case T::QUIT:
				should_quit = true;
				break;

			default: break;
		}
	};

	static auto do_ingame_menu_option = [](const ingame_menu_button_type t) {
		using T = decltype(t);

		switch (t) {
			case T::RESUME:
				ingame_menu.show = false;
				break;

			case T::QUIT_TO_MENU:
				launch_setup(launch_type::MAIN_MENU);
				break;

			case T::SETTINGS:
				settings_gui.show = true;
				ImGui::SetWindowFocus("Settings");
				break;

			case T::QUIT:
				should_quit = true;
				break;

			default: break;
		}
	};

	static auto setup_pre_solve = [](auto...) {
		renderer.save_debug_logic_step_lines_for_interpolation(DEBUG_LOGIC_STEP_LINES);
		DEBUG_LOGIC_STEP_LINES.clear();
	};

	/* 
		The audiovisual_step, advance_setup and advance_current_setup lambdas
		are separated only because MSVC outputs ICEs if they become nested.
	*/

	static visible_entities all_visible;

	static auto audiovisual_step = [](
		const augs::delta frame_delta,
		const double speed_multiplier,
		const config_lua_table& viewing_config
	) {
		const auto screen_size = window.get_screen_size();
		const auto viewed_character = get_viewed_character();
		const auto& cosmos = viewed_character.get_cosmos();
		
		audiovisuals.reserve_caches_for_entities(viewed_character.get_cosmos().get_entity_pool().capacity());
		
		auto& interp = audiovisuals.get<interpolation_system>();

		{
			auto scope = measure_scope(audiovisuals.profiler.interpolation);

			interp.integrate_interpolated_transforms(
				viewing_config.interpolation, 
				cosmos, 
				augs::delta(frame_delta) *= speed_multiplier, 
				cosmos.get_fixed_delta()
			);
		}

		gameplay_camera.tick(
			screen_size,
			interp,
			frame_delta,
			viewing_config.camera,
			viewed_character
		);

		{
			auto scope = measure_scope(profiler.camera_visibility_query);

			auto queried_camera = get_camera();
			queried_camera.zoom -= viewing_config.session.camera_query_expansion;

			all_visible.reacquire_all_and_sort({ viewed_character.get_cosmos(), queried_camera, screen_size, false });

			profiler.num_visible_entities.measure(all_visible.all.size());
		}

		const audiovisual_advance_input in(
			frame_delta,
			{ speed_multiplier },

			get_viewed_character(),
			get_camera(),
			vec2(screen_size),
			all_visible,

			get_viewable_defs().particle_effects,
			game_sounds,

			viewing_config.audio_volume
		);

		audiovisuals.advance(in);
	};

	static auto setup_post_solve = [](const const_logic_step step) {
		audiovisuals.standard_post_solve(step);
		game_gui.standard_post_solve(step);
	};

	static auto setup_post_cleanup = [](const const_logic_step step) {
		audiovisuals.standard_post_cleanup(step);
		game_gui.standard_post_cleanup(step);
		
		if (step.any_deletion_occured()) {
			all_visible.clear_dead_entities(step.cosm);
		}
	};

	static auto get_sampled_cosmos = [](auto& setup) {
		return std::addressof(setup.get_viewed_cosmos());
	};

	static const cosmos* last_sampled_cosmos = nullptr;

	static auto advance_setup = [](
		const augs::delta frame_delta,
		auto& setup,
		const cosmic_entropy& new_game_entropy,
		const config_lua_table& viewing_config
	) {
		setup.control(new_game_entropy);
		setup.accept_game_gui_events(game_gui.get_and_clear_pending_events());
		
		audiovisual_step(frame_delta, setup.get_audiovisual_speed(), viewing_config);

		/* MSVC ICE workaround */
		auto& _audiovisual_step = audiovisual_step;
		auto& _setup_post_solve = setup_post_solve;

		setup.advance(
			frame_delta,
			setup_pre_solve,
			[&](const const_logic_step step) {
				_setup_post_solve(step);
				_audiovisual_step(augs::delta::zero, 0.0, viewing_config);
			},
			setup_post_cleanup
		);

		if (const auto now_sampled = get_sampled_cosmos(setup);
			now_sampled != last_sampled_cosmos
		) {
#if 0
			audiovisuals.clear_dead_entities(*now_sampled);
#endif
			audiovisuals.clear();
			all_visible.clear_dead_entities(*now_sampled);

			/* TODO: We need to have one game gui per cosmos. */
			game_gui.clear_dead_entities(*now_sampled);

			last_sampled_cosmos = now_sampled;
			audiovisual_step(augs::delta::zero, 0.0, viewing_config);
		}
	};

	static auto advance_current_setup = [](
		const augs::delta frame_delta,
		const cosmic_entropy& new_game_entropy,
		const config_lua_table& viewing_config
	) { 
		visit_current_setup(
			[&](auto& setup) {
				advance_setup(frame_delta, setup, new_game_entropy, viewing_config);
			}
		);
	};

	if (!params.editor_target.empty()) {
		launch_editor(lua, params.editor_target);
	}
	else {
		launch_setup(config.get_launch_mode());
	}

	/* 
		The main loop variables.
	*/

	static augs::timer frame_timer;
	
	static augs::event::state common_input_state;

	static release_flags releases;

	/* MSVC ICE workaround */
	auto& _common_input_state = common_input_state;

	static auto make_create_game_gui_context = [&](const config_lua_table& cfg) {
		return [&]() {
			return game_gui.create_context(
				window.get_screen_size(),
				_common_input_state,
				get_viewed_character(),
				create_game_gui_deps()
			);
		};
	};

	/* MSVC ICE workaround */
	auto& _window = window;

	static auto make_create_menu_context = [&](const config_lua_table& cfg) {
		return [&](auto& gui) {
			return gui.create_context(
				_window.get_screen_size(),
				_common_input_state,
				create_menu_context_deps(cfg)
			);
		};
	};

	/* 
		MousePos is initially set to negative infinity.
	*/

	ImGui::GetIO().MousePos = { 0, 0 };

	LOG("Entered the main loop.");

	while (!should_quit) {
		auto scope = measure_scope(profiler.fps);
		
#if PLATFORM_UNIX
		if (signal_status != 0) {
			const auto sig = signal_status;

			LOG("%x received.", strsignal(sig));

			if(
				sig == SIGINT
				|| sig == SIGSTOP
				|| sig == SIGTERM
			) {
				LOG("Gracefully shutting down.");
				should_quit = true;
				
				break;
			}
		}
#endif

		const auto frame_delta = frame_timer.extract_delta();

		/* 
			The centralized transformation of all window inputs.
			No window inputs will be acquired and/or used beyond the scope of this lambda
			(except remote packets, received by the client/server setups).
			
			This is necessary because we need some complicated interactions between multiple GUI contexts,
			primarily in deciding what events should be propagated further, down to the gameplay itself.
			It is the easiest if every possibility is considered in one place. 
			We have decided that some stronger decoupling here would benefit nobody.

			The result, which is the collection of new game commands, will be passed further down the loop. 
		*/

		const auto [
			new_game_entropy, 
			viewing_config
		] = [frame_delta]() {
			static game_intents game_intents;
			static game_motions game_motions;

			game_intents.clear();
			game_motions.clear();

			static augs::local_entropy new_window_entropy;

			new_window_entropy.clear();

			/* Generate release events if the previous frame so requested. */

			releases.append_releases(new_window_entropy, common_input_state);
			releases = {};

			if (get_viewed_character().dead()) {
				game_gui_mode = true;
			}

			const bool in_direct_gameplay =
				!game_gui_mode
				&& current_setup.has_value()
				&& !ingame_menu.show
			;

			{
				auto scope = measure_scope(profiler.local_entropy);
				window.collect_entropy(new_window_entropy);
			}

			/*
				Top-level events, higher than IMGUI.
			*/
			
			{
				auto simulated_state = common_input_state;

				erase_if(new_window_entropy, [&](const augs::event::change e) {
					using namespace augs::event;
					using namespace augs::event::keys;

					simulated_state.apply(e);

					if (e.is_exit_message()) {
						should_quit = true;
						return true;
					}
					
					if (e.msg == message::deactivate) {
						releases.set_all();
						return true;
					}
					
					if (e.was_pressed(key::F11)) {
						bool& f = config.window.fullscreen;
						f = !f;
						return true;
					}

					if (ingame_menu.show) {
						return false;
					}
					
					/* MSVC ICE workaround */
					auto& _simulated_state = simulated_state;
					auto& _lua = lua;
					auto& _window = window;

					return visit_current_setup([&](auto& setup) {
						using T = std::decay_t<decltype(setup)>;

						if constexpr(T::handles_window_input) {
							return setup.handle_top_level_window_input(
								_simulated_state, e, _window, _lua
							);
						}

						return false;
					});
				});
			}

			/* 
				IMGUI is our top GUI whose priority precedes everything else. 
				It will eat from the window input vector that is later passed to the game and other GUIs.	
			*/

			configurables.sync_back_into(config);

			/*
				We "pause" the mouse cursor's position when we are in direct gameplay,
				so that when switching to GUI, the cursor appears where it disappeared.
				(it does not physically freeze the cursor, it just remembers the position)
			*/

			window.set_mouse_pos_paused(in_direct_gameplay);

			perform_imgui_pass(
				new_window_entropy,
				window.get_screen_size(),
				frame_delta,
				config,
				last_saved_config,
				local_config_path,
				settings_gui,
				lua,
				[&]() {
					/*
						The editor setup might want to use IMGUI to create views of entities or resources,
						thus we ask the current setup for its custom IMGUI logic.
					*/

					/* MSVC ICE workaround */
					auto& _lua = lua;
					auto& _window = window;
					
					const bool in_direct_gameplay = !game_gui_mode;

					visit_current_setup([&](auto& setup) {
						using T = std::decay_t<decltype(setup)>;

						if constexpr(std::is_same_v<T, editor_setup>) {
							/* Editor needs more goods */
							setup.perform_custom_imgui(_lua, _window, in_direct_gameplay, get_camera());
						}
						else {
							setup.perform_custom_imgui();
						}
					});
				},

				/* Flags controlling IMGUI behaviour */

				ingame_menu.show,
				current_setup.has_value(),

				in_direct_gameplay
			);
			
			/* MSVC ICE workaround */

			const auto& _config = config;
			auto& _window = window;

			const auto viewing_config = visit_current_setup([&_config](auto& setup) {
				auto config_copy = _config;

				/*
					For example, the main menu might want to disable HUD or tune down the sound effects.
					Editor might want to change the window name to the current file.
				*/

				setup.customize_for_viewing(config_copy);
				setup.apply(config_copy);

				return config_copy;
			});

			configurables.apply(viewing_config);

			if (window.is_active()
				&& (
					in_direct_gameplay
					|| (
						viewing_config.window.raw_mouse_input
#if TODO
						&& !viewing_config.session.use_system_cursor_for_gui
#endif
					)
				)
			) {
				window.clip_system_cursor();
				window.set_cursor_visible(false);
			}
			else {
				window.disable_cursor_clipping();
				window.set_cursor_visible(true);
			}

			releases.set_due_to_imgui(ImGui::GetIO());

			auto create_menu_context = make_create_menu_context(viewing_config);
			auto create_game_gui_context = make_create_game_gui_context(viewing_config);

			/*
				Since ImGUI has quite a different philosophy about input,
				we will need some ugly inter-op with our GUIs.
			*/

			if (ImGui::GetIO().WantCaptureMouse) {
				/* 
					If mouse enters IMGUI element, sync back its position. 
					Mousemotions are eaten from the vector already,
					so the common_input_state would otherwise not get updated.
				*/

				common_input_state.mouse.pos = ImGui::GetIO().MousePos;

				/* Neutralize hovers on all GUIs whose focus may have just been stolen. */

				game_gui.world.unhover_and_undrag(create_game_gui_context());
				
				if (main_menu.has_value()) {
					main_menu->gui.world.unhover_and_undrag(create_menu_context(main_menu->gui));
				}

				ingame_menu.world.unhover_and_undrag(create_menu_context(ingame_menu));

				on_specific_setup([](editor_setup& setup) {
					setup.unhover();
				});
			}

			/*
				We also need inter-op between our own GUIs, 
				because we have quite a lot of them.
			*/

			if (game_gui.world.wants_to_capture_mouse(create_game_gui_context())) {
				if (current_setup) {
					if (auto* editor = std::get_if<editor_setup>(&*current_setup)) {
						editor->unhover();
					}
				}
			}

			/* Maybe the game GUI was deactivated while the button was still hovered */

			else if (!game_gui_mode && current_setup.has_value()) {
				game_gui.world.unhover_and_undrag(create_game_gui_context());
			}

			/* 
				Distribution of all the remaining input happens here.
			*/

			for (const auto e : new_window_entropy) {
				using namespace augs::event;
				using namespace keys;
				
				/*
					Now is the time to actually track the input state
					and use the mouse position for GUI contexts or the key states
					for key combinations.
				*/

				common_input_state.apply(e);

				if (
					current_setup.has_value()
					&& e.was_pressed(key::ESC)
				) {
					if (visit_current_setup([&](auto& setup) {
						using T = std::decay_t<decltype(setup)>;

						if constexpr(T::handles_escape) {
							return setup.escape();
						}
						
						return false;
					})) {
						continue;
					}

					bool& f = ingame_menu.show;
					f = !f;
					releases.set_all();
					
					continue;
				}

				std::optional<intent_change> key_change;

				if (e.was_any_key_pressed()) {
					key_change = intent_change::PRESSED;
				}

				if (e.was_any_key_released()) {
					key_change = intent_change::RELEASED;
				}

				const bool was_pressed = key_change && *key_change == intent_change::PRESSED;
				const bool was_released = key_change && *key_change == intent_change::RELEASED;
				
				if (was_pressed || was_released) {
					const auto key = e.get_key();

					if (const auto it = mapped_or_nullptr(viewing_config.app_controls, key)) {
						if (was_pressed) {
							handle_app_intent(*it);
							continue;
						}
					}
				}

				{
					auto control_main_menu = [&]() {
						if (main_menu.has_value() && !current_setup.has_value()) {
							if (main_menu->gui.show) {
								main_menu->gui.control(create_menu_context(main_menu->gui), e, do_main_menu_option);
							}

							return true;
						}

						return false;
					};

					auto control_ingame_menu = [&]() {
						if (ingame_menu.show || was_released) {
							return ingame_menu.control(create_menu_context(ingame_menu), e, do_ingame_menu_option);
						}

						return false;
					};
					
					if (was_released) {
						control_main_menu();
						control_ingame_menu();
					}
					else {
						if (control_main_menu()) {
							continue;
						}

						if (control_ingame_menu()) {
							continue;
						}
					}
				}

				const auto viewed_character = get_viewed_character();

				const bool direct_gameplay_or_game_gui =
					viewed_character.alive()
					&& (
						current_setup.has_value()
						&& !ingame_menu.show
					)
				;

				if (was_pressed || was_released) {
					const auto key = e.get_key();

					if (direct_gameplay_or_game_gui || was_released) {
						if (const auto it = mapped_or_nullptr(viewing_config.app_ingame_controls, key)) {
							if (was_pressed) {
								handle_app_ingame_intent(*it);
								continue;
							}
						}
						if (const auto it = mapped_or_nullptr(viewing_config.game_gui_controls, key)) {
							if (should_draw_game_gui()) {
								game_gui.control_hotbar_and_action_button(viewed_character, { *it, *key_change });

								if (was_pressed) {
									continue;
								}
							}
						}
						if (const auto it = mapped_or_nullptr(viewing_config.game_controls, key)) {
							if (
								const bool leave_it_for_game_gui = e.uses_mouse() && game_gui_mode;
								!leave_it_for_game_gui
							) {
								game_intents.push_back({ *it, *key_change });

								if (was_pressed) {
									continue;
								}
							}
						}
					}
				}
				if (
					e.msg == message::mousemotion
					&& direct_gameplay_or_game_gui
					&& !game_gui_mode
				) {
					game_motions.push_back({ game_motion_type::MOVE_CROSSHAIR, e.data.mouse.rel });
					continue;
				}

				if (should_draw_game_gui() && (direct_gameplay_or_game_gui || was_released)) {
					if (game_gui.control_gui_world(create_game_gui_context(), e)) {
						continue;
					}
				}

				{
					/* MSVC ICE workaround */
					auto& _common_input_state = common_input_state;
					auto& _lua = lua;
					auto& _window = window;

					if (visit_current_setup([&](auto& setup) {
						using T = std::decay_t<decltype(setup)>;

						if constexpr(T::handles_window_input) {
							return setup.handle_unfetched_window_input(
								_common_input_state, e, _window, _lua, get_camera()
							);
						}

						return false;
					})) {
						continue;
					}
				}
			}

			/* 
				Notice that window inputs do not propagate
				beyond the closing of this scope.
			*/

			return std::make_tuple(
				cosmic_entropy(
					get_viewed_character(),
					game_intents,
					game_motions
				),
				viewing_config
			);
		}();

		/* 
			Viewables reloading pass.
		*/

		/* MSVC ICE workaround */
		auto& _load_all = load_all;

		visit_current_setup(
			[&](const auto& setup) {
				using T = std::decay_t<decltype(setup)>;
				using S = viewables_loading_type;

				constexpr auto s = T::loading_strategy;

				if constexpr(s == S::LOAD_ALL) {
					_load_all(setup.get_viewable_defs());
				}
				else if constexpr(s == S::LOAD_ONLY_NEAR_CAMERA){
					static_assert(always_false_v<T>, "Unimplemented");
				}
				else if constexpr(T::loading_strategy == S::LOAD_ALL_ONLY_ONCE) {
					/* Do nothing */
				}
				else {
					static_assert(always_false_v<T>, "Unknown viewables loading strategy.");
				}
			}
		);

		const auto screen_size = window.get_screen_size();

		auto create_menu_context = make_create_menu_context(viewing_config);
		auto create_game_gui_context = make_create_game_gui_context(viewing_config);

		/* 
			Advance the current setup's logic,
			and let the audiovisual_state sample the game world 
			that it chooses via get_viewed_cosmos.
		*/

		advance_current_setup(frame_delta, new_game_entropy, viewing_config);
		
		/*
			Game GUI might have been altered by the step's post-solve,
			therefore we need to rebuild its layouts (and from them, the tree data)
			for immediate visual response.
		*/

		const auto viewed_character = get_viewed_character();

		if (should_draw_game_gui()) {
			const auto context = create_game_gui_context();

			game_gui.advance(context, frame_delta);
			game_gui.rebuild_layouts(context);
			game_gui.build_tree_data(context);
		}

		/* 
			What follows is strictly view part,
			without advancement of any kind.
			
			No state is altered beyond this point,
			except for usage of graphical resources and profilers.
		*/

		if (const bool minimized = screen_size.is_zero()) {
			continue;
		}

		auto frame = measure_scope(profiler.frame);
		
		auto get_drawer = [&]() { 
			return augs::drawer_with_default {
				renderer.get_triangle_buffer(),
				necessary_atlas_entries[assets::necessary_image_id::BLANK]
			};
		};

		auto get_gui_text_style = [&]() {
			return augs::gui::text::style { get_gui_font(), cyan };
		};

		const auto interpolation_ratio = visit_current_setup([](auto& setup) {
			return setup.get_interpolation_ratio();
		});

		const auto context = viewing_game_gui_context {
			create_game_gui_context(),

			{
				audiovisuals.get<interpolation_system>(),
				audiovisuals.world_hover_highlighter,
				viewing_config.hotbar,
				viewing_config.drawing,
				viewing_config.game_gui_controls,
				get_camera(),
				get_drawer()
			}
		};

		/*
			Canonical rendering order of the Hypersomnia Universe:
			
			1.  Draw the cosmos in the vicinity of the viewed character.
				Both the cosmos and the character are specified by the current setup (main menu is a setup, too).
			
			2.	Draw the debug lines over the game world, if so is appropriate.
			
			3.	Draw the game GUI, if so is appropriate.
				Game GUI involves things like inventory buttons, hotbar and health bars.

			4.	Draw either the main menu buttons, or the in-game menu overlay accessed by ESC.
				These two are almost identical, except the layouts of the first (e.g. tweened buttons) 
				may also be influenced by a playing intro.

			5.	Draw IMGUI, which is the highest priority GUI. 
				This involves settings window, developer console and the like.

			6.	Draw the GUI cursor. It may be:
					- The cursor of the IMGUI, if it wants to capture the mouse.
					- Or, the cursor of the main menu or the in-game menu overlay, if either is currently active.
					- Or, the cursor of the game gui, with maybe tooltip, with maybe dragged item's ghost, if we're in-game in GUI mode.
		*/

		renderer.set_viewport({ vec2i{0, 0}, screen_size });
		
		if (augs::graphics::fbo::current_exists()) {
			augs::graphics::fbo::set_current_to_none();
		}

		renderer.clear_current_fbo();

		if (const bool has_something_to_view = viewed_character.alive()) {
			/* #1 */
			illuminated_rendering(
				{
					viewed_character,
					audiovisuals,
					viewing_config.drawing,
					necessary_atlas_entries,
					get_gui_font(),
					game_atlas_entries,
					screen_size,
					interpolation_ratio,
					renderer,
					*game_world_atlas,
					fbos,
					shaders,
					get_camera(),
					all_visible
				},
				false,
				[](const const_entity_handle handle) -> std::optional<rgba> { return std::nullopt; },
				[&](auto callback) {
					visit_current_setup([&](auto& setup) {
						using T = std::decay_t<decltype(setup)>;

						if constexpr(T::has_additional_highlights) {
							setup.for_each_highlight(callback);
						}
					});
				}
			);

			if (DEBUG_DRAWING.enabled) {
				/* #2 */
				renderer.draw_debug_lines(
					DEBUG_LOGIC_STEP_LINES,
					DEBUG_PERSISTENT_LINES,

					get_drawer().default_texture,
					interpolation_ratio
				);

				renderer.call_and_clear_lines();

				renderer.frame_lines.clear();
			}

			shaders.standard->set_projection(augs::orthographic_projection(vec2(screen_size)));

			/*
				Illuminated rendering leaves the renderer in a state
				where the default shader is being used and the game world atlas is still bound.

				It is the configuration required for further viewing of GUI.
			*/

			if (current_setup) {
				on_specific_setup([&](editor_setup& editor) {
					if (editor.rectangular_drag_origin.has_value()) {
						const auto camera = get_camera();

						const auto rectangular_selection = ltrb::from_points(
							common_input_state.mouse.pos,
							camera.to_screen_space(screen_size, *editor.rectangular_drag_origin)
						);

						get_drawer().aabb_with_border(
							rectangular_selection,
							viewing_config.editor.rectangular_selection_color,
							viewing_config.editor.rectangular_selection_border_color
						);
					}
				});
			}

			if (should_draw_game_gui()) {
				/* #3 */
				game_gui.world.draw(context);
			}
		}
		else {
			game_world_atlas->bind();
			shaders.standard->set_as_current();
			shaders.standard->set_projection(augs::orthographic_projection(vec2(screen_size)));

			if (std::addressof(viewed_character.get_cosmos()) ==
				std::addressof(cosmos::zero)
			) {
				get_drawer().color_overlay(screen_size, darkgray);
			}
		}

		const auto menu_chosen_cursor = [&](){
			if (current_setup.has_value()) {
				if (ingame_menu.show) {
					const auto context = create_menu_context(ingame_menu);
					ingame_menu.advance(context, frame_delta);

					/* #4 */
					return ingame_menu.draw({ context, get_drawer() });
				}

				return assets::necessary_image_id::INVALID;
			}
			else {
				const auto context = create_menu_context(main_menu->gui);

				main_menu->gui.advance(context, frame_delta);

				/* #4 */
				const auto cursor = main_menu->gui.draw({ context, get_drawer() });

				main_menu.value().draw_overlays(
					get_drawer(),
					necessary_atlas_entries,
					get_gui_font(),
					screen_size
				);

				return cursor;
			}
		}();
		
		renderer.call_and_clear_triangles();

		/* #5 */
		renderer.draw_call_imgui(
			imgui_atlas,
			*game_world_atlas
		);

		/* #6 */
		const bool should_draw_our_cursor = viewing_config.window.raw_mouse_input && !window.is_mouse_pos_paused();

		{
			const auto cursor_drawing_pos = common_input_state.mouse.pos;

			if (ImGui::GetIO().WantCaptureMouse) {
				if (should_draw_our_cursor) {
					get_drawer().cursor(necessary_atlas_entries, augs::imgui::get_cursor<assets::necessary_image_id>(), cursor_drawing_pos, white);
				}
			}
			else if (
				const bool we_drew_some_menu =
				menu_chosen_cursor != assets::necessary_image_id::INVALID
			) {
				if (should_draw_our_cursor) {
					get_drawer().cursor(necessary_atlas_entries, menu_chosen_cursor, cursor_drawing_pos, white);
				}
			}
			else if (game_gui_mode && should_draw_game_gui()) {
				const auto& character_gui = game_gui.get_character_gui(viewed_character);

				character_gui.draw_cursor_with_tooltip(context, should_draw_our_cursor);
			}
			else {
				if (should_draw_our_cursor) {
					on_specific_setup([&](editor_setup& setup) {
						if (setup.is_paused()) {
							get_drawer().cursor(necessary_atlas_entries, assets::necessary_image_id::GUI_CURSOR, cursor_drawing_pos, white);
						}
					});
				}
			}
		}

		if (viewing_config.session.show_developer_console) {
			draw_debug_details(
				get_drawer(),
				get_gui_font(),
				screen_size,
				viewed_character,
				profiler,
				audiovisuals.profiler
			);
		}

		renderer.call_and_clear_triangles();

		profiler.num_triangles.measure(renderer.num_total_triangles_drawn);
		renderer.num_total_triangles_drawn = 0u;

		window.swap_buffers();
	}

	return EXIT_SUCCESS;
}
catch (const config_read_error err) {
	LOG("Failed to read the initial config for the game!\n%x", err.what());
	return EXIT_FAILURE;
}
catch (const augs::audio_error& err) {
	LOG("Failed to establish the audio context:\n%x", err.what());
	return EXIT_FAILURE;
}
catch (const augs::window_error& err) {
	LOG("Failed to create an OpenGL window:\n%x", err.what());
	return EXIT_FAILURE;
}
catch (const augs::renderer_error& err) {
	LOG("Failed to initialize the renderer: %x", err.what());
	return EXIT_FAILURE;
}
catch (const necessary_resource_loading_error err) {
	LOG("Failed to load a resource necessary for the game to function!\n%x", err.what());
	return EXIT_FAILURE;
}
catch (const augs::lua_state_creation_error err) {
	LOG("Failed to create a lua state for the game!\n%x", err.what());
	return EXIT_FAILURE;
}
catch (const augs::unit_test_session_error err) {
	LOG("Unit test session failure:\n%x\ncout:%x\ncerr:%x\nclog:%x\n", 
		err.what(), err.cout_content, err.cerr_content, err.clog_content
	);

	return EXIT_FAILURE;
}
catch (const std::runtime_error err) {
	LOG("Runtime error: %x", err.what());

	return EXIT_FAILURE;
}
/* We want the debugger to break if it is not in production */
#if IS_PRODUCTION_BUILD
catch (...) {
	LOG("Unknown exception.");
	return EXIT_FAILURE;
}
#endif
