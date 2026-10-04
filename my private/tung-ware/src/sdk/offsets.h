#pragma once
/* =============================================================
/*                       theo's offsets                         
/*                  https://offsets.imtheo.lol                  
/* -------------------------------------------------------------
/*  Dumped With     : RbxDumperV2                               
/*  Source code     : https://git.imtheo.lol/theo/RbxDumperV2   
/*  Roblox Version  : version-02c37bc51a384b8f
/*  Dumper Version  : 2.2.4
/*  Dumped At       : 20:12 29/09/2026 (GMT)
/*  Total Offsets   : 392
/* -------------------------------------------------------------
/*  Join the discord!                                           
/*  https://offsets.imtheo.lol/discord                          
/* =============================================================
*/

#include <cstdint>
#include <string>
#include <unordered_map>

struct Offset {
    uintptr_t value;
    Offset(const char* path, uintptr_t val);
    operator uintptr_t() const { return value; }
    Offset& operator=(uintptr_t val) {
        value = val;
        return *this;
    }
};

namespace Offsets {
    inline std::string ClientVersion = "version-02c37bc51a384b8f";
    bool Update(const std::string& current_version);
    std::unordered_map<std::string, uintptr_t*>& GetRegistry();

    namespace AirProperties {
          inline Offset AirDensity = { "AirProperties::AirDensity", 0x18 };
          inline Offset GlobalWind = { "AirProperties::GlobalWind", 0x3c };
    }

    namespace AnimationTrack {
          inline Offset Animation = { "AnimationTrack::Animation", 0xa8 };
          inline Offset Animator = { "AnimationTrack::Animator", 0x100 };
          inline Offset IsPlaying = { "AnimationTrack::IsPlaying", 0xa48 };
          inline Offset Looped = { "AnimationTrack::Looped", 0xd5 };
          inline Offset Speed = { "AnimationTrack::Speed", 0xc4 };
          inline Offset TimePosition = { "AnimationTrack::TimePosition", 0xc8 };
    }

    namespace Animator {
          inline Offset ActiveAnimations = { "Animator::ActiveAnimations", 0xa80 };
    }

    namespace Atmosphere {
          inline Offset Color = { "Atmosphere::Color", 0xa8 };
          inline Offset Decay = { "Atmosphere::Decay", 0xb4 };
          inline Offset Density = { "Atmosphere::Density", 0xc0 };
          inline Offset Glare = { "Atmosphere::Glare", 0xc4 };
          inline Offset Haze = { "Atmosphere::Haze", 0xc8 };
          inline Offset Offset = { "Atmosphere::Offset", 0xcc };
    }

    namespace Attachment {
          inline Offset Position = { "Attachment::Position", 0xb4 };
    }

    namespace BasePart {
          inline Offset CastShadow = { "BasePart::CastShadow", 0x125 };
          inline Offset Color3 = { "BasePart::Color3", 0x198 };
          inline Offset Locked = { "BasePart::Locked", 0x126 };
          inline Offset Massless = { "BasePart::Massless", 0x127 };
          inline Offset Primitive = { "BasePart::Primitive", 0x178 };
          inline Offset Reflectance = { "BasePart::Reflectance", 0xfc };
          inline Offset Shape = { "BasePart::Shape", 0x1a8 };
          inline Offset Transparency = { "BasePart::Transparency", 0x120 };
    }

    namespace Beam {
          inline Offset Attachment0 = { "Beam::Attachment0", 0x150 };
          inline Offset Attachment1 = { "Beam::Attachment1", 0x160 };
          inline Offset Brightness = { "Beam::Brightness", 0x170 };
          inline Offset CurveSize0 = { "Beam::CurveSize0", 0x174 };
          inline Offset CurveSize1 = { "Beam::CurveSize1", 0x178 };
          inline Offset LightEmission = { "Beam::LightEmission", 0x17c };
          inline Offset LightInfluence = { "Beam::LightInfluence", 0x180 };
          inline Offset Texture = { "Beam::Texture", 0x130 };
          inline Offset TextureLength = { "Beam::TextureLength", 0x18c };
          inline Offset TextureSpeed = { "Beam::TextureSpeed", 0x194 };
          inline Offset Width0 = { "Beam::Width0", 0x198 };
          inline Offset Width1 = { "Beam::Width1", 0x19c };
          inline Offset ZOffset = { "Beam::ZOffset", 0x1a0 };
    }

    namespace BloomEffect {
          inline Offset Enabled = { "BloomEffect::Enabled", 0xa0 };
          inline Offset Intensity = { "BloomEffect::Intensity", 0xa8 };
          inline Offset Size = { "BloomEffect::Size", 0xac };
          inline Offset Threshold = { "BloomEffect::Threshold", 0xb0 };
    }

