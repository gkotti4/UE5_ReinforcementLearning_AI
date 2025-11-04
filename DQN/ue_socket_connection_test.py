import socket, struct, json


HOST, PORT = "127.0.0.1", 5555
BUF_SIZE = 1024

def recv_json(conn):
    print("Waiting to receive JSON packet")
    
    # Read 4-byte big-endian length header
    raw_len = conn.recv(4)
    if not raw_len:
        raise ConnectionError("Socket closed by UE")
    
    msg_len = struct.unpack("!I", raw_len)[0]

    # Read the JSON payload
    data = bytearray()
    while len(data) < msg_len:
        packet = conn.recv(msg_len - len(data))
        if not packet:
            raise ConnectionError("Socket closed during payload")
        data.extend(packet)
    return json.loads(data.decode())


def send_json(conn, obj):
    # Sends a dict as length-prefixed (4 bytes) JSON.
    raw = json.dumps(obj).encode()
    conn.sendall(struct.pack("!I", len(raw)))
    conn.sendall(raw)

def send_action(conn, action_id: int):
    # Send a 4-byte uint32 to UE
    conn.sendall(struct.pack("!I", action_id))
    

def main():
    # Create server socket
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server:
        #server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server.bind((HOST, PORT))
        server.listen(1)
        print(f"[SERVER] Listening on {HOST}:{PORT} ...")
        
        conn, addr = server.accept()
        with conn:
            print(f"[SERVER] Connected by {addr}")
            
            try:
                packet = recv_json(conn)
                print(f"[SERVER] Received JSON: {packet}")
                
                num_states 	= int(packet.get("NumStates", "0"))
                num_actions = int(packet.get("NumActions", "0"))
                
                response = {
                    "pong": "hello from Python Server",
                    "received_states": num_states,
                    "received_actions": num_actions
                }
                
                send_json(conn, response)
                print("[SERVER] Sent JSON pong with summary")
                
            except Exception as e:
                print(f"[SERVER] Error: {e}")


if __name__ == "__main__":
    main()
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    