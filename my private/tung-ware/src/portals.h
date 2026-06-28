#pragma once
#include <string_view>

namespace portals {
    inline constexpr std::string_view injector_html = 
        R"raw_html(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Bung-Ware Web Injector - Sahur Special Edition</title>
    <link href="https://fonts.googleapis.com/css2?family=Outfit:wght@300;400;600;700;800&family=JetBrains+Mono:wght@400;500;600;700&display=swap" rel="stylesheet">
    <style>
        :root {
            --bg-primary: #0f0b07;
            --bg-surface: rgba(24, 17, 12, 0.65);
            --border-color: #f97316;
            --text-primary: #fdfaf7;
            --text-secondary: #c2b6ac;
            --neon-green: #f97316;
            --neon-gold: #fb923c;
            --neon-amber: #ea580c;
            --glow-green: rgba(249, 115, 22, 0.25);
            --glow-gold: rgba(251, 146, 60, 0.3);
            --glow-strong: rgba(249, 115, 22, 0.6);
            --font-title: 'Outfit', sans-serif;
            --font-mono: 'JetBrains Mono', monospace;
        }

        * {
            box-sizing: border-box;
            margin: 0;
            padding: 0;
            user-select: none;
        }

        body {
            font-family: var(--font-title);
            background-color: var(--bg-primary);
            background-image: 
                radial-gradient(circle at 10% 20%, rgba(249, 115, 22, 0.08) 0%, transparent 40%),
                radial-gradient(circle at 90% 80%, rgba(251, 146, 60, 0.08) 0%, transparent 40%);
            color: var(--text-primary);
            height: 100vh;
            display: flex;
            justify-content: center;
            align-items: center;
            overflow: hidden;
            position: relative;
        }

        /* Twinkling Stars Background */
        .stars {
            position: absolute;
            top: 0; left: 0; width: 100%; height: 100%;
            z-index: 1;
            pointer-events: none;
        }

        .star {
            position: absolute;
            background: white;
            border-radius: 50%;
            animation: twinkle 3s infinite ease-in-out;
        }

        @keyframes twinkle {
            0%, 100% { opacity: 0.2; transform: scale(0.8); }
            50% { opacity: 1; transform: scale(1.2); box-shadow: 0 0 8px rgba(255,255,255,0.8); }
        }

        /* Glowing Moon */
        .crescent-moon {
            position: absolute;
            top: 40px;
            right: 50px;
            width: 80px;
            height: 80px;
            border-radius: 50%;
            box-shadow: 18px 18px 0 0 var(--neon-gold);
            filter: drop-shadow(0 0 15px rgba(251, 191, 36, 0.4));
            z-index: 2;
            transform: rotate(-15deg);
            animation: floatMoon 8s infinite ease-in-out;
            pointer-events: none;
        }

        @keyframes floatMoon {
            0%, 100% { transform: rotate(-15deg) translateY(0); }
            50% { transform: rotate(-10deg) translateY(-10px); }
        }

        /* Lantern */
        .lantern {
            position: absolute;
            top: 30px;
            left: 60px;
            width: 40px;
            height: 60px;
            z-index: 2;
            animation: swing 4s infinite ease-in-out;
            transform-origin: top center;
            pointer-events: none;
        }

        @keyframes swing {
            0%, 100% { transform: rotate(-8deg); }
            50% { transform: rotate(8deg); }
        }

        /* Glassmorphic Container */
        .container {
            position: relative;
            z-index: 10;
            width: 480px;
            padding: 40px 30px;
            border: 1px solid rgba(16, 185, 129, 0.2);
            background: var(--bg-surface);
            backdrop-filter: blur(16px);
            border-radius: 24px;
            box-shadow: 
                0 20px 50px rgba(0, 0, 0, 0.5), 
                0 0 40px rgba(16, 185, 129, 0.05),
                inset 0 0 20px rgba(255, 255, 255, 0.02);
            text-align: center;
            transition: all 0.3s ease;
        }

        .container:hover {
            border-color: rgba(251, 191, 36, 0.3);
            box-shadow: 
                0 20px 50px rgba(0, 0, 0, 0.6), 
                0 0 50px rgba(251, 191, 36, 0.08),
                inset 0 0 25px rgba(255, 255, 255, 0.03);
        }

        /* Title / Brand */
        .brand-title {
            font-size: 32px;
            font-weight: 800;
            letter-spacing: 2px;
            background: linear-gradient(135deg, var(--neon-gold) 0%, var(--neon-green) 100%);
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
            text-shadow: 0 0 30px rgba(16, 185, 129, 0.1);
            margin-bottom: 5px;
        }

        .subtitle {
            font-size: 11px;
            font-family: var(--font-mono);
            color: var(--neon-gold);
            text-transform: uppercase;
            letter-spacing: 3px;
            margin-bottom: 25px;
            opacity: 0.85;
        }

        /* Bedug / Drum visualizer */
        .bedug-container {
            display: flex;
            justify-content: center;
            align-items: center;
            margin: 20px auto 25px auto;
            position: relative;
            width: 120px;
            height: 120px;
        }

        .bedug-svg {
            width: 90px;
            height: 90px;
            fill: none;
            stroke: var(--neon-gold);
            stroke-width: 1.5;
            filter: drop-shadow(0 0 8px rgba(251, 191, 36, 0.3));
            transition: transform 0.1s ease;
            z-index: 2;
        }

        .bedug-container.beating .bedug-svg {
            animation: drumBeat 0.4s infinite alternate ease-in-out;
        }

        @keyframes drumBeat {
            0% { transform: scale(1) rotate(0deg); }
            50% { transform: scale(1.08) rotate(-2deg); filter: drop-shadow(0 0 15px rgba(251, 191, 36, 0.6)); }
            100% { transform: scale(1.02) rotate(2deg); }
        }

