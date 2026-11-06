// medula — olhar que segue, reflexo de susto e watchdog de servos (S11, aracno_jorge).
// Tudo no domínio de clk (50 MHz). Contrato com o firmware: docs/20-Specs/S11-medula-fpga.md, seção
// "Contrato FPGA ↔ firmware". Avalon-MM slave, endereço em palavras de 32 bits, readLatency = 1 (fixa).
//
// Visão geral:
//   - FRAME/BG: 160x120 de profundidade de 8 bits, em 4 lanes de 4800 bytes (pixel p → lane p%4, endereço p/4).
//   - GO: 1 pixel por ciclo (19 200 ciclos), erosão (esquerda e acima), fundo adaptativo, depois 3 divisões
//     sequenciais (centroide e profundidade média), alvo da cabeça, reflexo de susto e DONE.
//   - Cabeça: PWM próprio (pan = canal 18, tilt = canal 19) com slew aplicado só no início do período de 20 ms.
//   - Saída: pwm_out = pwm_in com troca de fonte e corte do watchdog só quando a fonte está em 0 (ou presa em 1
//     por mais de HIGH_MAX), para nunca deformar um pulso.
//   - Watchdog: corta os 20 canais se o firmware não der KICK dentro de WD_TIMEOUT_MS.
// reset_n não é usado: o reset do Qsys nunca é ativado depois da configuração da FPGA (todo estado tem
// inicializador, que vale como valor de power-up).
module medula (
    input  wire        clk,
    input  wire        reset_n,
    // Avalon-MM slave
    input  wire [14:0] avs_address,
    input  wire        avs_read,
    input  wire        avs_write,
    input  wire [31:0] avs_writedata,
    output reg  [31:0] avs_readdata,
    // PWM: entrada (Terasic) e saída (para os pinos)
    input  wire [19:0] pwm_in,
    output reg  [19:0] pwm_out
);
    localparam [31:0] ID = 32'h4D454431;                 // "MED1"
    localparam        NPIX = 19200;                      // 160 x 120
    localparam [11:0] PAN_MIN_US  = 12'd833;
    localparam [11:0] PAN_MAX_US  = 12'd2167;
    localparam [11:0] TILT_MIN_US = 12'd1222;
    localparam [11:0] TILT_MAX_US = 12'd1778;
    localparam [11:0] CENTER_US   = 12'd1500;
    localparam [19:0] PERIOD      = 20'd1000000;         // 20 ms
    localparam [16:0] HIGH_MAX    = 17'd125000;          // 2,5 ms (firmware/src/motor/pwm.h)

    localparam [3:0] S_IDLE = 4'd0, S_RUN = 4'd1,
                     S_DIV0 = 4'd2, S_DIV0W = 4'd3, S_DIV1 = 4'd4, S_DIV1W = 4'd5,
                     S_DIV2 = 4'd6, S_DIV2W = 4'd7, S_FIN1 = 4'd8, S_FIN2 = 4'd9;

    // ---- estado (todo com inicializador = valor de power-up) -------------------------------------
    reg  [3:0]  st = S_IDLE;
    wire        busy = (st != S_IDLE);

    reg         follow_en = 1'b0, reflex_en = 1'b0;
    reg         done = 1'b0, reflex_flag = 1'b0, wdrop = 1'b0;
    reg  [31:0] frames = 32'd0;
    reg  [31:0] lat = 32'd0, lat_cnt = 32'd0;
    reg  [31:0] reflex_cnt = 32'd0;

    // configuração visível
    reg  [7:0]  cfg_fg = 8'd8;
    reg  [15:0] cfg_cnt = 16'd40;
    reg  [7:0]  cfg_near = 8'd38, cfg_drop = 8'd4;
    reg  [15:0] cfg_hold = 16'd1500;
    reg  [15:0] cfg_pan_gain = 16'd66, cfg_tilt_gain = 16'd68;
    reg  [15:0] cfg_slew = 16'd26;
    // cópias internas, tiradas no GO aceito
    reg  [7:0]  l_fg = 8'd8;
    reg  [15:0] l_cnt = 16'd40;
    reg  [7:0]  l_near = 8'd38, l_drop = 8'd4;
    reg  [15:0] l_hold = 16'd1500;
    reg  signed [15:0] l_pan_gain = 16'sd66, l_tilt_gain = 16'sd68;
    reg  [15:0] l_slew = 16'd26;
    reg         l_bgreset = 1'b0;
    reg  [31:0] fi = 32'd0;                              // FRAMES no GO

    // resultados (travados no DONE)
    reg         r_target = 1'b0;
    reg  [15:0] r_count = 16'd0;
    reg  [7:0]  r_cx = 8'd0, r_cy = 8'd0, r_min = 8'd0, r_mean = 8'd0;
    reg  [7:0]  r_x0 = 8'd0, r_y0 = 8'd0, r_x1 = 8'd0, r_y1 = 8'd0;
    reg  [11:0] r_head_tilt = 12'd1500;                  // HEAD_TARGET[31:16]
    reg  [11:0] pan_t = 12'd1500, tilt_t = 12'd1500;     // último alvo (persiste entre quadros)
    reg         prev_target = 1'b0;
    reg  [7:0]  prev_min = 8'd0;
    reg         fire_ev = 1'b0;                          // pulso de 1 ciclo quando o reflexo dispara

    // watchdog
    reg         wd_enable = 1'b0, wd_tripped = 1'b0;
    reg  [15:0] wd_ms = 16'd0, wd_pre = 16'd0, wd_timeout = 16'd100;

    // reflexo ativo (segura TILT_MAX por HOLD_MS)
    reg  [15:0] hold_left = 16'd0, hold_pre = 16'd0;
    wire        reflex_active = (hold_left != 16'd0);
    wire        head_override = follow_en | reflex_active;

    // ---- decodificação estrita --------------------------------------------------------------------
    wire        reg_sel = (avs_address[14:5] == 10'd0) && (avs_address[4:0] <= 5'd18);
    wire        frm_sel = (avs_address[14:13] == 2'b01) && (avs_address[12:0] < 13'd4800);
    wire        bg_sel  = (avs_address[14:13] == 2'b10) && (avs_address[12:0] < 13'd4800);
    wire [4:0]  ra      = avs_address[4:0];
    wire        reg_wr  = avs_write && reg_sel;
    wire        go      = reg_wr && (ra == 5'd0) && avs_writedata[0] && !busy;
    wire        follow_rise = reg_wr && (ra == 5'd0) && avs_writedata[2] && !follow_en;

    // ---- memórias: 4 lanes de 4800 bytes para FRAME e para BG ---------------------------------------
    // Escritas em blocos sem reset e sem initial (senão o Quartus não infere M10K); leitura síncrona.
    reg [7:0] frame0 [0:4799];
    reg [7:0] frame1 [0:4799];
    reg [7:0] frame2 [0:4799];
    reg [7:0] frame3 [0:4799];
    reg [7:0] bg0 [0:4799];
    reg [7:0] bg1 [0:4799];
    reg [7:0] bg2 [0:4799];
    reg [7:0] bg3 [0:4799];

    // pipeline de pixels (declarações adiantadas porque as RAMs usam bw_*)
    reg  [3:0]  bw_we = 4'd0;
    reg  [12:0] bw_addr = 13'd0;
    reg  [7:0]  bw_data = 8'd0;
    reg  [14:0] q = 15'd0;                               // pixel sendo endereçado (estágio A)

    wire        frm_we = avs_write && frm_sel && !busy;
    wire [12:0] raddr  = busy ? q[14:2] : avs_address[12:0];
    reg  [7:0]  f0, f1, f2, f3, g0, g1, g2, g3;

    always @(posedge clk) begin
        if (frm_we) begin
            frame0[avs_address[12:0]] <= avs_writedata[7:0];
            frame1[avs_address[12:0]] <= avs_writedata[15:8];
            frame2[avs_address[12:0]] <= avs_writedata[23:16];
            frame3[avs_address[12:0]] <= avs_writedata[31:24];
        end
    end
    always @(posedge clk) if (bw_we[0]) bg0[bw_addr] <= bw_data;
    always @(posedge clk) if (bw_we[1]) bg1[bw_addr] <= bw_data;
    always @(posedge clk) if (bw_we[2]) bg2[bw_addr] <= bw_data;
    always @(posedge clk) if (bw_we[3]) bg3[bw_addr] <= bw_data;
    always @(posedge clk) begin
        f0 <= frame0[raddr]; f1 <= frame1[raddr]; f2 <= frame2[raddr]; f3 <= frame3[raddr];
        g0 <= bg0[raddr];    g1 <= bg1[raddr];    g2 <= bg2[raddr];    g3 <= bg3[raddr];
    end

    // ---- pipeline de 1 pixel por ciclo ---------------------------------------------------------------
    // A: endereça FRAME/BG (palavra q>>2). B: dados chegam (latência 1), fg_raw/erosão/novo fundo.
    // C: acumula. A escrita do novo fundo é feita 1 ciclo depois do B, só na lane do pixel.
    reg         issue = 1'b0;
    reg  [7:0]  qx = 8'd0;
    reg  [6:0]  qy = 7'd0;
    reg         vb = 1'b0, vc = 1'b0, last_c = 1'b0, fgc = 1'b0;
    reg  [14:0] bp = 15'd0;
    reg  [7:0]  bx = 8'd0;
    reg  [6:0]  by = 7'd0;
    reg  [31:0] bg_hold = 32'd0;
    reg  [159:0] sh = 160'd0;                            // fg_raw dos últimos 160 pixels
    reg  [7:0]  cur_c = 8'd0, cx_c = 8'd0;
    reg  [6:0]  cy_c = 7'd0;
    reg  [15:0] count = 16'd0;
    reg  [23:0] sum_x = 24'd0, sum_y = 24'd0, sum_d = 24'd0;
    reg  [7:0]  acc_min = 8'd255, acc_x0 = 8'd255, acc_y0 = 8'd255, acc_x1 = 8'd0, acc_y1 = 8'd0;

    wire [7:0]  cur_b = (bp[1:0] == 2'd0) ? f0 : (bp[1:0] == 2'd1) ? f1 : (bp[1:0] == 2'd2) ? f2 : f3;
    wire [31:0] bgw   = (bp[1:0] == 2'd0) ? {g3, g2, g1, g0} : bg_hold;
    wire [7:0]  bg_b  = (bp[1:0] == 2'd0) ? bgw[7:0]   : (bp[1:0] == 2'd1) ? bgw[15:8] :
                        (bp[1:0] == 2'd2) ? bgw[23:16] : bgw[31:24];
    wire [8:0]  dlt   = {1'b0, bg_b} - {1'b0, cur_b};
    wire        fg_raw = (cur_b != 8'd0) && (bg_b != 8'd0) && (bg_b > cur_b) && (dlt >= {1'b0, l_fg});
    wire        left_fg = (bx != 8'd0) && sh[0];
    wire        up_fg   = (by != 7'd0) && sh[159];
    wire        fg_clean = fg_raw && left_fg && up_fg;
    wire [7:0]  bg_step = bg_b + ((cur_b > bg_b) ? 8'd1 : 8'd0) - ((cur_b < bg_b) ? 8'd1 : 8'd0);
    wire [7:0]  nb = l_bgreset       ? cur_b :
                     (bg_b == 8'd0)  ? cur_b :
                     (cur_b == 8'd0) ? bg_b  :
                     fg_raw          ? ((fi[3:0] == 4'd0) ? bg_step : bg_b) :
                                       bg_step;
    wire        pix_end = vc && last_c;

    always @(posedge clk) begin
        // estágio A
        vb <= issue;
        if (go) begin
            issue <= 1'b1; q <= 15'd0; qx <= 8'd0; qy <= 7'd0;
        end else if (issue) begin
            bp <= q; bx <= qx; by <= qy;
            if (q == NPIX - 1) issue <= 1'b0; else q <= q + 15'd1;
            if (qx == 8'd159) begin qx <= 8'd0; qy <= qy + 7'd1; end else qx <= qx + 8'd1;
        end
        // estágio B
        if (vb) begin
            if (bp[1:0] == 2'd0) bg_hold <= {g3, g2, g1, g0};
            sh <= {sh[158:0], fg_raw};
        end
        bw_we   <= vb ? (4'b0001 << bp[1:0]) : 4'b0000;
        bw_addr <= bp[14:2];
        bw_data <= nb;
        vc <= vb; cur_c <= cur_b; fgc <= fg_clean; cx_c <= bx; cy_c <= by; last_c <= (bp == NPIX - 1);
        // estágio C
        if (go) begin
            count <= 16'd0; sum_x <= 24'd0; sum_y <= 24'd0; sum_d <= 24'd0;
            acc_min <= 8'd255; acc_x0 <= 8'd255; acc_y0 <= 8'd255; acc_x1 <= 8'd0; acc_y1 <= 8'd0;
        end else if (vc && fgc) begin
            count <= count + 16'd1;
            sum_x <= sum_x + {16'd0, cx_c};
            sum_y <= sum_y + {17'd0, cy_c};
            sum_d <= sum_d + {16'd0, cur_c};
            if (cur_c < acc_min) acc_min <= cur_c;
            if (cx_c < acc_x0) acc_x0 <= cx_c;
            if (cx_c > acc_x1) acc_x1 <= cx_c;
            if ({1'b0, cy_c} < acc_y0) acc_y0 <= {1'b0, cy_c};
            if ({1'b0, cy_c} > acc_y1) acc_y1 <= {1'b0, cy_c};
        end
    end

    // ---- divisor sequencial -------------------------------------------------------------------------
    reg         div_start = 1'b0;
    reg  [23:0] div_num = 24'd0;
    reg  [15:0] div_den = 16'd0;
    wire        div_done;
    wire [23:0] div_quo;
    medula_div u_div (.clk(clk), .start(div_start), .num(div_num), .den(div_den), .done(div_done), .quo(div_quo));

    // ---- alvo da cabeça e reflexo (combinacional sobre os resultados já divididos) ---------------------
    reg  [7:0]  rx = 8'd0, ry = 8'd0, rd = 8'd0;
    reg         tgt_w = 1'b0;
    reg  [11:0] pan_calc = 12'd1500, tilt_calc = 12'd1500;

    wire signed [8:0]  dxp = {1'b0, rx} - 9'd80;
    wire signed [8:0]  dyp = {1'b0, ry} - 9'd60;
    wire signed [24:0] pan_prod  = dxp * l_pan_gain;
    wire signed [24:0] tilt_prod = dyp * l_tilt_gain;
    wire signed [25:0] pan_raw  = 26'sd1500 + (pan_prod >>> 4);      // deslocamento aritmético (floor)
    wire signed [25:0] tilt_raw = 26'sd1500 - (tilt_prod >>> 4);
    wire [11:0] pan_clamped  = (pan_raw  < 26'sd833)  ? PAN_MIN_US  : (pan_raw  > 26'sd2167) ? PAN_MAX_US  : pan_raw[11:0];
    wire [11:0] tilt_clamped = (tilt_raw < 26'sd1222) ? TILT_MIN_US : (tilt_raw > 26'sd1778) ? TILT_MAX_US : tilt_raw[11:0];

    wire        valid_w = tgt_w && (count != 16'd0);                 // cx, cy, mean, min e bbox só valem com alvo
    wire [7:0]  m_min = valid_w ? acc_min : 8'd0;                    // min_depth informado
    // Reflexo: soma em 9 bits (nunca subtração sem sinal).
    wire        fire_w = reflex_en && tgt_w && prev_target &&
                         ({1'b0, prev_min} >= ({1'b0, m_min} + {1'b0, l_drop})) &&
                         (m_min <= l_near);

    // ---- registradores legíveis -----------------------------------------------------------------------
    reg  [11:0] pan_us = 12'd1500, tilt_us = 12'd1500;
    wire [31:0] status = {25'd0, wdrop, head_override, wd_tripped, reflex_flag, r_target, busy, done};

    reg [31:0] regval;
    always @(*) begin
        case (ra)
            5'd0:  regval = {28'd0, reflex_en, follow_en, 2'b00};
            5'd1:  regval = status;
            5'd2:  regval = frames;
            5'd3:  regval = {r_count, r_cy, r_cx};
            5'd4:  regval = {16'd0, r_mean, r_min};
            5'd5:  regval = ID;
            5'd6:  regval = {r_y1, r_x1, r_y0, r_x0};
            5'd7:  regval = lat;
            5'd8:  regval = {8'd0, cfg_cnt, cfg_fg};
            5'd9:  regval = {cfg_hold, cfg_drop, cfg_near};
            5'd10: regval = {16'd0, cfg_pan_gain};
            5'd11: regval = {16'd0, cfg_tilt_gain};
            5'd12: regval = {16'd0, cfg_slew};
            5'd13: regval = {4'd0, tilt_us, 4'd0, pan_us};
            5'd14: regval = {30'd0, wd_tripped, wd_enable};
            5'd15: regval = {16'd0, wd_ms};
            5'd16: regval = {16'd0, wd_timeout};
            5'd17: regval = reflex_cnt;
            5'd18: regval = {4'd0, r_head_tilt, 4'd0, pan_t};
            default: regval = 32'd0;
        endcase
    end

    // ---- leitura (latência 1) ---------------------------------------------------------------------------
    reg  [1:0]  rd_kind = 2'd0;                          // 0 = registrador/zero, 1 = FRAME, 2 = BG
    reg  [31:0] rd_reg = 32'd0;
    always @(posedge clk) begin
        rd_kind <= busy ? 2'd0 : frm_sel ? 2'd1 : bg_sel ? 2'd2 : 2'd0;
        rd_reg  <= reg_sel ? regval : 32'd0;
    end
    always @(*) begin
        case (rd_kind)
            2'd1:    avs_readdata = {f3, f2, f1, f0};
            2'd2:    avs_readdata = {g3, g2, g1, g0};
            default: avs_readdata = rd_reg;
        endcase
    end

    // ---- escritas do barramento + máquina de estados ------------------------------------------------------
    always @(posedge clk) begin
        fire_ev   <= 1'b0;
        div_start <= 1'b0;
        if (busy) lat_cnt <= lat_cnt + 32'd1;

        if (reg_wr) begin
            case (ra)
                5'd0: begin
                    follow_en <= avs_writedata[2];
                    reflex_en <= avs_writedata[3];
                    if (go) begin
                        st <= S_RUN; done <= 1'b0; lat_cnt <= 32'd0;
                        l_bgreset <= avs_writedata[1];
                        fi <= frames;
                        l_fg <= cfg_fg; l_cnt <= cfg_cnt; l_near <= cfg_near; l_drop <= cfg_drop;
                        l_hold <= cfg_hold; l_pan_gain <= cfg_pan_gain; l_tilt_gain <= cfg_tilt_gain;
                        l_slew <= cfg_slew;
                    end
                end
                5'd1: begin                                // W1C
                    if (avs_writedata[3]) reflex_flag <= 1'b0;
                    if (avs_writedata[6]) wdrop <= 1'b0;
                end
                5'd8:  begin cfg_fg <= avs_writedata[7:0]; cfg_cnt <= avs_writedata[23:8]; end
                5'd9:  begin cfg_near <= avs_writedata[7:0]; cfg_drop <= avs_writedata[15:8]; cfg_hold <= avs_writedata[31:16]; end
                5'd10: cfg_pan_gain  <= avs_writedata[15:0];
                5'd11: cfg_tilt_gain <= avs_writedata[15:0];
                5'd12: cfg_slew      <= avs_writedata[15:0];
                default: ;
            endcase
        end
        if (avs_write && frm_sel && busy) wdrop <= 1'b1;   // escrita em FRAME durante BUSY: descartada

        case (st)
            S_RUN: if (pix_end) st <= S_DIV0;
            S_DIV0: begin
                tgt_w <= (count >= l_cnt);
                if (count == 16'd0) begin                  // sem pixels: nada a dividir
                    rx <= 8'd0; ry <= 8'd0; rd <= 8'd0; st <= S_FIN1;
                end else begin
                    div_num <= sum_x; div_den <= count; div_start <= 1'b1; st <= S_DIV0W;
                end
            end
            S_DIV0W: if (div_done) begin rx <= div_quo[7:0]; st <= S_DIV1; end
            S_DIV1: begin div_num <= sum_y; div_den <= count; div_start <= 1'b1; st <= S_DIV1W; end
            S_DIV1W: if (div_done) begin ry <= div_quo[7:0]; st <= S_DIV2; end
            S_DIV2: begin div_num <= sum_d; div_den <= count; div_start <= 1'b1; st <= S_DIV2W; end
            S_DIV2W: if (div_done) begin rd <= div_quo[7:0]; st <= S_FIN1; end
            S_FIN1: begin
                pan_calc <= pan_clamped; tilt_calc <= tilt_clamped; st <= S_FIN2;
            end
            S_FIN2: begin
                r_target <= tgt_w;
                r_count  <= count;
                r_cx <= valid_w ? rx : 8'd0; r_cy <= valid_w ? ry : 8'd0;
                r_mean <= valid_w ? rd : 8'd0; r_min <= m_min;
                r_x0 <= valid_w ? acc_x0 : 8'd0;
                r_y0 <= valid_w ? acc_y0 : 8'd0;
                r_x1 <= valid_w ? acc_x1 : 8'd0;
                r_y1 <= valid_w ? acc_y1 : 8'd0;
                if (tgt_w) begin pan_t <= pan_calc; tilt_t <= tilt_calc; end
                r_head_tilt <= fire_w ? TILT_MAX_US : (tgt_w ? tilt_calc : tilt_t);
                prev_target <= tgt_w;
                prev_min <= m_min;
                if (fire_w) begin
                    reflex_flag <= 1'b1;
                    reflex_cnt <= reflex_cnt + 32'd1;
                    fire_ev <= 1'b1;
                end
                frames <= frames + 32'd1;
                lat <= lat_cnt + 32'd1;
                done <= 1'b1;
                st <= S_IDLE;
            end
            default: ;
        endcase
    end

    // ---- watchdog -------------------------------------------------------------------------------------
    // Prescaler próprio: KICK, CLEAR e a escrita com ENABLE=1 zeram wd_ms e o prescaler.
    always @(posedge clk) begin
        if (wd_pre == 16'd49999) begin
            wd_pre <= 16'd0;
            if (wd_ms != 16'hFFFF) wd_ms <= wd_ms + 16'd1;
        end else begin
            wd_pre <= wd_pre + 16'd1;
        end
        if (wd_enable && wd_ms >= wd_timeout) wd_tripped <= 1'b1;
        if (reg_wr) begin
            case (ra)
                5'd14: begin
                    wd_enable <= avs_writedata[0];
                    wd_ms <= 16'd0; wd_pre <= 16'd0;
                    if (!avs_writedata[0] || avs_writedata[1]) wd_tripped <= 1'b0;
                end
                5'd15: begin wd_ms <= 16'd0; wd_pre <= 16'd0; end
                5'd16: wd_timeout <= (avs_writedata[15:0] < 16'd20) ? 16'd20 :
                                     (avs_writedata[15:0] > 16'd1000) ? 16'd1000 : avs_writedata[15:0];
                default: ;
            endcase
        end
    end

    // ---- cabeça: PWM próprio -------------------------------------------------------------------------------
    function [11:0] step_toward;
        input [11:0] cur, tgt;
        input [15:0] slew;
        input [11:0] lo, hi;
        reg signed [17:0] c, t, s, d, n, l, h;
        begin
            c = {6'd0, cur}; t = {6'd0, tgt}; s = {2'd0, slew};
            l = {6'd0, lo};  h = {6'd0, hi};
            d = t - c;
            if (d > s)       n = c + s;
            else if (d < -s) n = c - s;
            else             n = t;
            if (n < l)       n = l;
            else if (n > h)  n = h;
            step_toward = n[11:0];
        end
    endfunction

    reg  [19:0] tick = 20'd0;
    reg  [17:0] pan_cyc = 18'd75000, tilt_cyc = 18'd75000;
    reg         follow_pend = 1'b0;                      // FOLLOW_EN 0→1: recentra no próximo período
    wire        head0 = (tick < {2'b00, pan_cyc});
    wire        head1 = (tick < {2'b00, tilt_cyc});

    always @(posedge clk) begin
        pan_cyc  <= pan_us  * 18'd50;
        tilt_cyc <= tilt_us * 18'd50;
        // reflexo: contador de ms próprio; recarrega se disparar de novo
        if (hold_left != 16'd0) begin
            if (hold_pre == 16'd49999) begin hold_pre <= 16'd0; hold_left <= hold_left - 16'd1; end
            else hold_pre <= hold_pre + 16'd1;
        end
        if (fire_ev) begin hold_left <= l_hold; hold_pre <= 16'd0; end

        if (tick == PERIOD - 20'd1) begin
            tick <= 20'd0;
            follow_pend <= 1'b0;
            pan_us  <= follow_pend ? CENTER_US : step_toward(pan_us, pan_t, l_slew, PAN_MIN_US, PAN_MAX_US);
            tilt_us <= reflex_active ? TILT_MAX_US :
                       follow_pend   ? CENTER_US :
                       step_toward(tilt_us, tilt_t, l_slew, TILT_MIN_US, TILT_MAX_US);
        end else begin
            tick <= tick + 20'd1;
        end
        if (follow_rise) follow_pend <= 1'b1;
    end

    // ---- troca de fonte e corte do watchdog sem pulso deformado ---------------------------------------------
    reg  [1:0]  sel_head = 2'b00;
    always @(posedge clk) begin
        if (!pwm_in[18] && !head0) sel_head[0] <= head_override;
        if (!pwm_in[19] && !head1) sel_head[1] <= head_override;
    end
    wire [19:0] src = {sel_head[1] ? head1 : pwm_in[19],
                       sel_head[0] ? head0 : pwm_in[18],
                       pwm_in[17:0]};

    reg  [19:0] gate;
    initial begin
        pwm_out = 20'd0;
        gate    = 20'hFFFFF;
    end
    genvar k;
    generate
        for (k = 0; k < 20; k = k + 1) begin : ch
            reg [16:0] hi = 17'd0;                      // ciclos seguidos com a fonte em 1 (satura em HIGH_MAX)
            always @(posedge clk) begin
                if (!src[k])            hi <= 17'd0;
                else if (hi < HIGH_MAX) hi <= hi + 17'd1;
                if (!src[k] || hi >= HIGH_MAX) gate[k] <= !wd_tripped;
                pwm_out[k] <= gate[k] & src[k];
            end
        end
    endgenerate
endmodule

// Divisor restaurador sequencial: num (24 bits) / den (16 bits), 24 ciclos. done pulsa 1 ciclo com quo válido.
// Com den = 0 o resultado é indefinido (quem chama não divide por zero).
module medula_div (
    input  wire        clk,
    input  wire        start,
    input  wire [23:0] num,
    input  wire [15:0] den,
    output reg         done = 1'b0,
    output reg  [23:0] quo  = 24'd0
);
    reg         busy = 1'b0;
    reg  [23:0] n = 24'd0;
    reg  [15:0] d = 16'd0, rem = 16'd0;
    reg  [4:0]  cnt = 5'd0;
    wire [16:0] trial = {rem, n[23]};
    wire        ge = (trial >= {1'b0, d});
    always @(posedge clk) begin
        done <= 1'b0;
        if (start) begin
            n <= num; d <= den; rem <= 16'd0; quo <= 24'd0; cnt <= 5'd24; busy <= 1'b1;
        end else if (busy) begin
            n   <= {n[22:0], 1'b0};
            rem <= ge ? (trial[15:0] - d) : trial[15:0];
            quo <= {quo[22:0], ge};
            cnt <= cnt - 5'd1;
            if (cnt == 5'd1) begin busy <= 1'b0; done <= 1'b1; end
        end
    end
endmodule
