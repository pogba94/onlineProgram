1. Overview
========================================================================================
(1) support two modes,mode 1 is only to program code to target board,mode 2 is to 
	get UID from server and program it to target board,user can switch mode by press
	sw3 button,it will be alternately flashes green and blue light about 3 seconds 
	when the mode is switched
(2) LCD Display supported
(3) Network supported by ethernet
(4) mbed OS 2.0
(5) FATFS supported and sd card is needed
(6) log file output supported

2. Hardware requirements
========================================================================================
(1) micro USB cables
(2) RJ45 cable
(3) sd card
(4) dupont lines

3. How to work
========================================================================================
(1) Download the code to the board FRDM-K64
(2) Connect FRDM-K64 board with target board by UART cable included TX¡¢RX and GND lines
(3) Plug the ethernet cable and make sure the network is connected,it it needed only in
    mode 1
(4) plug sd card to sd card slot,make sure the bin file exsit if you want to program code,
	it will be flashes red if bin file is not exsit
(5) Make sure the target board is in ISP mode
(6) Plug the USB cable to power on,the system will work in mode 1 at default
(7) In mode 1,RGB will be blue if connected to server and target board,it will be flashes
	blue if not connected to target board.if you want to program UID to target board,you 
	should press button SW2,the color will trun to green if sucess,otherwise,it will turn 
	to red.After that,press button SW2 again to confirm and color will turn to blue. 
(8) In mode 2,indicator light will be blue if connected to target board,it will be flashes
	blue if not connected to target board.if you want to program code to target board,you
	should press button SW2 to,it will be flashes green when programing,and then color will
	always be green if programmed successfully,otherwise,color will turn to red.you should
	press button SW2 again to confirm and the color will turn to blue.



