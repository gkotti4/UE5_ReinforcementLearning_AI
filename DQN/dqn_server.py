
'''
┌────── 4‑byte uint32 LEN (network order) ──────┐
│               JSON payload                    │
└───────────────── LEN bytes ────────────────────┘

Read logic (pseudo-code):
uint32 len = read_exactly(4);
uint32 msg = read_exactly(len);

'''

# -----------------------------
# Python DQN Server for UE5 Integration
# -----------------------------
# Launch this script BEFORE running the Unreal project,
#  there is no need to run the dqn_agent.py script.
# It will:
# 1. Listen on TCP port for incoming state/reward/done packets.
# 2. Parse each JSON message into Python primitives.
# 3. Maintain a DQNAgent for learning from experience replay.
# 4. Send back a 4-byte action index (uint32) to drive the UE enemy.

import argparse
import socket, struct, json
import numpy as np
import matplotlib.pyplot as plt

from dqn_agent import DQNAgent


# -- UE configuration parameters --
NUM_STATES = 8
NUM_ACTIONS = 5

# -- Agent configuration parameters --
LR = 0.0004					# Learning rate
BATCH_SIZE = 32				# Mini-batch size (training size)
GAMMA = 0.95				# Discount factor
EPSILON = 0.25				# Exploration rate
EPSILON_DECAY_STEPS = 2000	# Decay rate - how many steps until reaches eps_final=0.05
TARGET_TAU = 500			# Target network update (synch) rate (steps)


# -- Server configuration parameters --
HOST = "127.0.0.1" 
PORT = 5555



# -- Logging --



# -- Logging & Plotting functions --

def save_total_reward(total_reward, filename="reward_log.txt"):
    with open(filename, "a") as f:
        f.write(f"{total_reward}\n")
        print(f"[SERVER] Saved total reward {total_reward} at {filename}")

def load_total_rewards(filename="reward_log.txt"):
    rewards = []
    with open(filename) as f:
        rewards = [float(line.strip()) for line in f]
        print(f"[SERVER] Loaded total rewards from {filename} over {len(rewards)} episodes")
    return rewards

def smooth(values, window=5):
    if len(values) < window:
        return values
    box = np.ones(window) / window
    return np. convolve(values, box, mode='same')

def plot_rewards(reward_log, show=False, save_path="episode_rewards.png"):
    if not reward_log:
        return
    
    smoothed = smooth(reward_log)
    
    plt.figure(figsize=(10,5))
    plt.plot(reward_log, label="Raw")
    plt.plot(smoothed, label="Smoothed", linewidth=2)
    plt.xlabel("Episode")
    plt.ylabel("Total Reward")
    plt.title("Episode Reward Over Time")
    plt.legend()
    plt.grid(True)
    plt.tight_layout()
    
    plt.savefig(save_path)
    print(f"[SERVER] Saved episode rewards over time plot at {save_path}")
    
    if show:
        plt.show()
    plt.close()
    

# -- Networking helper functions --
def recv_json(conn):
    """
    Receives a length-prefixed JSON packet from the socket.

    Args:
        conn: active socket connection
    Returns:
        dict parsed from JSON
    Raises:
        ConnectionError if socket closes prematurely
    """
    # 1. Read 4-byte big-endian length header
    raw_len = conn.recv(4)
    if not raw_len:
        raise ConnectionError("Socket closed by UE")
    msg_len = struct.unpack("!I", raw_len)[0]

    # 2. Read the JSON payload
    data = bytearray()
    while len(data) < msg_len:
        packet = conn.recv(msg_len - len(data))
        if not packet:
            raise ConnectionError("Socket closed during payload")
        data.extend(packet)
    return json.loads(data.decode())


def send_json(conn, obj):
    """ Sends a dict as length-prefixed (4 bytes) JSON. """
    raw = json.dumps(obj).encode()
    conn.sendall(struct.pack("!I", len(raw)))
    conn.sendall(raw)

def send_action(conn, action_id: int):
    """
    Sends a 4-byte network-order uint32 action back to Unreal.

    Args:
        conn: active socket connection
        action_id: integer in [0, n_actions)
    """
    conn.sendall(struct.pack("!I", action_id))


def parse_args():
    parser = argparse.ArgumentParser(description="DQN Agent Configuration")
    
    parser.add_argument('--lr', type=float, default=LR, help='Learning rate')
    parser.add_argument('--batch_size', type=int, default=BATCH_SIZE, help='Batch size')
    parser.add_argument('--gamma', type=float, default=GAMMA, help='Discount factor')
    parser.add_argument('--epsilon', type=float, default=EPSILON, help='Initial exploration rate')
    parser.add_argument('--eps_decay_steps', type=int, default=EPSILON_DECAY_STEPS, help='Steps to decay epsilon')
    parser.add_argument('--target_tau', type=int, default=TARGET_TAU, help='Target network update frequency')

    return parser.parse_args()


