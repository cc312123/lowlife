#pragma once
#include <string>
#include <vector>
#include <chrono>
#include "../settings.h"

namespace notifications {
    enum class NotificationType {
        Info,       
        Success,    
        Warning,    
        Error       
    };

    struct Notification {
        std::string message;
        NotificationType type;
        float lifetime;
        float current_time;
        float slide_progress;  
        float fade_progress;   
        int id;
        
        Notification(const std::string& msg, NotificationType t, float life = 6.0f)
            : message(msg), type(t), lifetime(life), current_time(0.0f), 
              slide_progress(0.0f), fade_progress(0.0f), id(0) {}
    };

    
    
    
    
    
    void add(const std::string& message, NotificationType type = NotificationType::Info, float lifetime = 3.0f);
    
    
    void render();
    void update();
}

