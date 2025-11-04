from collections import deque, namedtuple
import random
import numpy as np 
import torch
import torch.nn as nn 
import torch.nn.functional as F 


# -- CONFIGURATION PARAMETERS --
MODEL_FILENAME="dqn_model.pt"




# -- ReplayBuffer parameters --
# capacity: int = max transitions stored
# memory: deque of Transition tuples
Transition = namedtuple("Transition", ("s", "a", "r", "s2", "done"))


class ReplayBuffer:
    def __init__(self, capacity: int):
        # capacity - maximum number of transitions to store
        self.capacity = capacity
        # memory - deque buffer storing Transition tuples
        self.memory = deque(maxlen=capacity)

    def push(self, *args):
        # args: (s, a, r, s2, done)
        self.memory.append(Transition(*args))

    def sample(self, batch_size: int):
        # batch_size - number of transitions to sample
        batch = random.sample(self.memory, batch_size)
        return Transition(*zip(*batch))

    def __len__(self):
        return len(self.memory)



# -- QNet parameters --
# obs_dim:      dimensionality of input state vector
# n_actions:    number of discrete actions
class QNet(nn.Module):
    def __init__(self, obs_dim: int, n_actions: int):
        super().__init__()
        self.obs_dim   = obs_dim
        self.n_actions = n_actions
        self.net = nn.Sequential(
            nn.Linear(obs_dim, 64), nn.ReLU(),
            nn.Linear(64,     64), nn.ReLU(),
            nn.Linear(64, n_actions)
        )

    def forward(self, x):
        # x: tensor of shape [batch_size, obs_dim]
        # returns: tensor [batch_size, n_actions]
        return self.net(x)



# -- DQNAgent parameters --
# obs_dim:      state vector size
# n_actions:    number of discrete actions
# lr:           learning rate for Adam optimizer
# gamma:        discount factor
# eps_start:    initial epsilon for exploration
# eps_final:    final epsilon
# eps_decay:    number of steps to decay epsilon
# buf_size:     replay buffer capacity
# batch_size:   minibatch size
# target_tau:   steps between target network sync
class DQNAgent:
    def __init__(self,
                 obs_dim: int,
                 n_actions: int,
                 lr: float = 0.0004,
                 gamma: float = 0.95,
                 eps_start: float = 0.25,
                 eps_final: float = 0.05,
                 eps_decay: int = 1000,
                 buf_size: int = 10_000,
                 batch_size: int = 16, # avg. 32
                 target_tau: int = 1000):
        # Initialize hyper-parameters
        self.obs_dim     = obs_dim
        self.n_actions   = n_actions
        self.lr          = lr
        self.gamma       = gamma
        self.eps_start   = eps_start
        self.eps_final   = eps_final
        self.eps_decay   = eps_decay
        self.buf_size    = buf_size
        self.batch_size  = batch_size
        self.target_tau  = target_tau
        self.learn_steps = 0

        # Create networks
        self.qnet = QNet(obs_dim, n_actions)
        self.target_qnet = QNet(obs_dim, n_actions)
        self.target_qnet.load_state_dict(self.qnet.state_dict())
        self.target_qnet.eval()

        # Optimizer and replay buffer
        self.optimizer    = torch.optim.Adam(self.qnet.parameters(), lr=lr)
        self.replay = ReplayBuffer(buf_size)
        

    def current_eps(self) -> float:
        # Linear decay of epsilon over eps_decay steps
        eps = self.eps_start - (self.eps_start - self.eps_final) * (self.learn_steps / self.eps_decay)
        return max(self.eps_final, eps)

    def act(self, state: list, eval_mode: bool = False) -> int:
        # state: list of floats length obs_dim
        # eval_mode: if True, use greedy policy (no exploration)
        if not eval_mode and random.random() < self.current_eps():
            return random.randrange(self.n_actions)
        with torch.no_grad():
            s = torch.tensor(state, dtype=torch.float32).unsqueeze(0)
            q = self.qnet(s)
            return int(torch.argmax(q, dim=1).item())

    def replay_add(self, s, a, r, s2, done: bool):
        # Store transition in replay buffer
        self.replay.push(
            np.array(s,  dtype=np.float32),
            int(a),
            float(r),
            np.array(s2, dtype=np.float32),
            bool(done)
        )

    def learn_step(self):
        # Perform a training step if enough samples are available
        if len(self.replay) < self.batch_size:
            return

        batch = self.replay.sample(self.batch_size)
        s, a, r, s2, d = batch

        # Convert to tensors
        #s   = torch.tensor(np.array(s),  dtype=torch.float32)
        #a   = torch.tensor(a,  dtype=torch.int64).unsqueeze(1) # Action indices
        #r   = torch.tensor(r,  dtype=torch.float32).unsqueeze(1) # Rewards
        #s2  = torch.tensor(np.array(s2), dtype=torch.float32)
        #d   = torch.tensor(d,  dtype=torch.float32).unsqueeze(1) # Done flags (0. or 1.)


        # Convert all to NumPy arrays first
        s   = np.array(s,  dtype=np.float32)
        a   = np.array(a,  dtype=np.int64).reshape(-1, 1)
        r   = np.array(r,  dtype=np.float32).reshape(-1, 1)
        s2  = np.array(s2, dtype=np.float32)
        d   = np.array(d,  dtype=np.float32).reshape(-1, 1)

        # Then convert to torch tensors
        s   = torch.from_numpy(s)
        a   = torch.from_numpy(a)
        r   = torch.from_numpy(r)
        s2  = torch.from_numpy(s2)
        d   = torch.from_numpy(d)

        # Compute current Q-values
        q_values = self.qnet(s).gather(1, a)

        # Compute TD targets using the target network
        with torch.no_grad():
            q_next = self.target_qnet(s2).max(1, keepdim=True)[0]
            target = r + self.gamma * q_next * (1.0 - d)

        # Compute loss (Huber)
        loss = F.smooth_l1_loss(q_values, target)

        # Gradient descent step
        self.optimizer.zero_grad()
        loss.backward()
        torch.nn.utils.clip_grad_norm_(self.qnet.parameters(), 1.0)
        self.optimizer.step()

        # Update counters and sync target network
        self.learn_steps += 1
        if self.learn_steps % self.target_tau == 0:
            self.target_qnet.load_state_dict(self.qnet.state_dict())


    def save(self, filename=MODEL_FILENAME):
        torch.save({
            'model_state_dict':self.qnet.state_dict(),
            'target_model_state_dict':self.target_qnet.state_dict(),
            'optimizer_state_dict':self.optimizer.state_dict()
        }, filename)
        print(f"[DQN] Saved model to {filename}")
    
    def load(self, filename=MODEL_FILENAME):
        try:
            checkpoint = torch.load(filename)
            self.qnet.load_state_dict(checkpoint['model_state_dict'])
            self.target_qnet.load_state_dict(checkpoint['target_model_state_dict'])
            self.optimizer.load_state_dict(checkpoint['optimizer_state_dict'])
            print(f"[DQN] Loaded model from {filename}")
            
        except FileNotFoundError:
            print(f"[DQN] No saved model found at {filename}, starting fresh.")




