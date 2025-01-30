import socket

HOST = "127.0.0.1"  # Server IP
PORT = 12345        # Server port

# Function to handle client communication
def start_client():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as client_socket:
        client_socket.connect((HOST, PORT))

        # Receive and send username for authentication
        username = input("Enter your username: ")
        client_socket.sendall(username.encode())

        # Receive and send password for authentication
        password = input("Enter your password: ")
        client_socket.sendall(password.encode())

        # Receive response from server
        data = client_socket.recv(1024)
        print(f"Server response: {data.decode()}")

        # If authentication is successful, continue communication
        if "Welcome" in data.decode():
            while True:
                message = input("Enter message (or 'exit' to disconnect): ")

                if message.lower() == "exit":
                    break
                client_socket.sendall(message.encode())
                data = client_socket.recv(1024)
                print(f"Server response: {data.decode()}")

if __name__ == "__main__":
    start_client()
