#pragma once
#include <string>
#include "augs/network/port_type.h"
#include "application/gui/connect_address_type.h"

struct address_and_port {
	std::string connect_address = "127.0.0.1";
	port_type default_port_when_no_specified = 8412;
};

struct client_start_input {
	// GEN INTROSPECTOR struct client_start_input
	port_type default_port_when_no_specified = 8412;
	connect_address_type chosen_address_type = connect_address_type::OFFICIAL;

	std::string custom_address = "127.0.0.1";
	std::string preferred_official_address = "";

	bool record_demo = true;
	// END GEN INTROSPECTOR

	address_and_port get_address_and_port() const;
	void set_custom(const std::string& target);
};

