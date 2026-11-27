---
tags: [skill-creator]
max_turns: 40
timeout_seconds: 900
allowed_tools: [Read, Glob, Grep, Skill, Write, Edit, Bash, WebFetch]
---

In my dooya_bidir ESPHome component, add an on_obstacle automation trigger on the motor so users can react in YAML when a motor reports that it stopped because of an obstacle (status register value 0x70). The trigger should give the motor address to the automation. Update example.yaml with a usage example.

Input files: dooya_bidir/, example.yaml
