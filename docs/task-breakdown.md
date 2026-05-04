# Agile Task Breakdown

## Tags

- Chassis
- Steering
- Circuit
- Controller

## Main Epic

### Task #1: Build a Small Remote-Controlled Car

**Tag:** Controller / Chassis / Circuit / Steering

**Problem to Solve:**
Build a small remote-controlled car that can drive forward and backward using rear motors and steer using front wheels.

**Proposed Solution:**
Use an ESP32 for Bluetooth communication, a motor driver for rear DC motors, a servo for front steering, and 3D printed parts for chassis and steering linkage.

**Acceptance Criteria:**
- Car moves forward and backward from phone commands
- Car steers left, right, and center from phone commands
- Speed can be adjusted
- Servo angle can be adjusted
- Motors stop automatically when command signal stops

**Dependencies:**
N/A

**Notes / Constraints:**
Parts may arrive after two weeks. CAD, code, circuit planning, and Agile setup can be done before hardware arrives.

## Ordered Tasks

1. Define project requirements — Controller
2. Choose system architecture — Circuit
3. Define control commands — Controller
4. Write initial control logic — Controller
5. Learn ESP32 pin layout — Circuit
6. Draw full circuit diagram — Circuit
7. Plan power distribution — Circuit
8. Design chassis layout — Chassis
9. Design chassis base — Chassis
10. Design rear motor mounts — Chassis
11. Design front wheel pivots — Steering
12. Design steering linkage — Steering
13. Design servo mount — Steering
14. Validate steering alignment in CAD — Steering
15. Implement Bluetooth communication — Controller
16. Implement motor control functions — Circuit
17. Implement servo control functions — Steering
18. Integrate full control system — Controller
19. Assemble chassis — Chassis
20. Install rear motors — Chassis
21. Install steering system — Steering
22. Wire motor driver — Circuit
23. Wire servo — Circuit
24. Connect power system — Circuit
25. Test motor movement — Circuit
26. Test steering movement — Steering
27. Test Bluetooth control — Controller
28. Debug power issues — Circuit
29. Tune steering angles — Steering
30. Final system test — Controller
