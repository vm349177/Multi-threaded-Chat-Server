#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <sstream>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <arpa/inet.h>
#include <fstream>
#include <algorithm>
#include <signal.h>

#define BUFFER_SIZE 1024
using namespace std;

// Global variables for thread-safe access
mutex clients_mutex;
mutex groups_mutex;

// Data structures
unordered_map<int, string> clients; // Client socket -> username
unordered_map<string, string> users; // Username -> password
unordered_map<string, unordered_set<int>> groups; // Group name -> client sockets

// Function to trim whitespaces from left and right sides of username and password
string trim(const string& name) {
    string final = "";
    for (char ch: name) {
        if (ch != ' ') final += ch;
    }
    return final;
}

// Function to send a message to a client
void send_msg(int client_socket, const string& msg) {
    send(client_socket, msg.c_str(), msg.size(), MSG_NOSIGNAL);
}

// Function to broadcast a message to all connected clients
void broadcast(int sender_socket, const string& msg) {
    lock_guard<mutex> lock(clients_mutex);
    for (const auto& client : clients) {
        if (client.first == sender_socket) continue;
        send_msg(client.first, msg);
    }
}

// Function to handle private messages
void private_msg(int sender_socket, const string& recipient_username, const string& msg) {
    lock_guard<mutex> lock(clients_mutex);
    if (count(msg.begin(),msg.end(),' ') == (int)msg.size()) {
        send_msg(sender_socket, "Error: Empty msg.");
        return;
    }
    for (const auto& client : clients) {
        if (client.second == recipient_username) {
            string formatted_msg = "[" + clients[sender_socket] + "]: " + msg;
            send_msg(client.first, formatted_msg);
            return;
        }
    }
    send_msg(sender_socket, "Error: User " + recipient_username + " not found.");
}

// Function to handle group messages
void group_msg(int sender_socket, const string& group_name, const string& msg) {
    lock_guard<mutex> lock(groups_mutex);
    if (groups.find(group_name) == groups.end()) {
        send_msg(sender_socket, "Error: Group " + group_name + " does not exist.");
        return;
    }
    if (groups[group_name].find(sender_socket) == groups[group_name].end()) {
        send_msg(sender_socket, "Error: You are not a member of group " + group_name + ".");
        return;
    }
    string formatted_msg = "[Group " + group_name + "]: " + msg;
    vector<int> remove_clients;
    for (const auto& client_socket : groups[group_name]) {
        if (clients.find(client_socket) == clients.end()) {  // checking for clients which have already left the chat
           remove_clients.push_back(client_socket);
           continue; 
        }
        if (client_socket != sender_socket) {
            send_msg(client_socket, formatted_msg);
        }
    }
    for (int socket : remove_clients) {    // lazily removing clients from group who have left the chat
        groups[group_name].erase(socket);
    }
}

// Function to handle group creation
void create_group(int client_socket, const string& group_name) {
    lock_guard<mutex> lock(groups_mutex);
    if (count(group_name.begin(),group_name.end(),' ')==(int)group_name.size()) {
        send_msg(client_socket, "Error: Group with no name cannot exist.");
        return;
    }
    if (groups.find(group_name) != groups.end()) {
        send_msg(client_socket, "Error: Group " + group_name + " already exist.");
        return;
    }
    groups[group_name] = {client_socket};
    send_msg(client_socket, "Group " + group_name + " created.");
}

// Function to handle group joining
void join_group(int client_socket, const string& group_name) {
    {
        lock_guard<mutex> lock(groups_mutex);
        if (groups.find(group_name) == groups.end()) {
            send_msg(client_socket, "Error: Group " + group_name + " does not exist.");
            return;
        }
        if (groups[group_name].find(client_socket) != groups[group_name].end()) {
            send_msg(client_socket, "Error: You are already a member of the group " + group_name + ".");
            return;
        }
    }
    {
        lock_guard<mutex> lock(groups_mutex);
        groups[group_name].insert(client_socket);
    }
    send_msg(client_socket, "You joined the group " + group_name + ".");
    group_msg(client_socket, group_name, clients[client_socket] + "has joined the group " + group_name + ".");
}

// Function to handle group leaving
void leave_group(int client_socket, const string& group_name) {
    {
        lock_guard<mutex> lock(groups_mutex);
        if (groups.find(group_name) == groups.end()) {
            send_msg(client_socket, "Error: Group " + group_name + " does not exist.");
            return;
        }
        if (groups[group_name].find(client_socket) == groups[group_name].end()) {
            send_msg(client_socket, "Error: You were not a member of the group " + group_name + ".");
            return;
        }
    }
    {
        lock_guard<mutex> lock(groups_mutex);
        groups[group_name].erase(client_socket);
    }
    send_msg(client_socket, "You left the group " + group_name + ".");
    group_msg(client_socket, group_name, clients[client_socket] + "has left the group " + group_name + ".");
}

