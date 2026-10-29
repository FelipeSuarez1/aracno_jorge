// cam_capture — captura da OV7670 (S09, aracno_jorge).
// Tudo no domínio de clk (50 MHz). PCLK/HREF/VSYNC/D da câmera passam por 2 flip-flops e a borda de
// subida de PCLK é detectada por amostragem: exige PCLK ≤ ~clk/6 (com XCLK 12,5 MHz e CLKRC ÷2,
// PCLK ≈ 6,25 MHz → 8 amostras por período). Os dados são sincronizados junto com PCLK, então o byte
// amostrado na borda detectada é o mesmo que a câmera mantinha estável na borda real (o OV7670 troca
// D na descida de PCLK).
//
// Contrato com o firmware: docs/20-Specs/S09-camera-ov7670.md, seção "Contrato FPGA ↔ firmware".
// Avalon-MM slave, endereço em palavras de 32 bits, readLatency = 1 (fixa).
module cam_capture (
    input  wire        clk,
    input  wire        reset_n,
    // Avalon-MM slave
    input  wire [13:0] avs_address,
    input  wire        avs_read,
    input  wire        avs_write,
    input  wire [31:0] avs_writedata,
    output reg  [31:0] avs_readdata,
    // câmera
    input  wire        cam_pclk,
    input  wire        cam_href,
    input  wire        cam_vsync,
    input  wire [7:0]  cam_d
);
    localparam W = 160, H = 120;
    localparam MAX_BYTES = 2 * W;          // bytes por linha em YUV422
    localparam ID = 32'h43414D31;          // "CAM1"

    // ---- sincronização ----------------------------------------------------------------------
    reg [1:0] pclk_s, href_s, vsync_s;
    reg [7:0] d_s1, d_s2;
    reg       pclk_q, vsync_q, href_q;
    always @(posedge clk) begin
        pclk_s  <= {pclk_s[0],  cam_pclk};
        href_s  <= {href_s[0],  cam_href};
        vsync_s <= {vsync_s[0], cam_vsync};
        d_s1 <= cam_d; d_s2 <= d_s1;
        pclk_q  <= pclk_s[1];
        vsync_q <= vsync_s[1];
        href_q  <= href_s[1];
    end
    wire pclk_rise  = pclk_s[1] & ~pclk_q;
    wire vsync_rise = vsync_s[1] & ~vsync_q;
    wire href       = href_s[1];
    wire href_fall  = ~href_s[1] & href_q;

    // ---- memória do quadro: 4 lanes de 4800 bytes (pixel p → lane p%4, endereço p/4) ----------
    reg [7:0] lane0 [0:W*H/4-1];
    reg [7:0] lane1 [0:W*H/4-1];
    reg [7:0] lane2 [0:W*H/4-1];
    reg [7:0] lane3 [0:W*H/4-1];

    // ---- registradores --------------------------------------------------------------------------
    reg        phase;              // CTRL bit1
    reg        armed, capturing, done, overflow;
    reg [31:0] frames;
    reg [15:0] line, last_lines;
    reg [15:0] byte_in_line, last_bpl;
    reg [14:0] pix;                // índice do pixel no quadro

    // Escrita na memória num bloco sem reset (senão o Quartus não infere M10K).
    wire in_window = (line < H) && (byte_in_line < MAX_BYTES);
    wire take = capturing && !vsync_rise && href && pclk_rise && (byte_in_line[0] == phase) && in_window;
    always @(posedge clk) begin
        if (take) begin
            case (pix[1:0])
                2'd0: lane0[pix[14:2]] <= d_s2;
                2'd1: lane1[pix[14:2]] <= d_s2;
                2'd2: lane2[pix[14:2]] <= d_s2;
                2'd3: lane3[pix[14:2]] <= d_s2;
            endcase
        end
    end

    always @(posedge clk or negedge reset_n) begin
        if (!reset_n) begin
            phase <= 1'b0; armed <= 1'b0; capturing <= 1'b0; done <= 1'b0; overflow <= 1'b0;
            frames <= 32'd0; line <= 16'd0; last_lines <= 16'd0; byte_in_line <= 16'd0; last_bpl <= 16'd0;
            pix <= 15'd0;
        end else begin
            // escrita do firmware
            if (avs_write && avs_address == 14'd0) begin
                phase <= avs_writedata[1];
                if (avs_writedata[0]) begin
                    armed <= 1'b1; capturing <= 1'b0; done <= 1'b0; overflow <= 1'b0;
                end
            end

            if (vsync_rise) begin
                frames <= frames + 32'd1;
                if (capturing) begin                 // fim do quadro capturado
                    capturing <= 1'b0; done <= 1'b1; last_lines <= line;
                end else if (armed) begin            // início do quadro seguinte
                    armed <= 1'b0; capturing <= 1'b1;
                end
                line <= 16'd0; byte_in_line <= 16'd0; pix <= 15'd0;
            end else begin
                if (href && pclk_rise) begin
                    byte_in_line <= byte_in_line + 16'd1;
                    if (capturing && byte_in_line[0] == phase) begin
                        if (in_window) pix <= pix + 15'd1;
                        else           overflow <= 1'b1;
                    end
                end
                if (href_fall) begin
                    if (capturing) begin
                        last_bpl <= byte_in_line;
                        // alinha o próximo pixel ao início da próxima linha (linha curta não desloca a imagem)
                        pix <= (line + 16'd1) * W;
                    end
                    line <= line + 16'd1;
                    byte_in_line <= 16'd0;
                end
            end
        end
    end

    // ---- leitura (latência 1) -----------------------------------------------------------------
    wire        is_frame = avs_address[13];                  // palavra ≥ 0x2000
    wire [12:0] waddr    = avs_address[12:0];
    reg  [7:0]  r0, r1, r2, r3;
    reg         rd_frame;
    reg  [31:0] rd_reg;
    always @(posedge clk) begin
        r0 <= lane0[waddr]; r1 <= lane1[waddr]; r2 <= lane2[waddr]; r3 <= lane3[waddr];
        rd_frame <= is_frame;
        case (avs_address[2:0])
            3'd0: rd_reg <= {30'd0, phase, armed};
            3'd1: rd_reg <= {29'd0, overflow, (armed | capturing), done};
            3'd2: rd_reg <= frames;
            3'd3: rd_reg <= {16'd0, last_lines};
            3'd4: rd_reg <= {16'd0, last_bpl};
            3'd5: rd_reg <= ID;
            default: rd_reg <= 32'd0;
        endcase
    end
    always @(*) avs_readdata = rd_frame ? {r3, r2, r1, r0} : rd_reg;
endmodule