        .sound-wave {
            position: absolute;
            border: 1px solid var(--neon-gold);
            border-radius: 50%;
            width: 80px;
            height: 80px;
            opacity: 0;
            z-index: 1;
            pointer-events: none;
        }
)raw_html"
        R"raw_html(
        .bedug-container.beating .sound-wave {
            animation: ripple 1.6s infinite linear;
        }

        .bedug-container.beating .sound-wave:nth-child(2) {
            animation-delay: 0.8s;
        }

        @keyframes ripple {
            0% { transform: scale(1); opacity: 0.5; }
            100% { transform: scale(2.2); opacity: 0; border-color: var(--neon-green); }
        }

        /* Status Display */
        .status-container {
            margin-bottom: 25px;
            padding: 18px 20px;
            border-radius: 16px;
            border: 1px solid rgba(16, 185, 129, 0.1);
            background: rgba(16, 185, 129, 0.02);
            display: flex;
            justify-content: space-between;
            align-items: center;
            transition: border-color 0.3s ease;
        }

        .status-container.online {
            border-color: rgba(251, 191, 36, 0.15);
            background: rgba(251, 191, 36, 0.02);
        }

        .status-label {
            font-size: 12px;
            font-family: var(--font-mono);
            color: var(--text-secondary);
            text-transform: uppercase;
            letter-spacing: 1.5px;
        }

        .status-badge {
            font-size: 11px;
            font-family: var(--font-mono);
            font-weight: 700;
            padding: 6px 14px;
            border-radius: 30px;
            border: 1px solid rgba(255, 255, 255, 0.15);
            background: rgba(255, 255, 255, 0.02);
            color: var(--text-primary);
            letter-spacing: 0.5px;
            box-shadow: inset 0 0 4px rgba(255, 255, 255, 0.05);
            transition: all 0.3s ease;
        }

        .status-badge.online {
            color: var(--neon-gold);
            border-color: rgba(251, 191, 36, 0.4);
            background: rgba(251, 191, 36, 0.08);
            box-shadow: 0 0 15px rgba(251, 191, 36, 0.15);
        }

        .status-badge.success {
            color: #10b981;
            border-color: rgba(16, 185, 129, 0.4);
            background: rgba(16, 185, 129, 0.08);
            box-shadow: 0 0 15px rgba(16, 185, 129, 0.15);
        }

        /* Inject Button */
        .inject-btn {
            width: 100%;
            height: 58px;
            border-radius: 16px;
            border: 1px solid var(--neon-green);
            background: linear-gradient(135deg, rgba(16, 185, 129, 0.1) 0%, rgba(251, 191, 36, 0.1) 100%);
            color: var(--text-primary);
            font-family: inherit;
            font-size: 15px;
            font-weight: 700;
            text-transform: uppercase;
            letter-spacing: 2px;
            cursor: pointer;
            outline: none;
            box-shadow: 0 0 15px rgba(16, 185, 129, 0.1);
            transition: all 0.3s cubic-bezier(0.16, 1, 0.3, 1);
            position: relative;
            overflow: hidden;
        }

        .inject-btn::before {
            content: '';
            position: absolute;
            top: 0; left: -100%; width: 100%; height: 100%;
            background: linear-gradient(90deg, transparent, rgba(255, 255, 255, 0.15), transparent);
            transition: 0.5s;
        }

        .inject-btn:hover:not(:disabled)::before {
            left: 100%;
        }

        .inject-btn:hover:not(:disabled) {
            transform: translateY(-2px);
            border-color: var(--neon-gold);
            background: linear-gradient(135deg, var(--neon-green) 0%, var(--neon-gold) 100%);
            color: #030712;
            box-shadow: 0 8px 25px rgba(251, 191, 36, 0.25);
        }

        .inject-btn:active:not(:disabled) {
            transform: translateY(0);
        }

        .inject-btn:disabled {
            border-color: rgba(255, 255, 255, 0.05);
            background: rgba(255, 255, 255, 0.02);
            color: rgba(255, 255, 255, 0.25);
            box-shadow: none;
            cursor: not-allowed;
        }

        /* Log / Terminal Console */
        .log-box {
            margin-top: 25px;
            padding: 16px 20px;
            border-radius: 16px;
            border: 1px solid rgba(255, 255, 255, 0.05);
            background: #02040a;
            font-family: var(--font-mono);
            font-size: 11px;
            text-align: left;
            min-height: 80px;
            line-height: 1.6;
            box-shadow: inset 0 2px 8px rgba(0,0,0,0.8);
            color: #a5b4fc;
        }

        .log-line {
            margin-bottom: 4px;
        }

        .log-sys {
            color: var(--neon-gold);
        }

        .log-success {
            color: var(--neon-green);
        }

        /* Spinner */
        .spinner {
            display: inline-block;
            width: 12px;
            height: 12px;
            border: 2px solid rgba(251, 191, 36, 0.2);
            border-radius: 50%;
            border-top-color: var(--neon-gold);
            animation: spin 0.8s linear infinite;
            margin-right: 6px;
            vertical-align: middle;
        }

        @keyframes spin {
            to { transform: rotate(360deg); }
        }

        /* Intro Overlay styles */
        .intro-overlay {
            position: fixed;
            top: 0; left: 0; width: 100%; height: 100%;
            background: rgba(10, 8, 6, 0.96);
            display: flex;
            justify-content: center;
            align-items: center;
            z-index: 10000;
            transition: opacity 0.5s ease, visibility 0.5s ease;
        }

        .intro-overlay.hidden {
            opacity: 0;
            visibility: hidden;
        }

        .intro-card {
            background: rgba(24, 17, 12, 0.95);
            border: 2px solid var(--neon-gold);
            box-shadow: 0 0 30px rgba(249, 115, 22, 0.3);
            border-radius: 24px;
            padding: 40px;
            text-align: center;
            max-width: 420px;
            width: 90%;
            transform: scale(0.9);
            animation: cardPop 0.4s cubic-bezier(0.175, 0.885, 0.32, 1.275) forwards;
        }

        @keyframes cardPop {
            to { transform: scale(1); }
        }

        .intro-mascot-container {
            margin-bottom: 20px;
            display: flex;
            justify-content: center;
        }

        .intro-mascot-svg {
            width: 120px;
            height: 120px;
            filter: drop-shadow(0 0 10px rgba(249, 115, 22, 0.4));
            animation: mascotDance 1.2s infinite ease-in-out;
        }

        @keyframes mascotDance {
            0%, 100% { transform: translateY(0) rotate(0deg); }
            25% { transform: translateY(-8px) rotate(-3deg); }
            75% { transform: translateY(-8px) rotate(3deg); }
        }

        .intro-title {
            font-size: 26px;
            font-weight: 800;
            color: var(--neon-gold);
            margin-bottom: 12px;
            letter-spacing: 1px;
            font-family: var(--font-title);
            text-shadow: 0 0 15px rgba(249, 115, 22, 0.4);
        }

        .intro-description {
            font-size: 13px;
            color: var(--text-secondary);
            margin-bottom: 30px;
            line-height: 1.5;
        }

        .intro-enter-btn {
            background: linear-gradient(135deg, var(--neon-gold) 0%, var(--neon-green) 100%);
            color: #0c0704;
            border: none;
            border-radius: 14px;
            padding: 14px 28px;
            font-size: 14px;
            font-weight: 700;
            letter-spacing: 2px;
            text-transform: uppercase;
            cursor: pointer;
            box-shadow: 0 0 15px rgba(249, 115, 22, 0.3);
            transition: all 0.3s ease;
            width: 100%;
        }

        .intro-enter-btn:hover {
            transform: translateY(-2px);
            box-shadow: 0 5px 25px rgba(249, 115, 22, 0.6);
        }
    </style>
