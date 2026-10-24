# GizmoDisplay-Arduino
Arduino code for the Gizmo Display Module that runs locally on the Display Module controller board.

Supports: 
1) One defined string for use on powerup
2) A lower power display when charging (only flashes the Error Box LED)
   - The number of flashes is the integer portion of the battery voltage.
     - 4 flashes is for a fully charged battery (3.9V or slightly higher)
     - 2 flashes is about as low a voltage as the battery should provide since it shuts off at about 2.5V
4) Bluetooth monitoring for commands from some external APP (work in progress!)

<img width="3546" height="2916" alt="P1340350_fix" src="https://github.com/user-attachments/assets/2bb12afe-3533-4745-b7fa-1a2be5620df6" />

## Battery Charging

Plug a USB cable into the Display Module's USB type C connector on the underside of the black display stand.  The other end of the cable can be plugged into a USB charger or a powered port on a computer.

The Display board should then be turned ON to connect the battery to the internal battery charger.  If the display modules power switch is turned off, the LED will periodically dimly flash once or twice, but nothing is actually being charged.

Charging rate is about 1/2 the rate of the battery usage with a continuous display, but not all segments in every display tube being lit.
The test pattern (Who you gonna Call..) easily ran 12 hours but took about 20 hours to fully recharge.


## Arduino Software setup

The Gizmo Display Module uses a Seeed Studio XIAO ESP32C6 based micro controller board.

Instructions for Arduino software installation (https://wiki.seeedstudio.com/xiao_esp32c6_getting_started/#software-preparation)


## Gizmo Display String Syntax



## Initial display string location in the Arduino Code

Search for the line that is 
// setup the default string to display on powerup

The last assignment to the String "input_string" will be used for the main string to display until it is change dynamically be some Bluetooth command.
On powerup, the code will using this display string since the Bluetooth overrides are not "sticky".

The default string is currently:
input_string = "       Who You Gonna Call ?        \n  GHOST\nBUSTERS!\n \n555-2368\n%P2000        Who You Gonna Call ?        \n  GHOST\nBUSTERS!\n \nJL5-2020\n%P2000 \nBAT %BBV\n "; // default if the incoming app2xaio is not one to display


## Using the Nordic nRF Connect APP

For Andriod and iOS, the Nordic Semiconductor nRF Connect APP can allow you to manually update the display.  I am working on an Android only APP that should be easier to use, but this Nordic APP is still very usable.

https://www.nordicsemi.com/Products/Development-tools/nRF-Connect-for-mobile

For Android, here is a step by step set of screenshots showing how to send a message to your Gizmo:

1. Open the app and it should start scanning for Bluetooth devices.  If you are very close to the gizmo and want a shorter list, you can use the "Filter" drop down (just below the blue top banner) and select a signal strength that removes A LOT of farther away devices!
The name "Gizmo42" as well as the UUIDs can be customized in the Arduino code, but this is the default I am sending out in the display modules.
After you find "Gizmo42" you tap on the "CONNECT" button circled in red the image below.
2. Once connected, the device should show this screen and you want to tap on the "Unknown Service" information.  Tap anywhere on this and the next screen should appear.
3. The additional information shows the "Unknown Characteristic", and this is what we use to send a message to the Gizmo display.  Tap on the "Upload" arrow pointing up icon shown circled in read in the image below.
<img width="270" height="602" alt="Screenshot_1" src="https://github.com/user-attachments/assets/5e8e60fe-6e3c-4d2c-8da7-ff5f43b0db19" />
<img width="270" height="602" alt="Screenshot_2" src="https://github.com/user-attachments/assets/d6ec1dd2-4600-4155-af7a-dc182679d14b" />
<img width="270" height="602" alt="Screenshot_3" src="https://github.com/user-attachments/assets/0412dce3-326a-4cab-bff0-46b1757b370e" />

4. This new dialog opens up for you to enter the data you want to send.
5. You _could_ enter the hex byte values, but that would be very tedious, so tap on the "BYTE.." drop down box area and you get to select a more human friendly format.
<img width="270" height="602" alt="Screenshot_4" src="https://github.com/user-attachments/assets/cae721e7-dcfe-4c96-a832-964903fc66d5" />
<img width="270" height="602" alt="Screenshot_5" src="https://github.com/user-attachments/assets/208f8509-ecdb-489d-9102-7cfcb73ddffb" />

6. Tap on the "New value" area below the "Write value" and circled in red and the keyboard should pop up and let you type in the message you want.
Not sure how to stop the entry from always adding a period after a word if you add two spaces, but when you care add two spaces and then delete twice and add the spaces again and the period is not added that time.
I copy and paste from some other app on the phone so I can have a longer message without having to actually type it in here.
ALso not sure how to manually enter a newline character with the keyboard, but the copy/paste works fine and passes the newline on in the paste operation.
Android allows you to save the value, but iOS APP does not support this feature ?!?
7. Once the text is entered, click on "SEND" as shown circled in read in the image below.
<img width="270" height="602" alt="Screenshot_6" src="https://github.com/user-attachments/assets/32ed034d-3224-4389-8ccf-0f6a838ce43d" />
<img width="270" height="602" alt="Screenshot_7" src="https://github.com/user-attachments/assets/409a2f8e-caef-4da2-b2d4-ad0134305c0a" />

8. The page then shows what was written out to the devie in Hex in the Value field pointed to by the red arrow in the image below.
<img width="270" height="602" alt="Screenshot_8" src="https://github.com/user-attachments/assets/6650bafb-f061-436c-850c-3f5dd3e84ea1" />

9. When you are done, you can DISCONNECT (upper right hand portion of the screen) to sever the connection with the phone.
