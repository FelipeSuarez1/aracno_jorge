module SMC_POWER_MONITOR(
	//////////// CLOCK //////////
	input 		          		MAX10_CLK1_24,
	//////////// LED //////////
	output		     	   	   MAX_WARN_LED,

	//////////// Power Monitor //////////
	output		          		PMONITOR_I2C_SCL,
	inout 		          		PMONITOR_I2C_SDA,
	
	//////////// PoweDown ///////////////
	output 				  		   SOC_SYS_PDN_n,
	/////////// Battery LED /////////////
	output 	[2:0]		     	   BATTERY_LED,

output reg [15:0] power_max,
output reg [29:0] power_max_tick_max,
output reg [29:0] power_max_tick
);



//=======================================================
//  REG/WIRE declarations
//=======================================================

//
//-- Power Monitor set ----
wire [15:0] CONFIGURATION;
wire [15:0] CALIBRATION ;
wire [15:0] MASK_ENABLE;	
wire [15:0] ALERT_LIMIT;

//-- Power Monitor result ----
wire [15:0] CURRENT;      
wire [24:0] VOLTAGE_BUS; 

wire [24:0] VOLTAGE_SENSE;
wire [15:0] WATTE;        
wire [15:0] DIE_ID;       


////////////////////////
// configuration

`define POWER_CHIP_ID 		16'h2260 //
`define CLOCK_FREQ         24000000  // 24 MHz
`define OVERPOWER_TIMEOUT  (`CLOCK_FREQ/5)   // 0.2 second 
`define LOWBAT_TIMEOUT  	(`CLOCK_FREQ*2)   // 2 second


`define CURRENT_LSB      1   // 1mA, max 30A
`define WATTE_LSB        (25*`CURRENT_LSB)   // 25mW
`define MAX_POWER        (65*1000)  //mw. 65W
`define POWER_THRESHOLD  (`MAX_POWER)/`WATTE_LSB  // digi * WATTE_LSB / 1000 = mW -> dig = mW * 1000 / WATTE_LSB

//voltage (digi x 1.25 = voltage)
`define VOL_5V9            (5900*1000/1250)  // 5.9V = 5900mV, 
`define VOL_6V12            (6120*1000/1250)  // 6.12V = 6120mV, 
`define VOL_6V35            (6350*1000/1250)  // 6.35V = 6350mV, 
`define BAT_LOW				`VOL_5V9    // 5.9V

/////////////////////////
// system control
assign SOC_SYS_PDN_n = (!system_power_on | BatLow_Assert | OverPower_Assert)?1'b0:1'b1;
assign MAX_WARN_LED = led_init?1'b1:((BatLow_Assert | OverPower_Assert)?1'b1:1'b0); // led is high-active
assign BATTERY_LED =  led_init?3'b111:(MAX_WARN_LED)?0:3'b111 >> (3-bat_power_level);

////////////////////////
// reset generator
reg reset_n;
reg [7:0] cnt;