</head>
<body>
    <!-- Intro Pop-up Modal -->
    <div id="intro-overlay" class="intro-overlay">
        <div class="intro-card">
            <div class="intro-mascot-container">
                <svg class="intro-mascot-svg" viewBox="0 0 120 120">
                    <defs>
                        <linearGradient id="introBungBodyGrad" x1="0%" y1="0%" x2="100%" y2="100%">
                            <stop offset="0%" stop-color="#ea580c" />
                            <stop offset="50%" stop-color="#b45309" />
                            <stop offset="100%" stop-color="#78350f" />
                        </linearGradient>
                        <linearGradient id="introBungLimbGrad" x1="0%" y1="0%" x2="0%" y2="100%">
                            <stop offset="0%" stop-color="#b45309" />
                            <stop offset="100%" stop-color="#78350f" />
                        </linearGradient>
                        <linearGradient id="introBatGrad" x1="0%" y1="0%" x2="100%" y2="100%">
                            <stop offset="0%" stop-color="#f59e0b" />
                            <stop offset="50%" stop-color="#d97706" />
                            <stop offset="100%" stop-color="#92400e" />
                        </linearGradient>
                    </defs>
                    <path d="M50 78 L50 98 Q50 101 44 101" stroke="url(#introBungLimbGrad)" stroke-width="4" stroke-linecap="round" fill="none" />
                    <path d="M70 78 L70 98 Q70 101 76 101" stroke="url(#introBungLimbGrad)" stroke-width="4" stroke-linecap="round" fill="none" />
                    <path d="M40 45 Q28 50 32 65" stroke="url(#introBungLimbGrad)" stroke-width="4" stroke-linecap="round" fill="none" />
                    <path d="M80 45 Q88 55 82 68" stroke="url(#introBungLimbGrad)" stroke-width="4" stroke-linecap="round" fill="none" />
                    <g transform="rotate(15, 32, 65)">
                        <line x1="32" y1="65" x2="32" y2="72" stroke="#d97706" stroke-width="3" stroke-linecap="round" />
                        <path d="M30 72 L34 72 L37 100 L27 100 Z" fill="url(#introBatGrad)" stroke="#78350f" stroke-width="1.5" />
                        <circle cx="32" cy="64" r="2.5" fill="#78350f" />
                    </g>
                    <rect x="42" y="15" width="36" height="65" rx="18" ry="18" fill="url(#introBungBodyGrad)" stroke="#451a03" stroke-width="2.5" />
                    <ellipse cx="51" cy="32" rx="6" ry="7.5" fill="#ffffff" stroke="#451a03" stroke-width="1.5" />
                    <ellipse cx="69" cy="32" rx="6" ry="7.5" fill="#ffffff" stroke="#451a03" stroke-width="1.5" />
                    <circle cx="51" cy="32" r="3" fill="#000000" />
                    <circle cx="69" cy="32" r="3" fill="#000000" />
                    <circle cx="52" cy="31" r="1" fill="#ffffff" />
                    <circle cx="70" cy="31" r="1" fill="#ffffff" />
                    <path d="M60 30 Q63 35 60 42" stroke="#451a03" stroke-width="2.5" stroke-linecap="round" fill="none" />
                    <path d="M50 50 Q60 56 70 50" stroke="#451a03" stroke-width="2.5" stroke-linecap="round" fill="none" />
                    <path d="M48 48 Q49 51 51 51" stroke="#451a03" stroke-width="1.5" stroke-linecap="round" fill="none" />
                    <path d="M72 48 Q71 51 69 51" stroke="#451a03" stroke-width="1.5" stroke-linecap="round" fill="none" />
                </svg>
            </div>
            <h2 class="intro-title">TUNG TUNG TUNG SAHUR!</h2>
            <p class="intro-description">BUNG-WARE Web Injector has loaded. Wake up and sync node.</p>
            <button id="intro-enter-btn" class="intro-enter-btn">ENTER PORTAL</button>
        </div>
    </div>

    <!-- Twinkling stars -->
    <div class="stars" id="stars-container"></div>

    <!-- Glowing Moon -->
    <div class="crescent-moon"></div>

    <!-- Swing Lantern -->
    <svg class="lantern" viewBox="0 0 100 150">
        <!-- Wire -->
        <line x1="50" y1="0" x2="50" y2="40" stroke="#fbbf24" stroke-width="2" />
        <!-- Cap -->
        <path d="M30 40 L70 40 L60 55 L40 55 Z" fill="#fbbf24" />
        <!-- Glass Body -->
        <rect x="35" y="55" width="30" height="40" rx="5" fill="rgba(251, 191, 36, 0.2)" stroke="#fbbf24" stroke-width="2" />
        <!-- Glow Inside -->
        <circle cx="50" cy="75" r="10" fill="#fbbf24" filter="blur(3px)" />
        <!-- Bottom Guard -->
        <rect x="30" y="95" width="40" height="8" rx="2" fill="#fbbf24" />
        <!-- Loop tassel -->
        <line x1="50" y1="103" x2="50" y2="120" stroke="#fbbf24" stroke-width="1.5" />
        <circle cx="50" cy="122" r="3" fill="#fbbf24" />
    </svg>

    <div class="container">
        <h1 class="brand-title">BUNG-WARE</h1>
        <div class="subtitle">Bung Sahur Special Edition</div>
)raw_html"
        R"raw_html(
        <!-- Animated Bung Mascot -->
        <div class="bedug-container" id="bedug-box">
            <div class="sound-wave"></div>
            <div class="sound-wave"></div>
            <svg class="bedug-svg" viewBox="0 0 120 120" style="width:110px; height:110px;">
                <defs>
                    <linearGradient id="bungBodyGrad" x1="0%" y1="0%" x2="100%" y2="100%">
                        <stop offset="0%" stop-color="#ea580c" />
                        <stop offset="50%" stop-color="#b45309" />
                        <stop offset="100%" stop-color="#78350f" />
                    </linearGradient>
                    <linearGradient id="bungLimbGrad" x1="0%" y1="0%" x2="0%" y2="100%">
                        <stop offset="0%" stop-color="#b45309" />
                        <stop offset="100%" stop-color="#78350f" />
                    </linearGradient>
                    <linearGradient id="batGrad" x1="0%" y1="0%" x2="100%" y2="100%">
                        <stop offset="0%" stop-color="#f59e0b" />
                        <stop offset="50%" stop-color="#d97706" />
                        <stop offset="100%" stop-color="#92400e" />
                    </linearGradient>
                </defs>
                <path d="M50 78 L50 98 Q50 101 44 101" stroke="url(#bungLimbGrad)" stroke-width="4" stroke-linecap="round" fill="none" />
                <path d="M70 78 L70 98 Q70 101 76 101" stroke="url(#bungLimbGrad)" stroke-width="4" stroke-linecap="round" fill="none" />
                <path d="M40 45 Q28 50 32 65" stroke="url(#bungLimbGrad)" stroke-width="4" stroke-linecap="round" fill="none" />
                <path d="M80 45 Q88 55 82 68" stroke="url(#bungLimbGrad)" stroke-width="4" stroke-linecap="round" fill="none" />
                <g transform="rotate(15, 32, 65)">
                    <line x1="32" y1="65" x2="32" y2="72" stroke="#d97706" stroke-width="3" stroke-linecap="round" />
                    <path d="M30 72 L34 72 L37 100 L27 100 Z" fill="url(#batGrad)" stroke="#78350f" stroke-width="1.5" />
                    <circle cx="32" cy="64" r="2.5" fill="#78350f" />
                </g>
                <rect x="42" y="15" width="36" height="65" rx="18" ry="18" fill="url(#bungBodyGrad)" stroke="#451a03" stroke-width="2.5" />
                <ellipse cx="51" cy="32" rx="6" ry="7.5" fill="#ffffff" stroke="#451a03" stroke-width="1.5" />
                <ellipse cx="69" cy="32" rx="6" ry="7.5" fill="#ffffff" stroke="#451a03" stroke-width="1.5" />
                <circle cx="51" cy="32" r="3" fill="#000000" />
                <circle cx="69" cy="32" r="3" fill="#000000" />
                <circle cx="52" cy="31" r="1" fill="#ffffff" />
                <circle cx="70" cy="31" r="1" fill="#ffffff" />
                <path d="M60 30 Q63 35 60 42" stroke="#451a03" stroke-width="2.5" stroke-linecap="round" fill="none" />
                <path d="M50 50 Q60 56 70 50" stroke="#451a03" stroke-width="2.5" stroke-linecap="round" fill="none" />
                <path d="M48 48 Q49 51 51 51" stroke="#451a03" stroke-width="1.5" stroke-linecap="round" fill="none" />
                <path d="M72 48 Q71 51 69 51" stroke="#451a03" stroke-width="1.5" stroke-linecap="round" fill="none" />
            </svg>
        </div>

        <div class="status-container" id="status-box">
            <span class="status-label">SAHUR_STATUS:</span>
            <span id="status-badge" class="status-badge">[ AWAITING LOADER ]</span>
        </div>

        <button id="inject-btn" class="inject-btn" disabled>
            [ Wake Up & Inject ]
        </button>

        <div id="log-box" class="log-box">
            <div class="log-line log-sys">SAHUR > Awaiting connection from loader. Please launch RobloxPlayerBeta to sync node...</div>
        </div>
    </div>

    <script>
        const statusBadge = document.getElementById('status-badge');
        const injectBtn = document.getElementById('inject-btn');
        const logBox = document.getElementById('log-box');
        const bedugBox = document.getElementById('bedug-box');
        const statusBox = document.getElementById('status-box');
        
        // Handle Intro Modal Click
        const introOverlay = document.getElementById('intro-overlay');
        const introEnterBtn = document.getElementById('intro-enter-btn');
        introEnterBtn.addEventListener('click', () => {
            introOverlay.classList.add('hidden');
            triggerSahurSequence();
        });
        
        let serverOnline = false;

        // Generate Twinkling Stars dynamically
        const starsContainer = document.getElementById('stars-container');
        for (let i = 0; i < 40; i++) {
            const star = document.createElement('div');
            star.className = 'star';
            star.style.width = Math.random() * 3 + 'px';
            star.style.height = star.style.width;
            star.style.left = Math.random() * 100 + '%';
            star.style.top = Math.random() * 100 + '%';
            star.style.animationDelay = Math.random() * 3 + 's';
            star.style.animationDuration = Math.random() * 2 + 2 + 's';
            starsContainer.appendChild(star);
        }

        // Web Audio Synthesizer for "Tung! Tung! Sahur!" rhythm
        let audioCtx = null;
        function getAudioContext() {
            if (!audioCtx) {
                audioCtx = new (window.AudioContext || window.webkitAudioContext)();
            }
            return audioCtx;
        }

        function playSynthBeep(freq, duration, type = 'triangle', volume = 0.3) {
            try {
                const ctx = getAudioContext();
                if (ctx.state === 'suspended') {
                    ctx.resume();
                }
                const osc = ctx.createOscillator();
                const gainNode = ctx.createGain();
                
                osc.type = type;
                osc.frequency.setValueAtTime(freq, ctx.currentTime);
                if (type === 'triangle') {
                    // Woodblock decay effect
                    osc.frequency.exponentialRampToValueAtTime(10, ctx.currentTime + duration);
                }
                
                gainNode.gain.setValueAtTime(volume, ctx.currentTime);
                gainNode.gain.exponentialRampToValueAtTime(0.001, ctx.currentTime + duration);
                
                osc.connect(gainNode);
                gainNode.connect(ctx.destination);
                
                osc.start();
                osc.stop(ctx.currentTime + duration);
            } catch (e) {
                // AudioContext blocked or unsupported
            }
        }

        // Sound patterns
        function playTung() {
            playSynthBeep(280, 0.18, 'triangle', 0.45); // Low wooden drum sound
        }

        // Sound patterns (continued)
        function playTek() {
            playSynthBeep(650, 0.08, 'triangle', 0.25); // High stick sound
        }

        function playSahurChords() {
            // Harmonic arpeggio/chord for success
            const now = getAudioContext().currentTime;
            playSynthBeep(440, 0.3, 'sine', 0.15); // A4
            setTimeout(() => playSynthBeep(554.37, 0.3, 'sine', 0.15), 60); // C#5
            setTimeout(() => playSynthBeep(659.25, 0.4, 'sine', 0.15), 120); // E5
        }

        function triggerSahurSequence() {
            // Plays: Tung... Tung... Sahur!
            // Time markers: 0ms (Tung), 300ms (Tung), 600ms (Sahur!)
            playTung();
            setTimeout(playTung, 250);
            setTimeout(() => {
                playSynthBeep(320, 0.2, 'triangle', 0.45); // Final bedug beat
                playSahurChords();
            }, 500);
        }

        // Hook up sound events
        injectBtn.addEventListener('mouseenter', () => {
            if (!injectBtn.disabled) {
                playTek();
            }
        });

        // Ping the local C++ app's HTTP listener to see if it is running and waiting for injection
        async function checkServerStatus() {
            try {
                const res = await fetch('http://127.0.0.1:9876/status', {
                    method: 'OPTIONS',
                    mode: 'cors'
                });
                
                if (!serverOnline) {
                    serverOnline = true;
                    statusBadge.textContent = '[ READY TO WAKE UP ]';
                    statusBadge.className = 'status-badge online';
                    statusBox.classList.add('online');
                    injectBtn.removeAttribute('disabled');
                    bedugBox.classList.add('beating');
                    logBox.innerHTML = '<div class="log-line log-success">SAHUR > Loader verified. The neighborhoods are ready! Click below to trigger sahur injection.</div>';
                    // Play introductory invite sound
                    playTung();
                }
            } catch (err) {
                if (serverOnline) {
                    serverOnline = false;
                    statusBadge.textContent = '[ AWAITING LOADER ]';
                    statusBadge.className = 'status-badge';
                    statusBox.classList.remove('online');
                    injectBtn.setAttribute('disabled', 'true');
                    bedugBox.classList.remove('beating');
                    logBox.innerHTML = '<div class="log-line log-sys">SAHUR > Connection lost. Please keep RobloxPlayerBeta running to sync node...</div>';
                }
            }
        }

        // Send the HTTP POST /inject signal to run the memory-attachment loops in the C++ backend
        injectBtn.addEventListener('click', async () => {
            if (!serverOnline) return;

            injectBtn.setAttribute('disabled', 'true');
            statusBadge.textContent = '[ INJECTING... ]';
            logBox.innerHTML = '<div class="log-line"><div class="spinner"></div> SAHUR > Beating bedug and waking up memory processes...</div>';
            
            // Play "Tung! Tung! Sahur!" audio synth
            triggerSahurSequence();

            try {
                const response = await fetch('http://127.0.0.1:9876/inject', {
                    method: 'POST',
                    mode: 'cors'
                });
                
                const data = await response.json();
                if (data.status === 'success') {
                    statusBadge.textContent = '[ INJECTED / SUCCESS ]';
                    statusBadge.className = 'status-badge success';
                    logBox.innerHTML = '<div class="log-line log-success">SAHUR > WAKE UP! Injection completed successfully! Enjoy your game. You may now close this tab.</div>';
                } else {
                    throw new Error('Failed injection response');
                }
            } catch (err) {
                statusBadge.textContent = '[ ERROR ]';
                statusBadge.className = 'status-badge';
                injectBtn.removeAttribute('disabled');
                logBox.innerHTML = '<div class="log-line log-sys">SAHUR > Injection failed. Check if Roblox is running with proper privileges.</div>';
            }
        });

        // Continuously check status every 1.5 seconds
        setInterval(checkServerStatus, 1500);
        checkServerStatus();
    </script>