'''
Deep Q-Networks (DQN)
DQN is Q-learning in which the action-value function Qθ(s,a) is represented by
a parameterised neural network and trained by stochastic gradient descent
with two stabilising techniques:
experience replay and a periodically frozen target network


Core Components
s		Environment state (vector of floats, images, ...)
r		Scalar reward returned by the environment
Qθ(s,a) NN's estimate of expected discounted return when taking a in s and following the greedy policy thereafter
Qθ		Target network (periodic copy of Qθ); held fixed while computing TD targets
D		FIFO experience replay buffer storing tuples (s,a,r,s',done)


Training Loop
1. ε-greedy action
2. Step the environment -> observe (r,s',done)
3. Store transition in D
4. Sample mini-batch {(si, ai, ri, s'i, di)}B i=1 uniformly from D
5. Compute TD target
6. Gradient step
7. Target-net update every τ steps:
    θ^- <- θ



Hyper-parameters

| Parameter      | Typical value (games)         |
| -------------- | ----------------------------- |
| γ (discount)   | 0.95 – 0.99                   |
| ε schedule     | linear/exponential 0.9 → 0.05 |
| Replay size    | 10 k – 1 M transitions        |
| Batch size     | 32 – 256                      |
| Target sync τ  | 500 – 10 k env steps          |
| Optimiser / lr | Adam 1 e‑4 – 5 e‑4            |
  α (lr)          = 1e‑4   →  Adam learning rate




Environment         Agent Network
(UE side)           (Python side)
----------------------------------------------------------
step N:
   state_S ………→│
               │ 1. Receive JSON  →  parse into float[3]
               │ 2. Store previous transition
               │    (Sₙ₋₁, Aₙ₋₁, Rₙ₋₁, Sₙ, doneₙ)  in replay
               │ 3. ε‑greedy choose Aₙ  using qnet
               │ 4. send Aₙ ──────────────┐
step N+1:                                     │
   UE executes Aₙ, computes Rₙ,             │
   builds new obs Sₙ₊₁, sends ……………………│
               │ 5. learn_step(): if buffer ≥ batch:
               │       • sample 64 transitions
               │       • compute targets with target_qnet
               │       • back‑prop, Adam step
               │       • every τ:  target_qnet ← qnet
----------------------------------------------------------




Summary
1. Observation: Unreal sends Python the latest game snapshot
(AI HP 60 %, Player HP 80 %, distance 145 cm) plus the reward from the action just finished (say, –1 because the AI took damage).

2. Book‑keeping: Python stores last step’s tuple in the replay buffer,
so the network can learn from it later.

3. Action selection: The policy net runs a forward pass on the current state,
producing four Q‑values. With probability ε (currently, maybe 0.18)
it ignores them and returns a random action; otherwise it picks the arg‑max (e.g., action “Dodge”).
That integer goes back to UE instantly.

4. Game executes: UE plays the Dodge montage, updates hit boxes, etc.—no learning yet.

5. Learning step (in parallel or just after):

    - If the replay buffer has ≥64 samples, it draws a random batch.

    - For each sample it computes the TD‑error target using the older target‑net.

    - It runs back‑prop on the policy net, nudging its weights to reduce the TD error (Huber loss).

    - Every 1000 steps it copies weights into the target net.

6. Loop: Unreal assembles the next observation/reward and the cycle continues.
Over thousands of loops, Q‑estimates converge and the AI starts choosing smarter actions.



Why replay & target nets are critical
    - Replay buffer: breaker the temporal correlation of on-policy data ->
    turns the RL problem into an i.i.d. supervised-learning-style regression problem that SGD can handle.
    
    - Target network: avoids chasing a moving target; the bootstrap value maxa' Qθ-(s',a')
    is evaluated by a quasi-static network, preventing positive-feedback explosions.

'''