always@(posedge MAX10_CLK1_24)
begin
	if (cnt == 8'hff)
	begin
		reset_n <= 1'b1;
	end
	else
	begin
		reset_n <= (cnt < 8'h4f)?1'b1:1'b0;
		cnt <= cnt + 1;
	end
end

////////////////////////
// power on control (power on if the power id is mathced)
reg system_power_on;
reg led_init;
reg [15:0] led_cnt;

always@(posedge MAX10_CLK1_24 or negedge reset_n)
begin
	if (!reset_n)
	begin
		system_power_on <= 0;
		led_init <= 1'b0;
	end
	else if (!system_power_on)
	begin
		if (DIE_ID == `POWER_CHIP_ID)
		begin
			system_power_on <= 1'b1;
			led_init <= 1'b1;
			led_cnt <= 16'hffff;
		end	
	end
	else
	begin
		if (led_cnt > 0)
			led_cnt <= led_cnt - 1;
		else
			led_init <= 1'b0;
	end
end


////////////////////////
// battery monitor
wire		   BatLow_Detected;
reg			BatLow_Assert;
reg [25:0]	BatLow_Tick;


assign BatLow_Detected = (VOLTAGE_BUS < `BAT_LOW)?1'b1:1'b0;
// battery low monitor
always@(posedge MAX10_CLK1_24 or negedge reset_n)
begin
	if (~reset_n)
	begin
		BatLow_Assert <= 1'b0;
		BatLow_Tick <= 0;
	end
	else if (system_power_on && !BatLow_Assert)
	begin
		if (BatLow_Detected)
		begin
			if (BatLow_Tick > `LOWBAT_TIMEOUT)
				BatLow_Assert <= 1'b1;
			else
				BatLow_Tick <= BatLow_Tick + 1;
		end
		else 
			BatLow_Tick <= 0;
	end
	else
		BatLow_Assert <= BatLow_Assert; 
end

// battery power level monitor
reg [1:0] bat_power_level;
always@(posedge MAX10_CLK1_24 or negedge reset_n)
begin
	if (~reset_n)
		bat_power_level <= 2'd0;
	else if (system_power_on)
	begin
		if (VOLTAGE_BUS>`VOL_6V35)
			bat_power_level <= 2'd3;
		else if (VOLTAGE_BUS>`VOL_6V12)
			bat_power_level <= 2'd2;
		else if (VOLTAGE_BUS>`VOL_5V9)
			bat_power_level <= 2'd1;
		else
			bat_power_level <= 2'd0;
	end
end


////////////////////////
// over power monitor
wire		   OverPower_Detected;
reg			OverPower_Assert;
reg [25:0]	OverPower_tick;

assign OverPower_Detected = (WATTE>`POWER_THRESHOLD)?1'b1:1'b0;

always@(posedge MAX10_CLK1_24 or negedge reset_n)
begin
	if (~reset_n)
	begin
		OverPower_Assert <= 1'b0;
		OverPower_tick <= 0;
	end
	else if (system_power_on && !OverPower_Assert)
	begin
		if (OverPower_Detected)
			begin
				if (OverPower_tick > `OVERPOWER_TIMEOUT)
					OverPower_Assert <= 1'b1;
				else
					OverPower_tick <= OverPower_tick + 1;
			end
		else 
			OverPower_tick <= 0;
	end
	else
		OverPower_Assert <= OverPower_Assert; 
end


//-- Power Monitor set ----
assign MASK_ENABLE   = 16'h0000;	 // Alert configuration and conversion ready 
assign ALERT_LIMIT   = 16'h0000;  // Contains the limit value to compare selected alert function.

//-- Power Monitor Configuration set ----
assign CONFIGURATION = 16'h4127;

//-- Power Monitor Calibration set ----
assign CALIBRATION   =  16'd2560  ; //Current 1LSB=1mA

//--Power Monitor Controller --
POWER_MONITOR rt1 ( 
//   .RESET_N (1'b1),
   .RESET_N (reset_n),
   .CLK_24  (MAX10_CLK1_24),	
	//---IC SIDE---
	.PMONITOR_ALERT  (),  
	.PMONITOR_I2C_SCL(PMONITOR_I2C_SCL),
	.PMONITOR_I2C_SDA(PMONITOR_I2C_SDA),	
	//----SETTING INPUT --- 
	.Configuration(CONFIGURATION),	
	.Calibration  (CALIBRATION  ), 	
	.Mask_Enable  (MASK_ENABLE  ), 	
	.Alert_Limit  (ALERT_LIMIT  ), 	
	//----OUTPUT --- 
	.Current       (CURRENT        ),
	.Bus_Voltage   (VOLTAGE_BUS    ),
	.Shunt_Voltage (VOLTAGE_SENSE  ),
	.Power         (WATTE          ), 
	.Die_ID    	   (DIE_ID         ),
	//----Ina230 Chip Number -----
	.SLAVE_ADDR_FLAG()
	);
	
endmodule