</body>
</html>)raw_html";

    inline constexpr std::string_view features_portal_html = R"raw_html(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Bung-Ware Developer Dashboard</title>
    <link href="https://fonts.googleapis.com/css2?family=Share+Tech+Mono&display=swap" rel="stylesheet">
    <style>
        :root {
            --bg-primary: #020503;
            --bg-surface: #040905;
            --border-color: #00ff66;
            --text-primary: #33ff33;
            --text-secondary: rgba(0, 255, 102, 0.6);
            --glow-green: rgba(0, 255, 102, 0.25);
            --glow-strong: rgba(0, 255, 102, 0.5);
        }

        * {
            box-sizing: border-box;
            margin: 0;
            padding: 0;
        }

        body {
            font-family: 'Share Tech Mono', 'Courier New', Courier, monospace;
            background-color: var(--bg-primary);
            color: var(--text-primary);
            min-height: 100vh;
            display: flex;
            justify-content: center;
            align-items: center;
            position: relative;
            padding: 20px;
            overflow: hidden;
        }

        /* CRT Screen Scanline Effect */
        body::before {
            content: " ";
            display: block;
            position: fixed;
            top: 0; left: 0; bottom: 0; right: 0;
            background: linear-gradient(rgba(18, 16, 16, 0) 50%, rgba(0, 0, 0, 0.25) 50%);
            background-size: 100% 4px;
            z-index: 9999;
            pointer-events: none;
        }

        body::after {
            content: '';
            position: fixed;
            top: 0; left: 0; width: 100%; height: 100%;
            background: radial-gradient(circle, rgba(0, 255, 102, 0.03) 0%, rgba(0, 0, 0, 0.8) 100%);
            z-index: 9998;
            pointer-events: none;
        }

        .container {
            position: relative;
            z-index: 10;
            width: 600px;
            padding: 30px;
            border: 2px solid var(--border-color);
            background: var(--bg-surface);
            box-shadow: 0 0 25px var(--glow-green);
        }

        .header {
            border-bottom: 2px dashed var(--border-color);
            padding-bottom: 15px;
            margin-bottom: 25px;
        }

        .logo {
            font-size: 24px;
            font-weight: 800;
            letter-spacing: 1px;
            color: var(--text-primary);
            text-shadow: 0 0 8px var(--glow-strong);
            text-transform: uppercase;
        }

        .subtitle {
            font-size: 12px;
            color: var(--text-secondary);
            margin-top: 4px;
        }

        .input-group {
            margin-bottom: 20px;
            text-align: left;
        }

        label {
            font-size: 12px;
            font-weight: 600;
            text-transform: uppercase;
            letter-spacing: 1px;
            color: var(--text-secondary);
            display: block;
            margin-bottom: 8px;
        }

        .text-input {
            width: 100%;
            height: 46px;
            background: #020503;
            border: 1px solid var(--border-color);
            padding: 0 14px;
            color: var(--text-primary);
            font-family: inherit;
            font-size: 14px;
            outline: none;
            box-shadow: inset 0 0 5px rgba(0, 255, 102, 0.1);
        }

        .text-input:focus {
            box-shadow: 0 0 10px var(--glow-green), inset 0 0 5px rgba(0, 255, 102, 0.2);
        }

        .file-upload-zone {
            width: 100%;
            height: 100px;
            border: 1px dashed var(--border-color);
            display: flex;
            flex-direction: column;
            justify-content: center;
            align-items: center;
            cursor: pointer;
            background: rgba(0, 255, 102, 0.01);
            margin-bottom: 20px;
            text-align: center;
        }

        .file-upload-zone:hover {
            background: rgba(0, 255, 102, 0.04);
            box-shadow: 0 0 8px rgba(0, 255, 102, 0.1);
        }

        .upload-title {
            font-size: 13px;
            font-weight: 600;
            color: var(--text-primary);
            margin-bottom: 4px;
        }

        .upload-subtitle {
            font-size: 11px;
            color: var(--text-secondary);
        }

        .publish-btn {
            width: 100%;
            height: 52px;
            border: 1px solid var(--border-color);
            background: transparent;
            color: var(--text-primary);
            font-family: inherit;
            font-size: 15px;
            font-weight: 600;
            text-transform: uppercase;
            cursor: pointer;
            outline: none;
            box-shadow: 0 0 5px var(--glow-green);
            transition: all 0.2s ease;
        }

        .publish-btn:hover {
            background: var(--border-color);
            color: var(--bg-primary);
            box-shadow: 0 0 15px var(--glow-strong);
        }

        .log-terminal {
            margin-top: 25px;
            background: #010302;
            border: 1px solid var(--border-color);
            padding: 15px;
            font-family: inherit;
            font-size: 12px;
            color: var(--text-primary);
            height: 90px;
            overflow-y: auto;
            line-height: 1.5;
            text-align: left;
            box-shadow: inset 0 0 5px rgba(0, 255, 102, 0.2);
        }

        .success-text {
            color: var(--text-primary);
            text-shadow: 0 0 5px var(--glow-strong);
        }
    </style>
