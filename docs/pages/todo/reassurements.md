---
title: Reassurements
hide_sidebar: true
permalink: reassurements
summary: We don't need to do this yet, because...
---

- what do we do with invalid sprite ids?
	- We guarantee the validity of the ids everywhere. Period.
		- That is because the editor will forbid from settings invalid values.
		- And adding a new value to the container will call a sane default provider.
		- Also removing a currently used id will be forbidden.
	- E.g. sprites, animations or other invariants.

- two animation usages will most probably only ever differ by speed.

- Later if we really need it for descriptions, we can retrieve the old values by accessing the field address, in just the same way as the command logic does.

- for now we'll put an animation id inside wandering pixels,
	- so let's just leave the unique_ptr<field_address> actually_element for later
		- i think we will be able to do this nicely
	- struct_field_address that will be returned by make_struct_field_address

- We will never be met with a situation where no images are in the project.
	- That is because common will always use some,
		- and will be initialized with sensible values from the start
	- and you can't erase images if they are used.