    namespace BlurEffect {
          inline Offset Enabled = { "BlurEffect::Enabled", 0xa0 };
          inline Offset Size = { "BlurEffect::Size", 0xa8 };
    }

    namespace ByteCode {
          inline Offset Pointer = { "ByteCode::Pointer", 0x10 };
          inline Offset Size = { "ByteCode::Size", 0x28 };
    }

    namespace CachedItem {
          inline Offset FileMeshData = { "CachedItem::FileMeshData", 0x40 };
    }

    namespace Camera {
          inline Offset CameraSubject = { "Camera::CameraSubject", 0xb8 };
          inline Offset CameraType = { "Camera::CameraType", 0x128 };
          inline Offset FieldOfView = { "Camera::FieldOfView", 0x130 };
          inline Offset ImagePlaneDepth = { "Camera::ImagePlaneDepth", 0x2c4 };
          inline Offset Position = { "Camera::Position", 0xec };
          inline Offset Rotation = { "Camera::Rotation", 0xc8 };
          inline Offset Viewport = { "Camera::Viewport", 0x27c };
          inline Offset ViewportSize = { "Camera::ViewportSize", 0x2bc };
    }

    namespace CharacterMesh {
          inline Offset BaseTextureId = { "CharacterMesh::BaseTextureId", 0xb8 };
          inline Offset BodyPart = { "CharacterMesh::BodyPart", 0x138 };
          inline Offset MeshId = { "CharacterMesh::MeshId", 0xe8 };
          inline Offset OverlayTextureId = { "CharacterMesh::OverlayTextureId", 0x118 };
    }

    namespace ClickDetector {
          inline Offset MaxActivationDistance = { "ClickDetector::MaxActivationDistance", 0xd8 };
          inline Offset MouseIcon = { "ClickDetector::MouseIcon", 0xb8 };
    }

    namespace Clothing {
          inline Offset Color3 = { "Clothing::Color3", 0x110 };
          inline Offset Template = { "Clothing::Template", 0xf0 };
    }

    namespace ColorCorrectionEffect {
          inline Offset Brightness = { "ColorCorrectionEffect::Brightness", 0xb4 };
          inline Offset Contrast = { "ColorCorrectionEffect::Contrast", 0xb8 };
          inline Offset Enabled = { "ColorCorrectionEffect::Enabled", 0xa0 };
          inline Offset TintColor = { "ColorCorrectionEffect::TintColor", 0xa8 };
    }

    namespace ColorGradingEffect {
          inline Offset Enabled = { "ColorGradingEffect::Enabled", 0xa0 };
          inline Offset TonemapperPreset = { "ColorGradingEffect::TonemapperPreset", 0xa8 };
    }

    namespace DataModel {
          inline Offset CreatorId = { "DataModel::CreatorId", 0x178 };
          inline Offset GameId = { "DataModel::GameId", 0x180 };
          inline Offset GameLoaded = { "DataModel::GameLoaded", 0x5d0 };
          inline Offset JobId = { "DataModel::JobId", 0x110 };
          inline Offset PlaceId = { "DataModel::PlaceId", 0x188 };
          inline Offset PlaceVersion = { "DataModel::PlaceVersion", 0x1a4 };
          inline Offset PrimitiveCount = { "DataModel::PrimitiveCount", 0x418 };
          inline Offset ScriptContext = { "DataModel::ScriptContext", 0x440 };
          inline Offset ServerIP = { "DataModel::ServerIP", 0x5b8 };
          inline Offset ToRenderView1 = { "DataModel::ToRenderView1", 0x1c0 };
          inline Offset ToRenderView2 = { "DataModel::ToRenderView2", 0x8 };
          inline Offset ToRenderView3 = { "DataModel::ToRenderView3", 0x28 };
          inline Offset Workspace = { "DataModel::Workspace", 0x150 };
    }

    namespace DepthOfFieldEffect {
          inline Offset Enabled = { "DepthOfFieldEffect::Enabled", 0xa0 };
          inline Offset FarIntensity = { "DepthOfFieldEffect::FarIntensity", 0xa8 };
          inline Offset FocusDistance = { "DepthOfFieldEffect::FocusDistance", 0xac };
          inline Offset InFocusRadius = { "DepthOfFieldEffect::InFocusRadius", 0xb0 };
          inline Offset NearIntensity = { "DepthOfFieldEffect::NearIntensity", 0xb4 };
    }

