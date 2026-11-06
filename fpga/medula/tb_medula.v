`timescale 1ns/1ps
// Testbench do medula (S11): cenas de profundidade sintéticas S0..S7, sequência F0..F19 do plano
// (centroide, bbox, reflexo, fundo adaptativo, BG_RESET), cabeça com slew, troca de fonte sem pulso deformado,
// watchdog, decodificação estrita. Gerador de pwm_in: canais 0–17 com 1500 us, 18/19 com 1000 us, período de
// 20 ms, fase deslocada de 7 ms. Imprime "TB OK: medula ..." ou "TB FALHOU: N erros".
module tb_medula;
    reg clk = 0, reset_n = 0;
    always #10 clk = ~clk;                       // 50 MHz

    reg  [14:0] address = 0;
    reg         read = 0, write = 0;
    reg  [31:0] writedata = 0;
    wire [31:0] readdata;
    reg  [19:0] pwm_in = 0;
    wire [19:0] pwm_out;

    medula dut(.clk(clk), .reset_n(reset_n), .avs_address(address), .avs_read(read), .avs_write(write),
               .avs_writedata(writedata), .avs_readdata(readdata), .pwm_in(pwm_in), .pwm_out(pwm_out));

    integer errors = 0;
    integer ftag = 0;                            // quadro corrente, para as mensagens
    task check(input integer cond, input [8*64-1:0] msg);
        begin if (!cond) begin $display("FALHA: %0s", msg); errors = errors + 1; end end
    endtask
    task check_eq(input [31:0] got, input [31:0] exp, input [8*64-1:0] msg);
        begin
            if (got !== exp) begin
                $display("FALHA F%0d: %0s: obtido %0d (0x%h), esperado %0d (0x%h)", ftag, msg, got, got, exp, exp);
                errors = errors + 1;
            end
        end
    endtask

    // ---- Avalon -------------------------------------------------------------------------------------
    task av_write(input [14:0] a, input [31:0] v);
        begin @(posedge clk); address <= a; writedata <= v; write <= 1; @(posedge clk); write <= 0; end
    endtask
    task av_read(input [14:0] a, output [31:0] v);
        begin @(posedge clk); address <= a; read <= 1; @(posedge clk); read <= 0; #1 v = readdata; end
    endtask

    // ---- gerador de pwm_in ---------------------------------------------------------------------------
    reg     stuck5 = 0;                          // prende pwm_in[5] em 1
    integer gt = 0, gph = 0;
    always @(posedge clk) begin
        gt  = (gt == 999999) ? 0 : gt + 1;
        gph = (gt >= 350000) ? gt - 350000 : gt + 650000;   // fase +7 ms em relação ao reset
        pwm_in[17:0]  <= (gph < 75000) ? 18'h3FFFF : 18'h00000;
        pwm_in[19:18] <= (gph < 50000) ? 2'b11 : 2'b00;
        if (stuck5) pwm_in[5] <= 1'b1;
    end

    // ---- monitores ---------------------------------------------------------------------------------------
    // largura dos pulsos de pwm_out[18]/[19] em ciclos; nenhum pode ficar fora de [833, 2167] us
    integer wc18 = 0, wc19 = 0, lw18 = 0, lw19 = 0;
    always @(posedge clk) begin
        if (pwm_out[18]) wc18 = wc18 + 1;
        else if (wc18 != 0) begin
            lw18 = wc18;
            if (wc18 < 833 * 50 || wc18 > 2167 * 50) begin
                $display("FALHA: pulso de pwm_out[18] com %0d ciclos fora de [41650, 108350]", wc18); errors = errors + 1;
            end
            wc18 = 0;
        end
        if (pwm_out[19]) wc19 = wc19 + 1;
        else if (wc19 != 0) begin
            lw19 = wc19;
            if (wc19 < 833 * 50 || wc19 > 2167 * 50) begin
                $display("FALHA: pulso de pwm_out[19] com %0d ciclos fora de [41650, 108350]", wc19); errors = errors + 1;
            end
            wc19 = 0;
        end
    end

    // passagem direta (pwm_out = pwm_in atrasado 1 ciclo) e corte total
    reg         pass_chk = 0, zero_chk = 0, pass_seen = 0;
    reg  [19:0] pwm_in_d = 0;
    integer     perr = 0;
    always @(posedge clk) pwm_in_d <= pwm_in;
    always @(negedge clk) begin
        if (pass_chk) begin
            if (pwm_out[0]) pass_seen = 1;
            if (pwm_out !== pwm_in_d) begin
                if (perr < 5) $display("FALHA: passagem direta: pwm_out=%h pwm_in(atrasado)=%h t=%0t", pwm_out, pwm_in_d, $time);
                perr = perr + 1; errors = errors + 1;
            end
        end
        if (zero_chk && pwm_out !== 20'd0) begin
            if (perr < 5) $display("FALHA: corte do watchdog: pwm_out=%h deveria ser 0 t=%0t", pwm_out, $time);
            perr = perr + 1; errors = errors + 1;
        end
    end

    time t_trip = 0;
    always @(posedge dut.wd_tripped) t_trip = $time;

    // ---- cenas de profundidade -----------------------------------------------------------------------------
    reg [7:0] img [0:19199];
    task make_scene(input integer id);
        integer x, y, k, rv;
        begin
            for (y = 0; y < 120; y = y + 1) for (x = 0; x < 160; x = x + 1) img[y * 160 + x] = 8'd150;
            if (id == 5)
                for (y = 0; y < 120; y = y + 1) for (x = 0; x < 10; x = x + 1) img[y * 160 + x] = 8'd0;
            if (id == 1 || id == 2 || id == 3 || id == 4 || id == 6) begin
                rv = (id == 2) ? 40 : (id == 3) ? 34 : (id == 4) ? 36 : 50;
                for (y = 40; y <= 69; y = y + 1) for (x = 60; x <= 79; x = x + 1) img[y * 160 + x] = rv;
                for (k = 0; k < 20; k = k + 1) img[100 * 160 + 10 + 7 * k] = 8'd50;
                if (id == 6)
                    for (y = 0; y < 120; y = y + 1) for (x = 0; x < 10; x = x + 1) img[y * 160 + x] = 8'd100;
            end
            if (id == 7)
                for (y = 20; y <= 24; y = y + 1) for (x = 100; x <= 104; x = x + 1) img[y * 160 + x] = 8'd50;
        end
    endtask

    task upload;
        integer w;
        begin
            for (w = 0; w < 4800; w = w + 1)
                av_write(15'h2000 + w, {img[4 * w + 3], img[4 * w + 2], img[4 * w + 1], img[4 * w]});
        end
    endtask

    reg [31:0] modes = 0;                        // bits 2/3 do CTRL (FOLLOW_EN, REFLEX_EN)
    reg [31:0] v, v2;
    reg [31:0] r_st, r_xy, r_dp, r_bb, r_lat, r_hd, r_rc;
    integer    i, n, lat_first, lat_target;
    time       t0;

    task start_frame(input integer sc, input integer bgr);
        begin make_scene(sc); upload; av_write(15'd0, 32'h1 | (bgr ? 32'h2 : 32'h0) | modes); end
    endtask
    task wait_done;
        begin
            n = 0; v = 0;
            while (!v[0] && n < 100000) begin av_read(15'd1, v); n = n + 1; end
            check(v[0] == 1, "DONE nunca subiu (timeout)");
        end
    endtask
    task read_results;
        begin
            av_read(15'd1, r_st); av_read(15'd3, r_xy); av_read(15'd4, r_dp); av_read(15'd6, r_bb);
            av_read(15'd7, r_lat); av_read(15'd18, r_hd); av_read(15'd17, r_rc);
            if (r_lat < 19200 || r_lat > 19400) begin
                $display("FALHA F%0d: LATENCY %0d fora de [19200, 19400]", ftag, r_lat); errors = errors + 1;
            end
        end
    endtask
    task chk_frame(input integer target, input integer count, input integer cx, input integer cy,
                   input integer mn, input integer mean, input integer x0, input integer y0,
                   input integer x1, input integer y1, input integer hpan, input integer htilt);
        begin
            check_eq(r_st[2], target, "STATUS.TARGET");
            check_eq(r_st[0], 1, "STATUS.DONE");
            check_eq(r_st[1], 0, "STATUS.BUSY");
            check_eq(r_xy[31:16], count, "count");
            check_eq(r_xy[7:0], cx, "cx");
            check_eq(r_xy[15:8], cy, "cy");
            check_eq(r_dp[7:0], mn, "min_depth");
            check_eq(r_dp[15:8], mean, "mean_depth");
            check_eq(r_bb[7:0], x0, "bbox x0");
            check_eq(r_bb[15:8], y0, "bbox y0");
            check_eq(r_bb[23:16], x1, "bbox x1");
            check_eq(r_bb[31:24], y1, "bbox y1");
            check_eq(r_hd[15:0], hpan, "HEAD_TARGET pan");
            check_eq(r_hd[31:16], htilt, "HEAD_TARGET tilt");
        end
    endtask
    task wait_edge18;
        begin @(posedge pwm_out[18]); repeat (2) @(posedge clk); end
    endtask
    task frame(input integer sc, input integer bgr);
        begin start_frame(sc, bgr); wait_done; read_results; end
    endtask
    function [31:0] bgbyte(input [31:0] w, input integer lane);
        begin bgbyte = (w >> (8 * lane)) & 32'hFF; end
    endfunction

    initial begin
        #100 reset_n = 1;
        // ---- valores de reset, ID e decodificação estrita ----
        av_read(15'd5, v);  check_eq(v, 32'h4D454431, "ID");
        av_read(15'd0, v);  check_eq(v, 0, "CTRL reset");
        av_read(15'd1, v);  check_eq(v, 0, "STATUS reset");
        av_read(15'd2, v);  check_eq(v, 0, "FRAMES reset");
        av_read(15'd8, v);  check_eq(v, 32'h00002808, "THRESH reset");
        av_read(15'd9, v);  check_eq(v, 32'h05DC0426, "REFLEX_CFG reset");
        av_read(15'd10, v); check_eq(v, 66, "PAN_GAIN reset");
        av_read(15'd11, v); check_eq(v, 68, "TILT_GAIN reset");
        av_read(15'd12, v); check_eq(v, 26, "SLEW_US reset");
        av_read(15'd13, v); check_eq(v, 32'h05DC05DC, "HEAD_US reset");
        av_read(15'd14, v); check_eq(v, 0, "WD_CTRL reset");
        av_read(15'd16, v); check_eq(v, 100, "WD_TIMEOUT_MS reset");
        av_read(15'd17, v); check_eq(v, 0, "REFLEX_COUNT reset");
        av_read(15'd18, v); check_eq(v, 32'h05DC05DC, "HEAD_TARGET reset");
        av_read(15'h0013, v); check_eq(v, 0, "0x0013 lê 0");
        av_read(15'h0100, v); check_eq(v, 0, "0x0100 lê 0");
        av_read(15'h1FFF, v); check_eq(v, 0, "0x1FFF lê 0");
        av_read(15'h32C0, v); check_eq(v, 0, "0x32C0 lê 0");
        av_read(15'h3300, v); check_eq(v, 0, "0x3300 lê 0");
        av_read(15'h52C0, v); check_eq(v, 0, "0x52C0 lê 0");
        av_read(15'h7FFF, v); check_eq(v, 0, "0x7FFF lê 0");
        av_write(15'h0013, 32'hFFFFFFFF); av_write(15'h0100, 32'hFFFFFFFF); av_write(15'h3300, 32'hFFFFFFFF);
        av_write(15'h52C0, 32'hFFFFFFFF); av_write(15'h4000, 32'hFFFFFFFF);   // BG é só leitura
        av_write(15'd5, 32'h12345678); av_write(15'd2, 32'h12345678);           // somente leitura
        av_read(15'h0013, v); check_eq(v, 0, "0x0013 ainda 0");
        av_read(15'h0100, v); check_eq(v, 0, "0x0100 ainda 0");
        av_read(15'h3300, v); check_eq(v, 0, "0x3300 ainda 0");
        av_read(15'd5, v);  check_eq(v, 32'h4D454431, "ID imutável");
        av_read(15'd2, v);  check_eq(v, 0, "FRAMES imutável");
        av_read(15'd1, v);  check_eq(v[6], 0, "escrita fora das faixas não marca WRITE_DROPPED");

        // ---- F0: S0 + BG_RESET ----
        ftag = 0; modes = 0;
        make_scene(0); upload;
        av_read(15'h2000 + 4002, v); check_eq(v, 32'h96969696, "FRAME lê o que foi escrito");
        av_write(15'd0, 32'h3);
        av_read(15'd1, v); check_eq(v[1], 1, "BUSY logo após o GO"); check_eq(v[0], 0, "DONE zera no GO");
        wait_done; read_results;
        chk_frame(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1500, 1500);
        check_eq(r_xy, 0, "TARGET_XY zero");
        lat_first = r_lat;                                        // sem pixels: não divide
        av_read(15'd2, v); check_eq(v, 1, "FRAMES = 1");
        av_read(15'h4000 + 2016, v); check_eq(v, 32'h96969696, "BG = quadro após BG_RESET");

        // ---- configuração: FOLLOW_EN, REFLEX_EN, HOLD_MS = 30; WD desligado ----
        modes = 32'hC;
        av_write(15'd0, modes);
        av_read(15'd0, v); check_eq(v, 32'hC, "CTRL lê FOLLOW|REFLEX");
        av_write(15'd9, 32'h001E0426);
        av_read(15'd9, v); check_eq(v, 32'h001E0426, "REFLEX_CFG lido");
        repeat (1100000) @(posedge clk);                        // sel_head só troca com as duas fontes em 0
        check(dut.sel_head == 2'b11, "sel_head = 11 depois de FOLLOW_EN");
        av_read(15'd1, v); check_eq(v[5], 1, "HEAD_OVERRIDE com FOLLOW_EN");

        // ---- F1 ----
        ftag = 1; frame(1, 0);
        chk_frame(1, 551, 70, 55, 50, 50, 61, 41, 79, 69, 1458, 1522);
        check_eq(r_st[3], 0, "REFLEX"); check_eq(r_rc, 0, "REFLEX_COUNT");
        wait_edge18; av_read(15'd13, v);
        check_eq(v, {16'd1522, 16'd1474}, "HEAD_US após F1");

        // ---- F2 ----
        ftag = 2; frame(2, 0);
        chk_frame(1, 551, 70, 55, 40, 40, 61, 41, 79, 69, 1458, 1522);
        check_eq(r_st[3], 0, "REFLEX"); check_eq(r_rc, 0, "REFLEX_COUNT");

        // ---- F3: reflexo ----
        ftag = 3; frame(3, 0);
        chk_frame(1, 551, 70, 55, 34, 34, 61, 41, 79, 69, 1458, 1778);
        check_eq(r_st[3], 1, "REFLEX"); check_eq(r_rc, 1, "REFLEX_COUNT");
        check_eq(r_st[5], 1, "HEAD_OVERRIDE");
        wait_edge18; av_read(15'd13, v);
        check_eq(v, {16'd1778, 16'd1458}, "HEAD_US após F3 (tilt sem slew)");
        repeat (120000) @(posedge clk);
        check_eq(lw18, 1458 * 50, "largura de pwm_out[18]");
        check_eq(lw19, 1778 * 50, "largura de pwm_out[19]");
        av_write(15'd1, 32'h48);                                // W1C de REFLEX e WRITE_DROPPED
        av_read(15'd1, v); check_eq(v[3], 0, "REFLEX limpo por W1C"); check_eq(v[6], 0, "WRITE_DROPPED limpo");

        // ---- F4: afastou ----
        ftag = 4; frame(4, 0);
        chk_frame(1, 551, 70, 55, 36, 36, 61, 41, 79, 69, 1458, 1522);
        check_eq(r_rc, 1, "REFLEX_COUNT"); check_eq(r_st[3], 0, "REFLEX");

        // ---- F5..F15 ----
        for (i = 5; i <= 15; i = i + 1) begin
            ftag = i; frame(3, 0);
            chk_frame(1, 551, 70, 55, 34, 34, 61, 41, 79, 69, 1458, 1522);
            check_eq(r_rc, 1, "REFLEX_COUNT"); check_eq(r_st[3], 0, "REFLEX");
        end
        av_read(15'h4000 + 2016, v); check_eq(bgbyte(v, 1), 150, "BG(65,50) após F15");

        // ---- F16 ----
        ftag = 16; frame(3, 0);
        chk_frame(1, 551, 70, 55, 34, 34, 61, 41, 79, 69, 1458, 1522);
        av_read(15'h4000 + 2016, v); check_eq(bgbyte(v, 1), 149, "BG(65,50) após F16");

        // ---- F17: S5 + BG_RESET ----
        ftag = 17; frame(5, 1);
        chk_frame(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1458, 1522);

        // ---- F18: S6 ----
        ftag = 18; frame(6, 0);
        chk_frame(1, 551, 70, 55, 50, 50, 61, 41, 79, 69, 1458, 1522);
        check_eq(r_rc, 1, "REFLEX_COUNT");
        av_read(15'h4000 + 201, v);  check_eq(bgbyte(v, 1), 100, "BG(5,5)");
        av_read(15'h4000 + 2016, v); check_eq(bgbyte(v, 1), 150, "BG(65,50)");

        // ---- F19: S7 (alvo pequeno) + escrita/leitura durante BUSY ----
        ftag = 19; make_scene(7); upload;
        av_write(15'd0, 32'h1 | modes);
        av_read(15'd1, v); check_eq(v[1], 1, "BUSY em F19");
        av_write(15'h2000, 32'h00000000);                       // descartada
        av_read(15'h2000, v);        check_eq(v, 0, "FRAME lê 0 durante BUSY");
        av_read(15'h4000 + 2016, v); check_eq(v, 0, "BG lê 0 durante BUSY");
        wait_done; read_results;
        chk_frame(0, 16, 0, 0, 0, 0, 0, 0, 0, 0, 1458, 1522);
        check_eq(r_xy, 32'h00100000, "TARGET_XY: count 16, resto 0");
        check_eq(r_st[6], 1, "WRITE_DROPPED");
        av_read(15'h2000, v); check_eq(v, 32'h96969696, "escrita durante BUSY não alterou o FRAME");
        av_write(15'd1, 32'h40);
        av_read(15'd1, v); check_eq(v[6], 0, "WRITE_DROPPED limpo");
        check_eq(r_rc, 1, "REFLEX_COUNT final");
        $display("LATENCY = %0d ciclos de 50 MHz (quadro com alvo, 3 divisões); F0 sem pixels: %0d", r_lat, lat_first);
        lat_target = r_lat;

        // ---- reflexo expirado: tilt volta a 1522 ----
        repeat (15000000) @(posedge clk);                        // 15 períodos
        av_read(15'd13, v); check_eq(v, {16'd1522, 16'd1458}, "HEAD_US 15 períodos depois");

        // ---- FOLLOW_EN = 0: passagem direta só depois de 1 período ----
        modes = 32'h8;
        av_write(15'd0, modes);
        av_read(15'd1, v); check_eq(v[5], 0, "HEAD_OVERRIDE = 0 sem FOLLOW_EN");
        repeat (1100000) @(posedge clk);
        check(dut.sel_head == 2'b00, "sel_head = 00 depois de FOLLOW_EN = 0");
        pass_chk = 1; pass_seen = 0;
        repeat (1000000) @(posedge clk);
        pass_chk = 0;
        check(pass_seen, "passagem direta viu pulsos");

        // ---- watchdog ----
        av_write(15'd16, 5);    av_read(15'd16, v); check_eq(v, 20, "WD_TIMEOUT_MS limitado em 20");
        av_write(15'd16, 5000); av_read(15'd16, v); check_eq(v, 1000, "WD_TIMEOUT_MS limitado em 1000");
        av_write(15'd16, 20);   av_read(15'd16, v); check_eq(v, 20, "WD_TIMEOUT_MS = 20");
        av_write(15'd14, 32'h1);
        av_read(15'd14, v); check_eq(v, 1, "WD_CTRL = ENABLE");
        for (i = 0; i < 6; i = i + 1) begin                     // kicks a cada 10 ms por 60 ms: nunca dispara
            repeat (500000) @(posedge clk);
            av_read(15'd1, v);  check_eq(v[4], 0, "WD não pode disparar com kicks");
            av_read(15'd15, v2); check(v2 >= 9 && v2 <= 10, "WD_KICK lê ~10 ms desde o último kick");
            av_write(15'd15, 32'h0);
        end
        stuck5 = 1;                                             // fonte do canal 5 presa em 1
        t0 = $time;
        n = 0; v = 0;
        while (!v[4] && n < 3000000) begin av_read(15'd1, v); n = n + 1; end
        check_eq(v[4], 1, "WD TRIPPED");
        begin : medida
            time dt;
            dt = $time - t0;
            $display("watchdog disparou %0d ns (~%0d us) depois do último kick", dt, dt / 1000);
            check(dt >= 20000000 && dt <= 21000000, "WD dispara entre 20 e 21 ms");
        end
        if ($time - t_trip < 125000 * 20) #(125000 * 20 - ($time - t_trip));
        check_eq(pwm_out, 0, "pwm_out = 0 em <= 125000 ciclos após TRIPPED");
        zero_chk = 1;
        repeat (2000000) @(posedge clk);
        zero_chk = 0;
        // CLEAR restaura (o canal 5 continua preso em 1 e passa a valer na saída)
        av_write(15'd14, 32'h3);
        av_read(15'd1, v); check_eq(v[4], 0, "CLEAR zera TRIPPED");
        repeat (150000) @(posedge clk);
        pass_chk = 1; pass_seen = 0;
        repeat (350000) @(posedge clk); av_write(15'd15, 0);
        repeat (500000) @(posedge clk); av_write(15'd15, 0);
        repeat (500000) @(posedge clk); av_write(15'd15, 0);
        pass_chk = 0;
        check(pass_seen, "após CLEAR a saída voltou a seguir pwm_in");
        av_read(15'd1, v); check_eq(v[4], 0, "WD não disparou com kicks após CLEAR");
        // ENABLE = 0 zera TRIPPED e desliga
        stuck5 = 0;
        n = 0; v = 0;
        while (!v[4] && n < 3000000) begin av_read(15'd1, v); n = n + 1; end
        check_eq(v[4], 1, "WD TRIPPED de novo sem kicks");
        av_write(15'd14, 32'h0);
        av_read(15'd1, v);  check_eq(v[4], 0, "ENABLE=0 zera TRIPPED");
        av_read(15'd14, v); check_eq(v, 0, "WD_CTRL = 0");
        repeat (150000) @(posedge clk);
        pass_chk = 1; pass_seen = 0;
        repeat (2000000) @(posedge clk);
        pass_chk = 0;
        check(pass_seen, "com WD desligado a saída segue pwm_in");
        av_read(15'd1, v); check_eq(v[4], 0, "WD desligado não dispara");

        if (errors == 0)
            $display("TB OK: medula — F0..F19 (centroide, bbox, reflexo, fundo), cabeça com slew, troca sem pulso deformado, watchdog; LATENCY = %0d ciclos", lat_target);
        else $display("TB FALHOU: %0d erros", errors);
        $finish;
    end
endmodule
