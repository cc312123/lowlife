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
#include <sdk/sdk.h>
#include <sdk/offsets.h>
#include <cache/cache.h>
#include <game/game.h>
#include <settings.h>
#include <check/typing_check.h>
#include "../shot_detection/shot_detection.h"
#include "aimbot.h"

namespace rbx::aimbot {
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

        float virtual_yaw = 0.0f;
        float virtual_pitch = 0.0f;
        bool virtual_angles_initialized = false;

        float spring_vel_yaw = 0.0f;
        float spring_vel_pitch = 0.0f;
        float spring_vel_mouse_x = 0.0f;
        float spring_vel_mouse_y = 0.0f;

        // Custom humanized features state
        math::vector3 current_target_offset = { 0.0f, 0.0f, 0.0f };
        std::uint64_t last_locked_address = 0;

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

        float vector3_length(const math::vector3& vec) {
            return std::sqrt(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z);
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
            return player.is_knocked;
        }

        bool is_on_same_team(const cache::entity_t& player, const std::string& local_crew_id) {
            if (local_crew_id.empty() || player.crew_id.empty()) return false;
            if (local_crew_id == "0" || player.crew_id == "0") return false;
            return local_crew_id == player.crew_id;
        }

        // Computes adaptive smoothing based on target screen offset distance
        float get_adaptive_smooth(float screen_dist_to_target) {
            if (!settings::aimbot::adaptive_smoothing) return 1.0f;
            float min_s = settings::aimbot::adaptive_smooth_min;
            float max_s = settings::aimbot::adaptive_smooth_max;
            if (min_s > max_s) std::swap(min_s, max_s);

            // Factor scales from 0.0 (directly on crosshair) to 1.0 (far away)
            float factor = std::clamp(screen_dist_to_target / 150.0f, 0.0f, 1.0f);
            // Linear interpolation: slower tracking (max_s) when close, faster snap (min_s) when far
            return max_s - (max_s - min_s) * factor;
        }

        // Generates/updates persistent humanized targeting offset on target lock switches
        void update_target_offset(std::uint64_t target_address) {
            if (target_address != last_locked_address) {
                last_locked_address = target_address;
                if (settings::aimbot::bone_random_offset && target_address != 0) {
                    float r = settings::aimbot::bone_random_offset_val;
                    float theta = (static_cast<float>(rand()) / RAND_MAX) * 2.f * M_PI_F;
                    float phi = (static_cast<float>(rand()) / RAND_MAX) * M_PI_F;
                    float radius = (static_cast<float>(rand()) / RAND_MAX) * r;
                    current_target_offset.x = radius * std::sin(phi) * std::cos(theta);
                    current_target_offset.y = radius * std::sin(phi) * std::sin(theta);
                    current_target_offset.z = radius * std::cos(phi);
                } else {
                    current_target_offset = { 0.0f, 0.0f, 0.0f };
                }
            }
        }

        rbx::part_t get_closest_part(cache::entity_t& player, const POINT& cursor_pt, const math::vector2& dims, const math::matrix4& view) {
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

            for (auto& pair : player.parts) {
                if (valid_hitparts.find(pair.first) == valid_hitparts.end()) continue;
                rbx::part_t part = pair.second;
                if (!part.address) continue;
                rbx::primitive_t primitive = part.get_primitive();
                math::vector3 world_pos = primitive.get_position();
                math::vector2 screen_pos = {};
                if (!game::visengine.world_to_screen(world_pos, screen_pos, dims, view)) continue;
                float dist = vector2_distance(screen_pos.x, screen_pos.y, cursor_x, cursor_y);
                if (dist < min_dist) {
                    min_dist = dist;
                    closest = part;
                }
            }
            return closest;
        }