    namespace DragDetector {
          inline Offset ActivatedCursorIcon = { "DragDetector::ActivatedCursorIcon", 0x1b0 };
          inline Offset CursorIcon = { "DragDetector::CursorIcon", 0xb8 };
          inline Offset MaxActivationDistance = { "DragDetector::MaxActivationDistance", 0xd8 };
          inline Offset MaxDragAngle = { "DragDetector::MaxDragAngle", 0x298 };
          inline Offset MaxDragTranslation = { "DragDetector::MaxDragTranslation", 0x25c };
          inline Offset MaxForce = { "DragDetector::MaxForce", 0x29c };
          inline Offset MaxTorque = { "DragDetector::MaxTorque", 0x2a0 };
          inline Offset MinDragAngle = { "DragDetector::MinDragAngle", 0x2a4 };
          inline Offset MinDragTranslation = { "DragDetector::MinDragTranslation", 0x268 };
          inline Offset ReferenceInstance = { "DragDetector::ReferenceInstance", 0x1e0 };
          inline Offset Responsiveness = { "DragDetector::Responsiveness", 0x2b0 };
    }

    namespace FakeDataModel {
          inline Offset Pointer = { "FakeDataModel::Pointer", 0x8b54980 };
          inline Offset RealDataModel = { "FakeDataModel::RealDataModel", 0x1f8 };
    }

    namespace FileMeshData {
          inline Offset AABBMax = { "FileMeshData::AABBMax", 0x18c };
          inline Offset AABBMin = { "FileMeshData::AABBMin", 0x180 };
          inline Offset Faces = { "FileMeshData::Faces", 0x30 };
          inline Offset FacesEnd = { "FileMeshData::FacesEnd", 0x38 };
          inline Offset Vertices = { "FileMeshData::Vertices", 0x0 };
          inline Offset VerticesEnd = { "FileMeshData::VerticesEnd", 0x8 };
    }

    namespace GuiBase2D {
          inline Offset AbsolutePosition = { "GuiBase2D::AbsolutePosition", 0xfc };
          inline Offset AbsoluteRotation = { "GuiBase2D::AbsoluteRotation", 0xd8 };
          inline Offset AbsoluteSize = { "GuiBase2D::AbsoluteSize", 0x0 };
    }

    namespace GuiObject {
          inline Offset BackgroundColor3 = { "GuiObject::BackgroundColor3", 0x530 };
          inline Offset BackgroundTransparency = { "GuiObject::BackgroundTransparency", 0x53c };
          inline Offset BorderColor3 = { "GuiObject::BorderColor3", 0x53c };
          inline Offset Image = { "GuiObject::Image", 0x990 };
          inline Offset LayoutOrder = { "GuiObject::LayoutOrder", 0x56c };
          inline Offset Position = { "GuiObject::Position", 0x500 };
          inline Offset RichText = { "GuiObject::RichText", 0xb88 };
          inline Offset Rotation = { "GuiObject::Rotation", 0xd8 };
          inline Offset ScreenGui_Enabled = { "GuiObject::ScreenGui_Enabled", 0x4b4 };
          inline Offset Size = { "GuiObject::Size", 0x520 };
          inline Offset Text = { "GuiObject::Text", 0xdf0 };
          inline Offset TextColor3 = { "GuiObject::TextColor3", 0xea0 };
          inline Offset Visible = { "GuiObject::Visible", 0x59d };
          inline Offset ZIndex = { "GuiObject::ZIndex", 0x594 };
    }

