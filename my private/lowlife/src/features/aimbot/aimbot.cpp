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

        float virtual_yaw = 0.0f;
        float virtual_pitch = 0.0f;
        bool virtual_angles_initialized = false;

        float accum_x = 0.0f;
        float accum_y = 0.0f;

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
            return player.is_knocked;
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
                if (!game::visengine.world_to_screen(world_pos, screen_pos, dims, view)) continue;
                float dist = vector2_distance(screen_pos.x, screen_pos.y, cursor_x, cursor_y);
                if (dist < min_dist) {
                    min_dist = dist;
                    closest = part;
                }
            }
            return closest;
        }

        rbx::part_t get_target_part(const cache::entity_t& player, int aim_part, const POINT& cursor_pt, const math::vector2& dims, const math::matrix4& view) {
            if (player.parts.empty()) return rbx::part_t{};

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

            auto rel_it = settings::player_relations::relations.find(player.name);
            if (rel_it != settings::player_relations::relations.end() && rel_it->second == 1) {
                return false;
            }

            if (settings::aimbot::team_check && is_on_same_team(player, local_crew_id)) {
                return false;
            }

            if (settings::aimbot::knocked_check && is_knocked(player)) return false;

            if (settings::aimbot::fov_check && !skip_fov_check) {
                rbx::part_t target_part = get_target_part(player, settings::aimbot::aimpart, cursor_pt, dims, view);
                if (!target_part.address) return false;

                rbx::primitive_t primitive = target_part.get_primitive();
                math::vector3 world_pos = primitive.get_position();
                math::vector2 screen_pos = {};

                if (!game::visengine.world_to_screen(world_pos, screen_pos, dims, view)) return false;

                float cursor_x = static_cast<float>(cursor_pt.x);
                float cursor_y = static_cast<float>(cursor_pt.y);
                float dist = vector2_distance(screen_pos.x, screen_pos.y, cursor_x, cursor_y);
                if (dist > settings::aimbot::fov) return false;
            }

            if (settings::aimbot::wall_check) {
                rbx::instance_t camera_inst = { memory->read<std::uint64_t>(game::workspace.address + Offsets::Workspace::CurrentCamera) };
                if (camera_inst.address != 0) {
                    rbx::camera_t camera{ camera_inst.address };
                    math::vector3 camera_pos = camera.get_position();
                    
                    bool any_part_visible = false;
                    const std::unordered_set<std::string> target_parts_to_check = {
                        "Head", "Torso", "UpperTorso", "LowerTorso",
                        "Left Arm", "LeftUpperArm", "LeftLowerArm", "LeftHand",
                        "Right Arm", "RightUpperArm", "RightLowerArm", "RightHand",
                        "Left Leg", "LeftUpperLeg", "LeftLowerLeg", "LeftFoot",
                        "Right Leg", "RightUpperLeg", "RightLowerLeg", "RightFoot",
                        "HumanoidRootPart"
                    };

                    for (const auto& pair : player.parts) {
                        if (target_parts_to_check.find(pair.first) == target_parts_to_check.end()) continue;
                        rbx::part_t part = pair.second;
                        if (!part.address) continue;
                        rbx::primitive_t primitive = part.get_primitive();
                        if (!primitive.address) continue;
                        math::vector3 world_pos = primitive.get_position();
                        if (!botter::is_occluded(camera_pos, world_pos)) {
                            any_part_visible = true;
                            break;
                        }
                    }
                    if (!any_part_visible) {
                        return false;
                    }
                }
            }

            return true;
        }

        cache::entity_t find_best_target(const std::string& local_crew_id, const POINT& cursor_pt, const math::vector2& dims, const math::matrix4& view, std::uint64_t local_player_addr) {
            cache::entity_t best = {};
            float best_dist = std::numeric_limits<float>::max();

            std::lock_guard<std::mutex> lock(cache::mtx);
            float cursor_x = static_cast<float>(cursor_pt.x);
            float cursor_y = static_cast<float>(cursor_pt.y);

            if (cache::cached_players) {
                for (const auto& player : *cache::cached_players) {
                    if (player.instance.address == 0 || player.instance.address == local_player_addr)
                        continue;

                    if (!is_target_valid(player, local_crew_id, cursor_pt, dims, view)) continue;

                    rbx::part_t target_part = get_target_part(player, settings::aimbot::aimpart, cursor_pt, dims, view);
                    if (!target_part.address) continue;

                    rbx::primitive_t primitive = target_part.get_primitive();
                    math::vector3 world_pos = primitive.get_position();
                    math::vector2 screen_pos = {};

                    if (!game::visengine.world_to_screen(world_pos, screen_pos, dims, view)) continue;

                    float dist = vector2_distance(screen_pos.x, screen_pos.y, cursor_x, cursor_y);
                    if (dist < best_dist) {
                        best_dist = dist;
                        best = player;
                    }
                }
            }
            return best;
        }

        math::vector3 apply_prediction(rbx::primitive_t primitive, bool is_camera) {
            math::vector3 pos = primitive.get_position();
            math::vector3 vel = primitive.get_velocity();

            if (!std::isfinite(vel.x) || !std::isfinite(vel.y) || !std::isfinite(vel.z) ||
                std::abs(vel.x) > MAX_VELOCITY || std::abs(vel.y) > MAX_VELOCITY || std::abs(vel.z) > MAX_VELOCITY)
                return pos;

            float px = is_camera ? settings::aimbot::camera_prediction_x : settings::aimbot::mouse_prediction_x;
            float py = is_camera ? settings::aimbot::camera_prediction_y : settings::aimbot::mouse_prediction_y;

            pos.x += vel.x * PREDICTION_SCALE * px;
            pos.y += vel.y * PREDICTION_SCALE * py;
            pos.z += vel.z * PREDICTION_SCALE * px;
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

        void execute_camera_aim(const math::vector3& target_pos, float dt) {
            rbx::instance_t camera_inst = { memory->read<std::uint64_t>(game::workspace.address + Offsets::Workspace::CurrentCamera) };
            if (!camera_inst.address) return;

            rbx::camera_t camera{ camera_inst.address };
            math::matrix3 current_rot = camera.get_rotation();
            math::vector3 camera_pos = camera.get_position();

            math::vector3 target_forward = normalize(vector3_sub(target_pos, camera_pos));
            math::vector3 smoothed_forward = target_forward;

            if (settings::aimbot::camera_smooth) {
                float sx = std::clamp(settings::aimbot::camera_smooth_x, 1.0f, 200.0f);
                float sy = std::clamp(settings::aimbot::camera_smooth_y, 1.0f, 200.0f);

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

                float t_x = (sx <= 1.05f) ? 1.0f : std::clamp(dt * (150.0f / sx), 0.0f, 1.0f);
                float t_y = (sy <= 1.05f) ? 1.0f : std::clamp(dt * (150.0f / sy), 0.0f, 1.0f);

                float eased_t_x = apply_easing(settings::aimbot::easing_style, t_x);
                float eased_t_y = apply_easing(settings::aimbot::easing_style, t_y);

                float final_yaw = current_yaw + yaw_diff * eased_t_x;
                float final_pitch = current_pitch + pitch_diff * eased_t_y;

                smoothed_forward = angles_to_vector(final_yaw, final_pitch);
                smoothed_forward = normalize(smoothed_forward);
            }

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
            if (!game::visengine.world_to_screen(target_pos, screen_pos, dims, view)) return;

            float dx = screen_pos.x - static_cast<float>(cursor_pt.x);
            float dy = screen_pos.y - static_cast<float>(cursor_pt.y);

            float sensitivity = std::clamp(settings::aimbot::mouse_sensitivity, 0.1f, 10.0f);
            dx *= sensitivity;
            dy *= sensitivity;

            if (settings::aimbot::mouse_smooth) {
                float sx = std::clamp(settings::aimbot::mouse_smooth_x, 1.0f, 200.0f);
                float sy = std::clamp(settings::aimbot::mouse_smooth_y, 1.0f, 200.0f);

                float t_x = (sx <= 1.05f) ? 1.0f : std::clamp(dt * (150.0f / sx), 0.0f, 1.0f);
                float t_y = (sy <= 1.05f) ? 1.0f : std::clamp(dt * (150.0f / sy), 0.0f, 1.0f);

                float eased_t_x = apply_easing(settings::aimbot::easing_style, t_x);
                float eased_t_y = apply_easing(settings::aimbot::easing_style, t_y);

                dx *= eased_t_x;
                dy *= eased_t_y;
            }

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
            Sleep(1);

            auto now = std::chrono::high_resolution_clock::now();
            float dt = std::chrono::duration<float>(now - last_tick).count();
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
                virtual_angles_initialized = false; 
                locked_part_name = "";
                accum_x = 0.0f;
                accum_y = 0.0f;
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
                virtual_angles_initialized = false; 
                locked_part_name = "";
                accum_x = 0.0f;
                accum_y = 0.0f;
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

            // Fetch visual engine parameters once per frame
            math::vector2 dims = game::visengine.get_dimensions();
            math::matrix4 view = game::visengine.get_viewmatrix();

            std::lock_guard<std::mutex> lock(mtx);

            std::vector<cache::entity_t> players_snapshot;
            cache::entity_t local_player_snapshot = {};
            {
                std::lock_guard<std::mutex> cache_lock(cache::mtx);
                if (cache::cached_players) {
                    players_snapshot = *cache::cached_players;
                }
                local_player_snapshot = cache::cached_local_player;
            }
            std::string local_crew_id = local_player_snapshot.crew_id;
            std::uint64_t local_player_addr = local_player_snapshot.instance.address;

            if (has_locked_target && locked_target.instance.address != 0) {
                bool found = false;
                for (const auto& player : players_snapshot) {
                    if (player.instance.address == locked_target.instance.address) {
                        locked_target = player;
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    locked_target = cache::entity_t{};
                    has_locked_target = false;
                    target_pos_initialized = false;
                    locked_part_name = "";
                }
            }

            cache::entity_t target = {};

            // Manual lock has priority
            if (g_aimbot_manual_locked && g_aimbot_manual_target.instance.address != 0) {
                bool found = false;
                for (const auto& player : players_snapshot) {
                    if (player.instance.address == g_aimbot_manual_target.instance.address) {
                        g_aimbot_manual_target = player;
                        found = true;
                        break;
                    }
                }
                if (found && is_target_valid(g_aimbot_manual_target, local_crew_id, cursor_pt, dims, view, true)) {
                    target = g_aimbot_manual_target;
                } else {
                    g_aimbot_manual_locked = false;
                    g_aimbot_manual_target = {};
                }
            }

            if (target.instance.address == 0) {
                if (settings::aimbot::sticky_aim && has_locked_target && locked_target.instance.address != 0) {
                    if (is_target_valid(locked_target, local_crew_id, cursor_pt, dims, view, true)) {
                        target = locked_target;  
                    }
                    else {
                        locked_target = cache::entity_t{};
                        has_locked_target = false;
                        needs_key_release = true; 
                        target_pos_initialized = false; 
                        locked_part_name = "";
                        accum_x = 0.0f;
                        accum_y = 0.0f;
                    }
                }
                else if (has_locked_target && locked_target.instance.address != 0) {
                    if (is_target_valid(locked_target, local_crew_id, cursor_pt, dims, view, false)) {
                        target = locked_target;
                    }
                    else {
                        locked_target = cache::entity_t{};
                        has_locked_target = false;
                        target_pos_initialized = false; 
                        locked_part_name = "";
                        accum_x = 0.0f;
                        accum_y = 0.0f;
                    }
                }
                else {
                    locked_target = cache::entity_t{};
                    has_locked_target = false;
                    target_pos_initialized = false; 
                    locked_part_name = "";
                    accum_x = 0.0f;
                    accum_y = 0.0f;
                }
            }

            if (target.instance.address == 0) {
                target = find_best_target(local_crew_id, cursor_pt, dims, view, local_player_addr);
                if (target.instance.address != 0) {
                    locked_target = target;
                    has_locked_target = true;
                    target_pos_initialized = false; 
                    locked_part_name = "";
                    accum_x = 0.0f;
                    accum_y = 0.0f;
                }
            }

            if (target.instance.address == 0) continue;

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
            math::vector3 target_pos = primitive.get_position();

            bool use_prediction = (settings::aimbot::aimbot_type == 0 && settings::aimbot::camera_prediction) ||
                                  (settings::aimbot::aimbot_type == 1 && settings::aimbot::mouse_prediction);
            if (use_prediction) {
                target_pos = apply_prediction(primitive, settings::aimbot::aimbot_type == 0);
            }

            filtered_target_pos = target_pos;
            target_pos_initialized = true;

            if (settings::aimbot::aimbot_type == 0) {
                execute_camera_aim(filtered_target_pos, dt);
            }
            else {
                execute_mouse_aim(filtered_target_pos, cursor_pt, dt, dims, view);
            }
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
        g_aimbot_manual_target = {};
        accum_x = 0.0f;
        accum_y = 0.0f;
    }

    void lock_target(const cache::entity_t& target) {
        std::lock_guard<std::mutex> lock(mtx);
        locked_target = target;
        has_locked_target = true;
        g_aimbot_manual_locked = true;
        g_aimbot_manual_target = target;
        target_pos_initialized = false;
        locked_part_name = "";
        accum_x = 0.0f;
        accum_y = 0.0f;
    }

    void unlock_target() {
        std::lock_guard<std::mutex> lock(mtx);
        locked_target = cache::entity_t{};
        has_locked_target = false;
        g_aimbot_manual_locked = false;
        g_aimbot_manual_target = {};
        target_pos_initialized = false;
        locked_part_name = "";
        accum_x = 0.0f;
        accum_y = 0.0f;
    }

    void render() {
        // Stub signature compatibility
    }
}