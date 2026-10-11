# GizmoDisplay-Arduino
Arduino code for the Gizmo Display Module that runs locally on the Display Module controller board.

Supports: 
1) One defined string for use on powerup
2) A lower power display when charging (only flashes the Error Box LED)
   The number of flashes is the integer portion of the battery voltage.
   4 flashes is for a fully charged battery (3.9V or slightly higher)
   2 flashes is about as low a voltage as the battery should provide since it shuts off at about 2.5V
4) Bluetooth monitoring for commands from some external APP (work in progress!)

<img width="3546" height="2916" alt="P1340350_fix" src="https://github.com/user-attachments/assets/2bb12afe-3533-4745-b7fa-1a2be5620df6" />

## Arduino Software setup

The Gizmo Display Module usus a Seeed Studio XIAO ESP32C6

Instructions for Arduino software installation (https://wiki.seeedstudio.com/xiao_esp32c6_getting_started/#software-preparation)

## Gizmo Display String Syntax

## Initial display string location in the Arduino Code

