#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <sstream>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1024

std::mutex cout_mutex;

// Function to handle receiving messages from the server in a separate thread
void handle_server_messages(int server_socket) {
    char buffer[BUFFER_SIZE];
    while (true) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(server_socket, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) {
            std::lock_guard<std::mutex> lock(cout_mutex);
            std::cout << "Disconnected from server." << std::endl;
            close(server_socket);
            exit(0);
        }
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cout << buffer << std::endl;
    }
}

// Function to send messages to the server
void send_message(int server_socket, const std::string& message) {
    send(server_socket, message.c_str(), message.size(), 0);
}

// Helper function to check if a string starts with a given prefix
bool starts_with(const std::string& str, const std::string& prefix) {
    return str.rfind(prefix, 0) == 0;
}

int main() {
    int client_socket;
    sockaddr_in server_address{};

    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0) {
        std::cerr << "Error creating socket." << std::endl;
        return 1;
    }

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(12345);
    server_address.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(client_socket, (sockaddr*)&server_address, sizeof(server_address)) < 0) {
        std::cerr << "Error connecting to server." << std::endl;
        return 1;
    }

    std::cout << "Connected to the server." << std::endl;

    // Authentication
    std::string username, password;
    char buffer[BUFFER_SIZE];

    memset(buffer, 0, BUFFER_SIZE);
    recv(client_socket, buffer, BUFFER_SIZE, 0); // Receive the message "Enter the user name" from the server
    std::cout << buffer;
    std::getline(std::cin, username);
    send(client_socket, username.c_str(), username.size(), 0);

    memset(buffer, 0, BUFFER_SIZE);
    recv(client_socket, buffer, BUFFER_SIZE, 0); // Receive the message "Enter the password" from the server
    std::cout << buffer;
    std::getline(std::cin, password);
    send(client_socket, password.c_str(), password.size(), 0);

    memset(buffer, 0, BUFFER_SIZE);
    // Depending on whether the authentication passes or not, receive the message "Authentication Failed" or "Welcome to the server"
    recv(client_socket, buffer, BUFFER_SIZE, 0);
    std::cout << buffer << std::endl;

    if (std::string(buffer).find("Authentication failed") != std::string::npos) {
        close(client_socket);
        return 1;
    }

    // Start a background thread for receiving messages from the server
    std::thread receive_thread(handle_server_messages, client_socket);
    receive_thread.detach(); // Detach the thread so it runs in the background

    // Send messages to the server
    while (true) {
        std::string message;
        std::getline(std::cin, message);

        if (message.empty()) continue;

        // Check if message is a special command
        if (starts_with(message, "/broadcast ")) {
            send_message(client_socket, message);  // Send broadcast message
        }
        else if (starts_with(message, "/msg ")) {
            // Ensure proper format for private message
            std::stringstream ss(message.substr(5));  // Remove "/msg " prefix
            std::string target_user;
            std::string msg_content;
            
            ss >> target_user;
            std::getline(ss, msg_content);
            
            if (target_user.empty() || msg_content.empty()) {
                std::cout << "Invalid /msg format. Usage: /msg <username> <message>" << std::endl;
            } else {
                // Construct the message and send it
                std::string full_message = "/msg " + target_user + " " + msg_content;
                send_message(client_socket, full_message);
            }
        }
        else if (starts_with(message, "/group msg ")) {
            send_message(client_socket, message);  // Send group message
        }
        else if (starts_with(message, "/create group ")) {
            send_message(client_socket, message);  // Create group
        }
        else if (starts_with(message, "/join group ")) {
            send_message(client_socket, message);  // Join group
        }
        else if (starts_with(message, "/leave group ")) {
            send_message(client_socket, message);  // Leave group
        }
        else if (message == "/exit") {
            send_message(client_socket, message);  // Exit the chat
            close(client_socket);
            break;
        } else {
            // Regular message (for private or group messages)
            send_message(client_socket, message);
        }
    }

    return 0;
}
