# smk-servo-motor-card-manual

> Texto extraído com pdftotext -layout. Terasic SMK (Servo Motor Kit / Servo Motor Card) User Manual, 2016-09-12 (V1.1.0 de 2019-07-30 na Mouser: https://www.mouser.com/datasheet/2/598/ervo_Motor_Kit-1020966.pdf). Origem desta cópia: https://www.ickey.cn/static-pf/datasheet/22/50/4772/81/06ce2871575ab7caef79fdf076d7313c.pdf
> Figuras não estão incluídas; numeração de páginas do PDF preservada no rodapé de cada página.

```text
SMK User Manual   www.terasic.com
                  September 12, 2016
Chapter 1            Servo Motor Kit .............................................................................. 1

1.1 Package Contents ...........................................................................................................1
1.2 SMK System CD ............................................................................................................2
1.3 Assemble the Servo Motor .............................................................................................2
1.4 Getting Help ...................................................................................................................5

Chapter 2            Servo Motor Card .......................................................................... 6

2.1 Features ..........................................................................................................................6
2.2 Block Diagram of Servo Motor Card .............................................................................7
2.3 Interface of Servo Motor Card .......................................................................................8
2.4 Power for Motor Card ....................................................................................................8
2.5 Interface for FPGA Host ................................................................................................9
2.6 Interface for Servo Motor ............................................................................................ 11
2.7 CPLD Code for Power Monitor ...................................................................................12
2.8 Servo Motor Control ....................................................................................................12

Chapter 3            Design Example .......................................................................... 14

3.1 Function Block Diagram ..............................................................................................14
3.2 Demo Setup ..................................................................................................................15




                     SMK User Manual                                                                                        www.terasic.com
                                                                                                                            September 12, 2016
                                                                       Chapter 1
                                                               Servo Motor Kit
The SMK (Servo Motor Kit) is designed for developer who wish to learn servo motor control and
build their robot machine. It includes one servo motor driver daughter card and one servo motor.
The motor driver card can drive 24 RC servo motors at most. The motor daughter card can be
linked to the FPGA development kit via the 2x20 Pin GPIO connector.

The PWM IP and some example designs on different FPGA development boards are also provided
in the kit. By referring these demos, the user can quickly learn how to work with the servo motor
and build their own robot machine.


  1.1 Package Contents

The SMK package includes
    Servo Motor Daughter Card
    RC Servo Motor
    40-pin Cable
    System CD Download Guide




                              Figure 1-1 The SMK package contents




            SMK User Manual                                                      www.terasic.com
                                                                                 September 12, 2016

                                                1
1.2 SMK System CD
The SMK System CD contains all the documents and supporting materials associated with Servo
Motor Kit, including the user manual, reference designs, and device datasheets. Users can
download this system CD from the link: http://smk.terasic.com/cd.


1.3 Assemble the Servo Motor
The servo motor and motor daughter card can be connected to any FPGA development kit that is
equipped with a 2x20 Pin GPIO connector. The pictures below show how the servo motor card is
connected to various Terasic FPGA Boards:




                 Figure 1-2 Connect the Servo Motor and Motor Card to DE10-Lite




            SMK User Manual                                                       www.terasic.com
                                                                                  September 12, 2016

                                               2
      Figure 1-3 Connect the Servo Motor and Motor Card to DE0-CV




     Figure 1-4 Connect the Servo Motor and Motor Card to DE0-Nano




SMK User Manual                                                      www.terasic.com
                                                                     September 12, 2016

                                   3
  Figure 1-5 Connect the Servo Motor and Motor Card to DE0-Nano-SoC




     Figure 1-6 Connect the Servo Motor and Motor Card to DE1-SoC




SMK User Manual                                                     www.terasic.com
                                                                    September 12, 2016

                                  4
                  Figure 1-7 Connect the Servo Motor and Motor Card to DE2-115




1.4 Getting Help
Here are the addresses where you can get help if you encounter any problems:
Terasic Technologies
9F., No.176, Sec.2, Gongdao 5th Rd, East Dist, Hsinchu City, 30070. Taiwan
Email: support@terasic.com
Tel.: +886-3-575-0880
Website: http://smk.terasic.com




            SMK User Manual                                                      www.terasic.com
                                                                                 September 12, 2016

                                               5
                                                                        Chapter 2
                                                            Servo Motor Card

This chapter will introduce the servo motor daughter card included in the Kit. The daughter card is
mainly responsible for converting the 3.3V PWM signals from FPGA into 5.0V PWM signals to
control the rotate angle of servo motors. Besides, the daughter card also provide 6V DC needed to
drive the motor.


2.1 Features
Figure 2-1 shows a photograph of the servo motor daughter card.




                                   Figure 2-1 Servo Motor Card

The major purpose of the servo motor card is to drive the serve motor. It receives PWM signal from
the host FPGA board, converts the voltage of the PWM, then sends the PWM to the servo motor.
The motor will rotate to a desired angle based on the PWM duty cycle it receives.

The features of the servo motor car are:
    Input 6.2~12V DC to generate 6V DC to drive servo motors.
    Translate 3.3V PWM to 5.0V PWM
    Provide 5V DC power output for host
    2X20 3.3V GPIO Interface for host
    Drive 24 servo motors at most
    Fuse protection for each servo driving port

            SMK User Manual                                                        www.terasic.com
                                                                                   September 12, 2016

                                                 6
     MIPI Build-In Altera CPLD and Power Monitor Chip:
      o Battery voltage monitor
      o Total current monitor
      o Auto shutdown when battery is low or total current is too high.
      o Source code is available for users to modify according to their application.




2.2 Block Diagram of Servo Motor Card
  Figure 2-2 shows the block diagram of the servo motor card. There are 24 level shift populated
  to convert the voltage level of PWM signals from 3.3V to 5.0V. The 6V power regulator
  provides 6V DC for the servo motor. The CPLD is configured to monitor the power constantly.
  When an exception is detected, it will automatically shut down the motor. The board power
  comes from the DC jack and 6.2 ~ 12V DC power is expected. The 5V DC output power can be
  used to supply 5V power for the FPGA mainboards designed with 5V input requirement.




                            Figure 2-2 Block Diagram of Servo Motor Card



          SMK User Manual                                                        www.terasic.com
                                                                                 September 12, 2016

                                                 7
2.3 Interface of Servo Motor Card
Figure 2-3 shows main interface of the servo motor card. The 2x20 GPIO is used to connect to the
FPGA mainboard to get 24 PWM control signals from the FPGA. There are 24 1x3 SERVO
HEADER on the board. These 24 header can be used to drive 24 servo motors. The DC JACK
receives 6.2~12.0V DC source to provide power for the motor card. DC 6V is generated to drive
servo motors. The DC 5V OUPUT connector can output 5V DC power which can be used to
provide power for FPGA main board. The POWER SWITCH is used to turn the power on/off the
motor card. When POWER SWITCH is set to off position, the servo motor will stop immediately.




                              Figure 2-3 Main interface of the servo motor card




2.4 Power for Motor Card
External 6.2V~12V DC power is required for the motor card to work. Users can provide the required
power from the J4 DC-Jack or the J1 2-pin connector (not installed) as shown in Figure 2-4. If
DC-Jack is used, please make sure the DC-Plug of the power source is meet required specification
as shown in Figure 2-5. Terasic also provide the power supply, user can change the power supply
from http://smk.terasic.com/buy.

Note, do not provide power from J4 DC Jack and J1 2-pin power input at the same time. Otherwise,
the board will be damaged.


            SMK User Manual                                                       www.terasic.com
                                                                                  September 12, 2016

                                                      8
                               Figure 2-4 Power Input J4 and J1




                                Figure 2-5 DC-Plug Specificaiton




2.5 Interface for FPGA Host
The servo motor card is connected to the host FPGA through the JP0 - a 2x20 GPIO header as
shown in Figure 2-6. There are 24 PWM channels, labeled as PWM0 to PWM23, in the 2x20 GPIO
header. When driving servo motor, the PMW signals is sent from the host FPGA with IO standard
3.3V. The Host FPGA board should then provide 3.3V in the VCC3P for the daughter card to use it


            SMK User Manual                                                    www.terasic.com
                                                                               September 12, 2016

                                               9
as reference voltage in level shift circuit. The VCC5_EXT is not used in general.




                                   Figure 2-6 2x20 GPIO Pinout


SOC_PM_I2C_SDA and SOC_PM_I2C_SCL are reserved signals. If users want the host FPGA to
directly control the power monitor chip INA230, they can remove the two 0 OMH resistor from R310
and R311 to R313 and R314 as shown in Figure 2-7.




                   Figure 2-7 SOC_PM_I2C_SDA and SOC_PM_IC2_SCL Signals


The SYS_PM_ALERT is an alert signal from the power monitor chip INA230. SOC_SYS_PDN_n is
a reserved signal. If users want host FPGA to directly control the daughter power, they can move 0
OMH from R318 to R319 and install the D3 component as shown in Figure 2-8. SOC_WARN_LED
is a reserved signal. If users want the host FPGA to directly control the warning led, they can move
0 OMH from R316 to R317 as shown in Figure 2-9.




             SMK User Manual                                                        www.terasic.com
                                                                                    September 12, 2016

                                                10
                                     Figure 2-8 SYS_PDN_n Signal




                                     Figure 2-9 WARN_LED Signal




2.6 Interface for Servo Motor
As shown in Figure 2-10, the JP1~JP12 provide 24 1x3 pin headers to connect to 24 servo motors.
The 24 servo motors are drive by 6V DC power. If developers wish to drive the 5V device, such as
an ultrasonic module, they can connect the 5V device to JP12 and set J2.1 and J2.2 short as
shown in Figure 2-11. In this current example, the 5V device will be driven by 5V coming from the
2x20 GPIO header.




                              Figure 2-10 24 1x3 header for 24 servo motors




                                Figure 2-11 JP12 power source selection


            SMK User Manual                                                      www.terasic.com
                                                                                 September 12, 2016

                                                   11
2.7 CPLD Code for Power Monitor
The power monitor in the motor card is implemented in the CPLD chip as shown in Figure 2-12.
The source code is available on the System CD. The main function of the CPLD is to turn off power
if battery power level is too low or overcurrent is detected. The developer can modify the threshold
according to their application. The power is turned off when battery power level is too low in order to
prevent battery overcharged. The power is turned off when overcurrent is detected in order to
prevent damages in the motor (when servo motor is stuck, power current will increase
significantly.).




                                 Figure 2-12 CPLD for Power Monitor




2.8 Servo Motor Control
The servo motor connector is consistent of three signals as shown in Figure 2-13. The red vcc wire
and the brown ground wire are used to provide power for the servo motor. The input orange PWM
wire is used to specify the rotate angle for the servo motor.




                           Figure 2-13 Three wires of server motor connector

The PWM period should be around 20 ms, and the duty cycle of PWM should determine the rotate
angle of the servo as shown in Figure 2-14. For the servo motor included in this kit, 0.5 ms duty
cycle represents 0 degree, 1.5 ms duty cycle represents 90 degree, and 2.5 ms duty cycle


             SMK User Manual                                                          www.terasic.com
                                                                                      September 12, 2016

                                                 12
represents 180 degree as shown in Figure 2-15.




                              Figure 2-14 PWM Duty Cycle and Period




                                Figure 2-15 Duty Cyclone V.S. Angle




            SMK User Manual                                           www.terasic.com
                                                                      September 12, 2016

                                               13
                                                                        Chapter 3
                                                               Design Example
This chapter will demonstrate how to control the angle servo motor by PMW signal generated by
the FPGA.


  3.1 Function Block Diagram
Figure 3-1 shows the Function block diagram of servo motor control demonstration. The FPGA
board generate 3.3V PWM signal and send the signal to the Servo Motor Card through GPIO
interface. The Server motor will translate the 3.3V PWM signal to 5.0V PWM signal, and send the
5.0V PWM signal to the servo motor. The servo motor will rotate to an angle specified by the duty
cyclone of PWM signal as shown in Figure 2-15 in chapter 2.




                                Figure 3-1 Function block diagram

In this demonstration, users use the KEY and SWITCH on FPGA mainboard to control the behavior
of the servo motor. Switches SW [0] and SW [1] are used to set the rotation speed of SERVO,
button KEY [0] is used to increase the angle, and button KEY [1] is used to decrease the angle. The
User Interface module will initially send the inputted angle to the PWM (Pulse Width Modulation)
Calculator module for calculations. After the PWM Calculator has converted the angle into pulsing
signals, the signals are sent to a PWM Generator module. Finally, the PWM Generator module will
output PWM signals to the GPIO header to drive the servo.




            SMK User Manual                                                        www.terasic.com
                                                                                   September 12, 2016

                                               14
  3.2 Demo Setup

   Design Tools
    Quartus II v15.1
    Nios II Eclipse 15.1

   Demonstration Source Code
    Quartus project directory: DE0-CV_SMK
    Nios II Eclipse project workspace: DE0-CV_SMK\software

   Demonstration Batch File
    Demo batch file folder: demonstrations\DE0-CV_SMK\demo_batch

   Demonstration Setup for DE0-CV
Please follow the procedures below to setup the demonstration as shown in Figure 3-2.

   1.  Power off the servo motor card.
   2.  Connect servo motor to the PWM0 on the servo motor card.
   3.  Connect the GPIO0 output of the DE0-CV board to the servo motor card.
   4.  Plug 12V DC to the servo motor card.
   5.  Power on the servo motor.
   6.  Connect a USB cable between the host PC and the USB connector (J3) on the DE0-CV.
   7.  Please make sure Quartus II has been installed on the host PC.
   8.  Execute the batch file “ DE0_CV_SMK.bat” under the demo_batch folder of DE0_CV_SMK
       project.
   9. Press KEY0 to increase servo angle.
   10. Press KEY1 to decrease servo angle.
   11. Adjust the speed by SW0 SW1.




            SMK User Manual                                                     www.terasic.com
                                                                                September 12, 2016

                                              15
                              Figure 3-2 Demo Setup with DE0-CV



   Demonstration Setup for DE0-Nano
Please follow the procedures below to setup the demonstration as shown in Figure 3-3

   1.  Power off the servo motor card.
   2.  Connect servo motor to the PWM2 on the servo motor card.
   3.  Connect the GPIO0 output of the DE0-Nano board to the servo motor card.
   4.  Plug 12V DC to the servo motor card.
   5.  Power on the servo motor.
   6.  Connect a USB cable between the host PC and the USB connector on the DE0-Nano.
   7.  Please make sure Quartus II has been installed on the host PC.
   8.  Execute the batch file “ DE0_Nano_SMK.bat” under the demo_batch folder of DE0_Nano_SMK
       project.
   9. Press KEY0 to increase servo angle.
   10. Press KEY1 to decrease servo angle.
   11. Adjust the speed by SW0 SW1.




            SMK User Manual                                                      www.terasic.com
                                                                                 September 12, 2016

                                              16
                  Figure 3-3 Demo Setup for DE0-Nano




SMK User Manual                                        www.terasic.com
                                                       September 12, 2016

                                 17
Mouser Electronics

Authorized Distributor



Click to View Pricing, Inventory, Delivery & Lifecycle Information:



Terasic:
 B0493 P0288 FXX-3037-TOP B0492
```
