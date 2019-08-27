#include "3rdparty/crc32/crc32.h"

#include "augs/readwrite/memory_stream.h"

#include "augs/misc/randomization.h"

#include "game/cosmos/cosmos.h"
#include "game/cosmos/entity_handle.h"
#include "game/cosmos/create_entity.hpp"
#include "game/cosmos/change_common_significant.hpp"
#include "game/cosmos/change_solvable_significant.h"

#include "augs/readwrite/lua_readwrite.h"
#include "augs/readwrite/byte_readwrite.h"

#include "game/cosmos/for_each_entity.h"

#include <atomic>

std::atomic<int> cosmos_counter = 0;

void cosmos::request_resample() {
	cosmos_id = cosmos_counter++;
}

cosmos::cosmos() : cosmos_id(cosmos_counter++) 
{
}

cosmos::cosmos(const cosmic_pool_size_type reserved_entities) 
	: solvable(reserved_entities), cosmos_id(cosmos_counter++)
{
}

cosmos::cosmos(const cosmos& b) : common(b.common), solvable(b.solvable), cosmos_id(cosmos_counter++), profiler(b.profiler) {
	cosmic::after_solvable_copy(*this, b);
}

cosmos& cosmos::operator=(const cosmos& b) {
	common = b.common;
	solvable = b.solvable;
	profiler = b.profiler;

	cosmic::after_solvable_copy(*this, b);
	return *this;
}

const cosmos cosmos::zero = {};

template <class T>
T cosmos::calculate_solvable_signi_hash() const {
	if constexpr(std::is_same_v<T, uint32_t>) {
		augs::memory_stream ss;

		augs::write_bytes(ss, get_clock().now);
		augs::write_bytes(ss, get_entities_count());

		for_each_having<components::sentience>(
			[&](const auto& it) {
				const auto& b = it.template get<components::rigid_body>();
				const auto& c = b.get_raw_component();
				const auto& s = it.template get<components::sentience>();

				augs::write_bytes(ss, c);
				augs::write_bytes(ss, s.meters);
			}
		);

		return crc32buf(reinterpret_cast<char*>(ss.data()), ss.get_write_pos());
	}
	else {
		static_assert(always_false_v<T>, "Unsupported hash type.");
	}

	return 0u;
}

template uint32_t cosmos::calculate_solvable_signi_hash() const;

std::string cosmos::summary() const {
	return typesafe_sprintf("Entities: %x\n", get_entities_count());
}

rng_seed_type cosmos::get_rng_seed_for(const entity_id id) const {
	const auto passed = get_total_steps_passed();

	if (const auto handle = operator[](id)) {
		return augs::simple_two_hash(handle.get_id().raw.indirection_index, passed);
	}

	return std::hash<std::remove_const_t<decltype(passed)>>()(passed);
}

randomization cosmos::get_rng_for(const entity_id id) const {
	return{ get_rng_seed_for(id) };
}

randomization cosmos::get_nontemporal_rng_for(const entity_id id) const {
	const auto handle = operator[](id);

	auto h = augs::hash_multiple(
		handle.get_id().raw.indirection_index, 
		handle.get_id().type_id.get_index(), 
		handle.when_born().step
	);

	if (const auto item = handle.find<components::item>()) {
		const auto num_charges = item->get_charges();
		if (num_charges != 1) {
			augs::hash_combine(h, num_charges);
		}
	}

	return h;
}

bool cosmos::empty() const {
	return get_solvable().empty();
}

void cosmos::set(const cosmos_solvable_significant& new_signi) {
	cosmic::change_solvable_significant(*this, [&](cosmos_solvable_significant& current_signi){ 
		{
			auto scope = measure_scope(profiler.duplication);
			current_signi = new_signi; 
		}

		return changer_callback_result::REFRESH; 
	});
}

void cosmos::reinfer_everything() {
	common.reinfer();
	cosmic::reinfer_solvable(*this);
}

void cosmos::set_fixed_delta(const augs::delta& dt) {
	cosmic::change_solvable_significant(*this, [&](cosmos_solvable_significant& current_signi){ 
		ensure_eq(0, current_signi.clk.now.step);
		current_signi.clk.dt = dt;
		return changer_callback_result::DONT_REFRESH; 
	});
}

void cosmos::assign_solvable(const cosmos& b) {
	solvable = b.solvable;

	cosmic::after_solvable_copy(*this, b);
}