    namespace Humanoid {
          inline Offset AutoJumpEnabled = { "Humanoid::AutoJumpEnabled", 0x1c4 };
          inline Offset AutoRotate = { "Humanoid::AutoRotate", 0x1c5 };
          inline Offset AutomaticScalingEnabled = { "Humanoid::AutomaticScalingEnabled", 0x1c6 };
          inline Offset BreakJointsOnDeath = { "Humanoid::BreakJointsOnDeath", 0x1c7 };
          inline Offset CameraOffset = { "Humanoid::CameraOffset", 0x118 };
          inline Offset DisplayDistanceType = { "Humanoid::DisplayDistanceType", 0x170 };
          inline Offset DisplayName = { "Humanoid::DisplayName", 0xa8 };
          inline Offset EvaluateStateMachine = { "Humanoid::EvaluateStateMachine", 0x1c8 };
          inline Offset FloorMaterial = { "Humanoid::FloorMaterial", 0x174 };
          inline Offset Health = { "Humanoid::Health", 0x180 };
          inline Offset HealthDisplayDistance = { "Humanoid::HealthDisplayDistance", 0x178 };
          inline Offset HealthDisplayType = { "Humanoid::HealthDisplayType", 0x17c };
          inline Offset HipHeight = { "Humanoid::HipHeight", 0x184 };
          inline Offset HumanoidRootPart = { "Humanoid::HumanoidRootPart", 0x458 };
          inline Offset HumanoidState = { "Humanoid::HumanoidState", 0x8a0 };
          inline Offset HumanoidStateID = { "Humanoid::HumanoidStateID", 0x20 };
          inline Offset IsWalking = { "Humanoid::IsWalking", 0xa1f };
          inline Offset Jump = { "Humanoid::Jump", 0x1ca };
          inline Offset JumpHeight = { "Humanoid::JumpHeight", 0x190 };
          inline Offset JumpPower = { "Humanoid::JumpPower", 0x194 };
          inline Offset MaxHealth = { "Humanoid::MaxHealth", 0x198 };
          inline Offset MaxSlopeAngle = { "Humanoid::MaxSlopeAngle", 0x19c };
          inline Offset MoveDirection = { "Humanoid::MoveDirection", 0x130 };
          inline Offset MoveToPart = { "Humanoid::MoveToPart", 0x108 };
          inline Offset MoveToPoint = { "Humanoid::MoveToPoint", 0x154 };
          inline Offset NameDisplayDistance = { "Humanoid::NameDisplayDistance", 0x1a0 };
          inline Offset NameOcclusion = { "Humanoid::NameOcclusion", 0x1a4 };
          inline Offset PlatformStand = { "Humanoid::PlatformStand", 0x1cc };
          inline Offset PlatformStatePointer = { "Humanoid::PlatformStatePointer", 0x0 };
          inline Offset RequiresNeck = { "Humanoid::RequiresNeck", 0x1cd };
          inline Offset RigType = { "Humanoid::RigType", 0x1b0 };
          inline Offset SeatPart = { "Humanoid::SeatPart", 0xf8 };
          inline Offset Sit = { "Humanoid::Sit", 0x1cd };
          inline Offset TargetPoint = { "Humanoid::TargetPoint", 0x13c };
          inline Offset UseJumpPower = { "Humanoid::UseJumpPower", 0x1d0 };
          inline Offset WalkTimer = { "Humanoid::WalkTimer", 0x0 };
          inline Offset Walkspeed = { "Humanoid::Walkspeed", 0x1c0 };
          inline Offset WalkspeedCheck = { "Humanoid::WalkspeedCheck", 0x39c };
    }

    namespace Instance {
          inline Offset ChildrenEnd = { "Instance::ChildrenEnd", 0x8 };
          inline Offset ChildrenStart = { "Instance::ChildrenStart", 0x78 };
          inline Offset ClassBase = { "Instance::ClassBase", 0x1b0 };
          inline Offset ClassDescriptor = { "Instance::ClassDescriptor", 0x18 };
          inline Offset ClassName = { "Instance::ClassName", 0x8 };
          inline Offset Name = { "Instance::Name", 0x8 };
          inline Offset NameContainer = { "Instance::NameContainer", 0x70 };
          inline Offset Parent = { "Instance::Parent", 0x68 };
          inline Offset This = { "Instance::This", 0x8 };
    }

    namespace LRUHolder {
          inline Offset MemEnforcedLRUCache = { "LRUHolder::MemEnforcedLRUCache", 0x20 };
    }

    namespace LRUNode {
          inline Offset AssetID = { "LRUNode::AssetID", 0x10 };
          inline Offset CachedItem = { "LRUNode::CachedItem", 0x40 };
          inline Offset Next = { "LRUNode::Next", 0x0 };
    }

    namespace Lighting {
          inline Offset Ambient = { "Lighting::Ambient", 0xc0 };
          inline Offset Brightness = { "Lighting::Brightness", 0x108 };
          inline Offset ClockTime = { "Lighting::ClockTime", 0xb8 };
          inline Offset ColorShift_Bottom = { "Lighting::ColorShift_Bottom", 0xd8 };
          inline Offset ColorShift_Top = { "Lighting::ColorShift_Top", 0xcc };
          inline Offset EnvironmentDiffuseScale = { "Lighting::EnvironmentDiffuseScale", 0x10c };
          inline Offset EnvironmentSpecularScale = { "Lighting::EnvironmentSpecularScale", 0x110 };
          inline Offset ExposureCompensation = { "Lighting::ExposureCompensation", 0x114 };
          inline Offset FogColor = { "Lighting::FogColor", 0xe4 };
          inline Offset FogEnd = { "Lighting::FogEnd", 0x11c };
          inline Offset FogStart = { "Lighting::FogStart", 0x120 };
          inline Offset GeographicLatitude = { "Lighting::GeographicLatitude", 0x124 };
          inline Offset GlobalShadows = { "Lighting::GlobalShadows", 0x134 };
          inline Offset GradientBottom = { "Lighting::GradientBottom", 0x180 };
          inline Offset GradientTop = { "Lighting::GradientTop", 0x140 };
          inline Offset LightColor = { "Lighting::LightColor", 0x14c };
          inline Offset LightDirection = { "Lighting::LightDirection", 0x158 };
          inline Offset MoonPosition = { "Lighting::MoonPosition", 0x174 };
          inline Offset OutdoorAmbient = { "Lighting::OutdoorAmbient", 0xf0 };
          inline Offset Sky = { "Lighting::Sky", 0x1b8 };
          inline Offset Source = { "Lighting::Source", 0x164 };
          inline Offset SunPosition = { "Lighting::SunPosition", 0x168 };
    }

