module POWER_MONITOR ( 
   input  RESET_N , 
   input  CLK_24         ,
	input  PMONITOR_ALERT  ,   
	output PMONITOR_I2C_SCL,   
	inout  PMONITOR_I2C_SDA,   
	//----Test or ST-BUS --- 
   input     [15:0] Configuration,
   input     [15:0] Calibration  ,  
   input     [15:0] Mask_Enable  ,  
   input     [15:0] Alert_Limit  , 
	

	output reg [15:0]Shunt_Voltage,
	output reg [15:0]Bus_Voltage,  
	output reg [15:0]Power,        
	output reg [15:0]Current,
	output reg [15:0]Die_ID,       

	output ALL_ALARM     ,
	output CURRENT_ALARM ,
	output VOLTAGE_ALARM ,
	output[15:0] KW_HR,		
	input [15:0] CURRENT_LIMIT,
	input [15:0] VOLTAGE_LIMIT ,
	
	output reg SLAVE_ADDR_FLAG,
	
	//test
	output reg  CLK_400K ,
   output reg  I2C_LO0P,
   output reg [7:0] ST ,
   output reg [7:0] CNT,
	output W_WORD_END ,
   output reg    W_WORD_GO ,
	
	output [7:0] WORD_ST,
	output [7:0] WORD_CNT,
	output [7:0] WORD_BYTE	,
   output [15:0]R_DATA
	
	);
	
//-- I2C clock 400k generater 
reg  [5:0]C_DELAY ;
always @(posedge CLK_24 ) begin 
  if ( C_DELAY > 30 ) begin  
    CLK_400K<=~CLK_400K ; C_DELAY <= 0 ; 
  end 
  else C_DELAY <=C_DELAY +1 ; 
end
  
//======== Main-ST =======
//==Slave Address == 
parameter SLAVE_ADDR      =8'h80;  

//==Pointer NUM== 
//-WRITE/READ-
parameter P_Configuration =8'h00; 
parameter P_Calibration   =8'h05; 
parameter P_Mask_Enable   =8'h06;
parameter P_Alert_Limit   =8'h07;  

//-READ-
parameter P_Shunt_Voltage =8'h01;
parameter P_Bus_Voltage   =8'h02;
parameter P_Power         =8'h03; 
parameter P_Current       =8'h04; 
parameter P_Die_ID        =8'hff;

reg [7:0] DELY ;

always @(negedge RESET_N or posedge CLK_400K )begin 
if (!RESET_N  )begin ST <=0;end
else 
case (ST)
0: begin 
   ST<=30; //Config Reg
	W_POINTER_GO <=1;
   R_GO  <=1 ;		 
	W_WORD_GO <=1;
   end
//<----------------READ -------	
1: begin 
   ST<=2; 
	end	
2: begin 
	     if ( CNT==0)  W_POINTER_REG <= P_Shunt_Voltage;
	else if ( CNT==1)  W_POINTER_REG <= P_Bus_Voltage;    
	else if ( CNT==2)  W_POINTER_REG <= P_Power;        
	else if ( CNT==3)  W_POINTER_REG <= P_Current;      
	else if ( CNT==4)  W_POINTER_REG <= P_Die_ID;       

	if ( W_POINTER_END ) begin  W_POINTER_GO  <=0; ST<=3 ; DELY<=0;  end
	end                // Write ID pointer 
3: begin 
    DELY  <=DELY +1;
    if ( DELY ==2 ) begin 

   W_POINTER_GO  <=1;
   ST<=4 ; 
		 end

	end       
4: begin 
   if  ( W_POINTER_END ) ST<=5 ; 	
	end              
5: begin ST<=6 ; end //delay
	
//read DATA 		 
6: begin 
	if ( R_END ) begin  R_GO  <=0; ST<=7 ; DELY<=0; end
	end                // Write ID pointer 
7: begin 
    DELY  <=DELY +1;
    if ( DELY ==2 ) begin 	 
    R_GO  <=1;
    ST<=8 ; 
	 end
	 
	end       
8: begin 
   ST<=9 ; 
	end       
	
9: begin 
   if  ( R_END ) begin 
	       if ( CNT==0) Shunt_Voltage<=R_DATA ; 
	  else if ( CNT==1) Bus_Voltage  <=R_DATA ; 
	  else if ( CNT==2) Power        <=R_DATA ;   
	  else if ( CNT==3) Current      <=R_DATA ;   
	  else if ( CNT==4) Die_ID       <=R_DATA ;   
	  ST<=10 ; 	
	  CNT<=CNT+1 ;
	 end
	end              