        // Dynamically scans bones to find a visible target fallback or resolves to static selection
        rbx::part_t get_best_bone(cache::entity_t& player, const math::vector3& camera_pos, const POINT& cursor_pt, const math::vector2& dims, const math::matrix4& view, std::string& chosen_bone_name) {
            if (player.parts.empty()) return rbx::part_t{};

            const std::vector<std::string> bone_priority = {
                "Head", "UpperTorso", "Torso", "LowerTorso", "HumanoidRootPart",
                "LeftUpperArm", "RightUpperArm", "LeftUpperLeg", "RightUpperLeg"
            };

            if (settings::aimbot::smart_bone) {
                for (const auto& bone_name : bone_priority) {
                    auto it = player.parts.find(bone_name);
                    if (it != player.parts.end() && it->second.address != 0) {
                        math::vector3 pos = it->second.get_primitive().get_position();
                        if (settings::aimbot::wall_check) {
                            if (!botter::is_occluded(camera_pos, pos)) {
                                chosen_bone_name = bone_name;
                                return it->second;
                            }
                        } else {
                            chosen_bone_name = bone_name;
                            return it->second;
                        }
                    }
                }
            }

            int aim_part = settings::aimbot::aimpart;
            chosen_bone_name = "";
            switch (aim_part) {
            case 0:
                if (auto it = player.parts.find("Head"); it != player.parts.end()) { chosen_bone_name = "Head"; return it->second; }
                break;
            case 1:
                if (auto it = player.parts.find("UpperTorso"); it != player.parts.end()) { chosen_bone_name = "UpperTorso"; return it->second; }
                if (auto it = player.parts.find("Torso"); it != player.parts.end()) { chosen_bone_name = "Torso"; return it->second; }
                break;
            case 2:
                if (auto it = player.parts.find("Torso"); it != player.parts.end()) { chosen_bone_name = "Torso"; return it->second; }
                if (auto it = player.parts.find("UpperTorso"); it != player.parts.end()) { chosen_bone_name = "UpperTorso"; return it->second; }
                break;
            case 3:
                if (auto it = player.parts.find("LowerTorso"); it != player.parts.end()) { chosen_bone_name = "LowerTorso"; return it->second; }
                if (auto it = player.parts.find("Torso"); it != player.parts.end()) { chosen_bone_name = "Torso"; return it->second; }
                break;
            case 4:
                if (auto it = player.parts.find("HumanoidRootPart"); it != player.parts.end()) { chosen_bone_name = "HumanoidRootPart"; return it->second; }
                break;
            case 5:
                if (auto it = player.parts.find("LeftUpperArm"); it != player.parts.end()) { chosen_bone_name = "LeftUpperArm"; return it->second; }
                if (auto it = player.parts.find("Left Arm"); it != player.parts.end()) { chosen_bone_name = "Left Arm"; return it->second; }
                break;
            case 6:
                if (auto it = player.parts.find("RightUpperArm"); it != player.parts.end()) { chosen_bone_name = "RightUpperArm"; return it->second; }
                if (auto it = player.parts.find("Right Arm"); it != player.parts.end()) { chosen_bone_name = "Right Arm"; return it->second; }
                break;
            case 7:
                if (auto it = player.parts.find("LeftUpperLeg"); it != player.parts.end()) { chosen_bone_name = "LeftUpperLeg"; return it->second; }
                if (auto it = player.parts.find("Left Leg"); it != player.parts.end()) { chosen_bone_name = "Left Leg"; return it->second; }
                break;
            case 8:
                if (auto it = player.parts.find("RightUpperLeg"); it != player.parts.end()) { chosen_bone_name = "RightUpperLeg"; return it->second; }
                if (auto it = player.parts.find("Right Leg"); it != player.parts.end()) { chosen_bone_name = "Right Leg"; return it->second; }
                break;
            case 9:
                {
                    rbx::part_t closest = get_closest_part(player, cursor_pt, dims, view);
                    if (closest.address != 0) {
                        chosen_bone_name = closest.get_name();
                        return closest;
                    }
                }
                break;
            }

            if (auto it = player.parts.find("HumanoidRootPart"); it != player.parts.end()) { chosen_bone_name = "HumanoidRootPart"; return it->second; }
            return rbx::part_t{};
        }

        bool is_target_cheap_valid(cache::entity_t& player, const std::string& local_crew_id) {
            if (player.instance.address == 0) return false;

            auto rel_it = settings::player_relations::relations.find(player.name);
            if (rel_it != settings::player_relations::relations.end() && rel_it->second == 1) {
                return false;
            }

            if (settings::aimbot::team_check && is_on_same_team(player, local_crew_id)) {
                return false;
            }

            if (settings::aimbot::knocked_check && is_knocked(player)) return false;

            return true;
        }

