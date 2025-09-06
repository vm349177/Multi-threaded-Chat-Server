# Multi-threaded-Chat-Server

## **Features**
- Multi-threaded TCP server for handling multiple concurrent clients.
- User authentication via a `users.txt` file.
- Private messaging between clients.
- Group messaging and group management commands.
- Broadcast messaging to all connected clients.
- Thread-safe operations using mutexes.
- Graceful handling of client disconnections.

## **Design Decisions**

### **Threading Model**
- A new thread is spawned for each client connection to ensure concurrent processing.
- `std::thread::detach()` is used to allow threads to run independently.

### **Synchronization Mechanisms**
- `std::mutex` is used to ensure thread-safe access to shared data structures:
  - `clients_mutex` protects `clients` (socket-to-username mapping).
  - `groups_mutex` protects `groups` (group membership information).

### **Data Structures Used**
- `std::unordered_map<std::string, std::string>`: Stores usernames and passwords.
- `std::unordered_map<int, std::string>`: Maps client sockets to usernames.
- `std::unordered_map<std::string, std::unordered_set<int>>`: Stores group membership.

---

## **Implementation Details**

### **Main Functions**
- `broadcast_message(const std::string&)`: Sends a message to all connected clients and also to himself.
- `send_private_message(const std::string&, const std::string&)`: Sends a direct message.
- `send_group_message(const std::string&, const std::string&)`: Sends a message to a group.
- `handle_client(int)`: Manages client authentication and message handling.
- `main()`: Loads users, initializes the server socket, and listens for new connections.

### **Code Flow**
1. The server starts and loads user credentials from `users.txt`.
2. It binds to a port (12345) and listens for incoming connections.
3. When a client connects:
   - The client is authenticated using `users.txt`.
   - If authenticated, they are added to the `clients` map.
   - They receive a welcome message and are notified of available commands.
4. The client can then:
   - Broadcast messages (`/broadcast <message>`).
   - Send private messages (`/msg <username> <message>`).
   - Manage groups (`/create_group`, `/join_group`, `/group_msg`, `/leave_group`).
   - List users and groups.
5. If a client disconnects, they are removed from `clients` and their groups.

---

## **Testing**

### **Correctness Testing**
- Verified login authentication using valid and invalid credentials.
- Tested all message types:
  - Private messages.
  - Broadcast messages.
  - Group messages.
- Checked that groups maintain correct memberships.
- Verified that users cannot send messages to groups they haven’t joined.

### **Stress Testing**
- Connected multiple clients simultaneously to assess scalability.
- Sent large messages to test buffer handling.

---

## **Server Restrictions**
- Maximum clients: Limited by system resources.
- Maximum groups: No fixed limit.
- Maximum group members: No fixed limit.
- Maximum message size: 1024 bytes (BUFFER_SIZE).

---

## **Challenges Faced**
- Handling concurrent access to shared resources required proper use of mutex locks.
- Parsing user commands efficiently to extract parameters.
- Ensuring proper cleanup of disconnected clients to prevent stale connections.

---

## **Sources Referred**
- Beej’s Guide to Network Programming.
- C++ Reference documentation for `std::thread`, `std::mutex`, `std::unordered_map`.

---

## **Declaration**
We declare that this assignment was completed independently and does not contain plagiarized content.

---

## **Feedback**
- The assignment was helpful in understanding socket programming and multithreading.
- Providing a sample implementation for one command would have been beneficial.
- A predefined testing script for checking correctness could improve debugging.

---

## **Compilation & Execution Instructions**

### **Compiling the Server**
```
make
```

### **Running the Server**
```
./server_grp
```

### **Running the Client**
```
./client_grp
```