def menu():
    global LR, BATCH_SIZE, GAMMA, EPSILON, EPSILON_DECAY_STEPS, TARGET_TAU
    
    while True:
        print("\n--- DQN Agent Parameter Menu ---")
        print(f"1. Learning Rate (LR): {LR}")
        print(f"2. Batch Size: {BATCH_SIZE}")
        print(f"3. Gamma (Discount Factor): {GAMMA}")
        print(f"4. Epsilon (Exploration Rate): {EPSILON}")
        print(f"5. Epsilon Decay Steps: {EPSILON_DECAY_STEPS}")
        print(f"6. Target Update Rate (TARGET_TAU): {TARGET_TAU}")
        print("--- Additional Features ---")
        print(f"7. Plot current total rewards")
        print("Type the number to change a parameter, or 'start' to run server with current parameters.")

        choice = input("Select option: ").strip().lower()
        
        if choice == "start":
            break
        elif choice == "1":
            LR = float(input("Enter new Learning Rate: "))
        elif choice == "2":
            BATCH_SIZE = int(input("Enter new Batch Size: "))
        elif choice == "3":
            GAMMA = float(input("Enter new Gamma: "))
        elif choice == "4":
            EPSILON = float(input("Enter new Start Epsilon: "))
        elif choice == "5":
            EPSILON_DECAY_STEPS = int(input("Enter new Epsilon Decay Steps: "))
        elif choice == "6":
            TARGET_TAU = int(input("Enter new Target Tau: "))
        elif choice == "7":
            all_total_rewards = load_total_rewards()
            plot_rewards(all_total_rewards, True)
        else:
            print("Invalid option. Try again.")


def main():
    """
    Main server loop:
    - Wait for Unreal to connect
    - Initialize DQNAgent
    - Iterate: receive packet, store transition, learn, send action
    """
    
    # 0. Initialize model parameters
    # Parse command line model parameters
    args = parse_args()
    
    LR = args.lr
    BATCH_SIZE = args.batch_size
    GAMMA = args.gamma
    EPSILON = args.epsilon
    EPSILON_DECAY_STEPS = args.eps_decay_steps
    TARGET_TAU = args.target_tau
    
    # Choose model parameters
    menu()
    
    # 1. Create and bind the listening socket
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server:
        server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server.bind((HOST, PORT))
        server.listen(1)
        print(f"[SERVER] Waiting for UE on {HOST}:{PORT}…")
        conn, addr = server.accept()
        with conn:
            print(f"[SERVER] Connected by {addr}")
            
            # Initialize UE-Server Connection
            try:
                packet = recv_json(conn)
                print(f"[SERVER] Received JSON: {packet}")
                
                NUM_STATES 	= int(packet.get("NumStates", "0"))
                NUM_ACTIONS = int(packet.get("NumActions", "0"))
                
                response = {
                    "pong": "hello from Python Server",
                    "received_states": NUM_STATES,
                    "received_actions": NUM_ACTIONS
                }
                
                send_json(conn, response)
                print("[SERVER] Sent JSON pong with summary")
                
            except Exception as e:
                print(f"[SERVER] Error: {e}")
            
            
            # 2. Instantiate DQN agent
            agent = DQNAgent(obs_dim=NUM_STATES, n_actions=NUM_ACTIONS)
            prev_state  = None    # Holds the last state for replay
            prev_action = None    # Holds the last action for replay
            episode     = 0
            total_reward = 0.0
            
            # Load model state
            agent.load()

            # 3. Server loop
            while True:
                try:
                    pkt = recv_json(conn)
                except ConnectionError as e:
                    print(f"[DQN] Connection error: {e}")
                    break

                # 4. Unpack packet
                state  = pkt.get("state", [])
                reward = float(pkt.get("reward", 0.0))
                done   = bool(pkt.get("done", False))

                # 5. Store transition from prev step
                if prev_state is not None and prev_action is not None:
                    agent.replay_add(prev_state, prev_action, reward, state, done)
                    total_reward += reward

                # 6. Select next action (epsilon-greedy)
                action = agent.act(state)

                # 7. Send action back to UE
                send_action(conn, action)

                # 8. Perform one learning step
                agent.learn_step()

                # 9. End of training management
                if done:
                    #episode += 1
                    #total_reward = 0.0
                    #prev_state  = None
                    #prev_action = None
                    print(f"[DQN] Training episode ended. Total reward = {total_reward:.2f}")
                    agent.save()
                    save_total_reward(total_reward)
                    all_total_rewards = load_total_rewards()
                    plot_rewards(all_total_rewards)
                    return
                else:
                    prev_state  = state
                    prev_action = action
                    
            

if __name__ == "__main__":
    main()










































