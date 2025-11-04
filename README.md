# UE5 Reinforcement Learning AI

Reinforcement Learning AI systems implemented in **Unreal Engine 5 (C++)**, exploring both classic **Q-Learning** and **Deep Q-Network (DQN)** approaches for adaptive enemy behavior in real-time combat.

> **Note:** This is a reupload of academic projects completed in 2024.  
> Not under active development, but preserved to demonstrate applied reinforcement learning techniques.  
> The `dqn_source/` and `qlearning_source/` folders contain only the AI-related C++ source files — not the full Unreal Engine project directories.  

---

## Overview
These projects demonstrate how tabular and neural reinforcement learning methods can be applied to enemy AI in Unreal Engine 5.  
Agents learn through reward signals to **attack, dodge, guard, and wait** based on real-time parameters.

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
