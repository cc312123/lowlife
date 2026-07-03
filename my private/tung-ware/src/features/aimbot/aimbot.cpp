#define NOMINMAX
#include <Windows.h>
#include <thread>
#include <vector>
#include <algorithm>
#include <cmath>
#include <limits>
#include <mutex>
#include <chrono>
#include <string>
#include <unordered_set>

#include <memory/memory.h>
#include <memory/driver.h>
#include <sdk/sdk.h>
#include <sdk/offsets.h>
#include <cache/cache.h>
#include <game/game.h>
#include <settings.h>
#include <check/typing_check.h>
#include "../triggerbot/triggerbot.h"
#include "aimbot.h"

namespace rbx::aimbot {
    bool g_aimbot_manual_locked = false;
    cache::entity_t g_aimbot_manual_target = {};

    namespace {
        constexpr float PREDICTION_SCALE = 0.016f;
        constexpr float MAX_VELOCITY = 1000.0f;
        constexpr float EPSILON = 0.001f;

        std::mutex mtx;
        cache::entity_t locked_target = {};
        bool has_locked_target = false;
        bool key_was_pressed = false;
        bool toggle_state = false;
        bool was_disabled_by_typing = false;
        bool needs_key_release = false;
        std::string locked_part_name = "";

        math::vector3 filtered_target_pos = { 0.0f, 0.0f, 0.0f };
        bool target_pos_initialized = false;
        math::vector3 smoothed_velocity = { 0.0f, 0.0f, 0.0f };
        bool velocity_initialized = false;
        math::vector3 smoothed_acceleration = { 0.0f, 0.0f, 0.0f };
        bool acceleration_initialized = false;
        math::vector3 last_velocity = { 0.0f, 0.0f, 0.0f };

        float virtual_yaw = 0.0f;
        float virtual_pitch = 0.0f;
        float last_written_yaw = 0.0f;
        float last_written_pitch = 0.0f;
        bool virtual_angles_initialized = false;

        float accum_x = 0.0f;
        float accum_y = 0.0f;

        // Spring velocity tracking states
        float camera_yaw_vel = 0.0f;
        float camera_pitch_vel = 0.0f;
        float mouse_vel_x = 0.0f;
        float mouse_vel_y = 0.0f;

        // Visibility caching
        struct visibility_cache_t {
            bool visible;
            std::chrono::steady_clock::time_point last_check;
        };
        std::unordered_map<std::uint64_t, visibility_cache_t> vis_cache;

        // Analytical critically damped spring-damper equations:
        // Returns updated current state, and updates the velocity reference
        float apply_spring_damper(float current, float target, float& velocity, float natural_frequency, float dt) {
            float y0 = current - target;
            float v0 = velocity;
            float omega = natural_frequency;
            float e = std::exp(-omega * dt);
            float temp = (v0 + omega * y0) * dt;
            float y_new = (y0 + temp) * e;
            velocity = (v0 - omega * temp) * e;
            return target + y_new;
        }

        float apply_spring_damper_angle(float current, float target, float& velocity, float natural_frequency, float dt) {
            float diff = current - target;
            diff = std::atan2(std::sin(diff), std::cos(diff));
            float y0 = diff;
            float v0 = velocity;
            float omega = natural_frequency;
            float e = std::exp(-omega * dt);
            float temp = (v0 + omega * y0) * dt;
            float y_new = (y0 + temp) * e;
            velocity = (v0 - omega * temp) * e;
            float result = target + y_new;
            return std::atan2(std::sin(result), std::cos(result));
        }

        // Lock tracking state for Easing
        std::chrono::steady_clock::time_point lock_start_time = {};
        bool is_lock_active = false;
        std::uint64_t last_target_address = 0;
        
        // Easing capture states
        float lock_start_yaw = 0.0f;
        float lock_start_pitch = 0.0f;
        bool lock_start_angles_captured = false;
        
        POINT lock_start_cursor_pos = {};
        bool lock_start_mouse_captured = false;

        // Sticky Aim Occlusion State
        std::chrono::steady_clock::time_point occlusion_start_time = {};
        bool is_currently_occluded = false;

        // Humanized Random Bone Offset
        math::vector3 target_random_offset = { 0.0f, 0.0f, 0.0f };

        void vector_to_angles(const math::vector3& forward, float& yaw, float& pitch) {
            pitch = std::asin(std::clamp(forward.y, -1.0f, 1.0f));
            yaw = std::atan2(-forward.x, -forward.z);
        }

        math::vector3 angles_to_vector(float yaw, float pitch) {
            float cos_pitch = std::cos(pitch);
            return {
                -std::sin(yaw) * cos_pitch,
                std::sin(pitch),
                -std::cos(yaw) * cos_pitch
            };
        }

        float vector2_length(float x, float y) {
            return std::sqrt(x * x + y * y);
        }

        float vector2_distance(float ax, float ay, float bx, float by) {
            float dx = ax - bx;
            float dy = ay - by;
            return vector2_length(dx, dy);
        }

        math::vector3 normalize(const math::vector3& vec) {
            float len = std::sqrt(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z);
            return len > EPSILON ? math::vector3{ vec.x / len, vec.y / len, vec.z / len } : math::vector3{ 0, 0, 0 };
        }

        math::vector3 vector3_sub(const math::vector3& a, const math::vector3& b) {
            return { a.x - b.x, a.y - b.y, a.z - b.z };
        }

        math::vector3 cross(const math::vector3& a, const math::vector3& b) {
            return {
                a.y * b.z - a.z * b.y,
                a.z * b.x - a.x * b.z,
                a.x * b.y - a.y * b.x
            };
        }

        math::matrix3 look_at(const math::vector3& from, const math::vector3& to) {
            math::vector3 forward = normalize(vector3_sub(to, from));
            math::vector3 right = normalize(cross(math::vector3{ 0, 1, 0 }, forward));
            math::vector3 up = cross(forward, right);

            math::matrix3 result = {};
            result.m[0] = right.x; result.m[1] = up.x; result.m[2] = -forward.x;
            result.m[3] = right.y; result.m[4] = up.y; result.m[5] = -forward.y;
            result.m[6] = right.z; result.m[7] = up.z; result.m[8] = -forward.z;
            return result;
        }

        constexpr float M_PI_F = 3.14159265f;

