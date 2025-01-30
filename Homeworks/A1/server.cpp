#include <iostream>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <string>
#include <sstream>
#include <cstring>
#include <netinet/in.h>
#include <unistd.h>

std::unordered_map<int, std::string> clients; // client socket -> username
std::unordered_map<std::string, std::string> users; // username -> password
std::unordered_map<std::string, std::unordered_set<int>> groups; // group -> members (client sockets)
std::mutex clients_mutex; // Mutex to protect the clients map

// Function to load users from users.txt for authentication
void load_users() {
    users["alice"] = "password123"; // Sample user
    users["bob"] = "qwerty456";  // Sample user
    // Add more users as needed
}

// Function to send message to all clients (broadcast)
void broadcast(const std::string& message, int sender_socket) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    for (const auto& client : clients) {
        if (client.first != sender_socket) {
            send(client.first, message.c_str(), message.length(), 0);
        }
    }
}

// Function to send a private message to a client
void private_message(int sender_socket, const std::string& username, const std::string& message) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    for (const auto& client : clients) {
        if (client.second == username) {
            send(client.first, message.c_str(), message.length(), 0);
            break;
        }
    }
}

// Function to send a message to a group
void group_message(int sender_socket, const std::string& group_name, const std::string& message) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    if (groups.find(group_name) != groups.end()) {
        for (int member_socket : groups[group_name]) {
            if (member_socket != sender_socket) {
                send(member_socket, message.c_str(), message.length(), 0);
            }
        }
    }
}

// Function to handle commands from clients
void handle_commands(int client_socket, const std::string& message) {
    std::istringstream iss(message);
    std::string command;
    iss >> command;

    if (command == "/msg") {
        std::string recipient, msg;
        iss >> recipient;
        std::getline(iss, msg);
        private_message(client_socket, recipient, msg);
    }
    else if (command == "/broadcast") {
        std::string msg;
        std::getline(iss, msg);
        broadcast(msg, client_socket);
    }
    else if (command == "/create_group") {
        std::string group_name;
        iss >> group_name;
        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            groups[group_name].insert(client_socket);
        }
        std::string response = "Group " + group_name + " created.";
        send(client_socket, response.c_str(), response.length(), 0);
    }
    else if (command == "/join_group") {
        std::string group_name;
        iss >> group_name;
        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            groups[group_name].insert(client_socket);
        }
        std::string response = "You joined the group " + group_name + ".";
        send(client_socket, response.c_str(), response.length(), 0);
    }
    else if (command == "/group_msg") {
        std::string group_name, msg;
        iss >> group_name;
        std::getline(iss, msg);
        group_message(client_socket, group_name, msg);
    }
}

// Function to authenticate client
bool authenticate_client(int client_socket) {
    send(client_socket, "Enter your username: ", 21, 0);
    char username[256];
    memset(username, 0, sizeof(username));
    recv(client_socket, username, sizeof(username) - 1, 0);  // Ensuring null termination

    send(client_socket, "Enter your password: ", 21, 0);
    char password[256];
    memset(password, 0, sizeof(password));
    recv(client_socket, password, sizeof(password) - 1, 0);  // Ensuring null termination

    std::string username_str(username);
    std::string password_str(password);

    // Remove any leading/trailing spaces from the received username and password
    username_str = username_str.substr(0, username_str.find_last_not_of(" \n\r\t") + 1);
    password_str = password_str.substr(0, password_str.find_last_not_of(" \n\r\t") + 1);

    if (users.find(username_str) != users.end() && users[username_str] == password_str) {
        std::string welcome_msg = "Welcome to the chat server!";
        send(client_socket, welcome_msg.c_str(), welcome_msg.length(), 0);
        std::lock_guard<std::mutex> lock(clients_mutex);
        clients[client_socket] = username_str;
        broadcast(username_str + " has joined the chat.\n", client_socket);
        return true;
    }
    else {
        send(client_socket, "Authentication failed.\n", 23, 0);
        return false;
    }
}

// Function to handle client connection
void handle_client(int client_socket) {
    if (!authenticate_client(client_socket)) {
        close(client_socket);
        return;
    }

    char buffer[1024];
    while (true) {
        memset(buffer, 0, sizeof(buffer));
        int bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
        if (bytes_received <= 0) {
            break;
        }

        std::string message(buffer);
        handle_commands(client_socket, message);
    }

    // Handle client disconnection
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        broadcast(clients[client_socket] + " has left the chat.\n", client_socket);
        clients.erase(client_socket);
    }

    close(client_socket);
}

// Main function to start server
int main() {
    load_users();

    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        std::cerr << "Failed to create server socket." << std::endl;
        return 1;
    }

    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(12345);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_socket, (sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        std::cerr << "Failed to bind socket." << std::endl;
        return 1;
    }

    if (listen(server_socket, 5) == -1) {
        std::cerr << "Failed to listen on socket." << std::endl;
        return 1;
    }

    std::cout << "Server listening on port 12345..." << std::endl;
    while (true) {
        int client_socket = accept(server_socket, nullptr, nullptr);
        if (client_socket != -1) {
            std::thread(handle_client, client_socket).detach();
        }
    }

    close(server_socket);
    return 0;
}