    namespace LocalScript {
          inline Offset ByteCode = { "LocalScript::ByteCode", 0x0 };
          inline Offset GUID = { "LocalScript::GUID", 0xc0 };
          inline Offset Hash = { "LocalScript::Hash", 0x190 };
    }

    namespace MaterialColors {
          inline Offset Asphalt = { "MaterialColors::Asphalt", 0x30 };
          inline Offset Basalt = { "MaterialColors::Basalt", 0x27 };
          inline Offset Brick = { "MaterialColors::Brick", 0xf };
          inline Offset Cobblestone = { "MaterialColors::Cobblestone", 0x33 };
          inline Offset Concrete = { "MaterialColors::Concrete", 0xc };
          inline Offset CrackedLava = { "MaterialColors::CrackedLava", 0x2d };
          inline Offset Glacier = { "MaterialColors::Glacier", 0x1b };
          inline Offset Grass = { "MaterialColors::Grass", 0x6 };
          inline Offset Ground = { "MaterialColors::Ground", 0x2a };
          inline Offset Ice = { "MaterialColors::Ice", 0x36 };
          inline Offset LeafyGrass = { "MaterialColors::LeafyGrass", 0x39 };
          inline Offset Limestone = { "MaterialColors::Limestone", 0x3f };
          inline Offset Mud = { "MaterialColors::Mud", 0x24 };
          inline Offset Pavement = { "MaterialColors::Pavement", 0x42 };
          inline Offset Rock = { "MaterialColors::Rock", 0x18 };
          inline Offset Salt = { "MaterialColors::Salt", 0x3c };
          inline Offset Sand = { "MaterialColors::Sand", 0x12 };
          inline Offset Sandstone = { "MaterialColors::Sandstone", 0x21 };
          inline Offset Slate = { "MaterialColors::Slate", 0x9 };
          inline Offset Snow = { "MaterialColors::Snow", 0x1e };
          inline Offset WoodPlanks = { "MaterialColors::WoodPlanks", 0x15 };
    }

    namespace MemEnforcedLRUCache {
          inline Offset Head = { "MemEnforcedLRUCache::Head", 0x8 };
    }

    namespace MeshContentProvider {
          inline Offset LRUHolder = { "MeshContentProvider::LRUHolder", 0xc8 };
    }

    namespace MeshPart {
          inline Offset MeshId = { "MeshPart::MeshId", 0x300 };
          inline Offset Texture = { "MeshPart::Texture", 0x330 };
    }

    namespace Misc {
          inline Offset Adornee = { "Misc::Adornee", 0xe0 };
          inline Offset AnimationId = { "Misc::AnimationId", 0xb0 };
          inline Offset StringLength = { "Misc::StringLength", 0x10 };
          inline Offset Value = { "Misc::Value", 0xa8 };
    }

    namespace Model {
          inline Offset PrimaryPart = { "Model::PrimaryPart", 0x248 };
          inline Offset Scale = { "Model::Scale", 0x134 };
    }

    namespace ModuleScript {
          inline Offset ByteCode = { "ModuleScript::ByteCode", 0x0 };
          inline Offset GUID = { "ModuleScript::GUID", 0xc0 };
          inline Offset Hash = { "ModuleScript::Hash", 0x350 };
          inline Offset IsCoreScript = { "ModuleScript::IsCoreScript", 0x0 };
    }

    namespace MouseService {
          inline Offset InputObject = { "MouseService::InputObject", 0xe0 };
          inline Offset InputObject2 = { "MouseService::InputObject2", 0xf0 };
          inline Offset MousePosition = { "MouseService::MousePosition", 0xc4 };
          inline Offset SensitivityPointer = { "MouseService::SensitivityPointer", 0x0 };
    }

    namespace ParticleEmitter {
          inline Offset Acceleration = { "ParticleEmitter::Acceleration", 0x1d0 };
          inline Offset Brightness = { "ParticleEmitter::Brightness", 0x20c };
          inline Offset Drag = { "ParticleEmitter::Drag", 0x210 };
          inline Offset Lifetime = { "ParticleEmitter::Lifetime", 0x1e4 };
          inline Offset LightEmission = { "ParticleEmitter::LightEmission", 0x228 };
          inline Offset LightInfluence = { "ParticleEmitter::LightInfluence", 0x22c };
          inline Offset Rate = { "ParticleEmitter::Rate", 0x238 };
          inline Offset RotSpeed = { "ParticleEmitter::RotSpeed", 0x1ec };
          inline Offset Rotation = { "ParticleEmitter::Rotation", 0x1f4 };
          inline Offset Speed = { "ParticleEmitter::Speed", 0x1fc };
          inline Offset SpreadAngle = { "ParticleEmitter::SpreadAngle", 0x204 };
          inline Offset Texture = { "ParticleEmitter::Texture", 0x1b0 };
          inline Offset TimeScale = { "ParticleEmitter::TimeScale", 0x24c };
          inline Offset VelocityInheritance = { "ParticleEmitter::VelocityInheritance", 0x250 };
          inline Offset ZOffset = { "ParticleEmitter::ZOffset", 0x254 };
    }