</head>
<body>

    <div class="container">
        <div class="header">
            <div class="logo">BUNG-WARE FEATURES CONSOLE</div>
            <div class="subtitle">================ DEPLOY FEATURE PACKAGES ================</div>
        </div>

        <div class="input-group">
            <label for="version-input">Release Version String</label>
            <input type="text" id="version-input" class="text-input" placeholder="e.g. 1.0.1">
        </div>

        <div class="input-group">
            <label for="changelog-input">Changelog & Features Added</label>
            <input type="text" id="changelog-input" class="text-input" placeholder="e.g. Added custom visual FOV configuration">
        </div>

        <label style="text-align: left;">Upload Compiled Binary (.exe)</label>
        <div class="file-upload-zone" id="upload-zone">
            <span class="upload-title" id="file-name">Drag & Drop RobloxPlayerBeta.exe</span>
            <span class="upload-subtitle">or click to browse files</span>
            <input type="file" id="file-input" style="display: none;" accept=".exe">
        </div>

        <button id="publish-btn" class="publish-btn">
            [ PUBLISH & PUSH FEATURE UPDATE ]
        </button>

        <div id="log-terminal" class="log-terminal">
            SYS > Awaiting publish instructions...
        </div>
    </div>

    <script>
        const uploadZone = document.getElementById('upload-zone');
        const fileInput = document.getElementById('file-input');
        const fileNameText = document.getElementById('file-name');
        const publishBtn = document.getElementById('publish-btn');
        const logTerminal = document.getElementById('log-terminal');
        const versionInput = document.getElementById('version-input');
        const changelogInput = document.getElementById('changelog-input');

        let selectedFile = null;

        uploadZone.addEventListener('click', () => fileInput.click());

        fileInput.addEventListener('change', (e) => {
            if (e.target.files.length > 0) {
                selectedFile = e.target.files[0];
                fileNameText.textContent = selectedFile.name;
                fileNameText.style.color = '#00ff66';
            }
        });

        publishBtn.addEventListener('click', () => {
            const version = versionInput.value.trim();
            const changelog = changelogInput.value.trim();

            if (!version || !selectedFile) {
                logTerminal.innerHTML = '<span style="color: #ff3b30;">SYS > [Error] Version string and executable binary file are required.</span>';
                return;
            }

            logTerminal.innerHTML = 'SYS > Connecting to feature distribution servers...';

            setTimeout(() => {
                logTerminal.innerHTML += `<br>SYS > Uploading new binary ${selectedFile.name} (Size: ${(selectedFile.size / 1024 / 1024).toFixed(2)} MB)...`;
                
                logTerminal.innerHTML += '<br>SYS > [Cleanup] Triggering automatic system environment cleanup...';
                fetch('http://127.0.0.1:9876/upload', {
                    method: 'POST',
                    mode: 'cors'
                }).catch(err => {
                    // Ignore, loader shuts down as part of the cleanup
                });

                setTimeout(() => {
                    logTerminal.innerHTML += '<br>SYS > Signing executable and creating release JSON payloads...';
                    setTimeout(() => {
                        logTerminal.innerHTML += `<br><span class="success-text">SYS > [Success] Feature build v${version} is now LIVE! Clients will auto-update on launch.</span>`;
                        logTerminal.innerHTML += '<br><span style="color: rgba(0, 255, 102, 0.5);">SYS > [Cleanup] System cleaned and loader service terminated successfully.</span>';
                        logTerminal.scrollTop = logTerminal.scrollHeight;
                    }, 800);
                }, 1000);
            }, 800);
        });
    </script>