        bool is_target_valid(cache::entity_t& player, const std::string& local_crew_id, const POINT& cursor_pt, const math::vector2& dims, const math::matrix4& view, const math::vector3& camera_pos, bool skip_fov_check = false) {
            if (!is_target_cheap_valid(player, local_crew_id)) return false;

            std::string temp_name = "";
            rbx::part_t target_part = get_best_bone(player, camera_pos, cursor_pt, dims, view, temp_name);
            if (!target_part.address) return false;

            math::vector3 world_pos = target_part.get_primitive().get_position();

            if (settings::aimbot::fov_check && !skip_fov_check) {
                math::vector2 screen_pos = {};
                if (!game::visengine.world_to_screen(world_pos, screen_pos, dims, view)) return false;

                float cursor_x = static_cast<float>(cursor_pt.x);
                float cursor_y = static_cast<float>(cursor_pt.y);
                float dist = vector2_distance(screen_pos.x, screen_pos.y, cursor_x, cursor_y);
                if (dist > settings::aimbot::fov) return false;
            }

            if (settings::aimbot::wall_check) {
                if (botter::is_occluded(camera_pos, world_pos)) {
                    return false;
                }
            }

            return true;
        }

        // Lock-free scans for best target using copied entities snapshots
        cache::entity_t find_best_target(
            std::vector<cache::entity_t>& players, 
            cache::entity_t& local_player, 
            const POINT& cursor_pt, 
            const math::vector2& dims, 
            const math::matrix4& view, 
            const math::vector3& camera_pos,
            rbx::part_t& out_best_part,
            std::string& out_best_part_name
        ) {
            cache::entity_t best = {};
            rbx::part_t best_part = {};
            std::string best_part_name = "";
            
            float best_score = std::numeric_limits<float>::max();

            float cursor_x = static_cast<float>(cursor_pt.x);
            float cursor_y = static_cast<float>(cursor_pt.y);

            math::vector3 local_pos = {};
            if (local_player.instance.address != 0) {
                auto hrp_it = local_player.parts.find("HumanoidRootPart");
                if (hrp_it != local_player.parts.end() && hrp_it->second.address != 0) {
                    local_pos = hrp_it->second.get_primitive().get_position();
                } else {
                    local_pos = camera_pos;
                }
            } else {
                local_pos = camera_pos;
            }

            std::string local_crew_id = local_player.crew_id;

            for (auto& player : players) {
                if (player.instance.address == 0 || player.instance.address == local_player.instance.address)
                    continue;

                if (!is_target_cheap_valid(player, local_crew_id)) continue;

                std::string chosen_bone_name = "";
                rbx::part_t target_part = get_best_bone(player, camera_pos, cursor_pt, dims, view, chosen_bone_name);
                if (!target_part.address) continue;

                math::vector3 world_pos = target_part.get_primitive().get_position();
                math::vector2 screen_pos = {};

                bool on_screen = game::visengine.world_to_screen(world_pos, screen_pos, dims, view);
                if (!on_screen && settings::aimbot::fov_check) continue;

                float cursor_dist = 0.0f;
                if (on_screen) {
                    cursor_dist = vector2_distance(screen_pos.x, screen_pos.y, cursor_x, cursor_y);
                } else {
                    cursor_dist = std::numeric_limits<float>::max();
                }

                if (settings::aimbot::fov_check && cursor_dist > settings::aimbot::fov) continue;

                if (settings::aimbot::wall_check) {
                    if (botter::is_occluded(camera_pos, world_pos)) {
                        continue;
                    }
                }

                float current_score = 0.0f;
                switch (settings::aimbot::target_selection_mode) {
                case 1: // 3D distance
                    current_score = vector3_length(world_pos - local_pos);
                    break;
                case 2: // Health-based
                    current_score = player.health;
                    break;
                case 0: // Crosshair 2D distance
                default:
                    current_score = cursor_dist;
                    break;
                }

                if (current_score < best_score) {
                    best_score = current_score;
                    best = player;
                    best_part = target_part;
                    best_part_name = chosen_bone_name;
                }
            }

            out_best_part = best_part;
            out_best_part_name = best_part_name;
            return best;
        }