    namespace Player {
          inline Offset AccountAge = { "Player::AccountAge", 0x34c };
          inline Offset CameraMode = { "Player::CameraMode", 0x360 };
          inline Offset DisplayName = { "Player::DisplayName", 0x128 };
          inline Offset HealthDisplayDistance = { "Player::HealthDisplayDistance", 0x384 };
          inline Offset LocalPlayer = { "Player::LocalPlayer", 0x120 };
          inline Offset LocaleId = { "Player::LocaleId", 0x108 };
          inline Offset MaxZoomDistance = { "Player::MaxZoomDistance", 0x358 };
          inline Offset MinZoomDistance = { "Player::MinZoomDistance", 0x35c };
          inline Offset ModelInstance = { "Player::ModelInstance", 0x288 };
          inline Offset Mouse = { "Player::Mouse", 0x1208 };
          inline Offset NameDisplayDistance = { "Player::NameDisplayDistance", 0x394 };
          inline Offset Team = { "Player::Team", 0x2c8 };
          inline Offset TeamColor = { "Player::TeamColor", 0x3a0 };
          inline Offset UserId = { "Player::UserId", 0xc0 };
    }

    namespace PlayerConfigurer {
          inline Offset Pointer = { "PlayerConfigurer::Pointer", 0x0 };
    }

    namespace PlayerMouse {
          inline Offset Icon = { "PlayerMouse::Icon", 0xb8 };
          inline Offset Workspace = { "PlayerMouse::Workspace", 0x140 };
    }

    namespace Primitive {
          inline Offset AssemblyAngularVelocity = { "Primitive::AssemblyAngularVelocity", 0xec };
          inline Offset AssemblyLinearVelocity = { "Primitive::AssemblyLinearVelocity", 0xe0 };
          inline Offset Flags = { "Primitive::Flags", 0x1b6 };
          inline Offset Material = { "Primitive::Material", 0x0 };
          inline Offset Owner = { "Primitive::Owner", 0x210 };
          inline Offset Position = { "Primitive::Position", 0xd4 };
          inline Offset Rotation = { "Primitive::Rotation", 0xb0 };
          inline Offset Size = { "Primitive::Size", 0x1bc };
          inline Offset Validate = { "Primitive::Validate", 0x6 };
    }

    namespace PrimitiveFlags {
          inline Offset Anchored = { "PrimitiveFlags::Anchored", 0x2 };
          inline Offset CanCollide = { "PrimitiveFlags::CanCollide", 0x8 };
          inline Offset CanQuery = { "PrimitiveFlags::CanQuery", 0x20 };
          inline Offset CanTouch = { "PrimitiveFlags::CanTouch", 0x10 };
    }

    namespace ProximityPrompt {
          inline Offset ActionText = { "ProximityPrompt::ActionText", 0xa0 };
          inline Offset Enabled = { "ProximityPrompt::Enabled", 0x126 };
          inline Offset GamepadKeyCode = { "ProximityPrompt::GamepadKeyCode", 0x10c };
          inline Offset HoldDuration = { "ProximityPrompt::HoldDuration", 0x110 };
          inline Offset KeyCode = { "ProximityPrompt::KeyCode", 0x114 };
          inline Offset MaxActivationDistance = { "ProximityPrompt::MaxActivationDistance", 0x118 };
          inline Offset ObjectText = { "ProximityPrompt::ObjectText", 0xc0 };
          inline Offset RequiresLineOfSight = { "ProximityPrompt::RequiresLineOfSight", 0x127 };
    }

    namespace RenderJob {
          inline Offset FakeDataModel = { "RenderJob::FakeDataModel", 0x38 };
          inline Offset RealDataModel = { "RenderJob::RealDataModel", 0x1f0 };
          inline Offset RenderView = { "RenderJob::RenderView", 0x1d8 };
    }

    namespace RenderView {
          inline Offset DeviceD3D11 = { "RenderView::DeviceD3D11", 0x0 };
          inline Offset LightingValid = { "RenderView::LightingValid", 0x0 };
          inline Offset SkyValid = { "RenderView::SkyValid", 0x0 };
          inline Offset VisualEngine = { "RenderView::VisualEngine", 0x0 };
    }