// Function to handle client connections
void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE];

    // Authentication
    send_msg(client_socket, "Enter username: ");
    memset(buffer, 0, BUFFER_SIZE);
    recv(client_socket, buffer, BUFFER_SIZE, 0);
    string username(buffer);

    send_msg(client_socket, "Enter password: ");
    memset(buffer, 0, BUFFER_SIZE);
    recv(client_socket, buffer, BUFFER_SIZE, 0);
    string password(buffer);

    //trim username and password
    username = trim(username);
    password = trim(password);

    // Check if username and password match
    if (users.find(username) == users.end() || users[username] != password) {
        send_msg(client_socket, "Authentication failed.");
        close(client_socket);
        return;
    }
    cout << username << " joined\n";
    // Add client to the list of connected clients
    {
        lock_guard<mutex> lock(clients_mutex);
        clients[client_socket] = username;
    }

    send_msg(client_socket, "Welcome to the chat server!");
    broadcast(client_socket, username + " has joined the chat.");

    // Handle client messages
    while (true) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) {
            break;
        }

        string text(buffer);
        if (text.empty()) continue;

        //Separate commands
        vector<string> commands;
        size_t pos = 0;
        while (pos < text.size()) {
            size_t next = text.find('/', pos+1);
            if (next == std::string::npos) {
                commands.push_back(text.substr(pos));
                break;
            }
            commands.push_back(text.substr(pos, next-pos));
            pos = next;
        }
        bool tobreak = false;
        // traverse commands
        for (const auto &msg : commands) {
            // Parse commands
            if (msg == "/exit") {
                tobreak = true;
                break;
            }
            if (msg.find("/msg ") != string::npos) {
                size_t space = msg.find(' ', 5);
                if (space != string::npos) {
                    string recipient = msg.substr(5, space - 5);
                    string message = msg.substr(space + 1);
                    private_msg(client_socket, recipient, message);
                }
                else {
                    send_msg(client_socket, "Error: Wrong Syntax.");
                }
            }
            else if (msg.find("/group_msg ") != string::npos) {
                size_t space = msg.find(' ', 11);
                if (space != string::npos) {
                    string group_name = msg.substr(11, space - 11);
                    string message = msg.substr(space + 1);
                    group_msg(client_socket, group_name, message);
                }
                else {
                    send_msg(client_socket, "Error: Wrong Syntax.");
                }
            }  
            else if (msg.find("/broadcast ") != string::npos) {
                string message = msg.substr(11);
                broadcast(client_socket, "[Broadcast from " + username + "]: " + message);
            }  
            else if (msg.find("/create_group ") != string::npos) {
                string group_name = msg.substr(14);
                create_group(client_socket, group_name);
            } 
            else if (msg.find("/join_group ") != string::npos) {
                string group_name = msg.substr(12);
                join_group(client_socket, group_name);
            } 
            else if (msg.find("/leave_group ") != string::npos) {
                string group_name = msg.substr(13);
                leave_group(client_socket, group_name);
            } 
            else if (msg == "/exit") {
                break;
            } 
            else {
                send_msg(client_socket, "Error: Unknown command.");
            }
        }
        if (tobreak) break;
    }
    broadcast(client_socket, username + " has left the chat.");
    // Remove client from the list of connected clients
    {
        lock_guard<mutex> lock(clients_mutex);
        clients.erase(client_socket);
    }

    close(client_socket);
}

// Function to load users from users.txt
void load_users(const string& filename) {
    ifstream file(filename);
    string line;
    while (getline(file, line)) {
        size_t colon = line.find(':');
        if (colon != string::npos) {
            string username = line.substr(0, colon);
            string password = line.substr(colon + 1);
            users[username] = password;
        }
    }
}

int main() {
    signal(SIGPIPE, SIG_IGN);
    // Load users from users.txt
    load_users("users.txt");

    // Create server socket
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        cout << "Error creating socket." << endl;
        return 1;
    }

    // Bind server socket
    sockaddr_in server_address{};
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(12345);
    server_address.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_socket, (sockaddr*)&server_address, sizeof(server_address)) < 0) {
        cout << "Error binding socket." << endl;
        return 1;
    }

    // Listen for incoming connections
    if (listen(server_socket, 100) < 0) {
        cout << "Error listening on socket." << endl;
        return 1;
    }

    cout << "Server listening on port 12345..." << endl;

    // Accept incoming connections
    while (true) {
        sockaddr_in client_address{};
        socklen_t client_address_len = sizeof(client_address);
        int client_socket = accept(server_socket, (sockaddr*)&client_address, &client_address_len);
        if (client_socket < 0) {
            cout << "Error accepting connection." << endl;
            continue;
        }

        // Handle client in a new thread
        thread(handle_client, client_socket).detach();
    }

    close(server_socket);
    return 0;
}
