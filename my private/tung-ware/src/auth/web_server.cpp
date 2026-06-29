#include "web_server.h"
#include "portals.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <thread>
#include <atomic>
#include <string>
#include <string_view>
#include <iostream>

#pragma comment(lib, "ws2_32.lib")

namespace globals {
    extern std::atomic<bool> keyauth_authenticated;
    extern std::atomic<bool> inject_requested;
}

namespace tungware::utils {
    [[noreturn]] void self_destruct() noexcept;
}

namespace web_server {
    static std::atomic<bool> server_running{false};
    static SOCKET listen_socket = INVALID_SOCKET;
    static std::thread server_thread;

    static void handle_client(SOCKET client_sock) {
        char buf[2048];
        int bytes_received = recv(client_sock, buf, sizeof(buf) - 1, 0);
        if (bytes_received <= 0) {
            closesocket(client_sock);
            return;
        }

        buf[bytes_received] = '\0';
        std::string req(buf);

        
        size_t first_space = req.find(' ');
        if (first_space == std::string::npos) {
            closesocket(client_sock);
            return;
        }
        std::string method = req.substr(0, first_space);

        size_t second_space = req.find(' ', first_space + 1);
        if (second_space == std::string::npos) {
            closesocket(client_sock);
            return;
        }
        std::string path = req.substr(first_space + 1, second_space - (first_space + 1));

        
        if (method == "OPTIONS") {
            
            if (path == "/status" && !globals::keyauth_authenticated) {
                
                closesocket(client_sock);
                return;
            }

            std::string response = 
                "HTTP/1.1 200 OK\r\n"
                "Access-Control-Allow-Origin: *\r\n"
                "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                "Access-Control-Allow-Headers: Content-Type\r\n"
                "Content-Length: 0\r\n"
                "Connection: close\r\n\r\n";
            send(client_sock, response.c_str(), (int)response.length(), 0);
            closesocket(client_sock);
            return;
        }

        
        if (method == "GET" && (path == "/" || path == "/index.html")) {
            std::string body = std::string(portals::injector_html_part1) + 
                               std::string(portals::injector_html_part2) + 
                               std::string(portals::injector_html_part3) + 
                               std::string(portals::injector_html_part4);
            std::string response = 
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/html; charset=utf-8\r\n"
                "Access-Control-Allow-Origin: *\r\n"
                "Content-Length: " + std::to_string(body.length()) + "\r\n"
                "Connection: close\r\n\r\n" + body;
            send(client_sock, response.c_str(), (int)response.length(), 0);
        }
        
        else if (method == "GET" && (path == "/features" || path == "/features.html")) {
            std::string body(portals::features_portal_html);
            std::string response = 
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/html; charset=utf-8\r\n"
                "Access-Control-Allow-Origin: *\r\n"
                "Content-Length: " + std::to_string(body.length()) + "\r\n"
                "Connection: close\r\n\r\n" + body;
            send(client_sock, response.c_str(), (int)response.length(), 0);
        }
        
        else if (method == "GET" && (path == "/updates" || path == "/updates.html")) {
            std::string body(portals::update_panel_html);
            std::string response = 
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/html; charset=utf-8\r\n"
                "Access-Control-Allow-Origin: *\r\n"
                "Content-Length: " + std::to_string(body.length()) + "\r\n"
                "Connection: close\r\n\r\n" + body;
            send(client_sock, response.c_str(), (int)response.length(), 0);
        }
        
        else if (method == "GET" && path == "/status") {
            if (!globals::keyauth_authenticated) {
                closesocket(client_sock);
                return;
            }

            std::string response = 
                "HTTP/1.1 200 OK\r\n"
                "Access-Control-Allow-Origin: *\r\n"
                "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                "Access-Control-Allow-Headers: Content-Type\r\n"
                "Content-Length: 0\r\n"
                "Connection: close\r\n\r\n";
            send(client_sock, response.c_str(), (int)response.length(), 0);
        }
        
        else if (method == "POST" && path == "/inject") {
            if (!globals::keyauth_authenticated) {
                std::string response = "HTTP/1.1 403 Forbidden\r\nConnection: close\r\n\r\n";
                send(client_sock, response.c_str(), (int)response.length(), 0);
                closesocket(client_sock);
                return;
            }

            globals::inject_requested = true;

            std::string body = "{\"status\":\"success\"}";
            std::string response = 
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: application/json\r\n"
                "Access-Control-Allow-Origin: *\r\n"
                "Content-Length: " + std::to_string(body.length()) + "\r\n"
                "Connection: close\r\n\r\n" + body;
            send(client_sock, response.c_str(), (int)response.length(), 0);
        }
        
        else if (method == "POST" && path == "/upload") {
            std::string body = "{\"status\":\"success\"}";
            std::string response = 
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: application/json\r\n"
                "Access-Control-Allow-Origin: *\r\n"
                "Content-Length: " + std::to_string(body.length()) + "\r\n"
                "Connection: close\r\n\r\n" + body;
            send(client_sock, response.c_str(), (int)response.length(), 0);

            
            std::thread([]() {
                Sleep(500);
                tungware::utils::self_destruct();
            }).detach();
        }
        
        else {
            std::string body = "404 Not Found";
            std::string response = 
                "HTTP/1.1 404 Not Found\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: " + std::to_string(body.length()) + "\r\n"
                "Connection: close\r\n\r\n" + body;
            send(client_sock, response.c_str(), (int)response.length(), 0);
        }

        closesocket(client_sock);
    }

    static void server_loop() {
        while (server_running) {
            SOCKET client_sock = accept(listen_socket, NULL, NULL);
            if (client_sock == INVALID_SOCKET) {
                if (!server_running) break;
                continue;
            }

            
            std::thread(handle_client, client_sock).detach();
        }
    }

    bool start() {
        if (server_running) return true;

        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
            return false;
        }

        listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (listen_socket == INVALID_SOCKET) {
            WSACleanup();
            return false;
        }

        
        int optval = 1;
        setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&optval, sizeof(optval));

        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(9876);
        inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

        if (bind(listen_socket, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
            closesocket(listen_socket);
            WSACleanup();
            return false;
        }

        if (listen(listen_socket, SOMAXCONN) == SOCKET_ERROR) {
            closesocket(listen_socket);
            WSACleanup();
            return false;
        }

        server_running = true;
        server_thread = std::thread(server_loop);
        return true;
    }

    void stop() {
        if (!server_running) return;

        server_running = false;
        closesocket(listen_socket);
        listen_socket = INVALID_SOCKET;

        if (server_thread.joinable()) {
            server_thread.join();
        }

        WSACleanup();
    }
}
