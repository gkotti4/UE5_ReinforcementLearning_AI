# UE5 Reinforcement Learning 

Reinforcement Learning AI systems implemented in **Unreal Engine 5 (C++)**, exploring both classic **Q-Learning** and **Deep Q-Network (DQN)** approaches for adaptive enemy behavior in real-time combat.

> **Note:** This is a reupload of academic projects completed in 2024.  
> Not under active development, but preserved to demonstrate applied reinforcement learning techniques.  
> Source directories contain only the AI-related C++ and Python files — not full Unreal Engine project data.  

---

## Overview
These projects demonstrate how tabular and neural reinforcement learning methods can be applied to enemy AI in Unreal Engine 5.  
Agents learn through reward signals to **attack, dodge, guard, and wait** based on real-time parameters.

---

## Repository Organization
```text
UE5_ReinforcementLearning_AI/  
├── QLearning/  
│ ├── ue5_source/ # Referenced UE5 C++ source files for Q-Learning logic  
│ └── report/     # Presentation and brief summary for Q-Learning project  
│  
└── DQN/  
├── ue5_source/   # UE5-side C++ code interacting with the Python server  
├── dqn_source/   # Python server code (model, training loop, comm layer)  
└── report/       # Presentation and brief summary for DQN project  
```
  
---

## Algorithms Implemented
- **Q-Learning:** Classic temporal-difference learning using discrete state-action pairs.  
- **DQN:** Neural approximation of Q-values with experience replay and epsilon-greedy exploration.  
- **TD-Update:** Both versions use `Q(s,a) ← Q(s,a) + α [r + γ max Q(s′, a′) − Q(s,a)]`.

---

## Reports & Slides
Detailed summaries and presentations for each project are included under their respective `report/` folders.

---

## Author
**George Kotti** — B.S. in Computer Science & Engineering (AI + Systems)  
Developed as part of academic research on Reinforcement Learning for Game AI.  
  
