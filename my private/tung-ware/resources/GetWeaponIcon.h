#pragma once
#include <string>
#include <unordered_map>

static const std::unordered_map<std::string, const char*> robloxWeaponIcons = 
{
    
    {"AK47", "W"},           
    {"AR", "T"},             
    {"SilencerAR", "T"},     
    {"AUG", "U"},            
    
    
    {"SMG", "K"},            
    {"P90", "O"},            
    {"DrumGun", "M"},        
    
    
    {"Shotgun", "e"},        
    {"TacticalShotgun", "b"}, 
    {"Double-Barrel SG", "c"}, 
    {"Double-Barrel", "c"},   
    {"DoubleBarrel", "c"},    
    {"DB", "c"},              
    
    
    {"Glock", "D"},          
    {"Silencer", "G"},       
    {"Revolver", "J"},       
    
    
    {"LMG", "f"},            
    {"RPG", "g"},            
    {"Flamethrower", "h"},   
    
    
    {"Knife", "["},          
    {"Sword", "]"},          
    
    
    {"Grenade", "j"},        
    {"Flashbang", "i"},      
    {"Smoke", "k"},          
    {"Molotov", "l"},        
};

struct WeaponIconSize
{
    float width;
    float height;
    float offsetX;
    float offsetY;
};

static std::unordered_map<std::string, WeaponIconSize> weaponIconSizes = 
{
    
    {"AK47", {16.0f, 10.0f, -3.0f, 0.0f}},
    {"AR", {16.0f, 10.0f, -3.0f, 0.0f}},
    {"SilencerAR", {18.0f, 10.0f, -4.0f, 0.0f}},
    {"AUG", {16.0f, 10.0f, -3.0f, 0.0f}},
    
    
    {"SMG", {14.0f, 10.0f, -2.0f, 0.0f}},
    {"P90", {14.0f, 10.0f, -2.0f, 0.0f}},
    {"DrumGun", {14.0f, 10.0f, -2.0f, 0.0f}},
    
    
    {"Shotgun", {14.0f, 10.0f, 0.0f, 0.0f}},
    {"TacticalShotgun", {16.0f, 10.0f, 0.0f, 0.0f}},
    {"Double-Barrel SG", {14.0f, 10.0f, 0.0f, 0.0f}},
    {"Double-Barrel", {14.0f, 10.0f, 0.0f, 0.0f}},
    {"DoubleBarrel", {14.0f, 10.0f, 0.0f, 0.0f}},
    {"DB", {14.0f, 10.0f, 0.0f, 0.0f}},
    
    
    {"Glock", {10.0f, 10.0f, -1.0f, 0.0f}},
    {"Silencer", {12.0f, 10.0f, -1.0f, 0.0f}},
    {"Revolver", {12.0f, 10.0f, -1.0f, 0.0f}},
    
    
    {"LMG", {18.0f, 10.0f, -4.0f, 0.0f}},
    {"RPG", {18.0f, 10.0f, -4.0f, 0.0f}},
    {"Flamethrower", {18.0f, 10.0f, -4.0f, 0.0f}},
    
    
    {"Knife", {13.0f, 13.0f, -5.0f, 0.0f}},
    {"Sword", {13.0f, 13.0f, -5.0f, 0.0f}},
    
    
    {"Grenade", {10.0f, 10.0f, 0.0f, 0.0f}},
    {"Flashbang", {10.0f, 10.0f, 0.0f, 0.0f}},
    {"Smoke", {10.0f, 10.0f, 0.0f, 0.0f}},
    {"Molotov", {10.0f, 10.0f, 0.0f, 0.0f}},
};

inline const char* GetWeaponIcon(const std::string& weaponName)
{
    std::string cleanName = weaponName;

    if (!cleanName.empty() && cleanName.front() == '[' && cleanName.back() == ']') {
        cleanName = cleanName.substr(1, cleanName.length() - 2);
    }
    
    auto it = robloxWeaponIcons.find(cleanName);
    if (it != robloxWeaponIcons.end()) {
        return it->second;
    }
    return "";
}

inline WeaponIconSize GetWeaponIconSize(const std::string& weaponName)
{
    std::string cleanName = weaponName;
    
    
    if (!cleanName.empty() && cleanName.front() == '[' && cleanName.back() == ']') {
        cleanName = cleanName.substr(1, cleanName.length() - 2);
    }
    
    auto it = weaponIconSizes.find(cleanName);
    if (it != weaponIconSizes.end()) {
        return it->second;
    }
    
    
    return {12.0f, 10.0f, 0.0f, 0.0f};
}
