---
tags: [skill-creator]
max_turns: 40
timeout_seconds: 900
allowed_tools: [Read, Glob, Grep, Skill, Write, Edit, Bash, WebFetch]
---

I want to write an ESPHome external component for a Sensirion SHT40 temperature/humidity sensor on I2C (address 0x44). It should report temperature and humidity, have an update_interval, and an optional precision setting (high/medium/low). Measurement in high precision takes about 9 ms after sending the command 0xFD. Please create components/sht40x/ with the Python and C++ files and an example.yaml that uses it on an esp32 board with esp-idf.
