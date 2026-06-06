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
#include <unordered_map>

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
    bool g_aimbot_manual_locked = false;
    cache::entity_t g_aimbot_manual_target = {};

    namespace {
        constexpr float M_PI_F = 3.14159265f;
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

        // Sub-pixel DDA mouse accumulator
        float accum_x = 0.0f;
        float accum_y = 0.0f;



        // Persistent bone random offset (per-lock)
        math::vector3 current_bone_offset = { 0.0f, 0.0f, 0.0f };

        float apply_easing(int style, float t) {
            if (style <= 0) return 1.0f;
            if (t >= 1.0f) return 1.0f;
            if (t <= 0.0f) return 0.0f;

            switch (style) {
            case 1: // Linear
                return t;
            case 2: // Sine In
                return 1.0f - std::cos(t * M_PI_F * 0.5f);
            case 3: // Sine Out
                return std::sin(t * M_PI_F * 0.5f);
            case 4: // Sine InOut
                return -(std::cos(M_PI_F * t) - 1.0f) * 0.5f;
            case 5: // Quad In
                return t * t;
            case 6: // Quad Out
                return 1.0f - (1.0f - t) * (1.0f - t);
            case 7: // Quad InOut
                return t < 0.5f ? 2.0f * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) * 0.5f;
            case 8: // Cubic In
                return t * t * t;
            case 9: // Cubic Out
                return 1.0f - std::pow(1.0f - t, 3.0f);
            case 10: // Cubic InOut
                return t < 0.5f ? 4.0f * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) * 0.5f;
            case 11: // Elastic Out
            {
                float c4 = (2.0f * M_PI_F) / 3.0f;
                return std::pow(2.0f, -10.0f * t) * std::sin((t * 10.0f - 10.75f) * c4) + 1.0f;
            }
            case 12: // Bounce Out
            {
                float n1 = 7.5625f;
                float d1 = 2.75f;
                if (t < 1.0f / d1) {
                    return n1 * t * t;
                } else if (t < 2.0f / d1) {
                    float t2 = t - 1.5f / d1;
                    return n1 * t2 * t2 + 0.75f;
                } else if (t < 2.5f / d1) {
                    float t2 = t - 2.25f / d1;
                    return n1 * t2 * t2 + 0.9375f;
                } else {
                    float t2 = t - 2.625f / d1;
                    return n1 * t2 * t2 + 0.984375f;
                }
            }
            default:
                return 1.0f;
            }
        }

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

        math::vector3 get_closest_point_on_part(rbx::part_t part, const math::vector3& camera_pos, const math::vector3& ray_dir) {
            rbx::primitive_t primitive = part.get_primitive();
            if (primitive.address == 0) return { 0, 0, 0 };

            math::vector3 center = primitive.get_position();
            math::matrix3 rot = primitive.get_rotation();
            math::vector3 size = primitive.get_size();

            if (!std::isfinite(size.x) || !std::isfinite(size.y) || !std::isfinite(size.z) ||
                size.x < 0.0f || size.y < 0.0f || size.z < 0.0f) {
                return center;
            }

            // Transform camera position to local space of the part
            math::vector3 local_cam = {
                rot.m[0] * (camera_pos.x - center.x) + rot.m[3] * (camera_pos.y - center.y) + rot.m[6] * (camera_pos.z - center.z),
                rot.m[1] * (camera_pos.x - center.x) + rot.m[4] * (camera_pos.y - center.y) + rot.m[7] * (camera_pos.z - center.z),
                rot.m[2] * (camera_pos.x - center.x) + rot.m[5] * (camera_pos.y - center.y) + rot.m[8] * (camera_pos.z - center.z)
            };

            // Transform ray direction to local space
            math::vector3 local_ray_dir = {
                rot.m[0] * ray_dir.x + rot.m[3] * ray_dir.y + rot.m[6] * ray_dir.z,
                rot.m[1] * ray_dir.x + rot.m[4] * ray_dir.y + rot.m[7] * ray_dir.z,
                rot.m[2] * ray_dir.x + rot.m[5] * ray_dir.y + rot.m[8] * ray_dir.z
            };

            float h[3] = { size.x * 0.5f, size.y * 0.5f, size.z * 0.5f };
            float C[3] = { local_cam.x, local_cam.y, local_cam.z };
            float D[3] = { local_ray_dir.x, local_ray_dir.y, local_ray_dir.z };

            float t_values[7];
            int t_count = 0;
            t_values[t_count++] = 0.0f;

            for (int i = 0; i < 3; ++i) {
                if (std::abs(D[i]) > 1e-6f) {
                    float t1 = (-h[i] - C[i]) / D[i];
                    float t2 = (h[i] - C[i]) / D[i];
                    if (t1 > 0.0f) t_values[t_count++] = t1;
                    if (t2 > 0.0f) t_values[t_count++] = t2;
                }
            }

            for (int i = 1; i < t_count; ++i) {
                float key = t_values[i];
                int j = i - 1;
                while (j >= 0 && t_values[j] > key) {
                    t_values[j + 1] = t_values[j];
                    j = j - 1;
                }
                t_values[j + 1] = key;
            }

            float best_t = 0.0f;
            float min_dist_sq = -1.0f;

            auto evaluate_t = [&](float t) {
                if (t < 0.0f) t = 0.0f;
                float px = C[0] + t * D[0];
                float py = C[1] + t * D[1];
                float pz = C[2] + t * D[2];

                float bx = std::clamp(px, -h[0], h[0]);
                float by = std::clamp(py, -h[1], h[1]);
                float bz = std::clamp(pz, -h[2], h[2]);

                float dx = px - bx;
                float dy = py - by;
                float dz = pz - bz;
                float dist_sq = dx * dx + dy * dy + dz * dz;

                if (min_dist_sq < 0.0f || dist_sq < min_dist_sq) {
                    min_dist_sq = dist_sq;
                    best_t = t;
                }
            };

            for (int i = 0; i < t_count; ++i) {
                evaluate_t(t_values[i]);
            }

            for (int j = 0; j < t_count; ++j) {
                float t_start = t_values[j];
                float t_end = (j < t_count - 1) ? t_values[j + 1] : (t_values[j] + 1e5f);
                float t_mid = t_start + 0.5f * (t_end - t_start);

                float a = 0.0f;
                float b = 0.0f;
                for (int i = 0; i < 3; ++i) {
                    float p_mid = C[i] + t_mid * D[i];
                    if (p_mid < -h[i]) {
                        a += D[i] * D[i];
                        b += D[i] * (C[i] + h[i]);
                    } else if (p_mid > h[i]) {
                        a += D[i] * D[i];
                        b += D[i] * (C[i] - h[i]);
                    }
                }

                if (a > 1e-6f) {
                    float t_star = -b / a;
                    if (t_star >= t_start && t_star <= t_end) {
                        evaluate_t(t_star);
                    }
                }
            }

            float best_px = C[0] + best_t * D[0];
            float best_py = C[1] + best_t * D[1];
            float best_pz = C[2] + best_t * D[2];

            math::vector3 closest_local = {
                std::clamp(best_px, -h[0], h[0]),
                std::clamp(best_py, -h[1], h[1]),
                std::clamp(best_pz, -h[2], h[2])
            };

            math::vector3 closest_world = {
                center.x + (rot.m[0] * closest_local.x + rot.m[1] * closest_local.y + rot.m[2] * closest_local.z),
                center.y + (rot.m[3] * closest_local.x + rot.m[4] * closest_local.y + rot.m[5] * closest_local.z),
                center.z + (rot.m[6] * closest_local.x + rot.m[7] * closest_local.y + rot.m[8] * closest_local.z)
            };

            return closest_world;
        }

        math::vector3 get_camera_ray_dir(const POINT& cursor_pt, const math::vector2& dims, const math::matrix3& cam_rot, float rad_fov) {
            float half_w = dims.x * 0.5f;
            float half_h = dims.y * 0.5f;

            POINT client_pt = cursor_pt;
            HWND roblox_window = game::wnd;
            if (roblox_window) {
                ScreenToClient(roblox_window, &client_pt);
            }

            float cx = 0.0f;
            float cy = 0.0f;
            if (half_h > 0.001f) {
                cx = (static_cast<float>(client_pt.x) - half_w) / half_h * std::tan(rad_fov * 0.5f);
                cy = -(static_cast<float>(client_pt.y) - half_h) / half_h * std::tan(rad_fov * 0.5f);
            }

            math::vector3 dir_cam = { cx, cy, -1.0f };
            math::vector3 dir_world = cam_rot * dir_cam;
            return normalize(dir_world);
        }

        bool is_knocked(const cache::entity_t& player) {
            return player.is_knocked;
        }

        bool is_on_same_team(const cache::entity_t& player, const std::string& local_crew_id) {
            if (local_crew_id.empty() || player.crew_id.empty()) return false;
            if (local_crew_id == "0" || player.crew_id == "0") return false;
            return local_crew_id == player.crew_id;
        }

        struct occlusion_cache_entry_t {
            bool occluded;
            std::chrono::steady_clock::time_point last_check_time;
        };

        bool is_bone_occluded_cached(std::uint64_t entity_address, const std::string& bone_name, const math::vector3& camera_pos, const math::vector3& bone_pos) {
            static std::unordered_map<std::uint64_t, std::unordered_map<std::string, occlusion_cache_entry_t>> occlusion_cache;
            auto now = std::chrono::steady_clock::now();

            if (occlusion_cache.size() > 200) {
                occlusion_cache.clear();
            }

            auto& entity_map = occlusion_cache[entity_address];
            auto it = entity_map.find(bone_name);
            if (it != entity_map.end()) {
                if (std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second.last_check_time).count() < 50) {
                    return it->second.occluded;
                }
            }

            bool occluded = botter::is_occluded(camera_pos, bone_pos);
            entity_map[bone_name] = { occluded, now };
            return occluded;
        }

        rbx::part_t get_closest_part(cache::entity_t& player, const POINT& cursor_pt, const math::vector2& dims, const math::matrix4& view, const math::vector3& camera_pos, const math::vector3& ray_dir, bool check_visibility = false) {
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
                math::vector3 closest_pt = get_closest_point_on_part(part, camera_pos, ray_dir);

                if (check_visibility && is_bone_occluded_cached(player.instance.address, pair.first, camera_pos, closest_pt)) {
                    continue;
                }

                math::vector2 screen_pos = {};
                if (!game::visengine.world_to_screen(closest_pt, screen_pos, dims, view)) continue;
                float dist = vector2_distance(screen_pos.x, screen_pos.y, cursor_x, cursor_y);
                if (dist < min_dist) {
                    min_dist = dist;
                    closest = part;
                }
            }
            return closest;
        }

        rbx::part_t get_best_bone_no_occlusion(cache::entity_t& player, const POINT& cursor_pt, const math::vector2& dims, const math::matrix4& view, const math::vector3& camera_pos, const math::vector3& ray_dir, std::string& chosen_bone_name) {
            if (player.parts.empty()) return rbx::part_t{};

            int aim_part = settings::aimbot::aimpart;
            chosen_bone_name = "";
            rbx::part_t target_part = {};

            switch (aim_part) {
            case 0:
                if (auto it = player.parts.find("Head"); it != player.parts.end()) { chosen_bone_name = "Head"; target_part = it->second; }
                break;
            case 1:
                if (auto it = player.parts.find("UpperTorso"); it != player.parts.end()) { chosen_bone_name = "UpperTorso"; target_part = it->second; }
                if (target_part.address == 0) {
                    if (auto it = player.parts.find("Torso"); it != player.parts.end()) { chosen_bone_name = "Torso"; target_part = it->second; }
                }
                break;
            case 2:
                if (auto it = player.parts.find("Torso"); it != player.parts.end()) { chosen_bone_name = "Torso"; target_part = it->second; }
                if (target_part.address == 0) {
                    if (auto it = player.parts.find("UpperTorso"); it != player.parts.end()) { chosen_bone_name = "UpperTorso"; target_part = it->second; }
                }
                break;
            case 3:
                if (auto it = player.parts.find("LowerTorso"); it != player.parts.end()) { chosen_bone_name = "LowerTorso"; target_part = it->second; }
                if (target_part.address == 0) {
                    if (auto it = player.parts.find("Torso"); it != player.parts.end()) { chosen_bone_name = "Torso"; target_part = it->second; }
                }
                break;
            case 4:
                if (auto it = player.parts.find("HumanoidRootPart"); it != player.parts.end()) { chosen_bone_name = "HumanoidRootPart"; target_part = it->second; }
                break;
            case 5:
                if (auto it = player.parts.find("LeftUpperArm"); it != player.parts.end()) { chosen_bone_name = "LeftUpperArm"; target_part = it->second; }
                if (target_part.address == 0) {
                    if (auto it = player.parts.find("Left Arm"); it != player.parts.end()) { chosen_bone_name = "Left Arm"; target_part = it->second; }
                }
                break;
            case 6:
                if (auto it = player.parts.find("RightUpperArm"); it != player.parts.end()) { chosen_bone_name = "RightUpperArm"; target_part = it->second; }
                if (target_part.address == 0) {
                    if (auto it = player.parts.find("Right Arm"); it != player.parts.end()) { chosen_bone_name = "Right Arm"; target_part = it->second; }
                }
                break;
            case 7:
                if (auto it = player.parts.find("LeftUpperLeg"); it != player.parts.end()) { chosen_bone_name = "LeftUpperLeg"; target_part = it->second; }
                if (target_part.address == 0) {
                    if (auto it = player.parts.find("Left Leg"); it != player.parts.end()) { chosen_bone_name = "Left Leg"; target_part = it->second; }
                }
                break;
            case 8:
                if (auto it = player.parts.find("RightUpperLeg"); it != player.parts.end()) { chosen_bone_name = "RightUpperLeg"; target_part = it->second; }
                if (target_part.address == 0) {
                    if (auto it = player.parts.find("Right Leg"); it != player.parts.end()) { chosen_bone_name = "Right Leg"; target_part = it->second; }
                }
                break;
            case 9:
                {
                    rbx::part_t closest = {};
                    if (settings::aimbot::wall_check) {
                        closest = get_closest_part(player, cursor_pt, dims, view, camera_pos, ray_dir, true);
                    }
                    if (closest.address == 0) {
                        closest = get_closest_part(player, cursor_pt, dims, view, camera_pos, ray_dir, false);
                    }
                    if (closest.address != 0) {
                        chosen_bone_name = closest.get_name();
                        target_part = closest;
                    }
                }
                break;
            }

            if (target_part.address != 0) {
                return target_part;
            }
            if (auto it = player.parts.find("HumanoidRootPart"); it != player.parts.end()) { chosen_bone_name = "HumanoidRootPart"; return it->second; }
            return rbx::part_t{};
        }

        rbx::part_t get_best_bone(cache::entity_t& player, const math::vector3& camera_pos, const POINT& cursor_pt, const math::vector2& dims, const math::matrix4& view, const math::vector3& ray_dir, std::string& chosen_bone_name) {
            rbx::part_t primary_part = get_best_bone_no_occlusion(player, cursor_pt, dims, view, camera_pos, ray_dir, chosen_bone_name);
            if (primary_part.address == 0) return rbx::part_t{};

            if (settings::aimbot::wall_check) {
                math::vector3 pos = get_closest_point_on_part(primary_part, camera_pos, ray_dir);
                if (is_bone_occluded_cached(player.instance.address, chosen_bone_name, camera_pos, pos)) {
                    if (settings::aimbot::smart_bone) {
                        const std::vector<std::string> bone_priority = {
                            "Head", "UpperTorso", "Torso", "LowerTorso", "HumanoidRootPart",
                            "LeftUpperArm", "Left Arm", "RightUpperArm", "Right Arm",
                            "LeftUpperLeg", "Left Leg", "RightUpperLeg", "Right Leg"
                        };
                        for (const auto& bone_name : bone_priority) {
                            if (bone_name == chosen_bone_name) continue;
                            auto it = player.parts.find(bone_name);
                            if (it != player.parts.end() && it->second.address != 0) {
                                math::vector3 bone_pos = get_closest_point_on_part(it->second, camera_pos, ray_dir);
                                if (!is_bone_occluded_cached(player.instance.address, bone_name, camera_pos, bone_pos)) {
                                    chosen_bone_name = bone_name;
                                    return it->second;
                                }
                            }
                        }
                    }
                }
            }
            return primary_part;
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

        bool is_target_valid(cache::entity_t& player, const std::string& local_crew_id, const POINT& cursor_pt, const math::vector2& dims, const math::matrix4& view, const math::vector3& camera_pos, const math::vector3& ray_dir, bool skip_fov_check = false, bool skip_wall_check = false) {
            if (!is_target_cheap_valid(player, local_crew_id)) return false;

            std::string temp_name = "";
            rbx::part_t target_part = get_best_bone_no_occlusion(player, cursor_pt, dims, view, camera_pos, ray_dir, temp_name);
            if (!target_part.address) return false;

            math::vector3 world_pos = get_closest_point_on_part(target_part, camera_pos, ray_dir);

            if (settings::aimbot::fov_check && !skip_fov_check) {
                math::vector2 screen_pos = {};
                if (!game::visengine.world_to_screen(world_pos, screen_pos, dims, view)) return false;

                float cursor_x = static_cast<float>(cursor_pt.x);
                float cursor_y = static_cast<float>(cursor_pt.y);
                float dist = vector2_distance(screen_pos.x, screen_pos.y, cursor_x, cursor_y);
                if (dist > settings::aimbot::fov) return false;
            }

            if (settings::aimbot::wall_check && !skip_wall_check) {
                if (is_bone_occluded_cached(player.instance.address, temp_name, camera_pos, world_pos)) {
                    if (settings::aimbot::smart_bone) {
                        const std::vector<std::string> bone_priority = {
                            "Head", "UpperTorso", "Torso", "LowerTorso", "HumanoidRootPart",
                            "LeftUpperArm", "Left Arm", "RightUpperArm", "Right Arm",
                            "LeftUpperLeg", "Left Leg", "RightUpperLeg", "Right Leg"
                        };
                        bool found_visible = false;
                        for (const auto& bone_name : bone_priority) {
                            if (bone_name == temp_name) continue;
                            auto it = player.parts.find(bone_name);
                            if (it != player.parts.end() && it->second.address != 0) {
                                math::vector3 pos = get_closest_point_on_part(it->second, camera_pos, ray_dir);
                                if (!is_bone_occluded_cached(player.instance.address, bone_name, camera_pos, pos)) {
                                    found_visible = true;
                                    break;
                                }
                            }
                        }
                        if (!found_visible) return false;
                    } else {
                        return false;
                    }
                }
            }

            return true;
        }

        cache::entity_t find_best_target(
            std::vector<cache::entity_t>& players,
            cache::entity_t& local_player,
            const POINT& cursor_pt,
            const math::vector2& dims,
            const math::matrix4& view,
            const math::vector3& camera_pos,
            const math::vector3& ray_dir,
            rbx::part_t& out_best_part,
            std::string& out_best_part_name
        ) {
            struct target_candidate_t {
                cache::entity_t player;
                rbx::part_t part;
                std::string part_name;
                float score;
                math::vector3 world_pos;
            };
            std::vector<target_candidate_t> candidates;

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
                rbx::part_t target_part = get_best_bone_no_occlusion(player, cursor_pt, dims, view, camera_pos, ray_dir, chosen_bone_name);
                if (!target_part.address) continue;

                math::vector3 world_pos = get_closest_point_on_part(target_part, camera_pos, ray_dir);
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

                candidates.push_back({ player, target_part, chosen_bone_name, current_score, world_pos });
            }

            if (candidates.empty()) {
                out_best_part = rbx::part_t{};
                out_best_part_name = "";
                return cache::entity_t{};
            }

            std::sort(candidates.begin(), candidates.end(), [](const target_candidate_t& a, const target_candidate_t& b) {
                return a.score < b.score;
            });

            for (const auto& candidate : candidates) {
                std::string actual_bone_name = candidate.part_name;
                rbx::part_t actual_part = candidate.part;
                math::vector3 actual_world_pos = candidate.world_pos;

                if (settings::aimbot::wall_check) {
                    if (is_bone_occluded_cached(candidate.player.instance.address, actual_bone_name, camera_pos, actual_world_pos)) {
                        if (settings::aimbot::smart_bone) {
                            const std::vector<std::string> bone_priority = {
                                "Head", "UpperTorso", "Torso", "LowerTorso", "HumanoidRootPart",
                                "LeftUpperArm", "Left Arm", "RightUpperArm", "Right Arm",
                                "LeftUpperLeg", "Left Leg", "RightUpperLeg", "Right Leg"
                            };
                            bool found_visible = false;
                            for (const auto& bone_name : bone_priority) {
                                if (bone_name == candidate.part_name) continue;
                                auto it = candidate.player.parts.find(bone_name);
                                if (it != candidate.player.parts.end() && it->second.address != 0) {
                                    rbx::part_t temp_part = it->second;
                                    math::vector3 pos = get_closest_point_on_part(temp_part, camera_pos, ray_dir);
                                    if (!is_bone_occluded_cached(candidate.player.instance.address, bone_name, camera_pos, pos)) {
                                        actual_bone_name = bone_name;
                                        actual_part = temp_part;
                                        actual_world_pos = pos;
                                        found_visible = true;
                                        break;
                                    }
                                }
                            }
                            if (!found_visible) {
                                continue;
                            }
                        } else {
                            continue;
                        }
                    }
                }
                out_best_part = actual_part;
                out_best_part_name = actual_bone_name;
                return candidate.player;
            }

            out_best_part = rbx::part_t{};
            out_best_part_name = "";
            return cache::entity_t{};
        }

        math::vector3 apply_prediction(std::uint64_t entity_address, rbx::primitive_t primitive, const math::vector3& target_pos, const math::vector3& camera_pos, bool is_camera) {
            math::vector3 pos = target_pos;
            math::vector3 vel = primitive.get_velocity();

            if (!std::isfinite(vel.x) || !std::isfinite(vel.y) || !std::isfinite(vel.z) ||
                std::abs(vel.x) > MAX_VELOCITY || std::abs(vel.y) > MAX_VELOCITY || std::abs(vel.z) > MAX_VELOCITY)
                return pos;

            static std::unordered_map<std::uint64_t, math::vector3> smoothed_velocities;
            static std::unordered_map<std::uint64_t, std::chrono::steady_clock::time_point> last_prediction_times;

            auto now = std::chrono::steady_clock::now();

            if (smoothed_velocities.size() > 200) {
                smoothed_velocities.clear();
                last_prediction_times.clear();
            }

            math::vector3& smooth_vel = smoothed_velocities[entity_address];
            auto last_time_it = last_prediction_times.find(entity_address);

            if (last_time_it == last_prediction_times.end()) {
                smooth_vel = vel;
            } else {
                float dt = std::chrono::duration<float>(now - last_time_it->second).count();
                if (dt > 0.1f) dt = 0.016f;

                float alpha = 1.0f - std::exp(-10.0f * dt);
                alpha = std::clamp(alpha, 0.0f, 1.0f);
                smooth_vel.x += (vel.x - smooth_vel.x) * alpha;
                smooth_vel.y += (vel.y - smooth_vel.y) * alpha;
                smooth_vel.z += (vel.z - smooth_vel.z) * alpha;
            }

            last_prediction_times[entity_address] = now;

            if (settings::aimbot::projectile_prediction) {
                float dist = vector3_length(pos - camera_pos);
                float speed = settings::aimbot::projectile_speed;
                if (speed < 1.0f) speed = 1000.0f;

                float travel_time = dist / speed;
                float latency_sec = settings::aimbot::latency_ms / 1000.0f;
                float total_time = travel_time + latency_sec;

                pos.x += smooth_vel.x * total_time;
                pos.y += smooth_vel.y * total_time;
                pos.z += smooth_vel.z * total_time;

                pos.y += 0.5f * settings::aimbot::projectile_gravity * travel_time * travel_time;
            } else {
                float px = is_camera ? settings::aimbot::camera_prediction_x : settings::aimbot::mouse_prediction_x;
                float py = is_camera ? settings::aimbot::camera_prediction_y : settings::aimbot::mouse_prediction_y;

                pos.x += smooth_vel.x * PREDICTION_SCALE * px;
                pos.y += smooth_vel.y * PREDICTION_SCALE * py;
                pos.z += smooth_vel.z * PREDICTION_SCALE * px;
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

        void execute_camera_aim(std::uint64_t target_address, const math::vector3& target_pos, float dt, bool reset_state, float ease_factor) {
            rbx::instance_t camera_inst = { memory->read<std::uint64_t>(game::workspace.address + Offsets::Workspace::CurrentCamera) };
            if (!camera_inst.address) return;

            rbx::camera_t camera{ camera_inst.address };
            math::matrix3 current_rot = camera.get_rotation();
            math::vector3 camera_pos = camera.get_position();

            math::vector3 target_forward = normalize(vector3_sub(target_pos, camera_pos));
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

            float final_yaw = target_yaw;
            float final_pitch = target_pitch;

            if (settings::aimbot::camera_smooth) {
                float sx = std::clamp(settings::aimbot::camera_smooth_x, 1.0f, 200.0f);
                float sy = std::clamp(settings::aimbot::camera_smooth_y, 1.0f, 200.0f);

                if (settings::aimbot::adaptive_smoothing) {
                    float total_diff = std::sqrt(yaw_diff * yaw_diff + pitch_diff * pitch_diff);
                    float max_expected_diff = 0.5f; // approx 28 degrees
                    float ratio = std::clamp(total_diff / max_expected_diff, 0.0f, 1.0f);
                    sx = settings::aimbot::adaptive_smooth_min + ratio * (settings::aimbot::adaptive_smooth_max - settings::aimbot::adaptive_smooth_min);
                    sy = sx;
                }

                // Smooth exponential dampening factor
                float factor_x = 1.0f - std::exp(-(1.5f / sx) * 60.0f * dt);
                float factor_y = 1.0f - std::exp(-(1.5f / sy) * 60.0f * dt);

                factor_x = std::clamp(factor_x, 0.0f, 1.0f);
                factor_y = std::clamp(factor_y, 0.0f, 1.0f);

                final_yaw = current_yaw + yaw_diff * factor_x;
                final_pitch = current_pitch + pitch_diff * factor_y;
            }

            final_yaw = std::atan2(std::sin(final_yaw), std::cos(final_yaw));
            final_pitch = std::clamp(final_pitch, -1.56f, 1.56f);

            math::vector3 smoothed_forward = angles_to_vector(final_yaw, final_pitch);
            smoothed_forward = normalize(smoothed_forward);

            math::matrix3 target_matrix = make_rotation_matrix(smoothed_forward);
            camera.write_rotation(target_matrix);
        }

        void execute_mouse_aim(std::uint64_t target_address, const math::vector3& target_pos, const POINT& cursor_pt, float dt, const math::vector2& dims, const math::matrix4& view, bool reset_state, float ease_factor) {
            if (reset_state) {
                accum_x = 0.0f;
                accum_y = 0.0f;
            }

            HWND roblox_wnd = game::wnd;
            float client_x = 0.0f;
            float client_y = 0.0f;
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

            bool right_click_held = (GetAsyncKeyState(VK_RBUTTON) & 0x8000);
            float target_ref_x = static_cast<float>(cursor_pt.x);
            float target_ref_y = static_cast<float>(cursor_pt.y);

            static bool session_captured = false;
            static auto last_near_center_time = std::chrono::steady_clock::now();
            auto current_time = std::chrono::steady_clock::now();

            bool near_center = (std::abs(target_ref_x - center_x) < 10.0f && std::abs(target_ref_y - center_y) < 10.0f);
            if (near_center || cursor_hidden || right_click_held) {
                session_captured = true;
                last_near_center_time = current_time;
            } else if (!cursor_hidden && !right_click_held) {
                if (std::chrono::duration_cast<std::chrono::milliseconds>(current_time - last_near_center_time).count() > 150) {
                    session_captured = false;
                }
            }

            if (session_captured) {
                target_ref_x = center_x;
                target_ref_y = center_y;
            }

            float dx = 0.0f;
            float dy = 0.0f;
            float sensitivity = std::clamp(settings::aimbot::mouse_sensitivity, 0.1f, 10.0f);

            if (session_captured) {
                rbx::instance_t camera_inst = { memory->read<std::uint64_t>(game::workspace.address + Offsets::Workspace::CurrentCamera) };
                if (camera_inst.address == 0) return;

                rbx::camera_t camera{ camera_inst.address };
                math::matrix3 current_rot = camera.get_rotation();
                math::vector3 camera_pos = camera.get_position();

                math::vector3 camera_forward = { -current_rot.m[2], -current_rot.m[5], -current_rot.m[8] };
                camera_forward = normalize(camera_forward);

                float actual_yaw = 0.0f;
                float actual_pitch = 0.0f;
                vector_to_angles(camera_forward, actual_yaw, actual_pitch);

                math::vector3 target_forward = normalize(vector3_sub(target_pos, camera_pos));
                float target_yaw = 0.0f;
                float target_pitch = 0.0f;
                vector_to_angles(target_forward, target_yaw, target_pitch);

                float err_yaw = target_yaw - actual_yaw;
                err_yaw = std::atan2(std::sin(err_yaw), std::cos(err_yaw));

                float err_pitch = target_pitch - actual_pitch;
                err_pitch = std::atan2(std::sin(err_pitch), std::cos(err_pitch));

                if (settings::aimbot::mouse_smooth) {
                    float sx = std::clamp(settings::aimbot::mouse_smooth_x, 1.0f, 200.0f);
                    float sy = std::clamp(settings::aimbot::mouse_smooth_y, 1.0f, 200.0f);

                    if (settings::aimbot::adaptive_smoothing) {
                        float total_diff = std::sqrt(err_yaw * err_yaw + err_pitch * err_pitch);
                        float max_expected_diff = 0.5f; // approx 28 degrees
                        float ratio = std::clamp(total_diff / max_expected_diff, 0.0f, 1.0f);
                        sx = settings::aimbot::adaptive_smooth_min + ratio * (settings::aimbot::adaptive_smooth_max - settings::aimbot::adaptive_smooth_min);
                        sy = sx;
                    }

                    float factor_x = 1.0f - std::exp(-(1.5f / sx) * 60.0f * dt);
                    float factor_y = 1.0f - std::exp(-(1.5f / sy) * 60.0f * dt);

                    factor_x = std::clamp(factor_x, 0.0f, 1.0f);
                    factor_y = std::clamp(factor_y, 0.0f, 1.0f);

                    float step_yaw = err_yaw * factor_x;
                    float step_pitch = err_pitch * factor_y;

                    dx = -step_yaw / (0.0022f * sensitivity);
                    dy = -step_pitch / (0.0022f * sensitivity);
                } else {
                    dx = -err_yaw / (0.0022f * sensitivity);
                    dy = -err_pitch / (0.0022f * sensitivity);
                }

            } else {
                math::vector2 screen_pos = {};
                if (!game::visengine.world_to_screen(target_pos, screen_pos, dims, view)) return;

                float err_x = screen_pos.x - target_ref_x;
                float err_y = screen_pos.y - target_ref_y;

                if (settings::aimbot::mouse_smooth) {
                    float sx = std::clamp(settings::aimbot::mouse_smooth_x, 1.0f, 200.0f);
                    float sy = std::clamp(settings::aimbot::mouse_smooth_y, 1.0f, 200.0f);

                    if (settings::aimbot::adaptive_smoothing) {
                        float total_diff = std::sqrt(err_x * err_x + err_y * err_y);
                        float fov = settings::aimbot::fov;
                        if (fov < 1.0f) fov = 200.0f;
                        float ratio = std::clamp(total_diff / fov, 0.0f, 1.0f);
                        sx = settings::aimbot::adaptive_smooth_min + ratio * (settings::aimbot::adaptive_smooth_max - settings::aimbot::adaptive_smooth_min);
                        sy = sx;
                    }

                    float factor_x = 1.0f - std::exp(-(1.5f / sx) * 60.0f * dt);
                    float factor_y = 1.0f - std::exp(-(1.5f / sy) * 60.0f * dt);

                    factor_x = std::clamp(factor_x, 0.0f, 1.0f);
                    factor_y = std::clamp(factor_y, 0.0f, 1.0f);

                    float step_x = err_x * factor_x;
                    float step_y = err_y * factor_y;

                    dx = step_x;
                    dy = step_y;
                } else {
                    dx = err_x;
                    dy = err_y;
                }
            }

            constexpr float max_mouse_delta = 100.0f;
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
        static auto lock_start_time = std::chrono::steady_clock::now();

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
                locked_part_name = "";
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
                locked_part_name = "";
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

            math::vector2 dims = game::visengine.get_dimensions();
            math::matrix4 view = game::visengine.get_viewmatrix();

            std::vector<cache::entity_t> players_snapshot;
            cache::entity_t local_player_snapshot = {};
            {
                std::lock_guard<std::mutex> cache_lock(cache::mtx);
                if (cache::cached_players) {
                    players_snapshot = *cache::cached_players;
                }
                local_player_snapshot = cache::cached_local_player;
            }

            math::vector3 camera_pos = {};
            math::matrix3 camera_rot = {};
            float camera_fov = 1.22f;
            rbx::instance_t camera_inst = { memory->read<std::uint64_t>(game::workspace.address + Offsets::Workspace::CurrentCamera) };
            if (camera_inst.address != 0) {
                rbx::camera_t camera{ camera_inst.address };
                camera_pos = camera.get_position();
                camera_rot = camera.get_rotation();
                camera_fov = memory->read<float>(camera_inst.address + Offsets::Camera::FieldOfView);
                if (camera_fov < 0.001f || camera_fov > 3.14f) {
                    camera_fov = 1.22f;
                }
            } else {
                camera_rot = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
            }

            math::vector3 ray_dir = get_camera_ray_dir(cursor_pt, dims, camera_rot, camera_fov);

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
                    }
                }
            }

            cache::entity_t target = {};
            rbx::part_t target_part = {};
            std::string local_crew_id = local_player_snapshot.crew_id;

            {
                std::lock_guard<std::mutex> lock(mtx);
                if (has_locked_target && locked_target.instance.address != 0) {
                    bool skip_fov = (settings::aimbot::sticky_aim || g_aimbot_manual_locked);
                    bool skip_wall = g_aimbot_manual_locked;

                    if (is_target_valid(locked_target, local_crew_id, cursor_pt, dims, view, camera_pos, ray_dir, skip_fov, true)) {
                        if (is_target_valid(locked_target, local_crew_id, cursor_pt, dims, view, camera_pos, ray_dir, skip_fov, skip_wall)) {
                            target = locked_target;
                            target_part = get_best_bone(target, camera_pos, cursor_pt, dims, view, ray_dir, locked_part_name);
                        }
                    } else {
                        bool was_cheap_valid = is_target_cheap_valid(locked_target, local_crew_id);

                        locked_target = cache::entity_t{};
                        has_locked_target = false;
                        target_pos_initialized = false;
                        locked_part_name = "";

                        if (!was_cheap_valid) {
                            g_aimbot_manual_locked = false;
                            g_aimbot_manual_target = cache::entity_t{};
                            if (settings::aimbot::sticky_aim && settings::aimbot::keybind_mode != 2) {
                                needs_key_release = true;
                            }
                        }
                    }
                }
            }

            if (!has_locked_target) {
                if (g_aimbot_manual_locked && g_aimbot_manual_target.instance.address != 0) {
                    std::lock_guard<std::mutex> lock(mtx);
                    locked_target = g_aimbot_manual_target;
                    has_locked_target = true;
                    target_pos_initialized = false;
                    target = locked_target;
                    locked_part_name = "";
                    target_part = get_best_bone(target, camera_pos, cursor_pt, dims, view, ray_dir, locked_part_name);
                } else {
                    std::string chosen_part_name = "";
                    rbx::part_t chosen_part = {};
                    target = find_best_target(
                        players_snapshot,
                        local_player_snapshot,
                        cursor_pt,
                        dims,
                        view,
                        camera_pos,
                        ray_dir,
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
                    }
                }
            }

            if (target.instance.address == 0 || !target_part.address) {
                Sleep(10);
                continue;
            }

            if (settings::aimbot::wall_check && g_aimbot_manual_locked) {
                math::vector3 raw_pos = get_closest_point_on_part(target_part, camera_pos, ray_dir);
                if (is_bone_occluded_cached(target.instance.address, locked_part_name.empty() ? "Head" : locked_part_name, camera_pos, raw_pos)) {
                    Sleep(1);
                    continue;
                }
            }

            bool was_initialized = target_pos_initialized;
            static std::uint64_t last_run_target_address = 0;
            bool reset_state = !was_initialized || (target.instance.address != last_run_target_address);
            last_run_target_address = target.instance.address;

            rbx::primitive_t primitive = target_part.get_primitive();
            math::vector3 target_pos = get_closest_point_on_part(target_part, camera_pos, ray_dir);

            if (reset_state) {
                if (settings::aimbot::bone_random_offset) {
                    float val = settings::aimbot::bone_random_offset_val;
                    float rx = (((float)rand() / RAND_MAX) * 2.0f - 1.0f) * val;
                    float ry = (((float)rand() / RAND_MAX) * 2.0f - 1.0f) * val;
                    float rz = (((float)rand() / RAND_MAX) * 2.0f - 1.0f) * val;
                    current_bone_offset = { rx, ry, rz };
                } else {
                    current_bone_offset = { 0.0f, 0.0f, 0.0f };
                }
            }

            target_pos.x += current_bone_offset.x;
            target_pos.y += current_bone_offset.y;
            target_pos.z += current_bone_offset.z;

            bool use_prediction = (settings::aimbot::aimbot_type == 0 && settings::aimbot::camera_prediction) ||
                                   (settings::aimbot::aimbot_type == 1 && settings::aimbot::mouse_prediction) ||
                                   (settings::aimbot::projectile_prediction);
            if (use_prediction) {
                target_pos = apply_prediction(target.instance.address, primitive, target_pos, camera_pos, settings::aimbot::aimbot_type == 0);
            }

            if (settings::aimbot::shake) {
                float time = std::chrono::duration<float>(std::chrono::steady_clock::now().time_since_epoch()).count();
                float offset_x = (std::sin(time * 15.0f) * 0.7f + std::sin(time * 23.5f) * 0.3f) * settings::aimbot::shake_x;
                float offset_y = (std::cos(time * 12.0f) * 0.7f + std::cos(time * 27.2f) * 0.3f) * settings::aimbot::shake_y;
                float offset_z = (std::sin(time * 18.2f) * 0.7f + std::cos(time * 21.1f) * 0.3f) * settings::aimbot::shake_x;

                target_pos.x += offset_x * 0.05f;
                target_pos.y += offset_y * 0.05f;
                target_pos.z += offset_z * 0.05f;
            }

            last_target_pos = target_pos;

            auto now = std::chrono::high_resolution_clock::now();
            float dt = std::chrono::duration<float>(now - last_tick).count();
            last_tick = now;

            if (dt > 0.1f) dt = 0.016f;

            filtered_target_pos = target_pos;
            target_pos_initialized = true;

            if (reset_state) {
                lock_start_time = std::chrono::steady_clock::now();
            }

            float ease_factor = 1.0f;
            if (settings::aimbot::easing_style > 0) {
                float elapsed = std::chrono::duration<float>(std::chrono::steady_clock::now() - lock_start_time).count();
                float duration = (settings::aimbot::ease_time > 0.001f) ? settings::aimbot::ease_time : 0.5f;
                float progress = elapsed / duration;
                if (progress > 1.0f) progress = 1.0f;
                ease_factor = apply_easing(settings::aimbot::easing_style, progress);
            }

            if (settings::aimbot::aimbot_type == 0) {
                execute_camera_aim(target.instance.address, filtered_target_pos, dt, reset_state, ease_factor);
            }
            else {
                execute_mouse_aim(target.instance.address, filtered_target_pos, cursor_pt, dt, dims, view, reset_state, ease_factor);
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
        g_aimbot_manual_locked = false;
        g_aimbot_manual_target = cache::entity_t{};
        accum_x = 0.0f;
        accum_y = 0.0f;

        current_bone_offset = { 0.0f, 0.0f, 0.0f };
    }

    void lock_target(const cache::entity_t& target) {
        std::lock_guard<std::mutex> lock(mtx);
        locked_target = target;
        has_locked_target = true;
        g_aimbot_manual_locked = true;
        g_aimbot_manual_target = target;
        target_pos_initialized = false;
        locked_part_name = "";

        current_bone_offset = { 0.0f, 0.0f, 0.0f };
    }

    void unlock_target() {
        std::lock_guard<std::mutex> lock(mtx);
        locked_target = cache::entity_t{};
        has_locked_target = false;
        g_aimbot_manual_locked = false;
        g_aimbot_manual_target = cache::entity_t{};
        target_pos_initialized = false;
        locked_part_name = "";

        current_bone_offset = { 0.0f, 0.0f, 0.0f };
    }

    void render() {
        // Stub/unused in modern render flow, kept for header signature compatibility
    }
}