    namespace RunService {
          inline Offset HeartbeatFPS = { "RunService::HeartbeatFPS", 0xc8 };
          inline Offset HeartbeatTask = { "RunService::HeartbeatTask", 0xe0 };
    }

    namespace Script {
          inline Offset ByteCode = { "Script::ByteCode", 0x0 };
          inline Offset GUID = { "Script::GUID", 0xc0 };
          inline Offset Hash = { "Script::Hash", 0x190 };
    }

    namespace ScriptContext {
          inline Offset RequireBypass = { "ScriptContext::RequireBypass", 0x0 };
    }

    namespace Seat {
          inline Offset Occupant = { "Seat::Occupant", 0x208 };
    }

    namespace Sky {
          inline Offset MoonAngularSize = { "Sky::MoonAngularSize", 0x234 };
          inline Offset MoonTextureId = { "Sky::MoonTextureId", 0xb8 };
          inline Offset SkyboxBk = { "Sky::SkyboxBk", 0xe8 };
          inline Offset SkyboxDn = { "Sky::SkyboxDn", 0x118 };
          inline Offset SkyboxFt = { "Sky::SkyboxFt", 0x148 };
          inline Offset SkyboxLf = { "Sky::SkyboxLf", 0x178 };
          inline Offset SkyboxOrientation = { "Sky::SkyboxOrientation", 0x228 };
          inline Offset SkyboxRt = { "Sky::SkyboxRt", 0x1a8 };
          inline Offset SkyboxUp = { "Sky::SkyboxUp", 0x1d8 };
          inline Offset StarCount = { "Sky::StarCount", 0x238 };
          inline Offset SunAngularSize = { "Sky::SunAngularSize", 0x22c };
          inline Offset SunTextureId = { "Sky::SunTextureId", 0x208 };
    }

    namespace Sound {
          inline Offset IsPlaying = { "Sound::IsPlaying", 0x130 };
          inline Offset Looped = { "Sound::Looped", 0x12d };
          inline Offset PlaybackSpeed = { "Sound::PlaybackSpeed", 0x10c };
          inline Offset RollOffMaxDistance = { "Sound::RollOffMaxDistance", 0x110 };
          inline Offset RollOffMinDistance = { "Sound::RollOffMinDistance", 0x114 };
          inline Offset SoundGroup = { "Sound::SoundGroup", 0xd8 };
          inline Offset SoundId = { "Sound::SoundId", 0xb8 };
          inline Offset Volume = { "Sound::Volume", 0x120 };
    }

    namespace SpawnLocation {
          inline Offset AllowTeamChangeOnTouch = { "SpawnLocation::AllowTeamChangeOnTouch", 0x3d };
          inline Offset Enabled = { "SpawnLocation::Enabled", 0x1e1 };
          inline Offset ForcefieldDuration = { "SpawnLocation::ForcefieldDuration", 0x1d8 };
          inline Offset Neutral = { "SpawnLocation::Neutral", 0x1e2 };
          inline Offset TeamColor = { "SpawnLocation::TeamColor", 0x1dc };
    }

    namespace SpecialMesh {
          inline Offset MeshId = { "SpecialMesh::MeshId", 0xe8 };
          inline Offset Scale = { "SpecialMesh::Scale", 0xb4 };
    }

    namespace StatsItem {
          inline Offset Value = { "StatsItem::Value", 0x1259 };
    }

    namespace SunRaysEffect {
          inline Offset Enabled = { "SunRaysEffect::Enabled", 0xa0 };
          inline Offset Intensity = { "SunRaysEffect::Intensity", 0xa8 };
          inline Offset Spread = { "SunRaysEffect::Spread", 0xac };
    }

    namespace SurfaceAppearance {
          inline Offset AlphaMode = { "SurfaceAppearance::AlphaMode", 0x1e0 };
          inline Offset Color = { "SurfaceAppearance::Color", 0x1c8 };
          inline Offset ColorMap = { "SurfaceAppearance::ColorMap", 0xb8 };
          inline Offset EmissiveMaskContent = { "SurfaceAppearance::EmissiveMaskContent", 0xe8 };
          inline Offset EmissiveStrength = { "SurfaceAppearance::EmissiveStrength", 0x1e4 };
          inline Offset EmissiveTint = { "SurfaceAppearance::EmissiveTint", 0x1d4 };
          inline Offset MetalnessMap = { "SurfaceAppearance::MetalnessMap", 0x118 };
          inline Offset NormalMap = { "SurfaceAppearance::NormalMap", 0x148 };
          inline Offset RoughnessMap = { "SurfaceAppearance::RoughnessMap", 0x178 };
    }