10: begin      
     if  (CNT ==5)   CNT <= 0 ; 
	 
	  W_POINTER_GO <=1;
     R_GO         <=1 ;		 
	  W_WORD_GO    <=1;
		 if (( Die_ID !=16'h2260)  &&  ( CNT ==5) )  ST<=30;
		 else ST<=1;
	 end //delay
//<----------------------------------READ-----------------------	 
 
//<----------------------------------WRITE WORD-----------------
30: begin 
    ST<=31; 
	 CNT=0 ; 
    end	
31: begin 
	     if ( CNT==0) {W_WORD_REG[7:0] ,W_WORD_DATA[15:0]} <= { P_Configuration[7:0] ,Configuration  }; 
	else if ( CNT==1) {W_WORD_REG[7:0] ,W_WORD_DATA[15:0]} <= { P_Calibration  [7:0] ,Calibration    }; 
	else if ( CNT==2) {W_WORD_REG[7:0] ,W_WORD_DATA[15:0]} <= { P_Mask_Enable  [7:0] ,Mask_Enable    };  
	else if ( CNT==3) {W_WORD_REG[7:0] ,W_WORD_DATA[15:0]} <= { P_Alert_Limit  [7:0] ,Alert_Limit    };  	
	if (  W_WORD_END ) begin  W_WORD_GO  <=0; ST<=32 ;  DELY<=0;  end
	end                // Write ID pointer 
32: begin 
    DELY  <=DELY +1;
    if ( DELY ==2 ) begin 
    W_WORD_GO  <=1;
    ST<=33 ; 
	 end
	 
	end       
33: begin 
    ST<=34 ; 
	end       	
34: begin 
     if  ( W_WORD_END )  begin 
			ST<=35 ; 	
			CNT<=CNT+1 ;
	  end
	end              
35: begin 
    if (CNT ==4)  begin ST<=1 ;  CNT <=0; I2C_LO0P <= ~I2C_LO0P ; end 
	        else  ST<=31 ; 	 
	 end 
endcase 
end
//<-----------------------------MAIN-ST END ------------------------------------------
//I2C-BUS
wire   SDAO  ; 
assign PMONITOR_I2C_SCL = W_POINTER_SCL  & R_SCL   & W_WORD_SCL;
assign SDAO             = W_POINTER_SDAO & R_SDAO  & W_WORD_SDAO;
assign PMONITOR_I2C_SDA = (SDAO)?1'bz :1'b0 ; 



//==== I2C WRITE WORD ===

wire   W_WORD_SCL ; 
reg    [7:0]  W_WORD_REG ; 
reg    [15:0] W_WORD_DATA;

wire   W_WORD_SDAO ;  

I2C_WRITE_WORD  wrd(
   .RESET_N (RESET_N),
	.PT_CK        (CLK_400K),
	.GO           (W_WORD_GO),
	.POINTER      (W_WORD_REG),
   .WDATA16	     (W_WORD_DATA),
	.SLAVE_ADDRESS(SLAVE_ADDR),
	.SDAI  (PMONITOR_I2C_SDA),
	.SDAO  (W_WORD_SDAO),
	.SCLO  (W_WORD_SCL ),
	.END_OK(W_WORD_END),
	//--for test 
	.ST  (WORD_ST ),
	.CNT (WORD_CNT),
	.BYTE(WORD_BYTE),
	.ACK_OK()
	
);

//==== I2C WRITE POINTER ===

wire   W_POINTER_SCL ; 
wire   W_POINTER_END ; 
reg    W_POINTER_GO ; 
reg    [7:0] W_POINTER_REG ; 
wire   W_POINTER_SDAO ;  

I2C_WRITE_POINTER  wpt(
   .RESET_N (RESET_N),
	.PT_CK        (CLK_400K),
	.GO           (W_POINTER_GO),
	.POINTER      (W_POINTER_REG),
	.SLAVE_ADDRESS(SLAVE_ADDR),	
	.SDAI  (PMONITOR_I2C_SDA),
	.SDAO  (W_POINTER_SDAO),
	.SCLO  (W_POINTER_SCL ),
	.END_OK(W_POINTER_END),
	//--for test 
	.ST (),
	.ACK_OK(),
	.CNT (),
	.BYTE()  	
);


//==== I2C READ ===

wire R_SCL; 
wire R_END; 
reg  R_GO; 
wire R_SDAO;  


I2C_READ rd(
   .RESET_N (RESET_N),
	.PT_CK        (CLK_400K),
	.GO           (R_GO),
	.SLAVE_ADDRESS(SLAVE_ADDR),	
	.SDAI  (PMONITOR_I2C_SDA),
	.SDAO  (R_SDAO),
	.SCLO  (R_SCL),
	.END_OK(R_END),
	.DATA16(R_DATA),
	//--for test 
	.ST    (),
	.ACK_OK(),
	.CNT   (),
	.BYTE  ()  	
);
	
endmodule
	