        float ease_linear(float t) { return t; }
        float ease_in_sine(float t) { return 1.0f - std::cos(t * M_PI_F * 0.5f); }
        float ease_out_sine(float t) { return std::sin(t * M_PI_F * 0.5f); }
        float ease_in_out_sine(float t) { return -(std::cos(M_PI_F * t) - 1.0f) * 0.5f; }
        float ease_in_quad(float t) { return t * t; }
        float ease_out_quad(float t) { return 1.0f - (1.0f - t) * (1.0f - t); }
        float ease_in_out_quad(float t) { return t < 0.5f ? 2.0f * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) * 0.5f; }
        float ease_in_cubic(float t) { return t * t * t; }
        float ease_out_cubic(float t) { return 1.0f - std::pow(1.0f - t, 3.0f); }
        float ease_in_out_cubic(float t) { return t < 0.5f ? 4.0f * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) * 0.5f; }
        float ease_out_elastic(float t) {
            float c4 = (2.0f * M_PI_F) / 3.0f;
            return t == 0.0f ? 0.0f : (t == 1.0f ? 1.0f : std::pow(2.0f, -10.0f * t) * std::sin((t * 10.0f - 0.75f) * c4) + 1.0f);
        }
        float ease_out_bounce(float t) {
            float n1 = 7.5625f;
            float d1 = 2.75f;
            if (t < 1.0f / d1) {
                return n1 * t * t;
            } else if (t < 2.0f / d1) {
                t -= 1.5f / d1;
                return n1 * t * t + 0.75f;
            } else if (t < 2.5f / d1) {
                t -= 2.25f / d1;
                return n1 * t * t + 0.9375f;
            } else {
                t -= 2.625f / d1;
                return n1 * t * t + 0.984375f;
            }
        }

        float apply_easing(int style, float t) {
            t = std::clamp(t, 0.0f, 1.0f);
            switch (style) {
                case 0: return ease_linear(t);
                case 1: return ease_in_sine(t);
                case 2: return ease_out_sine(t);
                case 3: return ease_in_out_sine(t);
                case 4: return ease_in_quad(t);
                case 5: return ease_out_quad(t);
                case 6: return ease_in_out_quad(t);
                case 7: return ease_in_cubic(t);
                case 8: return ease_out_cubic(t);
                case 9: return ease_in_out_cubic(t);
                case 10: return ease_out_elastic(t);
                case 11: return ease_out_bounce(t);
                default: return t;
            }
        }

        bool is_knocked(const cache::entity_t& player) {
            if (player.is_knocked) return true;
            if (player.humanoid.address != 0) {
                try {
                    float health = const_cast<cache::entity_t&>(player).humanoid.get_health();
                    if (health <= 0.0f || !std::isfinite(health)) {
                        return true;
                    }
                    
                    bool platform_stand = memory->read<bool>(player.humanoid.address + Offsets::Humanoid::PlatformStand);
                    bool is_sitting = memory->read<bool>(player.humanoid.address + Offsets::Humanoid::Sit);
                    
                    if (player.ko_address != 0) {
                        if (memory->read<bool>(player.ko_address + Offsets::Misc::Value) || (platform_stand && !is_sitting)) {
                            return true;
                        }
                    } else {
                        if ((health <= 20.0f && platform_stand && !is_sitting)) {
                            return true;
                        }
                    }
                } catch (...) {
                    return true;
                }
            }
            return false;
        }

        bool is_player_visible(const cache::entity_t& player) {
            if (player.instance.address == 0) return false;

            auto now = std::chrono::steady_clock::now();
            auto cache_it = vis_cache.find(player.instance.address);
            if (cache_it != vis_cache.end()) {
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - cache_it->second.last_check).count();
                if (elapsed < 60) {
                    return cache_it->second.visible;
                }
            }

            rbx::instance_t camera_inst = { memory->read<std::uint64_t>(game::workspace.address + Offsets::Workspace::CurrentCamera) };
            if (camera_inst.address != 0) {
                rbx::camera_t camera{ camera_inst.address };
                math::vector3 camera_pos = camera.get_position();

                // Only check Head and Torso/HumanoidRootPart to minimize raycasts (reduces checks from 21 down to 2)
                const std::vector<std::string> parts_to_check = { "Head", "HumanoidRootPart", "Torso", "UpperTorso" };
                
                bool is_visible = false;
                int checked_count = 0;
                for (const auto& name : parts_to_check) {
                    if (checked_count >= 2) break; // Maximum 2 points checked per player
                    
                    auto it = player.parts.find(name);
                    if (it == player.parts.end()) continue;
                    rbx::part_t part = it->second;
                    if (!part.address) continue;
                    rbx::primitive_t primitive = part.get_primitive();
                    if (!primitive.address) continue;
                    
                    math::vector3 world_pos = primitive.get_position();
                    checked_count++;
                    if (!botter::is_occluded(camera_pos, world_pos)) {
                        is_visible = true;
                        break;
                    }
                }

                vis_cache[player.instance.address] = { is_visible, now };
                return is_visible;
            }

            vis_cache[player.instance.address] = { true, now };
            return true;
        }

        rbx::part_t get_closest_part(const cache::entity_t& player, const POINT& cursor_pt, const math::vector2& dims, const math::matrix4& view) {
            rbx::part_t closest = {};
            float min_dist = std::numeric_limits<float>::max();
            float cursor_x = static_cast<float>(cursor_pt.x);
            float cursor_y = static_cast<float>(cursor_pt.y);

            const std::unordered_set<std::string> valid_hitparts = {
                "Head",
                "Torso", "UpperTorso", "LowerTorso",
                "Left Arm", "LeftUpperArm", "LeftLowerArm", "LeftHand",
                "Right Arm", "RightUpperArm", "RightLowerArm", "RightHand",
                "Left Leg", "LeftUpperLeg", "LeftLowerLeg", "LeftFoot",
                "Right Leg", "RightUpperLeg", "RightLowerLeg", "RightFoot",
                "HumanoidRootPart"
            };

            for (const auto& pair : player.parts) {
                if (valid_hitparts.find(pair.first) == valid_hitparts.end()) continue;
                rbx::part_t part = pair.second;
                if (!part.address) continue;
                rbx::primitive_t primitive = part.get_primitive();
                math::vector3 world_pos = primitive.get_position();
                math::vector2 screen_pos = {};
                if (!game::visengine.world_to_client(world_pos, screen_pos, dims, view)) continue;
                float dist = vector2_distance(screen_pos.x, screen_pos.y, cursor_x, cursor_y);
                if (dist < min_dist) {
                    min_dist = dist;
                    closest = part;
                }
            }
            return closest;
        }

        math::vector3 get_player_pos(const cache::entity_t& player) {
            std::string parts_to_check[] = { "HumanoidRootPart", "Head", "Torso", "UpperTorso" };
            for (const auto& name : parts_to_check) {
                auto it = player.parts.find(name);
                if (it != player.parts.end() && it->second.address != 0) {
                    rbx::part_t part = it->second;
                    rbx::primitive_t prim = part.get_primitive();
                    if (prim.address != 0) {
                        return prim.get_position();
                    }
                }
            }
            for (const auto& pair : player.parts) {
                if (pair.second.address != 0) {
                    rbx::part_t part = pair.second;
                    rbx::primitive_t prim = part.get_primitive();
                    if (prim.address != 0) {
                        return prim.get_position();
                    }
                }
            }
            return { 0.0f, 0.0f, 0.0f };
        }

        rbx::part_t resolve_smart_bone(const cache::entity_t& player, const POINT& cursor_pt, const math::vector2& dims, const math::matrix4& view, const math::vector3& camera_pos) {
            const std::vector<std::string> target_bones = {
                "Head", "UpperTorso", "Torso", "LowerTorso", "HumanoidRootPart"
            };

            rbx::part_t best_part = {};
            float min_dist = std::numeric_limits<float>::max();
            float cursor_x = static_cast<float>(cursor_pt.x);
            float cursor_y = static_cast<float>(cursor_pt.y);

            for (const auto& bone_name : target_bones) {
                auto it = player.parts.find(bone_name);
                if (it == player.parts.end() || !it->second.address) continue;

                rbx::part_t part = it->second;
                rbx::primitive_t primitive = part.get_primitive();
                if (!primitive.address) continue;

                math::vector3 world_pos = primitive.get_position();

                if (settings::aimbot::wall_check) {
                    if (botter::is_occluded(camera_pos, world_pos)) {
                        continue;
                    }
                }

                math::vector2 screen_pos = {};
                if (!game::visengine.world_to_client(world_pos, screen_pos, dims, view)) continue;

                float dist = vector2_distance(screen_pos.x, screen_pos.y, cursor_x, cursor_y);
                if (dist < min_dist) {
                    min_dist = dist;
                    best_part = part;
                }
            }

            if (!best_part.address) {
                for (const auto& bone_name : target_bones) {
                    auto it = player.parts.find(bone_name);
                    if (it == player.parts.end() || !it->second.address) continue;

                    rbx::part_t part = it->second;
                    rbx::primitive_t primitive = part.get_primitive();
                    if (!primitive.address) continue;

                    math::vector3 world_pos = primitive.get_position();
                    math::vector2 screen_pos = {};
                    if (!game::visengine.world_to_client(world_pos, screen_pos, dims, view)) continue;

                    float dist = vector2_distance(screen_pos.x, screen_pos.y, cursor_x, cursor_y);
                    if (dist < min_dist) {
                        min_dist = dist;
                        best_part = part;
                    }
                }
            }

            if (!best_part.address) {
                best_part = get_closest_part(player, cursor_pt, dims, view);
            }

            return best_part;
        }

        void update_random_offset() {
            if (settings::aimbot::bone_random_offset) {
                float max_offset = settings::aimbot::bone_random_offset_val;
                if (max_offset > 0.0f) {
                    float theta = (static_cast<float>(rand()) / RAND_MAX) * 2.0f * 3.14159265f;
                    float phi = std::acos((static_cast<float>(rand()) / RAND_MAX) * 2.0f - 1.0f);
                    float r = (static_cast<float>(rand()) / RAND_MAX) * max_offset;
                    
                    target_random_offset.x = r * std::sin(phi) * std::cos(theta);
                    target_random_offset.y = r * std::sin(phi) * std::sin(theta);
                    target_random_offset.z = r * std::cos(phi);
                } else {
                    target_random_offset = { 0.0f, 0.0f, 0.0f };
                }
            } else {
                target_random_offset = { 0.0f, 0.0f, 0.0f };
            }
        }

        rbx::part_t get_target_part(const cache::entity_t& player, int aim_part, const POINT& cursor_pt, const math::vector2& dims, const math::matrix4& view) {
            if (player.parts.empty()) return rbx::part_t{};

            if (settings::aimbot::smart_bone) {
                rbx::instance_t camera_inst = { memory->read<std::uint64_t>(game::workspace.address + Offsets::Workspace::CurrentCamera) };
                math::vector3 camera_pos = { 0.0f, 0.0f, 0.0f };
                if (camera_inst.address != 0) {
                    rbx::camera_t camera{ camera_inst.address };
                    camera_pos = camera.get_position();
                }
                return resolve_smart_bone(player, cursor_pt, dims, view, camera_pos);
            }

            switch (aim_part) {
            case 0: 
                if (auto it = player.parts.find("Head"); it != player.parts.end()) return it->second;
                break;
            case 1: 
                if (auto it = player.parts.find("UpperTorso"); it != player.parts.end()) return it->second;
                if (auto it = player.parts.find("Torso"); it != player.parts.end()) return it->second;
                break;
            case 2: 
                if (auto it = player.parts.find("Torso"); it != player.parts.end()) return it->second;
                if (auto it = player.parts.find("UpperTorso"); it != player.parts.end()) return it->second;
                break;
            case 3: 
                if (auto it = player.parts.find("LowerTorso"); it != player.parts.end()) return it->second;
                if (auto it = player.parts.find("Torso"); it != player.parts.end()) return it->second;
                break;
            case 4: 
                if (auto it = player.parts.find("HumanoidRootPart"); it != player.parts.end()) return it->second;
                break;
            case 5: 
                if (auto it = player.parts.find("LeftUpperArm"); it != player.parts.end()) return it->second;
                if (auto it = player.parts.find("Left Arm"); it != player.parts.end()) return it->second;
                break;
            case 6: 
                if (auto it = player.parts.find("RightUpperArm"); it != player.parts.end()) return it->second;
                if (auto it = player.parts.find("Right Arm"); it != player.parts.end()) return it->second;
                break;
            case 7: 
                if (auto it = player.parts.find("LeftUpperLeg"); it != player.parts.end()) return it->second;
                if (auto it = player.parts.find("Left Leg"); it != player.parts.end()) return it->second;
                break;
            case 8: 
                if (auto it = player.parts.find("RightUpperLeg"); it != player.parts.end()) return it->second;
                if (auto it = player.parts.find("Right Leg"); it != player.parts.end()) return it->second;
                break;
            case 9: 
                return get_closest_part(player, cursor_pt, dims, view);
            }
            if (auto it = player.parts.find("HumanoidRootPart"); it != player.parts.end()) return it->second;
            return rbx::part_t{};
        }

        bool is_on_same_team(const cache::entity_t& player, const std::string& local_crew_id) {
            if (local_crew_id.empty() || player.crew_id.empty()) return false;
            if (local_crew_id == "0" || player.crew_id == "0") return false;
            return local_crew_id == player.crew_id;
        }

        bool is_target_valid(const cache::entity_t& player, const std::string& local_crew_id, const POINT& cursor_pt, const math::vector2& dims, const math::matrix4& view, bool skip_fov_check = false) {
            if (player.instance.address == 0) return false;

            bool relation_invalid = false;
            {
                std::lock_guard<std::mutex> lock(settings::player_relations::relations_mutex);
                auto rel_it = settings::player_relations::relations.find(player.name);
                if (rel_it != settings::player_relations::relations.end() && rel_it->second == 1) {
                    relation_invalid = true;
                }
            }
            if (relation_invalid) {
                return false;
            }

            if (settings::aimbot::team_check && is_on_same_team(player, local_crew_id)) {
                return false;
            }

            if (player.health <= 0.0f) return false;
            if (player.humanoid.address == 0) return false;
            try {
                float health = const_cast<cache::entity_t&>(player).humanoid.get_health();
                if (health <= 0.0f || !std::isfinite(health)) {
                    return false;
                }
            } catch (...) {
                return false;
            }

            if (settings::aimbot::knocked_check && is_knocked(player)) return false;

            if (settings::aimbot::fov_check && !skip_fov_check) {
                rbx::part_t target_part = get_target_part(player, settings::aimbot::aimpart, cursor_pt, dims, view);
                if (!target_part.address) return false;

                rbx::primitive_t primitive = target_part.get_primitive();
                math::vector3 world_pos = primitive.get_position();
                math::vector2 screen_pos = {};

                if (!game::visengine.world_to_client(world_pos, screen_pos, dims, view)) return false;

                float cursor_x = static_cast<float>(cursor_pt.x);
                float cursor_y = static_cast<float>(cursor_pt.y);
                float dist = vector2_distance(screen_pos.x, screen_pos.y, cursor_x, cursor_y);
                if (dist > settings::aimbot::fov) return false;
            }

            return true;
        }

        cache::entity_t find_best_target(const std::vector<cache::entity_t>& players_snapshot, const std::string& local_crew_id, const POINT& cursor_pt, const math::vector2& dims, const math::matrix4& view, std::uint64_t local_player_addr) {
            cache::entity_t best = {};
            float best_value = std::numeric_limits<float>::max();

            float cursor_x = static_cast<float>(cursor_pt.x);
            float cursor_y = static_cast<float>(cursor_pt.y);

            math::vector3 local_pos = { 0.0f, 0.0f, 0.0f };
            bool has_local_pos = false;
            if (settings::aimbot::target_selection_mode == 1) {
                local_pos = get_player_pos(cache::cached_local_player);
                if (local_pos.x != 0.0f || local_pos.y != 0.0f || local_pos.z != 0.0f) {
                    has_local_pos = true;
                }
            }

            for (const auto& player : players_snapshot) {
                if (cache::is_local_player(player))
                    continue;

                if (!is_target_valid(player, local_crew_id, cursor_pt, dims, view)) continue;

                rbx::part_t target_part = get_target_part(player, settings::aimbot::aimpart, cursor_pt, dims, view);
                if (!target_part.address) continue;

                rbx::primitive_t primitive = target_part.get_primitive();
                math::vector3 world_pos = primitive.get_position();

                float score = 0.0f;
                if (settings::aimbot::target_selection_mode == 0) { // Crosshair distance
                    math::vector2 screen_pos = {};
                    if (!game::visengine.world_to_client(world_pos, screen_pos, dims, view)) continue;
                    score = vector2_distance(screen_pos.x, screen_pos.y, cursor_x, cursor_y);
                }
                else if (settings::aimbot::target_selection_mode == 1) { // 3D Distance
                    if (!has_local_pos) {
                        math::vector2 screen_pos = {};
                        if (!game::visengine.world_to_client(world_pos, screen_pos, dims, view)) continue;
                        score = vector2_distance(screen_pos.x, screen_pos.y, cursor_x, cursor_y);
                    } else {
                        float dx = world_pos.x - local_pos.x;
                        float dy = world_pos.y - local_pos.y;
                        float dz = world_pos.z - local_pos.z;
                        score = std::sqrt(dx * dx + dy * dy + dz * dz);
                    }
                }
                else if (settings::aimbot::target_selection_mode == 2) { // Health
                    score = player.health;
                }

                if (score < best_value) {
                    best_value = score;
                    best = player;
                }
            }
            return best;
        }

        math::vector3 get_local_player_velocity() {
            auto local = cache::cached_local_player;
            if (local.instance.address != 0) {
                std::string parts_to_check[] = { "HumanoidRootPart", "Torso", "UpperTorso" };
                for (const auto& name : parts_to_check) {
                    auto it = local.parts.find(name);
                    if (it != local.parts.end() && it->second.address != 0) {
                        rbx::part_t part = it->second;
                        rbx::primitive_t prim = part.get_primitive();
                        if (prim.address != 0) {
                            return prim.get_velocity();
                        }
                    }
                }
            }
            return { 0.0f, 0.0f, 0.0f };
        }

        math::vector3 apply_prediction(const math::vector3& base_pos, rbx::primitive_t primitive, bool is_camera, float dt, bool is_in_air) {
            math::vector3 pos = base_pos;
            math::vector3 raw_vel = primitive.get_velocity();

            if (!std::isfinite(raw_vel.x) || !std::isfinite(raw_vel.y) || !std::isfinite(raw_vel.z) ||
                std::abs(raw_vel.x) > MAX_VELOCITY || std::abs(raw_vel.y) > MAX_VELOCITY || std::abs(raw_vel.z) > MAX_VELOCITY) {
                return pos;
            }

            if (!velocity_initialized) {
                smoothed_velocity = raw_vel;
                last_velocity = raw_vel;
                velocity_initialized = true;
                smoothed_acceleration = { 0.0f, 0.0f, 0.0f };
                acceleration_initialized = true;
            } else {
                float vel_alpha = std::clamp(dt * 15.0f, 0.0f, 1.0f);
                smoothed_velocity.x += (raw_vel.x - smoothed_velocity.x) * vel_alpha;
                smoothed_velocity.y += (raw_vel.y - smoothed_velocity.y) * vel_alpha;
                smoothed_velocity.z += (raw_vel.z - smoothed_velocity.z) * vel_alpha;

                // Calculate raw acceleration: a = (current_vel - last_vel) / dt
                math::vector3 raw_accel = { 0.0f, 0.0f, 0.0f };
                if (dt > 0.0001f) {
                    raw_accel.x = (smoothed_velocity.x - last_velocity.x) / dt;
                    raw_accel.y = (smoothed_velocity.y - last_velocity.y) / dt;
                    raw_accel.z = (smoothed_velocity.z - last_velocity.z) / dt;
                }

                // Smooth acceleration to filter out noise
                float accel_alpha = std::clamp(dt * 8.0f, 0.0f, 1.0f);
                smoothed_acceleration.x += (raw_accel.x - smoothed_acceleration.x) * accel_alpha;
                smoothed_acceleration.y += (raw_accel.y - smoothed_acceleration.y) * accel_alpha;
                smoothed_acceleration.z += (raw_accel.z - smoothed_acceleration.z) * accel_alpha;

                last_velocity = smoothed_velocity;
            }

            math::vector3 vel = smoothed_velocity;
            math::vector3 accel = smoothed_acceleration;

            // Clamp max acceleration to prevent jumpiness on lag spikes
            constexpr float MAX_ACCEL = 500.0f;
            accel.x = std::clamp(accel.x, -MAX_ACCEL, MAX_ACCEL);
            accel.y = std::clamp(accel.y, -MAX_ACCEL, MAX_ACCEL);
            accel.z = std::clamp(accel.z, -MAX_ACCEL, MAX_ACCEL);

            // Relative Velocity Compensation (subtract local player's movement)
            math::vector3 local_vel = get_local_player_velocity();
            math::vector3 rel_vel = {
                vel.x - local_vel.x,
                vel.y - local_vel.y,
                vel.z - local_vel.z
            };

            // Dynamic Ping Resolution
            float latency = settings::aimbot::latency_ms / 1000.0f;
            if (latency < 0.0f) latency = 0.0f;

            static std::uint64_t ping_item_addr = 0;
            static std::chrono::steady_clock::time_point last_ping_resolve = {};
            auto now_time = std::chrono::steady_clock::now();
            if (ping_item_addr == 0 || std::chrono::duration_cast<std::chrono::seconds>(now_time - last_ping_resolve).count() > 10) {
                ping_item_addr = 0;
                last_ping_resolve = now_time;
                if (game::datamodel.address != 0) {
                    rbx::instance_t stats = game::datamodel.find_first_child("Stats");
                    if (stats.address != 0) {
                        rbx::instance_t network = stats.find_first_child("Network");
                        if (network.address != 0) {
                            rbx::instance_t ping_item = network.find_first_child("Server Ping");
                            if (ping_item.address != 0) {
                                ping_item_addr = ping_item.address;
                            }
                        }
                    }
                }
            }

            if (ping_item_addr != 0) {
                try {
                    double ping_sec = memory->read<double>(ping_item_addr + Offsets::StatsItem::Value);
                    if (ping_sec > 0.001 && ping_sec < 5.0) {
                        latency = static_cast<float>(ping_sec);
                    } else if (ping_sec > 1.0 && ping_sec < 5000.0) {
                        latency = static_cast<float>(ping_sec / 1000.0);
                    }
                } catch (...) {}
            }

            float px = is_camera ? settings::aimbot::camera_prediction_x : settings::aimbot::mouse_prediction_x;
            float py = is_camera ? settings::aimbot::camera_prediction_y : settings::aimbot::mouse_prediction_y;

            float time_x = PREDICTION_SCALE * px + latency;
            float time_y = PREDICTION_SCALE * py + latency;

            if (settings::aimbot::projectile_prediction) {
                float speed = settings::aimbot::projectile_speed;
                if (speed <= 0.0f) speed = 1000.0f;

                rbx::instance_t camera_inst = { memory->read<std::uint64_t>(game::workspace.address + Offsets::Workspace::CurrentCamera) };
                if (camera_inst.address != 0) {
                    rbx::camera_t camera{ camera_inst.address };
                    math::vector3 camera_pos = camera.get_position();

                    float dx = pos.x - camera_pos.x;
                    float dy = pos.y - camera_pos.y;
                    float dz = pos.z - camera_pos.z;
                    float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
                    float travel_time = dist / speed;

                    time_x = travel_time + latency;
                    time_y = travel_time + latency;

                    // Apply projectile gravity drop (bullet drop = 0.5 * g * t^2)
                    pos.y += 0.5f * settings::aimbot::projectile_gravity * time_y * time_y;
                }
            }

            // Apply Second-Order relative prediction
            pos.x += rel_vel.x * time_x + 0.5f * accel.x * time_x * time_x;
            pos.y += rel_vel.y * time_y + 0.5f * accel.y * time_y * time_y;
            pos.z += rel_vel.z * time_x + 0.5f * accel.z * time_x * time_x;

            if (is_in_air) {
                // target gravity acceleration prediction (vertical drop/rise over prediction time)
                float drop_compensation = 0.5f * 196.2f * time_y * time_y;
                drop_compensation = std::clamp(drop_compensation, 0.0f, 50.0f);
                pos.y -= drop_compensation;
            }

            return pos;
        }

        math::matrix3 make_rotation_matrix(const math::vector3& forward) {
            math::vector3 up_dir = { 0.0f, 1.0f, 0.0f };
            if (std::abs(forward.y) > 0.9999f) {
                up_dir = { 0.0f, 0.0f, forward.y > 0.0f ? 1.0f : -1.0f };
            }
            
            math::vector3 right = normalize(cross(forward, up_dir));
            math::vector3 up = normalize(cross(right, forward));

            math::matrix3 result = {};
            result.m[0] = right.x; result.m[1] = up.x; result.m[2] = -forward.x;
            result.m[3] = right.y; result.m[4] = up.y; result.m[5] = -forward.y;
            result.m[6] = right.z; result.m[7] = up.z; result.m[8] = -forward.z;
            return result;
        }

        void execute_camera_aim(const math::vector3& target_pos, const POINT& cursor_pt, float dt, const math::vector2& dims, const math::matrix4& view) {
            rbx::instance_t camera_inst = { memory->read<std::uint64_t>(game::workspace.address + Offsets::Workspace::CurrentCamera) };
            if (!camera_inst.address) return;

            rbx::camera_t camera{ camera_inst.address };
            math::matrix3 current_rot = camera.get_rotation();
            math::vector3 camera_pos = camera.get_position();

            math::vector3 target_forward = normalize(vector3_sub(target_pos, camera_pos));

            math::vector3 current_forward = { -current_rot.m[2], -current_rot.m[5], -current_rot.m[8] };
            current_forward = normalize(current_forward);

            float current_yaw = 0.0f, current_pitch = 0.0f;
            vector_to_angles(current_forward, current_yaw, current_pitch);

            math::vector2 screen_pos = {};
            float screen_dist = 0.0f;
            if (game::visengine.world_to_client(target_pos, screen_pos, dims, view)) {
                float dx = screen_pos.x - static_cast<float>(cursor_pt.x);
                float dy = screen_pos.y - static_cast<float>(cursor_pt.y);
                screen_dist = std::sqrt(dx * dx + dy * dy);
            }

            float final_yaw = 0.0f;
            float final_pitch = 0.0f;

            if (settings::aimbot::camera_smooth) {
                float sx = std::clamp(settings::aimbot::camera_smooth_x, 1.0f, 200.0f);
                float sy = std::clamp(settings::aimbot::camera_smooth_y, 1.0f, 200.0f);

                if (settings::aimbot::adaptive_smoothing) {
                    float fov_r = settings::aimbot::fov;
                    if (fov_r <= 0.0f) fov_r = 300.0f;
                    float ratio = std::clamp(screen_dist / fov_r, 0.0f, 1.0f);
                    float adaptive_val = settings::aimbot::adaptive_smooth_min + (settings::aimbot::adaptive_smooth_max - settings::aimbot::adaptive_smooth_min) * (1.0f - ratio);
                    sx = adaptive_val;
                    sy = adaptive_val;
                }

                float target_yaw = 0.0f, target_pitch = 0.0f;
                vector_to_angles(target_forward, target_yaw, target_pitch);

                if (!virtual_angles_initialized) {
                    virtual_yaw = current_yaw;
                    virtual_pitch = current_pitch;
                    last_written_yaw = current_yaw;
                    last_written_pitch = current_pitch;
                    camera_yaw_vel = 0.0f;
                    camera_pitch_vel = 0.0f;
                    virtual_angles_initialized = true;
                } else {
                    float manual_yaw_diff = current_yaw - last_written_yaw;
                    manual_yaw_diff = std::atan2(std::sin(manual_yaw_diff), std::cos(manual_yaw_diff));

                    float manual_pitch_diff = current_pitch - last_written_pitch;
                    manual_pitch_diff = std::atan2(std::sin(manual_pitch_diff), std::cos(manual_pitch_diff));

                    if (std::abs(manual_yaw_diff) > 0.0001f) {
                        virtual_yaw += manual_yaw_diff;
                        lock_start_yaw += manual_yaw_diff;
                    }
                    if (std::abs(manual_pitch_diff) > 0.0001f) {
                        virtual_pitch += manual_pitch_diff;
                        lock_start_pitch += manual_pitch_diff;
                    }
                }

                if (settings::aimbot::spring_damping) {
                    float natural_freq_x = 800.0f / sx;
                    float natural_freq_y = 800.0f / sy;
                    virtual_yaw = apply_spring_damper_angle(virtual_yaw, target_yaw, camera_yaw_vel, natural_freq_x, dt);
                    virtual_pitch = apply_spring_damper(virtual_pitch, target_pitch, camera_pitch_vel, natural_freq_y, dt);
                } else {
                    if (!lock_start_angles_captured) {
                        lock_start_yaw = current_yaw;
                        lock_start_pitch = current_pitch;
                        lock_start_angles_captured = true;
                    }

                    float ease_time = settings::aimbot::ease_time;
                    if (ease_time <= 0.0f) ease_time = 0.01f;

                    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - lock_start_time
                    ).count() / 1000.0f;

                    float t = std::clamp(elapsed / ease_time, 0.0f, 1.0f);

                    if (t < 1.0f) {
                        float eased_t = apply_easing(settings::aimbot::easing_style, t);

                        float yaw_diff = target_yaw - lock_start_yaw;
                        yaw_diff = std::atan2(std::sin(yaw_diff), std::cos(yaw_diff));

                        float pitch_diff = target_pitch - lock_start_pitch;
                        pitch_diff = std::atan2(std::sin(pitch_diff), std::cos(pitch_diff));

                        virtual_yaw = lock_start_yaw + yaw_diff * eased_t;
                        virtual_pitch = lock_start_pitch + pitch_diff * eased_t;
                    } else {
                        float yaw_diff = target_yaw - virtual_yaw;
                        yaw_diff = std::atan2(std::sin(yaw_diff), std::cos(yaw_diff));

                        float pitch_diff = target_pitch - virtual_pitch;
                        pitch_diff = std::atan2(std::sin(pitch_diff), std::cos(pitch_diff));

                        float t_x = (sx <= 1.05f) ? 1.0f : std::clamp(1.0f - std::exp(-(800.0f / sx) * dt), 0.0f, 1.0f);
                        float t_y = (sy <= 1.05f) ? 1.0f : std::clamp(1.0f - std::exp(-(800.0f / sy) * dt), 0.0f, 1.0f);

                        virtual_yaw += yaw_diff * t_x;
                        virtual_pitch += pitch_diff * t_y;
                    }
                }

                virtual_pitch = std::clamp(virtual_pitch, -1.48f, 1.48f);

                last_written_yaw = virtual_yaw;
                last_written_pitch = virtual_pitch;

                final_yaw = virtual_yaw;
                final_pitch = virtual_pitch;
            } else {
                float target_yaw = 0.0f, target_pitch = 0.0f;
                vector_to_angles(target_forward, target_yaw, target_pitch);
                final_yaw = target_yaw;
                final_pitch = target_pitch;
            }

            math::vector3 smoothed_forward = angles_to_vector(final_yaw, final_pitch);
            smoothed_forward = normalize(smoothed_forward);

            if (settings::aimbot::shake) {
                float sx = settings::aimbot::shake_x;
                float sy = settings::aimbot::shake_y;
                if (std::abs(sx) > 0.001f || std::abs(sy) > 0.001f) {
                    float factor_x = sx * 0.01f;
                    float factor_y = sy * 0.01f;
                    smoothed_forward.x += (static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * 2.0f - 1.0f) * factor_x;
                    smoothed_forward.y += (static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * 2.0f - 1.0f) * factor_y;
                    smoothed_forward.z += (static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * 2.0f - 1.0f) * factor_x;
                    smoothed_forward = normalize(smoothed_forward);
                }
            }

            math::matrix3 target_matrix = make_rotation_matrix(smoothed_forward);
            camera.write_rotation(target_matrix);
        }

        void execute_mouse_aim(const math::vector3& target_pos, const POINT& cursor_pt, float dt, const math::vector2& dims, const math::matrix4& view) {
            math::vector2 screen_pos = {};
            if (!game::visengine.world_to_client(target_pos, screen_pos, dims, view)) return;

            float center_x = dims.x / 2.0f;
            float center_y = dims.y / 2.0f;

            float ref_x = static_cast<float>(cursor_pt.x);
            float ref_y = static_cast<float>(cursor_pt.y);

            if (std::abs(ref_x - center_x) <= 1.5f && std::abs(ref_y - center_y) <= 1.5f) {
                ref_x = center_x;
                ref_y = center_y;
            }

            float dx = screen_pos.x - ref_x;
            float dy = screen_pos.y - ref_y;
            float screen_dist = std::sqrt(dx * dx + dy * dy);

            if (settings::aimbot::mouse_smooth) {
                if (settings::aimbot::spring_damping) {
                    float sx = std::clamp(settings::aimbot::mouse_smooth_x, 1.0f, 200.0f);
                    float sy = std::clamp(settings::aimbot::mouse_smooth_y, 1.0f, 200.0f);

                    if (settings::aimbot::adaptive_smoothing) {
                        float fov_r = settings::aimbot::fov;
                        if (fov_r <= 0.0f) fov_r = 300.0f;
                        float ratio = std::clamp(screen_dist / fov_r, 0.0f, 1.0f);
                        float adaptive_val = settings::aimbot::adaptive_smooth_min + (settings::aimbot::adaptive_smooth_max - settings::aimbot::adaptive_smooth_min) * (1.0f - ratio);
                        sx = adaptive_val;
                        sy = adaptive_val;
                    }

                    float natural_freq_x = 800.0f / sx;
                    float natural_freq_y = 800.0f / sy;

                    float y0_x = -dx;
                    float y0_y = -dy;
                    float v0_x = mouse_vel_x;
                    float v0_y = mouse_vel_y;

                    float e_x = std::exp(-natural_freq_x * dt);
                    float e_y = std::exp(-natural_freq_y * dt);

                    float temp_x = (v0_x + natural_freq_x * y0_x) * dt;
                    float temp_y = (v0_y + natural_freq_y * y0_y) * dt;

                    float y_new_x = (y0_x + temp_x) * e_x;
                    float y_new_y = (y0_y + temp_y) * e_y;

                    mouse_vel_x = (v0_x - natural_freq_x * temp_x) * e_x;
                    mouse_vel_y = (v0_y - natural_freq_y * temp_y) * e_y;

                    dx = dx + y_new_x;
                    dy = dy + y_new_y;
                } else {
                    if (!lock_start_mouse_captured) {
                        lock_start_cursor_pos = cursor_pt;
                        lock_start_mouse_captured = true;
                    }

                    float ease_time = settings::aimbot::ease_time;
                    if (ease_time <= 0.0f) ease_time = 0.01f;

                    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - lock_start_time
                    ).count() / 1000.0f;

                    float t = std::clamp(elapsed / ease_time, 0.0f, 1.0f);

                    if (t < 1.0f) {
                        float eased_t = apply_easing(settings::aimbot::easing_style, t);

                        float start_x = static_cast<float>(lock_start_cursor_pos.x);
                        float start_y = static_cast<float>(lock_start_cursor_pos.y);

                        float virtual_screen_x = start_x + (screen_pos.x - start_x) * eased_t;
                        float virtual_screen_y = start_y + (screen_pos.y - start_y) * eased_t;

                        dx = virtual_screen_x - ref_x;
                        dy = virtual_screen_y - ref_y;
                    } else {
                        float sx = std::clamp(settings::aimbot::mouse_smooth_x, 1.0f, 200.0f);
                        float sy = std::clamp(settings::aimbot::mouse_smooth_y, 1.0f, 200.0f);

                        if (settings::aimbot::adaptive_smoothing) {
                            float fov_r = settings::aimbot::fov;
                            if (fov_r <= 0.0f) fov_r = 300.0f;
                            float ratio = std::clamp(screen_dist / fov_r, 0.0f, 1.0f);
                            float adaptive_val = settings::aimbot::adaptive_smooth_min + (settings::aimbot::adaptive_smooth_max - settings::aimbot::adaptive_smooth_min) * (1.0f - ratio);
                            sx = adaptive_val;
                            sy = adaptive_val;
                        }

                        float t_x = (sx <= 1.05f) ? 1.0f : std::clamp(1.0f - std::exp(-(800.0f / sx) * dt), 0.0f, 1.0f);
                        float t_y = (sy <= 1.05f) ? 1.0f : std::clamp(1.0f - std::exp(-(800.0f / sy) * dt), 0.0f, 1.0f);

                        dx = dx * t_x;
                        dy = dy * t_y;
                    }
                }
            }

            float sensitivity = std::clamp(settings::aimbot::mouse_sensitivity, 0.1f, 10.0f);
            dx *= sensitivity;
            dy *= sensitivity;

            if (settings::aimbot::shake) {
                float sx = std::abs(settings::aimbot::shake_x);
                float sy = std::abs(settings::aimbot::shake_y);
                int range_x = static_cast<int>(sx * 2.0f) + 1;
                int range_y = static_cast<int>(sy * 2.0f) + 1;
                dx += static_cast<float>(rand() % range_x) - sx;
                dy += static_cast<float>(rand() % range_y) - sy;
            }

            float max_mouse_delta = 80.0f;
            dx = std::clamp(dx, -max_mouse_delta, max_mouse_delta);
            dy = std::clamp(dy, -max_mouse_delta, max_mouse_delta);

            if (std::isfinite(dx) && std::isfinite(dy)) {
                accum_x += dx;
                accum_y += dy;

                LONG move_x = static_cast<LONG>(accum_x);
                LONG move_y = static_cast<LONG>(accum_y);

                accum_x -= static_cast<float>(move_x);
                accum_y -= static_cast<float>(move_y);

                if (move_x != 0 || move_y != 0) {
                    input::move_mouse_relative(move_x, move_y);
                }
            } else {
                accum_x = 0.0f;
                accum_y = 0.0f;
            }
        }

        bool get_key_state() {
            switch (settings::aimbot::keybind_mode) {
            case 0: 
                return GetAsyncKeyState(settings::aimbot::keybind) & 0x8000;
            case 1: 
            {
                bool pressed = GetAsyncKeyState(settings::aimbot::keybind) & 0x8000;
                if (pressed && !key_was_pressed) toggle_state = !toggle_state;
                key_was_pressed = pressed;
                return toggle_state;
            }
            case 2: 
                return true;
            default:
                return false;
            }
        }
    }

    void run() {
        POINT cursor_pt = {};
        static auto last_tick = std::chrono::high_resolution_clock::now();
        static math::matrix4 last_view = {};
        static math::vector3 last_target_pos = {};

        while (true) {
            Sleep(1);

            auto now = std::chrono::high_resolution_clock::now();
            float dt = std::chrono::duration<float>(now - last_tick).count();
            
            if (dt < 0.001f) {
                continue;
            }
            last_tick = now;

            if (dt > 0.1f) dt = 0.016f;
            else if (dt < 0.0005f) dt = 0.0005f;

             if (!settings::aimbot::enabled ||
                (settings::aimbot::aimbot_type < 0 || settings::aimbot::aimbot_type > 1) ||
                check::textchatopen || !game::workspace.address) {
                std::lock_guard<std::mutex> lock(mtx);
                locked_target = cache::entity_t{};
                has_locked_target = false;
                needs_key_release = false;
                was_disabled_by_typing = check::textchatopen;
                target_pos_initialized = false; 
                velocity_initialized = false;
                virtual_angles_initialized = false; 
                locked_part_name = "";
                accum_x = 0.0f;
                accum_y = 0.0f;

                is_lock_active = false;
                last_target_address = 0;
                lock_start_angles_captured = false;
                lock_start_mouse_captured = false;
                is_currently_occluded = false;
                target_random_offset = { 0.0f, 0.0f, 0.0f };
                continue;
            }

            if (was_disabled_by_typing && !check::textchatopen) {
                was_disabled_by_typing = false;
            }

            if (!get_key_state()) {
                std::lock_guard<std::mutex> lock(mtx);
                locked_target = cache::entity_t{};
                has_locked_target = false;
                needs_key_release = false;
                target_pos_initialized = false; 
                velocity_initialized = false;
                virtual_angles_initialized = false; 
                locked_part_name = "";
                accum_x = 0.0f;
                accum_y = 0.0f;

                is_lock_active = false;
                last_target_address = 0;
                lock_start_angles_captured = false;
                lock_start_mouse_captured = false;
                is_currently_occluded = false;
                target_random_offset = { 0.0f, 0.0f, 0.0f };
                continue;
            }

            if (needs_key_release) {
                continue;
            }

            if (!GetCursorPos(&cursor_pt)) continue;
            HWND roblox_wnd = game::wnd;
            if (!roblox_wnd) {
                roblox_wnd = FindWindowA(nullptr, "Roblox");
                if (roblox_wnd) game::wnd = roblox_wnd;
            }
            if (!roblox_wnd || !ScreenToClient(roblox_wnd, &cursor_pt)) continue;

            math::vector2 dims = game::visengine.get_dimensions();
            math::matrix4 view = game::visengine.get_viewmatrix();

            std::lock_guard<std::mutex> lock(mtx);

            std::vector<cache::entity_t> players_snapshot;
            cache::entity_t local_player_snapshot = {};
            {
                std::lock_guard<std::mutex> cache_lock(cache::mtx);
                std::shared_ptr<std::vector<cache::entity_t>> cached_ptr_copy = cache::cached_players;
                if (cached_ptr_copy) {
                    players_snapshot = *cached_ptr_copy;
                }
                local_player_snapshot = cache::cached_local_player;
            }
            std::string local_crew_id = local_player_snapshot.crew_id;
            std::uint64_t local_player_addr = local_player_snapshot.instance.address;

            // 1. Check if the previously locked target died/got knocked/left
            static std::uint64_t aimbot_last_seen_address = 0;
            static std::chrono::steady_clock::time_point aimbot_last_seen_time = std::chrono::steady_clock::now();
            static cache::entity_t last_known_aimbot_target = {};

            bool locked_target_still_exists = false;
            cache::entity_t updated_locked_target = {};
            if (has_locked_target && locked_target.instance.address != 0) {
                for (const auto& player : players_snapshot) {
                    if (player.instance.address == locked_target.instance.address) {
                        updated_locked_target = player;
                        locked_target_still_exists = true;
                        break;
                    }
                }
            }

            bool locked_target_died = false;
            if (has_locked_target && locked_target.instance.address != 0) {
                if (locked_target.instance.address != aimbot_last_seen_address) {
                    aimbot_last_seen_address = locked_target.instance.address;
                    aimbot_last_seen_time = std::chrono::steady_clock::now();
                }

                if (locked_target_still_exists) {
                    last_known_aimbot_target = updated_locked_target;
                } else {
                    auto missing_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - aimbot_last_seen_time
                    ).count();
                    if (missing_duration > 150) {
                        locked_target_died = true;
                    } else {
                        updated_locked_target = last_known_aimbot_target;
                        locked_target_still_exists = true;
                    }
                }

                if (!locked_target_died) {
                    // Check health
                    if (updated_locked_target.humanoid.address == 0) {
                        locked_target_died = true;
                    } else {
                        try {
                            float health = const_cast<cache::entity_t&>(updated_locked_target).humanoid.get_health();
                            if (health <= 0.0f || !std::isfinite(health)) {
                                locked_target_died = true;
                            }
                        } catch (...) {
                            locked_target_died = true;
                        }
                    }
                    // Check if knocked (if knocked_check is enabled)
                    if (!locked_target_died && settings::aimbot::knocked_check && is_knocked(updated_locked_target)) {
                        locked_target_died = true;
                    }
                }
            }

            if (locked_target_died) {
                locked_target = cache::entity_t{};
                has_locked_target = false;
                target_pos_initialized = false;
                velocity_initialized = false;
                virtual_angles_initialized = false;
                last_written_yaw = 0.0f;
                last_written_pitch = 0.0f;
                locked_part_name = "";
                accum_x = 0.0f;
                accum_y = 0.0f;

                is_lock_active = false;
                last_target_address = 0;
                lock_start_angles_captured = false;
                lock_start_mouse_captured = false;
                is_currently_occluded = false;
                target_random_offset = { 0.0f, 0.0f, 0.0f };

                {
                    std::lock_guard<std::mutex> lock_g(g_aimbot_mutex);
                    g_aimbot_manual_locked = false;
                    g_aimbot_manual_target = {};
                }
                if (settings::aimbot::knocked_check) {
                    needs_key_release = true;
                }
                continue;
            }

            cache::entity_t target = {};

            // 2. Try manual target lock first
            bool is_manual_locked = false;
            cache::entity_t manual_target_snap = {};
            {
                std::lock_guard<std::mutex> lock(g_aimbot_mutex);
                is_manual_locked = g_aimbot_manual_locked;
                manual_target_snap = g_aimbot_manual_target;
            }

            if (is_manual_locked && manual_target_snap.instance.address != 0) {
                bool found = false;
                for (const auto& player : players_snapshot) {
                    if (player.instance.address == manual_target_snap.instance.address) {
                        manual_target_snap = player;
                        found = true;
                        break;
                    }
                }
                if (found) {
                    bool relation_invalid = false;
                    {
                        std::lock_guard<std::mutex> lock(settings::player_relations::relations_mutex);
                        auto rel_it = settings::player_relations::relations.find(manual_target_snap.name);
                        if (rel_it != settings::player_relations::relations.end() && rel_it->second == 1) {
                            relation_invalid = true;
                        }
                    }
                    if (settings::aimbot::team_check && is_on_same_team(manual_target_snap, local_crew_id)) {
                        relation_invalid = true;
                    }

                    if (!relation_invalid) {
                        bool knocked_invalid = false;
                        if (settings::aimbot::knocked_check && is_knocked(manual_target_snap)) {
                            knocked_invalid = true;
                        }
                        if (!knocked_invalid && is_target_valid(manual_target_snap, local_crew_id, cursor_pt, dims, view, true)) {
                            target = manual_target_snap;
                            {
                                std::lock_guard<std::mutex> lock(g_aimbot_mutex);
                                g_aimbot_manual_target = manual_target_snap;
                            }
                        }
                    }
                }
            }

            // 3. Resolve target for automatic aimbot
            if (target.instance.address == 0) {
                if (has_locked_target && locked_target.instance.address != 0) {
                    // Check if relation, team or knocked is invalid
                    bool relation_valid = true;
                    {
                        std::lock_guard<std::mutex> lock(settings::player_relations::relations_mutex);
                        auto rel_it = settings::player_relations::relations.find(updated_locked_target.name);
                        if (rel_it != settings::player_relations::relations.end() && rel_it->second == 1) {
                            relation_valid = false;
                        }
                    }
                    bool team_valid = !(settings::aimbot::team_check && is_on_same_team(updated_locked_target, local_crew_id));
                    bool knocked_valid = !(settings::aimbot::knocked_check && is_knocked(updated_locked_target));

                    if (relation_valid && team_valid && knocked_valid) {
                        bool within_fov = true;
                        if (!settings::aimbot::sticky_aim) {
                            if (settings::aimbot::fov_check) {
                                rbx::part_t target_part = get_closest_part(updated_locked_target, cursor_pt, dims, view);
                                if (target_part.address != 0) {
                                    rbx::primitive_t primitive = target_part.get_primitive();
                                    if (primitive.address != 0) {
                                        math::vector3 world_pos = primitive.get_position();
                                        math::vector2 screen_pos = {};
                                        if (game::visengine.world_to_client(world_pos, screen_pos, dims, view)) {
                                            float cursor_x = static_cast<float>(cursor_pt.x);
                                            float cursor_y = static_cast<float>(cursor_pt.y);
                                            float dist = vector2_distance(screen_pos.x, screen_pos.y, cursor_x, cursor_y);
                                            if (dist > settings::aimbot::fov) {
                                                within_fov = false;
                                            }
                                        } else {
                                            within_fov = false;
                                        }
                                    } else {
                                        within_fov = false;
                                    }
                                } else {
                                    within_fov = false;
                                }
                            }
                        }

                        if (within_fov) {
                            bool visible = true;
                            // if (settings::aimbot::wall_check && !is_player_visible(updated_locked_target)) {
                            //     visible = false;
                            // }
                            if (visible) {
                                target = updated_locked_target;
                                is_currently_occluded = false;
                                locked_target = updated_locked_target; // keep it updated
                            } else {
                                if (settings::aimbot::sticky_aim) {
                                    if (!is_currently_occluded) {
                                        is_currently_occluded = true;
                                        occlusion_start_time = std::chrono::steady_clock::now();
                                    } else {
                                        auto elapsed_occluded = std::chrono::duration_cast<std::chrono::milliseconds>(
                                            std::chrono::steady_clock::now() - occlusion_start_time
                                        ).count();
                                        if (elapsed_occluded > 500) {
                                            bool is_dead = false;
                                            if (updated_locked_target.humanoid.address == 0) {
                                                is_dead = true;
                                            } else {
                                                try {
                                                    float health = const_cast<cache::entity_t&>(updated_locked_target).humanoid.get_health();
                                                    if (health <= 0.0f || !std::isfinite(health)) {
                                                        is_dead = true;
                                                    }
                                                } catch (...) {
                                                    is_dead = true;
                                                }
                                            }

                                            locked_target = cache::entity_t{};
                                            has_locked_target = false;
                                            is_currently_occluded = false;
                                            target_pos_initialized = false;
                                            velocity_initialized = false;
                                            virtual_angles_initialized = false;
                                            last_written_yaw = 0.0f;
                                            last_written_pitch = 0.0f;
                                            locked_part_name = "";
                                            accum_x = 0.0f;
                                            accum_y = 0.0f;

                                            is_lock_active = false;
                                            last_target_address = 0;
                                            lock_start_angles_captured = false;
                                            lock_start_mouse_captured = false;
                                            target_random_offset = { 0.0f, 0.0f, 0.0f };

                                            {
                                                std::lock_guard<std::mutex> lock_g(g_aimbot_mutex);
                                                g_aimbot_manual_locked = false;
                                                g_aimbot_manual_target = {};
                                            }
                                            if (!is_dead) {
                                                needs_key_release = true;
                                            }
                                            continue;
                                        }
                                    }
                                    if (has_locked_target) {
                                        locked_target = updated_locked_target; // keep it updated while occluded
                                    }
                                } else {
                                    // Non-sticky aim: break lock instantly if occluded
                                    locked_target = cache::entity_t{};
                                    has_locked_target = false;
                                    target_pos_initialized = false;
                                    velocity_initialized = false;
                                    virtual_angles_initialized = false;
                                    last_written_yaw = 0.0f;
                                    last_written_pitch = 0.0f;
                                    locked_part_name = "";
                                    accum_x = 0.0f;
                                    accum_y = 0.0f;

                                    is_lock_active = false;
                                    last_target_address = 0;
                                    lock_start_angles_captured = false;
                                    lock_start_mouse_captured = false;
                                    target_random_offset = { 0.0f, 0.0f, 0.0f };

                                    {
                                        std::lock_guard<std::mutex> lock_g(g_aimbot_mutex);
                                        g_aimbot_manual_locked = false;
                                        g_aimbot_manual_target = {};
                                    }
                                    needs_key_release = true;
                                    continue;
                                }
                            }
                        } else {
                            // Non-sticky: out of FOV, break lock
                            locked_target = cache::entity_t{};
                            has_locked_target = false;
                            target_pos_initialized = false;
                            velocity_initialized = false;
                            virtual_angles_initialized = false;
                            last_written_yaw = 0.0f;
                            last_written_pitch = 0.0f;
                            locked_part_name = "";
                            accum_x = 0.0f;
                            accum_y = 0.0f;

                            is_lock_active = false;
                            last_target_address = 0;
                            lock_start_angles_captured = false;
                            lock_start_mouse_captured = false;
                            target_random_offset = { 0.0f, 0.0f, 0.0f };

                            {
                                std::lock_guard<std::mutex> lock_g(g_aimbot_mutex);
                                g_aimbot_manual_locked = false;
                                g_aimbot_manual_target = {};
                            }
                            needs_key_release = true;
                            continue;
                        }
                    } else {
                        // Dead/knocked/team change/relations: break lock
                        locked_target = cache::entity_t{};
                        has_locked_target = false;
                        target_pos_initialized = false;
                        velocity_initialized = false;
                        virtual_angles_initialized = false;
                        last_written_yaw = 0.0f;
                        last_written_pitch = 0.0f;
                        locked_part_name = "";
                        accum_x = 0.0f;
                        accum_y = 0.0f;

                        is_lock_active = false;
                        last_target_address = 0;
                        lock_start_angles_captured = false;
                        lock_start_mouse_captured = false;
                        target_random_offset = { 0.0f, 0.0f, 0.0f };

                        {
                            std::lock_guard<std::mutex> lock_g(g_aimbot_mutex);
                            g_aimbot_manual_locked = false;
                            g_aimbot_manual_target = {};
                        }

                        needs_key_release = true;
                        continue;
                    }
                }

                if (target.instance.address == 0 && !has_locked_target) {
                    target = find_best_target(players_snapshot, local_crew_id, cursor_pt, dims, view, local_player_addr);
                    if (target.instance.address != 0) {
                        locked_target = target;
                        has_locked_target = true;
                        target_pos_initialized = false;
                        velocity_initialized = false;
                        locked_part_name = "";
                        accum_x = 0.0f;
                        accum_y = 0.0f;
                    }
                }
            }

            if (target.instance.address == 0) continue;

            // Update lock start states if target changed
            if (!is_lock_active || target.instance.address != last_target_address) {
                is_lock_active = true;
                last_target_address = target.instance.address;
                lock_start_time = std::chrono::steady_clock::now();
                lock_start_angles_captured = false;
                lock_start_mouse_captured = false;
                is_currently_occluded = false;
                update_random_offset();

                camera_yaw_vel = 0.0f;
                camera_pitch_vel = 0.0f;
                mouse_vel_x = 0.0f;
                mouse_vel_y = 0.0f;

                velocity_initialized = false;
                acceleration_initialized = false;
                smoothed_velocity = { 0.0f, 0.0f, 0.0f };
                smoothed_acceleration = { 0.0f, 0.0f, 0.0f };
                last_velocity = { 0.0f, 0.0f, 0.0f };
            }

            rbx::part_t target_part = {};
            if (settings::aimbot::aimpart == 9) { 
                rbx::part_t closest = get_closest_part(target, cursor_pt, dims, view);
                if (closest.address != 0) {
                    locked_part_name = closest.get_name();
                    target_part = closest;
                }
            } else {
                target_part = get_target_part(target, settings::aimbot::aimpart, cursor_pt, dims, view);
            }

            if (!target_part.address) continue;

            rbx::primitive_t primitive = target_part.get_primitive();
            math::vector3 raw_target_pos = primitive.get_position();

            if (!target_pos_initialized) {
                filtered_target_pos = raw_target_pos;
                target_pos_initialized = true;
            } else {
                float filter_constant = 35.0f;
                float alpha = std::clamp(dt * filter_constant, 0.0f, 1.0f);
                filtered_target_pos.x += (raw_target_pos.x - filtered_target_pos.x) * alpha;
                filtered_target_pos.y += (raw_target_pos.y - filtered_target_pos.y) * alpha;
                filtered_target_pos.z += (raw_target_pos.z - filtered_target_pos.z) * alpha;
            }

            math::vector3 final_target_pos = filtered_target_pos;

            bool use_prediction = (settings::aimbot::aimbot_type == 0 && settings::aimbot::camera_prediction) ||
                                  (settings::aimbot::aimbot_type == 1 && settings::aimbot::mouse_prediction);
            if (use_prediction) {
                bool target_in_air = false;
                if (target.humanoid.address != 0) {
                    try {
                        std::uint32_t floor_mat = memory->read<std::uint32_t>(target.humanoid.address + Offsets::Humanoid::FloorMaterial);
                        target_in_air = (floor_mat == 0);
                    } catch (...) {}
                }
                final_target_pos = apply_prediction(filtered_target_pos + target_random_offset, primitive, settings::aimbot::aimbot_type == 0, dt, target_in_air);
            } else {
                final_target_pos = filtered_target_pos + target_random_offset;
            }

            if (settings::aimbot::aimbot_type == 0) {
                execute_camera_aim(final_target_pos, cursor_pt, dt, dims, view);
            }
            else {
                execute_mouse_aim(final_target_pos, cursor_pt, dt, dims, view);
            }
        }
    }

    void initialize() {
        std::lock_guard<std::mutex> lock(mtx);
        std::lock_guard<std::mutex> lock_g(g_aimbot_mutex);
        locked_target = cache::entity_t{};
        has_locked_target = false;
        key_was_pressed = false;
        toggle_state = false;
        was_disabled_by_typing = false;
        needs_key_release = false;
        target_pos_initialized = false;
        velocity_initialized = false;
        virtual_angles_initialized = false;
        last_written_yaw = 0.0f;
        last_written_pitch = 0.0f;
        locked_part_name = "";
        g_aimbot_manual_locked = false;
        g_aimbot_manual_target = {};
        accum_x = 0.0f;
        accum_y = 0.0f;

        camera_yaw_vel = 0.0f;
        camera_pitch_vel = 0.0f;
        mouse_vel_x = 0.0f;
        mouse_vel_y = 0.0f;

        // Reset tracking states
        is_lock_active = false;
        last_target_address = 0;
        lock_start_angles_captured = false;
        lock_start_mouse_captured = false;
        is_currently_occluded = false;
        target_random_offset = { 0.0f, 0.0f, 0.0f };

        acceleration_initialized = false;
        smoothed_velocity = { 0.0f, 0.0f, 0.0f };
        smoothed_acceleration = { 0.0f, 0.0f, 0.0f };
        last_velocity = { 0.0f, 0.0f, 0.0f };
    }

    void lock_target(const cache::entity_t& target) {
        std::lock_guard<std::mutex> lock(mtx);
        std::lock_guard<std::mutex> lock_g(g_aimbot_mutex);
        locked_target = target;
        has_locked_target = true;
        g_aimbot_manual_locked = true;
        g_aimbot_manual_target = target;
        target_pos_initialized = false;
        velocity_initialized = false;
        virtual_angles_initialized = false;
        last_written_yaw = 0.0f;
        last_written_pitch = 0.0f;
        locked_part_name = "";
        accum_x = 0.0f;
        accum_y = 0.0f;

        camera_yaw_vel = 0.0f;
        camera_pitch_vel = 0.0f;
        mouse_vel_x = 0.0f;
        mouse_vel_y = 0.0f;

        // Reset tracking states
        is_lock_active = true;
        last_target_address = target.instance.address;
        lock_start_time = std::chrono::steady_clock::now();
        lock_start_angles_captured = false;
        lock_start_mouse_captured = false;
        is_currently_occluded = false;
        update_random_offset();

        acceleration_initialized = false;
        smoothed_velocity = { 0.0f, 0.0f, 0.0f };
        smoothed_acceleration = { 0.0f, 0.0f, 0.0f };
        last_velocity = { 0.0f, 0.0f, 0.0f };
    }

    void unlock_target() {
        std::lock_guard<std::mutex> lock(mtx);
        std::lock_guard<std::mutex> lock_g(g_aimbot_mutex);
        locked_target = cache::entity_t{};
        has_locked_target = false;
        g_aimbot_manual_locked = false;
        g_aimbot_manual_target = {};
        target_pos_initialized = false;
        velocity_initialized = false;
        virtual_angles_initialized = false;
        last_written_yaw = 0.0f;
        last_written_pitch = 0.0f;
        locked_part_name = "";
        accum_x = 0.0f;
        accum_y = 0.0f;

        camera_yaw_vel = 0.0f;
        camera_pitch_vel = 0.0f;
        mouse_vel_x = 0.0f;
        mouse_vel_y = 0.0f;

        // Reset tracking states
        is_lock_active = false;
        last_target_address = 0;
        lock_start_angles_captured = false;
        lock_start_mouse_captured = false;
        is_currently_occluded = false;
        target_random_offset = { 0.0f, 0.0f, 0.0f };

        acceleration_initialized = false;
        smoothed_velocity = { 0.0f, 0.0f, 0.0f };
        smoothed_acceleration = { 0.0f, 0.0f, 0.0f };
        last_velocity = { 0.0f, 0.0f, 0.0f };
    }

    void render() {
        
    }
}