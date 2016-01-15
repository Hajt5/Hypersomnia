#include "car_system.h"
#include "../messages/trigger_hit_confirmation_message.h"
#include "../messages/car_ownership_change_message.h"
#include "../messages/intent_message.h"

#include "entity_system/world.h"

#include "../components/trigger_component.h"
#include <Box2D\Box2D.h>

#include "render_system.h"

void car_system::set_steering_flags_from_intents() {
	auto& intents = parent_world.get_message_queue<messages::intent_message>();

	for (auto& it : intents) {
		auto* maybe_car = it.subject->find<components::car>();
		if (maybe_car == nullptr) continue;

		auto& car = *maybe_car;

		switch (it.intent) {
		case messages::intent_message::intent_type::MOVE_FORWARD:
			car.accelerating = it.pressed_flag;
			break;
		case messages::intent_message::intent_type::MOVE_BACKWARD:
			car.deccelerating = it.pressed_flag;
			break;
		case messages::intent_message::intent_type::MOVE_LEFT:
			car.turning_left = it.pressed_flag;
			break;
		case messages::intent_message::intent_type::MOVE_RIGHT:
			car.turning_right = it.pressed_flag;
			break;
		default: break;
		}
	}
}

#include <iostream>
void car_system::apply_movement_forces() {
	//auto& renderer = parent_world.get_system<render_system>();
	auto& lines = renderer::get_current().logic_lines;

	for (auto it : targets) {
		auto& car = it->get<components::car>();
		auto& physics = it->get<components::physics>();

		vec2 resultant;

		vec2 forward_dir;
		vec2 right_normal;

		forward_dir = -forward_dir.set_from_radians(physics.body->GetAngle()).perpendicular_cw();
		right_normal = forward_dir.perpendicular_cw();

		resultant.x = car.turning_right * car.input_acceleration.x - car.turning_left * car.input_acceleration.x;
		resultant.y = car.accelerating * car.input_acceleration.y  - car.deccelerating * car.input_acceleration.y;
		
		//if (car.deccelerating && !car.accelerating)
		//	resultant.x *= -1;

		if (car.acceleration_length > 0.f) {
			if (resultant.length() > car.acceleration_length) {
				resultant.set_length(car.acceleration_length);
			}
		}

		if (resultant.non_zero()) {
			vec2 force = resultant.y * forward_dir + right_normal * resultant.x;
			
			vec2 forward_tire_force = vec2(forward_dir).set_length(force.length()) * sgn(resultant.y);

			physics.apply_force(force * physics.get_mass()/4, forward_dir * 200 + vec2(right_normal).set_length(125));
			physics.apply_force(force * physics.get_mass()/4, forward_dir * 200 - vec2(right_normal).set_length(125));
			physics.apply_force(forward_tire_force * physics.get_mass()/4, forward_dir * -200 + vec2(right_normal).set_length(125));
			physics.apply_force(forward_tire_force * physics.get_mass()/4, forward_dir * -200 - vec2(right_normal).set_length(125));
		}

		if (car.braking_damping >= 0.f) {
			//physics.set_linear_damping_vec(vec2(
			//	resultant.x_non_zero() ? 0.f : car.braking_damping,
			//	resultant.y_non_zero() ? 0.f : car.braking_damping));

			physics.set_linear_damping(resultant.non_zero() ? 0.f : car.braking_damping);
		}

		vec2 vel = physics.velocity();
		auto speed = vel.length();
	

		vec2 lateral = right_normal * right_normal.dot(vel);
		vec2 forwardal = forward_dir * forward_dir.dot(vel);
		auto forwardal_speed = forwardal.length();
		forwardal.normalize_hint(forwardal_speed);

		physics.apply_force(-forwardal * 0.00006 * forwardal_speed * forwardal_speed);

		//std::cout << lateral.length() << std::endl;
		//lateral.set_length(0.2f*speed);
		
		if (lateral.length() > 20)
			lateral.set_length(20);
			
		physics.apply_impulse(-lateral * physics.get_mass() * 0.75);
		//physics.apply_impulse(-lateral * physics.get_mass() * 0.75 / 30, forward_dir * 200 + vec2(right_normal).set_length(125), true);
		//physics.apply_impulse(-lateral * physics.get_mass() * 0.75 / 30, forward_dir * 200 - vec2(right_normal).set_length(125), true);
		
		//lines.draw_red(physics.get_world_center(), physics.get_world_center() + forward_dir * 100);
		//lines.draw_blue(physics.get_world_center(), physics.get_world_center() + right_normal *100);

		auto body = physics.body;

		auto ang_impulse = 0.06f * body->GetInertia() * -body->GetAngularVelocity();
		
		if (ang_impulse > 0.3)
			ang_impulse = 0.3; 
		if (ang_impulse < -0.3)
			ang_impulse = -0.3;

		body->ApplyAngularImpulse(ang_impulse, true);
	}
}