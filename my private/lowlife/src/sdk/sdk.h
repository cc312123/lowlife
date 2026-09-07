#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <memory/memory.h>
#include <sdk/offsets.h>
#include "math/math.h"

namespace rbx
{
	struct instance_t;
	struct primitive_t;
	struct model_instance_t;

	struct addressable_t
	{
		std::uint64_t address;

		addressable_t() : address(0) {}
		addressable_t(std::uint64_t address) : address(address) {}
	};

	struct nameable_t : public addressable_t
	{
		using addressable_t::addressable_t;

		std::string get_name();
		std::string get_class_name();
	};

	struct interface_t
	{
		template <typename T>
		std::vector<T> get_children();

		std::vector<rbx::instance_t> get_children();
		size_t get_children_count();
		rbx::instance_t find_first_child(std::string_view str);
		rbx::instance_t find_first_child_by_class(std::string_view str);
		rbx::instance_t find_descendant_value_by_name_substrings(const std::vector<std::string>& patterns, int max_depth = 5);
	};

	struct instance_t : public nameable_t, public interface_t
	{
		using nameable_t::nameable_t;
	};

	struct player_t final : public instance_t
	{
		using instance_t::instance_t;

		rbx::model_instance_t get_model_instance();
		std::int64_t get_user_id();
		std::string get_crew_id();
	};

	struct model_instance_t final : public instance_t
	{
		using instance_t::instance_t;
	};
	
	struct humanoid_t final : public addressable_t
	{
		using addressable_t::addressable_t;

		std::uint8_t get_rig_type();
		float get_health();
		float get_max_health();
	};

	struct part_t : public instance_t
	{
		using instance_t::instance_t;

		rbx::primitive_t get_primitive();
	};

	struct primitive_t final : public addressable_t
	{
		using addressable_t::addressable_t;

		math::vector3 get_size();
		void set_size(const math::vector3& size);
		math::vector3 get_position();
		math::matrix3 get_rotation();
		math::vector3 get_velocity();
		bool get_can_collide();
		bool set_can_collide(bool enable);
		bool set_anchored(bool enable);
	};

	struct visualengine_t final : public addressable_t
	{
		math::vector2 get_dimensions();
		math::matrix4 get_viewmatrix();
		bool world_to_screen(const math::vector3& world, math::vector2& out, const math::vector2& dims, const math::matrix4& view);
		bool world_to_client(const math::vector3& world, math::vector2& out, const math::vector2& dims, const math::matrix4& view);
	};

	struct camera_t final : public instance_t
	{
		using instance_t::instance_t;
		
		math::vector3 get_position();
		math::matrix3 get_rotation();
		void write_rotation(const math::matrix3& rotation);
	};
}

template <typename T>
std::vector<T> rbx::interface_t::get_children()
{
	rbx::instance_t* base = static_cast<rbx::instance_t*>(this);
	if (base->address == 0) return {};

	std::uint64_t array_start = 0;
	std::uint64_t array_end = 0;

	// Method 1: Check pointer container at ChildrenStart (0x78)
	std::uint64_t start = memory->read<std::uint64_t>(base->address + Offsets::Instance::ChildrenStart);
	if (start != 0 && (start & 0x7) == 0 && start > 0x10000)
	{
		std::uint64_t a_start = memory->read<std::uint64_t>(start);
		std::uint64_t a_end = memory->read<std::uint64_t>(start + Offsets::Instance::ChildrenEnd);
		if (a_start != 0 && a_end != 0 && a_start < a_end && ((a_end - a_start) % 16 == 0))
		{
			array_start = a_start;
			array_end = a_end;
		}
	}

	// Method 2: Check direct vector at ChildrenStart
	if (array_start == 0)
	{
		std::uint64_t a_start = memory->read<std::uint64_t>(base->address + Offsets::Instance::ChildrenStart);
		std::uint64_t a_end = memory->read<std::uint64_t>(base->address + Offsets::Instance::ChildrenStart + Offsets::Instance::ChildrenEnd);
		if (a_start != 0 && a_end != 0 && a_start < a_end && ((a_end - a_start) % 16 == 0))
		{
			array_start = a_start;
			array_end = a_end;
		}
	}

	// Method 3: Scan candidate offsets on base->address if Methods 1 & 2 failed
	if (array_start == 0)
	{
		static const std::uint64_t candidate_offsets[] = { 0x50, 0x60, 0x68, 0x70, 0x78, 0x80, 0x88, 0x90, 0x48, 0x58 };
		for (std::uint64_t off : candidate_offsets)
		{
			std::uint64_t ptr = memory->read<std::uint64_t>(base->address + off);
			if (ptr != 0 && (ptr & 0x7) == 0 && ptr > 0x10000)
			{
				std::uint64_t a_start = memory->read<std::uint64_t>(ptr);
				std::uint64_t a_end = memory->read<std::uint64_t>(ptr + Offsets::Instance::ChildrenEnd);
				if (a_start != 0 && a_end != 0 && a_start < a_end && ((a_end - a_start) % 16 == 0))
				{
					std::uint64_t count = (a_end - a_start) / 16;
					if (count > 0 && count <= 50000)
					{
						array_start = a_start;
						array_end = a_end;
						Offsets::Instance::ChildrenStart = off;
						break;
					}
				}
			}

			std::uint64_t a_start = ptr;
			std::uint64_t a_end = memory->read<std::uint64_t>(base->address + off + Offsets::Instance::ChildrenEnd);
			if (a_start != 0 && a_end != 0 && a_start < a_end && ((a_end - a_start) % 16 == 0))
			{
				std::uint64_t count = (a_end - a_start) / 16;
				if (count > 0 && count <= 50000)
				{
					array_start = a_start;
					array_end = a_end;
					Offsets::Instance::ChildrenStart = off;
					break;
				}
			}
		}
	}

	if (array_start == 0 || array_end == 0 || array_start >= array_end)
	{
		return {};
	}

	std::uint64_t size_bytes = array_end - array_start;
	std::uint64_t count = size_bytes / 16;

	if (count == 0 || count > 50000)
	{
		return {};
	}

	struct raw_shared_ptr {
		std::uint64_t ptr;
		std::uint64_t ref_count;
	};

	std::vector<raw_shared_ptr> raw_ptrs(count);
	Luck_ReadVirtualMemory(memory->get_process_handle(), reinterpret_cast<void*>(array_start), raw_ptrs.data(), static_cast<ULONG>(count * sizeof(raw_shared_ptr)), nullptr);

	std::vector<T> children;
	children.reserve(count);

	for (std::uint64_t i = 0; i < count; ++i)
	{
		std::uint64_t child_address = raw_ptrs[i].ptr;
		if (child_address != 0 && (child_address & 0x7) == 0 && child_address > 0x10000)
		{
			children.emplace_back(child_address);
		}
	}

	return children;
}