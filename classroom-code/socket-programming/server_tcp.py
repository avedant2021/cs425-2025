import socket
import threading

HOST = "127.0.0.1"  # Server IP
PORT = 12345        # Server port

# Dictionary to store connected clients and their usernames
clients = {}
lock = threading.Lock()

# Dictionary to store groups and their members
groups = {}

# Function to load users from users.txt for authentication
def load_users():
    users = {}
    try:
        with open("users.txt", "r") as file:
            for line in file:
                username, password = line.strip().split(":")
                users[username] = password
    except FileNotFoundError:
        print("users.txt file not found!")
    return users

# Function to authenticate client
def authenticate_client(conn):
    users = load_users()
    conn.sendall(b"Enter your username: ")
    username = conn.recv(1024).decode().strip()

    conn.sendall(b"Enter your password: ")
    password = conn.recv(1024).decode().strip()

    if username in users and users[username] == password:
        conn.sendall(f"Welcome, {username}!\n".encode())
        return username
    else:
        conn.sendall(b"Authentication failed. Disconnecting...\n")
        return None

# Function to broadcast a message to all clients
def broadcast(message, sender_conn):
    with lock:
        for conn in clients:
            if conn != sender_conn:  # Do not send to the sender
                try:
                    conn.sendall(message.encode())
                except:
                    pass  # Ignore disconnected clients

# Function to send a private message to a specific user
def private_message(username, message, sender_conn):
    with lock:
        for conn, user in clients.items():
            if user == username:
                try:
                    conn.sendall(message.encode())
                    break
                except:
                    pass

# Function to send a message to a group
def group_message(group_name, message):
    if group_name in groups:
        members = groups[group_name]
        for member in members:
            for conn, user in clients.items():
                if user == member:
                    try:
                        conn.sendall(message.encode())
                    except:
                        pass

# Function to handle communication with each client
def handle_client(conn, addr):
    print(f"Connected by {addr}")

    # Authenticate client
    username = authenticate_client(conn)
    if not username:
        conn.close()
        return

    # Add the client to the connected clients
    with lock:
        clients[conn] = username

    while True:
        data = conn.recv(1024)
        if not data:
            break

        message = data.decode().strip()

        if message.startswith("/broadcast "):
            broadcast(message[11:], conn)

        elif message.startswith("/msg "):
            parts = message.split(" ", 2)
            if len(parts) == 3:
                private_message(parts[1], f"Private message from {username}: {parts[2]}", conn)

        elif message.startswith("/group msg "):
            parts = message.split(" ", 3)
            if len(parts) == 4:
                group_message(parts[2], f"Group message from {username}: {parts[3]}")

        else:
            # Echo the message back to the sender if it's not a special command
            conn.sendall(f"Message from {username}: {message}\n".encode())

    # Remove the client when they disconnect
    with lock:
        del clients[conn]

    print(f"Disconnected: {addr}")

# Create and bind the server socket
def start_server():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server_socket:
        server_socket.bind((HOST, PORT))
        server_socket.listen()
        print(f"Server listening on {HOST}:{PORT}")

        while True:
            conn, addr = server_socket.accept()
            threading.Thread(target=handle_client, args=(conn, addr), daemon=True).start()

if __name__ == "__main__":
    start_server()
