#pragma once

std::string format_field_name(std::string s);

template <class List>
class type_in_list_id;

template <class T>
std::string get_type_name();

template <class T>
std::string get_type_name(const T&);

template <class T>
std::string get_type_name(const type_in_list_id<T>& id);

template <class T>
std::string get_type_name_strip_namespace();

template <class T>
std::string get_type_name_strip_namespace(const T&);

std::string to_forward_slashes(std::string);
std::string to_forward_slashes(std::string);

std::string to_lowercase(std::string);
std::string to_uppercase(std::string);