        // Advanced prediction equations compensating for speed, gravity, and latency
        math::vector3 apply_prediction(rbx::primitive_t primitive, const math::vector3& camera_pos, bool is_camera) {
            math::vector3 pos = primitive.get_position();
            math::vector3 vel = primitive.get_velocity();

            if (!std::isfinite(vel.x) || !std::isfinite(vel.y) || !std::isfinite(vel.z) ||
                std::abs(vel.x) > MAX_VELOCITY || std::abs(vel.y) > MAX_VELOCITY || std::abs(vel.z) > MAX_VELOCITY)
                return pos;

            if (settings::aimbot::projectile_prediction) {
                float dist = vector3_length(pos - camera_pos);
                float speed = settings::aimbot::projectile_speed;
                if (speed < 1.0f) speed = 1000.0f;

                float travel_time = dist / speed;
                float latency_sec = settings::aimbot::latency_ms / 1000.0f;
                float total_time = travel_time + latency_sec;

                pos.x += vel.x * total_time;
                pos.y += vel.y * total_time;
                pos.z += vel.z * total_time;

                pos.y += 0.5f * settings::aimbot::projectile_gravity * travel_time * travel_time;
            } else {
                float px = is_camera ? settings::aimbot::camera_prediction_x : settings::aimbot::mouse_prediction_x;
                float py = is_camera ? settings::aimbot::camera_prediction_y : settings::aimbot::mouse_prediction_y;

                pos.x += vel.x * PREDICTION_SCALE * px;
                pos.y += vel.y * PREDICTION_SCALE * py;
                pos.z += vel.z * PREDICTION_SCALE * px;
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

        void execute_camera_aim(const math::vector3& target_pos, float dt, float screen_dist) {
            rbx::instance_t camera_inst = { memory->read<std::uint64_t>(game::workspace.address + Offsets::Workspace::CurrentCamera) };
            if (!camera_inst.address) return;

            rbx::camera_t camera{ camera_inst.address };
            math::matrix3 current_rot = camera.get_rotation();
            math::vector3 camera_pos = camera.get_position();

            math::vector3 target_forward = normalize(vector3_sub(target_pos, camera_pos));
            math::vector3 smoothed_forward = target_forward;

            if (settings::aimbot::camera_smooth || settings::aimbot::adaptive_smoothing) {
                float sx = std::clamp(settings::aimbot::camera_smooth_x, 1.0f, 200.0f);
                float sy = std::clamp(settings::aimbot::camera_smooth_y, 1.0f, 200.0f);

                if (settings::aimbot::adaptive_smoothing) {
                    float adaptive_factor = get_adaptive_smooth(screen_dist);
                    sx = adaptive_factor;
                    sy = adaptive_factor;
                }

                math::vector3 current_forward = { -current_rot.m[2], -current_rot.m[5], -current_rot.m[8] };
                current_forward = normalize(current_forward);

                float current_yaw = 0.0f, current_pitch = 0.0f;
                float target_yaw = 0.0f, target_pitch = 0.0f;

                vector_to_angles(current_forward, current_yaw, current_pitch);
                vector_to_angles(target_forward, target_yaw, target_pitch);

                float yaw_diff = target_yaw - current_yaw;
                yaw_diff = std::atan2(std::sin(yaw_diff), std::cos(yaw_diff));

                float pitch_diff = target_pitch - current_pitch;
                pitch_diff = std::atan2(std::sin(pitch_diff), std::cos(pitch_diff));

                float t_x = std::clamp(dt * (45.0f / sx), 0.0f, 1.0f);
                float t_y = std::clamp(dt * (45.0f / sy), 0.0f, 1.0f);

                float eased_t_x = apply_easing(settings::aimbot::easing_style, t_x);
                float eased_t_y = apply_easing(settings::aimbot::easing_style, t_y);

                float final_yaw = current_yaw + yaw_diff * eased_t_x;
                float final_pitch = current_pitch + pitch_diff * eased_t_y;

                smoothed_forward = angles_to_vector(final_yaw, final_pitch);
                smoothed_forward = normalize(smoothed_forward);
            }

            if (settings::aimbot::shake) {
                static float shake_time = 0.0f;
                shake_time += dt * 8.0f; // Human hand tremor frequency
                
                float factor_x = settings::aimbot::shake_x * 0.001f;
                float factor_y = settings::aimbot::shake_y * 0.001f;
                
                smoothed_forward.x += std::sin(shake_time) * factor_x;
                smoothed_forward.y += std::cos(shake_time * 1.3f) * factor_y;
                smoothed_forward.z += std::sin(shake_time * 0.7f) * factor_x;
                smoothed_forward = normalize(smoothed_forward);
            }

            math::matrix3 target_matrix = make_rotation_matrix(smoothed_forward);
            camera.write_rotation(target_matrix);
        }

        void execute_mouse_aim(const math::vector3& target_pos, const POINT& cursor_pt, float dt, const math::vector2& dims, const math::matrix4& view, float screen_dist) {
            math::vector2 screen_pos = {};
            if (!game::visengine.world_to_screen(target_pos, screen_pos, dims, view)) return;

            float target_ref_x = static_cast<float>(cursor_pt.x);
            float target_ref_y = static_cast<float>(cursor_pt.y);

            float client_x = 0.0f;
            float client_y = 0.0f;
            HWND roblox_wnd = game::wnd;
            if (roblox_wnd) {
                RECT client_rect = {};
                POINT client_pt = {};
                if (GetClientRect(roblox_wnd, &client_rect)) {
                    client_pt.x = client_rect.left;
                    client_pt.y = client_rect.top;
                    ClientToScreen(roblox_wnd, &client_pt);
                    client_x = static_cast<float>(client_pt.x);
                    client_y = static_cast<float>(client_pt.y);
                }
            }

            CURSORINFO ci = { sizeof(CURSORINFO) };
            bool cursor_hidden = false;
            if (GetCursorInfo(&ci)) {
                if (ci.flags == 0) {
                    cursor_hidden = true;
                }
            }

            float center_x = (dims.x / 2.0f) + client_x;
            float center_y = (dims.y / 2.0f) + client_y;

            if (cursor_hidden || (std::abs(target_ref_x - center_x) < 20.0f && std::abs(target_ref_y - center_y) < 20.0f)) {
                target_ref_x = center_x;
                target_ref_y = center_y;
            }

            float dx = screen_pos.x - target_ref_x;
            float dy = screen_pos.y - target_ref_y;

            float sensitivity = std::clamp(settings::aimbot::mouse_sensitivity, 0.1f, 10.0f);
            dx *= sensitivity;
            dy *= sensitivity;

            if (settings::aimbot::mouse_smooth || settings::aimbot::adaptive_smoothing) {
                float sx = std::clamp(settings::aimbot::mouse_smooth_x, 1.0f, 200.0f);
                float sy = std::clamp(settings::aimbot::mouse_smooth_y, 1.0f, 200.0f);

                if (settings::aimbot::adaptive_smoothing) {
                    float adaptive_factor = get_adaptive_smooth(screen_dist);
                    sx = adaptive_factor;
                    sy = adaptive_factor;
                }

                float t_x = std::clamp(dt * (45.0f / sx), 0.0f, 1.0f);
                float t_y = std::clamp(dt * (45.0f / sy), 0.0f, 1.0f);

                float eased_t_x = apply_easing(settings::aimbot::easing_style, t_x);
                float eased_t_y = apply_easing(settings::aimbot::easing_style, t_y);

                dx *= eased_t_x;
                dy *= eased_t_y;
            }

            if (settings::aimbot::shake) {
                static float shake_time = 0.0f;
                shake_time += dt * 8.0f;
                
                float sx = std::abs(settings::aimbot::shake_x);
                float sy = std::abs(settings::aimbot::shake_y);
                
                dx += std::sin(shake_time) * sx;
                dy += std::cos(shake_time * 1.3f) * sy;
            }

            float max_mouse_delta = 80.0f;
            dx = std::clamp(dx, -max_mouse_delta, max_mouse_delta);
            dy = std::clamp(dy, -max_mouse_delta, max_mouse_delta);

            static float accum_x = 0.0f;
            static float accum_y = 0.0f;

            if (std::isfinite(dx) && std::isfinite(dy)) {
                accum_x += dx;
                accum_y += dy;

                LONG move_x = static_cast<LONG>(accum_x);
                LONG move_y = static_cast<LONG>(accum_y);

                accum_x -= static_cast<float>(move_x);
                accum_y -= static_cast<float>(move_y);

                if (move_x != 0 || move_y != 0) {
                    INPUT input = {};
                    input.type = INPUT_MOUSE;
                    input.mi.dx = move_x;
                    input.mi.dy = move_y;
                    input.mi.dwFlags = MOUSEEVENTF_MOVE;
                    SendInput(1, &input, sizeof(INPUT));
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
            if (!settings::aimbot::enabled ||
                (settings::aimbot::aimbot_type < 0 || settings::aimbot::aimbot_type > 1) ||
                check::textchatopen || !game::workspace.address) {
                
                {
                    std::lock_guard<std::mutex> lock(mtx);
                    locked_target = cache::entity_t{};
                    has_locked_target = false;
                }
                needs_key_release = false;
                was_disabled_by_typing = check::textchatopen;
                target_pos_initialized = false; 
                virtual_angles_initialized = false; 
                spring_vel_yaw = 0.0f;
                spring_vel_pitch = 0.0f;
                spring_vel_mouse_x = 0.0f;
                spring_vel_mouse_y = 0.0f;
                locked_part_name = "";
                update_target_offset(0);
                last_tick = std::chrono::high_resolution_clock::now();
                Sleep(100);
                continue;
            }

            if (was_disabled_by_typing && !check::textchatopen) {
                was_disabled_by_typing = false;
            }

            if (!get_key_state()) {
                {
                    std::lock_guard<std::mutex> lock(mtx);
                    locked_target = cache::entity_t{};
                    has_locked_target = false;
                }
                needs_key_release = false;
                target_pos_initialized = false; 
                virtual_angles_initialized = false; 
                spring_vel_yaw = 0.0f;
                spring_vel_pitch = 0.0f;
                spring_vel_mouse_x = 0.0f;
                spring_vel_mouse_y = 0.0f;
                locked_part_name = "";
                update_target_offset(0);
                last_tick = std::chrono::high_resolution_clock::now();
                Sleep(30);
                continue;
            }

            if (needs_key_release) {
                last_tick = std::chrono::high_resolution_clock::now();
                Sleep(10);
                continue;
            }

            if (!GetCursorPos(&cursor_pt)) {
                Sleep(1);
                continue;
            }
            HWND roblox_wnd = game::wnd;
            if (!roblox_wnd) {
                roblox_wnd = FindWindowA(nullptr, "Roblox");
                if (roblox_wnd) game::wnd = roblox_wnd;
            }
            if (!roblox_wnd) {
                Sleep(10);
                continue;
            }

            // Fetch visual engine parameters once per frame
            math::vector2 dims = game::visengine.get_dimensions();
            math::matrix4 view = game::visengine.get_viewmatrix();

            // 1. Safe, short-lock thread cache snapshot copying
            std::vector<cache::entity_t> players_snapshot;
            cache::entity_t local_player_snapshot = {};
            {
                std::lock_guard<std::mutex> cache_lock(cache::mtx);
                if (cache::cached_players) {
                    players_snapshot = *cache::cached_players;
                }
                local_player_snapshot = cache::cached_local_player;
            }

            // Get camera position
            math::vector3 camera_pos = {};
            rbx::instance_t camera_inst = { memory->read<std::uint64_t>(game::workspace.address + Offsets::Workspace::CurrentCamera) };
            if (camera_inst.address != 0) {
                rbx::camera_t camera{ camera_inst.address };
                camera_pos = camera.get_position();
            }

            // 2. Validate current locked target from our local snapshot
            bool target_alive = false;
            {
                std::lock_guard<std::mutex> lock(mtx);
                if (has_locked_target && locked_target.instance.address != 0) {
                    for (auto& player : players_snapshot) {
                        if (player.instance.address == locked_target.instance.address) {
                            locked_target = player;
                            target_alive = true;
                            break;
                        }
                    }
                    if (!target_alive) {
                        locked_target = cache::entity_t{};
                        has_locked_target = false;
                        target_pos_initialized = false;
                        locked_part_name = "";
                        update_target_offset(0);
                    }
                }
            }

            cache::entity_t target = {};
            rbx::part_t target_part = {};
            std::string local_crew_id = local_player_snapshot.crew_id;

            // 3. Process sticky lock target evaluation
            {
                std::lock_guard<std::mutex> lock(mtx);
                if (has_locked_target && locked_target.instance.address != 0) {
                    bool skip_fov = (settings::aimbot::sticky_aim);
                    if (is_target_valid(locked_target, local_crew_id, cursor_pt, dims, view, camera_pos, skip_fov)) {
                        target = locked_target;
                        target_part = get_best_bone(target, camera_pos, cursor_pt, dims, view, locked_part_name);
                    } else {
                        locked_target = cache::entity_t{};
                        has_locked_target = false;
                        target_pos_initialized = false;
                        locked_part_name = "";
                        update_target_offset(0);
                        if (settings::aimbot::sticky_aim) {
                            needs_key_release = true;
                        }
                    }
                }
            }

            // 4. Find new best target if no locked target exists
            bool newly_locked = false;
            if (!has_locked_target) {
                std::string chosen_part_name = "";
                rbx::part_t chosen_part = {};
                target = find_best_target(
                    players_snapshot,
                    local_player_snapshot,
                    cursor_pt,
                    dims,
                    view,
                    camera_pos,
                    chosen_part,
                    chosen_part_name
                );

                if (target.instance.address != 0) {
                    std::lock_guard<std::mutex> lock(mtx);
                    locked_target = target;
                    has_locked_target = true;
                    target_pos_initialized = false;
                    locked_part_name = chosen_part_name;
                    target_part = chosen_part;
                    newly_locked = true;
                }
            }

            // Generate or load target offset (outside mutex to prevent lockups)
            if (target.instance.address != 0) {
                update_target_offset(target.instance.address);
            }

            if (target.instance.address == 0 || !target_part.address) {
                Sleep(10);
                continue;
            }

            // 5. Get position and apply humanized bone offset
            rbx::primitive_t primitive = target_part.get_primitive();
            math::vector3 target_pos = primitive.get_position();
            
            target_pos.x += current_target_offset.x;
            target_pos.y += current_target_offset.y;
            target_pos.z += current_target_offset.z;

            // 6. Compute prediction
            bool use_prediction = (settings::aimbot::aimbot_type == 0 && settings::aimbot::camera_prediction) ||
                                  (settings::aimbot::aimbot_type == 1 && settings::aimbot::mouse_prediction) ||
                                  (settings::aimbot::projectile_prediction);
            if (use_prediction) {
                target_pos = apply_prediction(primitive, camera_pos, settings::aimbot::aimbot_type == 0);
            }

            // 7. Check if screen position of target pos is valid
            math::vector2 screen_pos = {};
            float cursor_dist = 0.0f;
            if (game::visengine.world_to_screen(target_pos, screen_pos, dims, view)) {
                cursor_dist = vector2_distance(screen_pos.x, screen_pos.y, (float)cursor_pt.x, (float)cursor_pt.y);
            }

            // Check if view matrix or target position actually changed
            bool view_changed = false;
            for (int i = 0; i < 16; ++i) {
                if (view.m[i] != last_view.m[i]) {
                    view_changed = true;
                    break;
                }
            }

            bool target_changed = (target_pos.x != last_target_pos.x || 
                                   target_pos.y != last_target_pos.y || 
                                   target_pos.z != last_target_pos.z);

            if (target_pos_initialized && !view_changed && !target_changed) {
                Sleep(1);
                continue;
            }

            last_view = view;
            last_target_pos = target_pos;

            auto now = std::chrono::high_resolution_clock::now();
            float dt = std::chrono::duration<float>(now - last_tick).count();
            last_tick = now;

            if (dt > 0.1f) dt = 0.016f;

            bool is_smooth_enabled = (settings::aimbot::aimbot_type == 0) ? settings::aimbot::camera_smooth : settings::aimbot::mouse_smooth;
            if (!is_smooth_enabled && !settings::aimbot::adaptive_smoothing) {
                filtered_target_pos = target_pos;
                target_pos_initialized = true;
            } else {
                if (!target_pos_initialized) {
                    filtered_target_pos = target_pos;
                    target_pos_initialized = true;
                } else {
                    float target_ema_factor = 1.0f - std::exp(-12.0f * dt);
                    target_ema_factor = std::clamp(target_ema_factor, 0.0f, 1.0f);
                    filtered_target_pos.x += (target_pos.x - filtered_target_pos.x) * target_ema_factor;
                    filtered_target_pos.y += (target_pos.y - filtered_target_pos.y) * target_ema_factor;
                    filtered_target_pos.z += (target_pos.z - filtered_target_pos.z) * target_ema_factor;
                }
            }

            if (settings::aimbot::aimbot_type == 0) {
                execute_camera_aim(filtered_target_pos, dt, cursor_dist);
            }
            else {
                execute_mouse_aim(filtered_target_pos, cursor_pt, dt, dims, view, cursor_dist);
            }
            Sleep(1);
        }
    }

    void initialize() {
        std::lock_guard<std::mutex> lock(mtx);
        locked_target = cache::entity_t{};
        has_locked_target = false;
        key_was_pressed = false;
        toggle_state = false;
        was_disabled_by_typing = false;
        needs_key_release = false;
        target_pos_initialized = false;
        locked_part_name = "";
        update_target_offset(0);
    }
}