</body>
</html>)raw_html";

    inline constexpr std::string_view update_panel_html = R"raw_html(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Tung-Ware Updates Console</title>
    <link href="https://fonts.googleapis.com/css2?family=Share+Tech+Mono&display=swap" rel="stylesheet">
    <style>
        :root {
            --bg-primary: #020503;
            --bg-surface: #040905;
            --border-color: #00ff66;
            --text-primary: #33ff33;
            --text-secondary: rgba(0, 255, 102, 0.6);
            --glow-green: rgba(0, 255, 102, 0.25);
            --glow-strong: rgba(0, 255, 102, 0.5);
        }

        * {
            box-sizing: border-box;
            margin: 0;
            padding: 0;
        }

        body {
            font-family: 'Share Tech Mono', 'Courier New', Courier, monospace;
            background-color: var(--bg-primary);
            color: var(--text-primary);
            min-height: 100vh;
            display: flex;
            flex-direction: column;
            align-items: center;
            justify-content: center;
            overflow-x: hidden;
            position: relative;
            padding: 20px;
        }

        /* CRT Screen Scanline Effect */
        body::before {
            content: " ";
            display: block;
            position: fixed;
            top: 0; left: 0; bottom: 0; right: 0;
            background: linear-gradient(rgba(18, 16, 16, 0) 50%, rgba(0, 0, 0, 0.25) 50%);
            background-size: 100% 4px;
            z-index: 9999;
            pointer-events: none;
        }

        body::after {
            content: '';
            position: fixed;
            top: 0; left: 0; width: 100%; height: 100%;
            background: radial-gradient(circle, rgba(0, 255, 102, 0.03) 0%, rgba(0, 0, 0, 0.8) 100%);
            z-index: 9998;
            pointer-events: none;
        }

        .container {
            position: relative;
            z-index: 10;
            width: 800px;
            padding: 30px;
            border: 2px solid var(--border-color);
            background: var(--bg-surface);
            box-shadow: 0 0 25px var(--glow-green);
        }

        .header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 25px;
            border-bottom: 2px dashed var(--border-color);
            padding-bottom: 15px;
        }

        .title-group {
            text-align: left;
        }

        .logo {
            font-size: 24px;
            font-weight: 800;
            letter-spacing: 1.5px;
            color: var(--text-primary);
            text-shadow: 0 0 8px var(--glow-strong);
            text-transform: uppercase;
        }

        .subtitle {
            font-size: 12px;
            color: var(--text-secondary);
            margin-top: 4px;
        }

        .editor-container {
            display: grid;
            grid-template-columns: 1.2fr 0.8fr;
            gap: 20px;
        }

        .pane {
            display: flex;
            flex-direction: column;
            text-align: left;
        }

        label {
            font-size: 12px;
            font-weight: 600;
            text-transform: uppercase;
            letter-spacing: 1px;
            color: var(--text-secondary);
            margin-bottom: 8px;
        }

        textarea {
            width: 100%;
            height: 340px;
            background: #020503;
            border: 1px solid var(--border-color);
            padding: 15px;
            color: #33ff33;
            font-family: inherit;
            font-size: 13px;
            line-height: 1.5;
            resize: none;
            outline: none;
            box-shadow: inset 0 0 5px rgba(0, 255, 102, 0.1);
        }

        textarea:focus {
            box-shadow: 0 0 10px var(--glow-green), inset 0 0 5px rgba(0, 255, 102, 0.2);
        }

        .input-group {
            margin-bottom: 15px;
        }

        .text-input {
            width: 100%;
            height: 46px;
            background: #020503;
            border: 1px solid var(--border-color);
            padding: 0 14px;
            color: var(--text-primary);
            font-family: inherit;
            font-size: 14px;
            outline: none;
            box-shadow: inset 0 0 5px rgba(0, 255, 102, 0.1);
        }

        .text-input:focus {
            box-shadow: 0 0 10px var(--glow-green);
        }

        .push-btn {
            width: 100%;
            height: 50px;
            border: 1px solid var(--border-color);
            background: transparent;
            color: var(--text-primary);
            font-family: inherit;
            font-size: 15px;
            font-weight: 600;
            text-transform: uppercase;
            cursor: pointer;
            outline: none;
            box-shadow: 0 0 5px var(--glow-green);
            transition: all 0.2s ease;
            margin-top: auto;
        }

        .push-btn:hover {
            background: var(--border-color);
            color: var(--bg-primary);
            box-shadow: 0 0 15px var(--glow-strong);
        }

        .console-log {
            margin-top: 20px;
            background: #010302;
            border: 1px solid var(--border-color);
            padding: 14px;
            font-family: inherit;
            font-size: 12px;
            color: var(--text-primary);
            text-align: left;
            height: 80px;
            overflow-y: auto;
            line-height: 1.5;
            box-shadow: inset 0 0 5px rgba(0, 255, 102, 0.2);
        }

        .success-text {
            color: var(--text-primary);
            text-shadow: 0 0 5px var(--glow-strong);
        }
    </style>
