`timescale 1ns/1ps
// Testbench do cam_capture: câmera sintética em YUV422 (Y na fase 0, U/V = 0x80 na fase 1),
// Y(x,y) = (x + 3*y) & 0xFF. Confere pixel a pixel, registradores, fase, linha curta, overflow.
module tb_cam_capture;
    reg clk = 0, reset_n = 0;
    always #10 clk = ~clk;                       // 50 MHz

    reg  [13:0] address = 0;
    reg         read = 0, write = 0;
    reg  [31:0] writedata = 0;
    wire [31:0] readdata;

    reg pclk = 0, href = 0, vsync = 0;
    reg [7:0] d = 0;

    cam_capture dut(.clk(clk), .reset_n(reset_n), .avs_address(address), .avs_read(read), .avs_write(write),
                    .avs_writedata(writedata), .avs_readdata(readdata),
                    .cam_pclk(pclk), .cam_href(href), .cam_vsync(vsync), .cam_d(d));

    integer errors = 0;
    task check(input integer cond, input [8*64-1:0] msg);
        begin if (!cond) begin $display("FALHA: %0s", msg); errors = errors + 1; end end
    endtask

    // ---- Avalon -------------------------------------------------------------------------------
    task av_write(input [13:0] a, input [31:0] v);
        begin @(posedge clk); address <= a; writedata <= v; write <= 1; @(posedge clk); write <= 0; end
    endtask
    task av_read(input [13:0] a, output [31:0] v);
        begin @(posedge clk); address <= a; read <= 1; @(posedge clk); read <= 0; #1 v = readdata; end
    endtask

    // ---- câmera sintética: PCLK 6,25 MHz (período 160 ns); D muda na descida -----------------
    task pclk_cycle(input [7:0] val);
        begin #40 pclk = 0; d = val; #40; #40 pclk = 1; #40; end
    endtask
    task blank(input integer n);
        integer i; begin for (i = 0; i < n; i = i + 1) pclk_cycle(8'h00); end
    endtask
    // um quadro com `lines` linhas de `bytes` bytes cada
    task frame(input integer lines, input integer bytes);
        integer y, b;
        begin
            vsync = 1; blank(20); vsync = 0; blank(20);
            for (y = 0; y < lines; y = y + 1) begin
                href = 1;
                for (b = 0; b < bytes; b = b + 1)
                    pclk_cycle(b[0] ? 8'h80 : ((b / 2) + 3 * y) & 8'hFF);
                href = 0;
                blank(10);
            end
            blank(10);
        end
    endtask

    reg [31:0] v;
    integer x, y, p, timeout;
    reg [7:0] expect_y;
    reg [7:0] got;

    task capture(input integer phase_bit, input integer lines, input integer bytes);
        begin
            av_write(14'd0, 32'h1 | (phase_bit << 1));
            fork
                begin frame(lines, bytes); frame(lines, bytes); end   // um quadro p/ armar, outro p/ capturar
                begin
                    timeout = 0;
                    v = 0;
                    while (!(v & 1) && timeout < 2000000) begin av_read(14'd1, v); timeout = timeout + 1; end
                end
            join
        end
    endtask

    initial begin
        #100 reset_n = 1;
        av_read(14'd5, v); check(v == 32'h43414D31, "ID = CAM1");
        av_read(14'd1, v); check(v == 0, "STATUS ocioso = 0");

        // ---- 1. quadro normal, fase 0 (Y) ----
        capture(0, 120, 320);
        av_read(14'd1, v);
        check(v[0] == 1, "DONE"); check(v[1] == 0, "não BUSY após DONE"); check(v[2] == 0, "sem OVERFLOW");
        av_read(14'd3, v); check(v == 120, "LINES = 120");
        av_read(14'd4, v); check(v == 320, "BYTES_PER_LINE = 320");
        av_read(14'd2, v); check(v >= 2, "FRAMES conta VSYNC");
        p = 0;
        for (y = 0; y < 120; y = y + 1)
            for (x = 0; x < 160; x = x + 4) begin
                av_read(14'h2000 + (y * 160 + x) / 4, v);
                for (p = 0; p < 4; p = p + 1) begin
                    expect_y = ((x + p) + 3 * y) & 8'hFF;
                    got = v >> (8 * p);
                    if (got !== expect_y) begin
                        if (errors < 10) $display("FALHA pixel (%0d,%0d): esperado %h, obtido %h", x + p, y, expect_y, got);
                        errors = errors + 1;
                    end
                end
            end

        // ---- 2. fase 1 (U/V) → todos 0x80 ----
        capture(1, 120, 320);
        av_read(14'h2000, v);   check(v == 32'h80808080, "fase 1: primeira palavra = U/V 0x80");
        av_read(14'h2000 + 4799, v); check(v == 32'h80808080, "fase 1: última palavra = U/V 0x80");
        av_read(14'd0, v); check(v[1] == 1, "CTRL lê PHASE = 1");

        // ---- 3. linha curta (300 bytes): BPL = 300, linha seguinte começa alinhada ----
        capture(0, 120, 300);
        av_read(14'd4, v); check(v == 300, "linha curta: BYTES_PER_LINE = 300");
        av_read(14'h2000 + (1 * 160) / 4, v); check(v[7:0] == 8'd3, "linha 1 começa no pixel (0,1) = 3");
        av_read(14'd1, v); check(v[2] == 0, "linha curta não é overflow");

        // ---- 4. linha longa (360 bytes) e quadro alto (130 linhas) → OVERFLOW, quadro íntegro ----
        capture(0, 130, 360);
        av_read(14'd1, v); check(v[2] == 1, "OVERFLOW em linha/quadro grande");
        av_read(14'd3, v); check(v == 130, "LINES conta 130");
        av_read(14'h2000 + (119 * 160 + 156) / 4, v);
        check(v[31:24] == ((159 + 3 * 119) & 8'hFF), "último pixel (159,119) correto mesmo com overflow");

        if (errors == 0) $display("TB OK: cam_capture — quadro 160x120 pixel a pixel, fase, linha curta, overflow");
        else $display("TB FALHOU: %0d erros", errors);
        $finish;
    end
endmodule
