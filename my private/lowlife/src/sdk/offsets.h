#pragma once
/* =============================================================
/*                       RbxDumperV2                            
/*               https://imtheo.lol/Offsets                     
/* -------------------------------------------------------------
/*  Dumped By       : theo (https://imtheo.lol)                 
/*  Roblox Version  : version-76173e47a79145c7
/*  Dumper Version  : 2.1.7
/*  Dumped At       : 18:55 09/06/2026 (GMT)
/*  Total Offsets   : 390
/* -------------------------------------------------------------
/*  Join the discord!                                           
/*  https://discord.gg/rbxoffsets                               
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
    inline std::string ClientVersion = "version-76173e47a79145c7";
    bool Update(const std::string& current_version);
    std::unordered_map<std::string, uintptr_t*>& GetRegistry();

    namespace AirProperties {
         inline Offset AirDensity = { "AirProperties::AirDensity", 0x18 };
         inline Offset GlobalWind = { "AirProperties::GlobalWind", 0x3c };
    }

    namespace AnimationTrack {
         inline Offset Animation = { "AnimationTrack::Animation", 0xd0 };
         inline Offset Animator = { "AnimationTrack::Animator", 0x118 };
         inline Offset IsPlaying = { "AnimationTrack::IsPlaying", 0xa10 };
         inline Offset Looped = { "AnimationTrack::Looped", 0xf5 };
         inline Offset Speed = { "AnimationTrack::Speed", 0xe4 };
         inline Offset TimePosition = { "AnimationTrack::TimePosition", 0xe8 };
    }

    namespace Animator {
         inline Offset ActiveAnimations = { "Animator::ActiveAnimations", 0x880 };
    }

    namespace Atmosphere {
         inline Offset Color = { "Atmosphere::Color", 0xd0 };
         inline Offset Decay = { "Atmosphere::Decay", 0xdc };
         inline Offset Density = { "Atmosphere::Density", 0xe8 };
         inline Offset Glare = { "Atmosphere::Glare", 0xec };
         inline Offset Haze = { "Atmosphere::Haze", 0xf0 };
         inline Offset Offset = { "Atmosphere::Offset", 0xf4 };
    }

    namespace Attachment {
         inline Offset Position = { "Attachment::Position", 0xdc };
    }

    namespace BasePart {
         inline Offset CastShadow = { "BasePart::CastShadow", 0xf5 };
         inline Offset Color3 = { "BasePart::Color3", 0x194 };
         inline Offset Locked = { "BasePart::Locked", 0xf6 };
         inline Offset Massless = { "BasePart::Massless", 0xf7 };
         inline Offset Primitive = { "BasePart::Primitive", 0x148 };
         inline Offset Reflectance = { "BasePart::Reflectance", 0xec };
         inline Offset Shape = { "BasePart::Shape", 0x1b1 };
         inline Offset Transparency = { "BasePart::Transparency", 0xf0 };
    }

    namespace Beam {
         inline Offset Attachment0 = { "Beam::Attachment0", 0x178 };
         inline Offset Attachment1 = { "Beam::Attachment1", 0x188 };
         inline Offset Brightness = { "Beam::Brightness", 0x198 };
         inline Offset CurveSize0 = { "Beam::CurveSize0", 0x19c };
         inline Offset CurveSize1 = { "Beam::CurveSize1", 0x1a0 };
         inline Offset LightEmission = { "Beam::LightEmission", 0x1a4 };
         inline Offset LightInfluence = { "Beam::LightInfluence", 0x1a8 };
         inline Offset Texture = { "Beam::Texture", 0x158 };
         inline Offset TextureLength = { "Beam::TextureLength", 0x1b4 };
         inline Offset TextureSpeed = { "Beam::TextureSpeed", 0x1bc };
         inline Offset Width0 = { "Beam::Width0", 0x1c0 };
         inline Offset Width1 = { "Beam::Width1", 0x1c4 };
         inline Offset ZOffset = { "Beam::ZOffset", 0x1c8 };
    }

    namespace BloomEffect {
         inline Offset Enabled = { "BloomEffect::Enabled", 0xc8 };
         inline Offset Intensity = { "BloomEffect::Intensity", 0xd0 };
         inline Offset Size = { "BloomEffect::Size", 0xd4 };
         inline Offset Threshold = { "BloomEffect::Threshold", 0xd8 };
    }

    namespace BlurEffect {
         inline Offset Enabled = { "BlurEffect::Enabled", 0xc8 };
         inline Offset Size = { "BlurEffect::Size", 0xd0 };
    }

    namespace ByteCode {
         inline Offset Pointer = { "ByteCode::Pointer", 0x10 };
         inline Offset Size = { "ByteCode::Size", 0x20 };
    }

    namespace Camera {
         inline Offset CameraSubject = { "Camera::CameraSubject", 0xe8 };
         inline Offset CameraType = { "Camera::CameraType", 0x158 };
         inline Offset FieldOfView = { "Camera::FieldOfView", 0x160 };
         inline Offset ImagePlaneDepth = { "Camera::ImagePlaneDepth", 0x2f0 };
         inline Offset Position = { "Camera::Position", 0x11c };
         inline Offset Rotation = { "Camera::Rotation", 0xf8 };
         inline Offset Viewport = { "Camera::Viewport", 0x2ac };
         inline Offset ViewportSize = { "Camera::ViewportSize", 0x2e8 };
    }

    namespace CharacterMesh {
         inline Offset BaseTextureId = { "CharacterMesh::BaseTextureId", 0xe0 };
         inline Offset BodyPart = { "CharacterMesh::BodyPart", 0x160 };
         inline Offset MeshId = { "CharacterMesh::MeshId", 0x110 };
         inline Offset OverlayTextureId = { "CharacterMesh::OverlayTextureId", 0x140 };
    }

    namespace ClickDetector {
         inline Offset MaxActivationDistance = { "ClickDetector::MaxActivationDistance", 0x100 };
         inline Offset MouseIcon = { "ClickDetector::MouseIcon", 0xe0 };
    }

    namespace Clothing {
         inline Offset Color3 = { "Clothing::Color3", 0x128 };
         inline Offset Template = { "Clothing::Template", 0x108 };
    }

    namespace ColorCorrectionEffect {
         inline Offset Brightness = { "ColorCorrectionEffect::Brightness", 0xdc };
         inline Offset Contrast = { "ColorCorrectionEffect::Contrast", 0xe0 };
         inline Offset Enabled = { "ColorCorrectionEffect::Enabled", 0xc8 };
         inline Offset TintColor = { "ColorCorrectionEffect::TintColor", 0xd0 };
    }

    namespace ColorGradingEffect {
         inline Offset Enabled = { "ColorGradingEffect::Enabled", 0xc8 };
         inline Offset TonemapperPreset = { "ColorGradingEffect::TonemapperPreset", 0xd0 };
    }

    namespace DataModel {
         inline Offset CreatorId = { "DataModel::CreatorId", 0x190 };
         inline Offset GameId = { "DataModel::GameId", 0x198 };
         inline Offset GameLoaded = { "DataModel::GameLoaded", 0x670 };
         inline Offset JobId = { "DataModel::JobId", 0x138 };
         inline Offset PlaceId = { "DataModel::PlaceId", 0x1a0 };
         inline Offset PlaceVersion = { "DataModel::PlaceVersion", 0x1bc };
         inline Offset PrimitiveCount = { "DataModel::PrimitiveCount", 0x4a0 };
         inline Offset ScriptContext = { "DataModel::ScriptContext", 0x440 };
         inline Offset ServerIP = { "DataModel::ServerIP", 0x658 };
         inline Offset ToRenderView1 = { "DataModel::ToRenderView1", 0x1d8 };
         inline Offset ToRenderView2 = { "DataModel::ToRenderView2", 0x8 };
         inline Offset ToRenderView3 = { "DataModel::ToRenderView3", 0x28 };
         inline Offset Workspace = { "DataModel::Workspace", 0x178 };
    }

    namespace DepthOfFieldEffect {
         inline Offset Enabled = { "DepthOfFieldEffect::Enabled", 0xc8 };
         inline Offset FarIntensity = { "DepthOfFieldEffect::FarIntensity", 0xd0 };
         inline Offset FocusDistance = { "DepthOfFieldEffect::FocusDistance", 0xd4 };
         inline Offset InFocusRadius = { "DepthOfFieldEffect::InFocusRadius", 0xd8 };
         inline Offset NearIntensity = { "DepthOfFieldEffect::NearIntensity", 0xdc };
    }

    namespace DragDetector {
         inline Offset ActivatedCursorIcon = { "DragDetector::ActivatedCursorIcon", 0x1d8 };
         inline Offset CursorIcon = { "DragDetector::CursorIcon", 0xe0 };
         inline Offset MaxActivationDistance = { "DragDetector::MaxActivationDistance", 0x100 };
         inline Offset MaxDragAngle = { "DragDetector::MaxDragAngle", 0x2c0 };
         inline Offset MaxDragTranslation = { "DragDetector::MaxDragTranslation", 0x284 };
         inline Offset MaxForce = { "DragDetector::MaxForce", 0x2c4 };
         inline Offset MaxTorque = { "DragDetector::MaxTorque", 0x2c8 };
         inline Offset MinDragAngle = { "DragDetector::MinDragAngle", 0x2cc };
         inline Offset MinDragTranslation = { "DragDetector::MinDragTranslation", 0x290 };
         inline Offset ReferenceInstance = { "DragDetector::ReferenceInstance", 0x208 };
         inline Offset Responsiveness = { "DragDetector::Responsiveness", 0x2d8 };
    }

    namespace FakeDataModel {
         inline Offset Pointer = { "FakeDataModel::Pointer", 0x7a39ad8 };
         inline Offset RealDataModel = { "FakeDataModel::RealDataModel", 0x1d8 };
    }

    namespace GuiBase2D {
         inline Offset AbsolutePosition = { "GuiBase2D::AbsolutePosition", 0x110 };
         inline Offset AbsoluteRotation = { "GuiBase2D::AbsoluteRotation", 0x188 };
         inline Offset AbsoluteSize = { "GuiBase2D::AbsoluteSize", 0x118 };
    }

    namespace GuiObject {
         inline Offset BackgroundColor3 = { "GuiObject::BackgroundColor3", 0x540 };
         inline Offset BackgroundTransparency = { "GuiObject::BackgroundTransparency", 0x54c };
         inline Offset BorderColor3 = { "GuiObject::BorderColor3", 0x54c };
         inline Offset Image = { "GuiObject::Image", 0x988 };
         inline Offset LayoutOrder = { "GuiObject::LayoutOrder", 0x580 };
         inline Offset Position = { "GuiObject::Position", 0x510 };
         inline Offset RichText = { "GuiObject::RichText", 0xb50 };
         inline Offset Rotation = { "GuiObject::Rotation", 0x188 };
         inline Offset ScreenGui_Enabled = { "GuiObject::ScreenGui_Enabled", 0x4c4 };
         inline Offset Size = { "GuiObject::Size", 0x530 };
         inline Offset Text = { "GuiObject::Text", 0xda0 };
         inline Offset TextColor3 = { "GuiObject::TextColor3", 0xe50 };
         inline Offset Visible = { "GuiObject::Visible", 0x5ad };
         inline Offset ZIndex = { "GuiObject::ZIndex", 0x19b };
    }

    namespace Humanoid {
         inline Offset AutoJumpEnabled = { "Humanoid::AutoJumpEnabled", 0x1e0 };
         inline Offset AutoRotate = { "Humanoid::AutoRotate", 0x1e1 };
         inline Offset AutomaticScalingEnabled = { "Humanoid::AutomaticScalingEnabled", 0x1e2 };
         inline Offset BreakJointsOnDeath = { "Humanoid::BreakJointsOnDeath", 0x1e3 };
         inline Offset CameraOffset = { "Humanoid::CameraOffset", 0x140 };
         inline Offset DisplayDistanceType = { "Humanoid::DisplayDistanceType", 0x18c };
         inline Offset DisplayName = { "Humanoid::DisplayName", 0xd0 };
         inline Offset EvaluateStateMachine = { "Humanoid::EvaluateStateMachine", 0x1e4 };
         inline Offset FloorMaterial = { "Humanoid::FloorMaterial", 0x190 };
         inline Offset Health = { "Humanoid::Health", 0x194 };
         inline Offset HealthDisplayDistance = { "Humanoid::HealthDisplayDistance", 0x198 };
         inline Offset HealthDisplayType = { "Humanoid::HealthDisplayType", 0x19c };
         inline Offset HipHeight = { "Humanoid::HipHeight", 0x1a0 };
         inline Offset HumanoidRootPart = { "Humanoid::HumanoidRootPart", 0x480 };
         inline Offset HumanoidState = { "Humanoid::HumanoidState", 0x8a0 };
         inline Offset HumanoidStateID = { "Humanoid::HumanoidStateID", 0x20 };
         inline Offset IsWalking = { "Humanoid::IsWalking", 0x91f };
         inline Offset Jump = { "Humanoid::Jump", 0x1e6 };
         inline Offset JumpHeight = { "Humanoid::JumpHeight", 0x1ac };
         inline Offset JumpPower = { "Humanoid::JumpPower", 0x1b0 };
         inline Offset MaxHealth = { "Humanoid::MaxHealth", 0x1b4 };
         inline Offset MaxSlopeAngle = { "Humanoid::MaxSlopeAngle", 0x1b8 };
         inline Offset MoveDirection = { "Humanoid::MoveDirection", 0x158 };
         inline Offset MoveToPart = { "Humanoid::MoveToPart", 0x130 };
         inline Offset MoveToPoint = { "Humanoid::MoveToPoint", 0x17c };
         inline Offset NameDisplayDistance = { "Humanoid::NameDisplayDistance", 0x1bc };
         inline Offset NameOcclusion = { "Humanoid::NameOcclusion", 0x1c0 };
         inline Offset PlatformStand = { "Humanoid::PlatformStand", 0x1e8 };
         inline Offset PlatformStatePointer = { "Humanoid::PlatformStatePointer", 0x0 };
         inline Offset RequiresNeck = { "Humanoid::RequiresNeck", 0x1e9 };
         inline Offset RigType = { "Humanoid::RigType", 0x1cc };
         inline Offset SeatPart = { "Humanoid::SeatPart", 0x120 };
         inline Offset Sit = { "Humanoid::Sit", 0x1e9 };
         inline Offset TargetPoint = { "Humanoid::TargetPoint", 0x164 };
         inline Offset UseJumpPower = { "Humanoid::UseJumpPower", 0x1ec };
         inline Offset WalkTimer = { "Humanoid::WalkTimer", 0x410 };
         inline Offset Walkspeed = { "Humanoid::Walkspeed", 0x1dc };
         inline Offset WalkspeedCheck = { "Humanoid::WalkspeedCheck", 0x3c4 };
    }

    namespace Instance {
         inline Offset AttributeContainer = { "Instance::AttributeContainer", 0x48 };
         inline Offset AttributeList = { "Instance::AttributeList", 0x18 };
         inline Offset AttributeToNext = { "Instance::AttributeToNext", 0x58 };
         inline Offset AttributeToValue = { "Instance::AttributeToValue", 0x18 };
         inline Offset ChildrenEnd = { "Instance::ChildrenEnd", 0x8 };
         inline Offset ChildrenStart = { "Instance::ChildrenStart", 0x78 };
         inline Offset ClassBase = { "Instance::ClassBase", 0xd50 };
         inline Offset ClassDescriptor = { "Instance::ClassDescriptor", 0x18 };
         inline Offset ClassName = { "Instance::ClassName", 0x8 };
         inline Offset Name = { "Instance::Name", 0xb0 };
         inline Offset Parent = { "Instance::Parent", 0x70 };
         inline Offset This = { "Instance::This", 0x8 };
    }

    namespace Lighting {
         inline Offset Ambient = { "Lighting::Ambient", 0xe0 };
         inline Offset Brightness = { "Lighting::Brightness", 0x128 };
         inline Offset ClockTime = { "Lighting::ClockTime", 0x1c0 };
         inline Offset ColorShift_Bottom = { "Lighting::ColorShift_Bottom", 0xf8 };
         inline Offset ColorShift_Top = { "Lighting::ColorShift_Top", 0xec };
         inline Offset EnvironmentDiffuseScale = { "Lighting::EnvironmentDiffuseScale", 0x12c };
         inline Offset EnvironmentSpecularScale = { "Lighting::EnvironmentSpecularScale", 0x130 };
         inline Offset ExposureCompensation = { "Lighting::ExposureCompensation", 0x134 };
         inline Offset FogColor = { "Lighting::FogColor", 0x104 };
         inline Offset FogEnd = { "Lighting::FogEnd", 0x13c };
         inline Offset FogStart = { "Lighting::FogStart", 0x140 };
         inline Offset GeographicLatitude = { "Lighting::GeographicLatitude", 0x198 };
         inline Offset GlobalShadows = { "Lighting::GlobalShadows", 0x150 };
         inline Offset GradientBottom = { "Lighting::GradientBottom", 0x19c };
         inline Offset GradientTop = { "Lighting::GradientTop", 0x158 };
         inline Offset LightColor = { "Lighting::LightColor", 0x164 };
         inline Offset LightDirection = { "Lighting::LightDirection", 0x170 };
         inline Offset MoonPosition = { "Lighting::MoonPosition", 0x18c };
         inline Offset OutdoorAmbient = { "Lighting::OutdoorAmbient", 0x110 };
         inline Offset Sky = { "Lighting::Sky", 0x1e0 };
         inline Offset Source = { "Lighting::Source", 0x17c };
         inline Offset SunPosition = { "Lighting::SunPosition", 0x180 };
    }

    namespace LocalScript {
         inline Offset ByteCode = { "LocalScript::ByteCode", 0x1a8 };
         inline Offset GUID = { "LocalScript::GUID", 0xe8 };
         inline Offset Hash = { "LocalScript::Hash", 0x1b8 };
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

    namespace MeshContentProvider {
         inline Offset AssetID = { "MeshContentProvider::AssetID", 0x10 };
         inline Offset Cache = { "MeshContentProvider::Cache", 0xe8 };
         inline Offset LRUCache = { "MeshContentProvider::LRUCache", 0x20 };
         inline Offset MeshData = { "MeshContentProvider::MeshData", 0x40 };
         inline Offset ToMeshData = { "MeshContentProvider::ToMeshData", 0x40 };
    }

    namespace MeshData {
         inline Offset FaceEnd = { "MeshData::FaceEnd", 0x38 };
         inline Offset FaceStart = { "MeshData::FaceStart", 0x30 };
         inline Offset VertexEnd = { "MeshData::VertexEnd", 0x8 };
         inline Offset VertexStart = { "MeshData::VertexStart", 0x0 };
    }

    namespace MeshPart {
         inline Offset MeshId = { "MeshPart::MeshId", 0x2f8 };
         inline Offset Texture = { "MeshPart::Texture", 0x328 };
    }

    namespace Misc {
         inline Offset Adornee = { "Misc::Adornee", 0x108 };
         inline Offset AnimationId = { "Misc::AnimationId", 0xd8 };
         inline Offset StringLength = { "Misc::StringLength", 0x10 };
         inline Offset Value = { "Misc::Value", 0xd0 };
    }

    namespace Model {
         inline Offset PrimaryPart = { "Model::PrimaryPart", 0x278 };
         inline Offset Scale = { "Model::Scale", 0x164 };
    }

    namespace ModuleScript {
         inline Offset ByteCode = { "ModuleScript::ByteCode", 0x150 };
         inline Offset GUID = { "ModuleScript::GUID", 0xe8 };
         inline Offset Hash = { "ModuleScript::Hash", 0x160 };
         inline Offset IsCoreScript = { "ModuleScript::IsCoreScript", 0x0 };
    }

    namespace MouseService {
         inline Offset InputObject = { "MouseService::InputObject", 0x108 };
         inline Offset InputObject2 = { "MouseService::InputObject2", 0x118 };
         inline Offset MousePosition = { "MouseService::MousePosition", 0xec };
         inline Offset SensitivityPointer = { "MouseService::SensitivityPointer", 0x307 };
    }

    namespace ParticleEmitter {
         inline Offset Acceleration = { "ParticleEmitter::Acceleration", 0x1f8 };
         inline Offset Brightness = { "ParticleEmitter::Brightness", 0x234 };
         inline Offset Drag = { "ParticleEmitter::Drag", 0x238 };
         inline Offset Lifetime = { "ParticleEmitter::Lifetime", 0x20c };
         inline Offset LightEmission = { "ParticleEmitter::LightEmission", 0x250 };
         inline Offset LightInfluence = { "ParticleEmitter::LightInfluence", 0x254 };
         inline Offset Rate = { "ParticleEmitter::Rate", 0x260 };
         inline Offset RotSpeed = { "ParticleEmitter::RotSpeed", 0x214 };
         inline Offset Rotation = { "ParticleEmitter::Rotation", 0x21c };
         inline Offset Speed = { "ParticleEmitter::Speed", 0x224 };
         inline Offset SpreadAngle = { "ParticleEmitter::SpreadAngle", 0x22c };
         inline Offset Texture = { "ParticleEmitter::Texture", 0x1d8 };
         inline Offset TimeScale = { "ParticleEmitter::TimeScale", 0x274 };
         inline Offset VelocityInheritance = { "ParticleEmitter::VelocityInheritance", 0x278 };
         inline Offset ZOffset = { "ParticleEmitter::ZOffset", 0x27c };
    }

    namespace Player {
         inline Offset AccountAge = { "Player::AccountAge", 0x34c };
         inline Offset CameraMode = { "Player::CameraMode", 0x358 };
         inline Offset DisplayName = { "Player::DisplayName", 0x150 };
         inline Offset HealthDisplayDistance = { "Player::HealthDisplayDistance", 0x378 };
         inline Offset LocalPlayer = { "Player::LocalPlayer", 0x138 };
         inline Offset LocaleId = { "Player::LocaleId", 0x130 };
         inline Offset MaxZoomDistance = { "Player::MaxZoomDistance", 0x349 };
         inline Offset MinZoomDistance = { "Player::MinZoomDistance", 0x354 };
         inline Offset ModelInstance = { "Player::ModelInstance", 0x3c8 };
         inline Offset Mouse = { "Player::Mouse", 0x1180 };
         inline Offset NameDisplayDistance = { "Player::NameDisplayDistance", 0x388 };
         inline Offset Team = { "Player::Team", 0x2d0 };
         inline Offset TeamColor = { "Player::TeamColor", 0x394 };
         inline Offset UserId = { "Player::UserId", 0x2f8 };
    }

    namespace PlayerConfigurer {
         inline Offset Pointer = { "PlayerConfigurer::Pointer", 0x0 };
    }

    namespace PlayerMouse {
         inline Offset Icon = { "PlayerMouse::Icon", 0xe0 };
         inline Offset Workspace = { "PlayerMouse::Workspace", 0x168 };
    }

    namespace Primitive {
         inline Offset AssemblyAngularVelocity = { "Primitive::AssemblyAngularVelocity", 0x104 };
         inline Offset AssemblyLinearVelocity = { "Primitive::AssemblyLinearVelocity", 0xf8 };
         inline Offset Flags = { "Primitive::Flags", 0x1b6 };
         inline Offset Material = { "Primitive::Material", 0x0 };
         inline Offset Owner = { "Primitive::Owner", 0x200 };
         inline Offset Position = { "Primitive::Position", 0xec };
         inline Offset Rotation = { "Primitive::Rotation", 0xc8 };
         inline Offset Size = { "Primitive::Size", 0x1b8 };
         inline Offset Validate = { "Primitive::Validate", 0x6 };
    }

    namespace PrimitiveFlags {
         inline Offset Anchored = { "PrimitiveFlags::Anchored", 0x2 };
         inline Offset CanCollide = { "PrimitiveFlags::CanCollide", 0x8 };
         inline Offset CanQuery = { "PrimitiveFlags::CanQuery", 0x20 };
         inline Offset CanTouch = { "PrimitiveFlags::CanTouch", 0x10 };
    }

    namespace ProximityPrompt {
         inline Offset ActionText = { "ProximityPrompt::ActionText", 0xc8 };
         inline Offset Enabled = { "ProximityPrompt::Enabled", 0x14e };
         inline Offset GamepadKeyCode = { "ProximityPrompt::GamepadKeyCode", 0x134 };
         inline Offset HoldDuration = { "ProximityPrompt::HoldDuration", 0x138 };
         inline Offset KeyCode = { "ProximityPrompt::KeyCode", 0x13c };
         inline Offset MaxActivationDistance = { "ProximityPrompt::MaxActivationDistance", 0x140 };
         inline Offset ObjectText = { "ProximityPrompt::ObjectText", 0xe8 };
         inline Offset RequiresLineOfSight = { "ProximityPrompt::RequiresLineOfSight", 0x14f };
    }

    namespace RenderJob {
         inline Offset FakeDataModel = { "RenderJob::FakeDataModel", 0x38 };
         inline Offset RealDataModel = { "RenderJob::RealDataModel", 0x1c8 };
         inline Offset RenderView = { "RenderJob::RenderView", 0x1d0 };
    }

    namespace RenderView {
         inline Offset DeviceD3D11 = { "RenderView::DeviceD3D11", 0x8 };
         inline Offset LightingValid = { "RenderView::LightingValid", 0x150 };
         inline Offset SkyValid = { "RenderView::SkyValid", 0x28d };
         inline Offset VisualEngine = { "RenderView::VisualEngine", 0x10 };
    }

    namespace RunService {
         inline Offset HeartbeatFPS = { "RunService::HeartbeatFPS", 0xb8 };
         inline Offset HeartbeatTask = { "RunService::HeartbeatTask", 0xf8 };
    }

    namespace Script {
         inline Offset ByteCode = { "Script::ByteCode", 0x1a8 };
         inline Offset GUID = { "Script::GUID", 0xe8 };
         inline Offset Hash = { "Script::Hash", 0x1b8 };
    }

    namespace ScriptContext {
         inline Offset RequireBypass = { "ScriptContext::RequireBypass", 0x0 };
    }

    namespace Seat {
         inline Offset Occupant = { "Seat::Occupant", 0x218 };
    }

    namespace Sky {
         inline Offset MoonAngularSize = { "Sky::MoonAngularSize", 0x25c };
         inline Offset MoonTextureId = { "Sky::MoonTextureId", 0xe0 };
         inline Offset SkyboxBk = { "Sky::SkyboxBk", 0x110 };
         inline Offset SkyboxDn = { "Sky::SkyboxDn", 0x140 };
         inline Offset SkyboxFt = { "Sky::SkyboxFt", 0x170 };
         inline Offset SkyboxLf = { "Sky::SkyboxLf", 0x1a0 };
         inline Offset SkyboxOrientation = { "Sky::SkyboxOrientation", 0x250 };
         inline Offset SkyboxRt = { "Sky::SkyboxRt", 0x1d0 };
         inline Offset SkyboxUp = { "Sky::SkyboxUp", 0x200 };
         inline Offset StarCount = { "Sky::StarCount", 0x260 };
         inline Offset SunAngularSize = { "Sky::SunAngularSize", 0x254 };
         inline Offset SunTextureId = { "Sky::SunTextureId", 0x230 };
    }

    namespace Sound {
         inline Offset Looped = { "Sound::Looped", 0x155 };
         inline Offset PlaybackSpeed = { "Sound::PlaybackSpeed", 0x134 };
         inline Offset RollOffMaxDistance = { "Sound::RollOffMaxDistance", 0x138 };
         inline Offset RollOffMinDistance = { "Sound::RollOffMinDistance", 0x13c };
         inline Offset SoundGroup = { "Sound::SoundGroup", 0x100 };
         inline Offset SoundId = { "Sound::SoundId", 0xe0 };
         inline Offset Volume = { "Sound::Volume", 0x148 };
    }

    namespace SpawnLocation {
         inline Offset AllowTeamChangeOnTouch = { "SpawnLocation::AllowTeamChangeOnTouch", 0x3d };
         inline Offset Enabled = { "SpawnLocation::Enabled", 0x1f1 };
         inline Offset ForcefieldDuration = { "SpawnLocation::ForcefieldDuration", 0x1e8 };
         inline Offset Neutral = { "SpawnLocation::Neutral", 0x1f2 };
         inline Offset TeamColor = { "SpawnLocation::TeamColor", 0x1ec };
    }

    namespace SpecialMesh {
         inline Offset MeshId = { "SpecialMesh::MeshId", 0x110 };
         inline Offset Scale = { "SpecialMesh::Scale", 0xdc };
    }

    namespace StatsItem {
         inline Offset Value = { "StatsItem::Value", 0xc8 };
    }

    namespace SunRaysEffect {
         inline Offset Enabled = { "SunRaysEffect::Enabled", 0xc8 };
         inline Offset Intensity = { "SunRaysEffect::Intensity", 0xd0 };
         inline Offset Spread = { "SunRaysEffect::Spread", 0xd4 };
    }

    namespace SurfaceAppearance {
         inline Offset AlphaMode = { "SurfaceAppearance::AlphaMode", 0x2a0 };
         inline Offset Color = { "SurfaceAppearance::Color", 0x288 };
         inline Offset ColorMap = { "SurfaceAppearance::ColorMap", 0xe0 };
         inline Offset EmissiveMaskContent = { "SurfaceAppearance::EmissiveMaskContent", 0x110 };
         inline Offset EmissiveStrength = { "SurfaceAppearance::EmissiveStrength", 0x2a4 };
         inline Offset EmissiveTint = { "SurfaceAppearance::EmissiveTint", 0x294 };
         inline Offset MetalnessMap = { "SurfaceAppearance::MetalnessMap", 0x140 };
         inline Offset NormalMap = { "SurfaceAppearance::NormalMap", 0x170 };
         inline Offset RoughnessMap = { "SurfaceAppearance::RoughnessMap", 0x1a0 };
    }

    namespace TaskScheduler {
         inline Offset JobEnd = { "TaskScheduler::JobEnd", 0xd0 };
         inline Offset JobName = { "TaskScheduler::JobName", 0x18 };
         inline Offset JobStart = { "TaskScheduler::JobStart", 0xc8 };
         inline Offset MaxFPS = { "TaskScheduler::MaxFPS", 0xb0 };
         inline Offset Pointer = { "TaskScheduler::Pointer", 0x7fcb088 };
    }

    namespace Team {
         inline Offset BrickColor = { "Team::BrickColor", 0xd0 };
    }

    namespace Terrain {
         inline Offset GrassLength = { "Terrain::GrassLength", 0x1f0 };
         inline Offset MaterialColors = { "Terrain::MaterialColors", 0x4a0 };
         inline Offset WaterColor = { "Terrain::WaterColor", 0x1e0 };
         inline Offset WaterReflectance = { "Terrain::WaterReflectance", 0x1f8 };
         inline Offset WaterTransparency = { "Terrain::WaterTransparency", 0x1fc };
         inline Offset WaterWaveSize = { "Terrain::WaterWaveSize", 0x200 };
         inline Offset WaterWaveSpeed = { "Terrain::WaterWaveSpeed", 0x204 };
    }

    namespace Textures {
         inline Offset Decal_Texture = { "Textures::Decal_Texture", 0x198 };
         inline Offset Texture_Texture = { "Textures::Texture_Texture", 0x198 };
    }

    namespace Tool {
         inline Offset CanBeDropped = { "Tool::CanBeDropped", 0x4c8 };
         inline Offset Enabled = { "Tool::Enabled", 0x4c9 };
         inline Offset Grip = { "Tool::Grip", 0x4bc };
         inline Offset ManualActivationOnly = { "Tool::ManualActivationOnly", 0x4ca };
         inline Offset RequiresHandle = { "Tool::RequiresHandle", 0x4cb };
         inline Offset TextureId = { "Tool::TextureId", 0x370 };
         inline Offset Tooltip = { "Tool::Tooltip", 0x478 };
    }

    namespace UnionOperation {
         inline Offset AssetId = { "UnionOperation::AssetId", 0x2f0 };
    }

    namespace UserInputService {
         inline Offset WindowInputState = { "UserInputService::WindowInputState", 0x2d8 };
    }

    namespace VehicleSeat {
         inline Offset MaxSpeed = { "VehicleSeat::MaxSpeed", 0x230 };
         inline Offset SteerFloat = { "VehicleSeat::SteerFloat", 0x238 };
         inline Offset ThrottleFloat = { "VehicleSeat::ThrottleFloat", 0x240 };
         inline Offset Torque = { "VehicleSeat::Torque", 0x244 };
         inline Offset TurnSpeed = { "VehicleSeat::TurnSpeed", 0x248 };
    }

    namespace VisualEngine {
         inline Offset Dimensions = { "VisualEngine::Dimensions", 0xab0 };
         inline Offset FakeDataModel = { "VisualEngine::FakeDataModel", 0xa90 };
         inline Offset Pointer = { "VisualEngine::Pointer", 0x8158b80 };
         inline Offset RenderView = { "VisualEngine::RenderView", 0xbb0 };
         inline Offset ViewMatrix = { "VisualEngine::ViewMatrix", 0x150 };
    }

    namespace Weld {
         inline Offset Part0 = { "Weld::Part0", 0x130 };
         inline Offset Part1 = { "Weld::Part1", 0x140 };
    }

    namespace WeldConstraint {
         inline Offset Part0 = { "WeldConstraint::Part0", 0xd0 };
         inline Offset Part1 = { "WeldConstraint::Part1", 0xe0 };
    }

    namespace WindowInputState {
         inline Offset CapsLock = { "WindowInputState::CapsLock", 0x40 };
         inline Offset CurrentTextBox = { "WindowInputState::CurrentTextBox", 0x48 };
    }

    namespace Workspace {
         inline Offset CurrentCamera = { "Workspace::CurrentCamera", 0x4b0 };
         inline Offset DistributedGameTime = { "Workspace::DistributedGameTime", 0x4d0 };
         inline Offset ReadOnlyGravity = { "Workspace::ReadOnlyGravity", 0x9f0 };
         inline Offset World = { "Workspace::World", 0x408 };
    }

    namespace World {
         inline Offset AirProperties = { "World::AirProperties", 0x218 };
         inline Offset FallenPartsDestroyHeight = { "World::FallenPartsDestroyHeight", 0x208 };
         inline Offset Gravity = { "World::Gravity", 0x210 };
         inline Offset Primitives = { "World::Primitives", 0x280 };
         inline Offset worldStepsPerSec = { "World::worldStepsPerSec", 0x678 };
    }

}