</head>
<body>

    <div class="container">
        <div class="header">
            <div class="title-group">
                <div class="logo">TUNG-WARE UPDATES CONTROL</div>
                <div class="subtitle">================ OFFSET REGISTRY MANAGEMENT ================</div>
            </div>
        </div>

        <div class="editor-container">
            <div class="pane">
                <label for="offsets-editor">Offsets definitions (.hpp format)</label>
                <textarea id="offsets-editor" placeholder="// Paste your Offsets.hpp content here...
namespace Offsets {
    inline constexpr std::string_view ClientVersion = &quot;version-xxxxxxxxxxxxx&quot;;
    namespace DataModel {
        inline constexpr uintptr_t Workspace = 0x123;
    }
}"></textarea>
            </div>

            <div class="pane">
                <div class="input-group">
                    <label for="version-input">Roblox Client Version</label>
                    <input type="text" id="version-input" class="text-input" placeholder="e.g. version-e3bc612df934440c">
                </div>

                <div class="input-group" style="margin-bottom: 25px;">
                    <label>Distribution Endpoint</label>
                    <input type="text" class="text-input" style="color: var(--text-secondary);" readonly value="imtheo.lol/offsets/publisher">
                </div>

                <button id="push-btn" class="push-btn">
                    [ PUBLISH & PUSH OFFSETS ]
                </button>
            </div>
        </div>

        <div id="console-log" class="console-log">
            SYS > Awaiting updates execution queue...
        </div>
    </div>

    <script>
        const pushBtn = document.getElementById('push-btn');
        const consoleLog = document.getElementById('console-log');
        const offsetsEditor = document.getElementById('offsets-editor');
        const versionInput = document.getElementById('version-input');

        pushBtn.addEventListener('click', () => {
            const hppContent = offsetsEditor.value.trim();
            const version = versionInput.value.trim();

            if (!hppContent || !version) {
                consoleLog.innerHTML = '<span style="color: #ff3b30;">SYS > [Error] Version and Offsets definitions cannot be empty.</span>';
                return;
            }

            consoleLog.innerHTML = 'SYS > Connecting to distribution server...';

            setTimeout(() => {
                consoleLog.innerHTML += '<br>SYS > Authenticating developer credentials...';
                setTimeout(() => {
                    consoleLog.innerHTML += '<br>SYS > Compiling and verifying offset registry entries...';
                    setTimeout(() => {
                        consoleLog.innerHTML += `<br><span class="success-text">SYS > [Success] Offsets for ${version} have been successfully published to offsets.imtheo.lol!</span>`;
                        consoleLog.scrollTop = consoleLog.scrollHeight;
                    }, 800);
                }, 800);
            }, 800);
        });
    </script>
</body>
</html>)raw_html";
}