    namespace TaskScheduler {
          inline Offset JobEnd = { "TaskScheduler::JobEnd", 0xd0 };
          inline Offset JobName = { "TaskScheduler::JobName", 0x18 };
          inline Offset JobStart = { "TaskScheduler::JobStart", 0xc8 };
          inline Offset MaxFPS = { "TaskScheduler::MaxFPS", 0xb0 };
          inline Offset Pointer = { "TaskScheduler::Pointer", 0x8aff2a0 };
    }

    namespace Team {
          inline Offset BrickColor = { "Team::BrickColor", 0xa8 };
    }

    namespace Terrain {
          inline Offset GrassLength = { "Terrain::GrassLength", 0x1e0 };
          inline Offset MaterialColors = { "Terrain::MaterialColors", 0x4a8 };
          inline Offset WaterColor = { "Terrain::WaterColor", 0x1d0 };
          inline Offset WaterReflectance = { "Terrain::WaterReflectance", 0x1e8 };
          inline Offset WaterTransparency = { "Terrain::WaterTransparency", 0x1ec };
          inline Offset WaterWaveSize = { "Terrain::WaterWaveSize", 0x1f0 };
          inline Offset WaterWaveSpeed = { "Terrain::WaterWaveSpeed", 0x1f4 };
    }

    namespace Textures {
          inline Offset Decal_Texture = { "Textures::Decal_Texture", 0x1d0 };
          inline Offset Texture_Texture = { "Textures::Texture_Texture", 0x1d0 };
    }

    namespace Tool {
          inline Offset CanBeDropped = { "Tool::CanBeDropped", 0x4a8 };
          inline Offset Enabled = { "Tool::Enabled", 0x4a9 };
          inline Offset Grip = { "Tool::Grip", 0x49c };
          inline Offset ManualActivationOnly = { "Tool::ManualActivationOnly", 0x4aa };
          inline Offset RequiresHandle = { "Tool::RequiresHandle", 0x4ab };
          inline Offset TextureId = { "Tool::TextureId", 0x350 };
          inline Offset Tooltip = { "Tool::Tooltip", 0x458 };
    }

    namespace UnionOperation {
          inline Offset AssetId = { "UnionOperation::AssetId", 0x300 };
    }

    namespace UserInputService {
          inline Offset WindowInputState = { "UserInputService::WindowInputState", 0x2b0 };
    }

    namespace VehicleSeat {
          inline Offset MaxSpeed = { "VehicleSeat::MaxSpeed", 0x218 };
          inline Offset SteerFloat = { "VehicleSeat::SteerFloat", 0x21c };
          inline Offset ThrottleFloat = { "VehicleSeat::ThrottleFloat", 0x220 };
          inline Offset Torque = { "VehicleSeat::Torque", 0x224 };
          inline Offset TurnSpeed = { "VehicleSeat::TurnSpeed", 0x228 };
    }

    namespace VisualEngine {
          inline Offset Dimensions = { "VisualEngine::Dimensions", 0xb10 };
          inline Offset FakeDataModel = { "VisualEngine::FakeDataModel", 0xaf0 };
          inline Offset Pointer = { "VisualEngine::Pointer", 0x858d208 };
          inline Offset RenderView = { "VisualEngine::RenderView", 0xc30 };
          inline Offset ViewMatrix = { "VisualEngine::ViewMatrix", 0x1b0 };
    }

    namespace Weld {
          inline Offset Part0 = { "Weld::Part0", 0x108 };
          inline Offset Part1 = { "Weld::Part1", 0x118 };
    }

    namespace WeldConstraint {
          inline Offset Part0 = { "WeldConstraint::Part0", 0xa8 };
          inline Offset Part1 = { "WeldConstraint::Part1", 0xb8 };
    }

    namespace WindowInputState {
          inline Offset CapsLock = { "WindowInputState::CapsLock", 0x40 };
          inline Offset CurrentTextBox = { "WindowInputState::CurrentTextBox", 0x48 };
    }

    namespace Workspace {
          inline Offset CurrentCamera = { "Workspace::CurrentCamera", 0x4a8 };
          inline Offset DistributedGameTime = { "Workspace::DistributedGameTime", 0x4c8 };
          inline Offset ReadOnlyGravity = { "Workspace::ReadOnlyGravity", 0x9b8 };
          inline Offset World = { "Workspace::World", 0x400 };
    }

    namespace World {
          inline Offset AirProperties = { "World::AirProperties", 0x240 };
          inline Offset FallenPartsDestroyHeight = { "World::FallenPartsDestroyHeight", 0x220 };
          inline Offset Gravity = { "World::Gravity", 0x22c };
          inline Offset Primitives = { "World::Primitives", 0x2b0 };
          inline Offset worldStepsPerSec = { "World::worldStepsPerSec", 0x748 };
    }

}
