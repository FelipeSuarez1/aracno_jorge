// Teste da R200 no Pi (pi/r200/build-librealsense.sh) com librealsense v1.12.1: liga o emissor, captura cor/IR/profundidade,
// grava PPM/PGM e imprime estatística da profundidade. Uso: ./r200_teste [quadros]
#include <librealsense/rs.hpp>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <vector>
#include <algorithm>

static void save_pgm(const char* path, const uint8_t* p, int w, int h) {
    FILE* f = std::fopen(path, "wb"); std::fprintf(f, "P5\n%d %d\n255\n", w, h);
    std::fwrite(p, 1, w * h, f); std::fclose(f);
}
static void save_ppm(const char* path, const uint8_t* p, int w, int h) {
    FILE* f = std::fopen(path, "wb"); std::fprintf(f, "P6\n%d %d\n255\n", w, h);
    std::fwrite(p, 1, w * h * 3, f); std::fclose(f);
}

int main(int argc, char** argv) try {
    int n = argc > 1 ? std::atoi(argv[1]) : 60;
    rs::context ctx;
    std::printf("dispositivos: %d\n", ctx.get_device_count());
    if (ctx.get_device_count() == 0) return 2;
    rs::device* dev = ctx.get_device(0);
    std::printf("%s serial %s firmware %s\n", dev->get_name(), dev->get_serial(), dev->get_firmware_version());
    dev->enable_stream(rs::stream::depth, rs::preset::best_quality);
    dev->enable_stream(rs::stream::color, 640, 480, rs::format::rgb8, 30);
    dev->enable_stream(rs::stream::infrared, rs::preset::best_quality);
    dev->start();
    dev->set_option(rs::option::r200_emitter_enabled, 1);
    dev->set_option(rs::option::r200_lr_auto_exposure_enabled, 1);
    const float scale = dev->get_depth_scale();
    for (int i = 0; i < n; ++i) dev->wait_for_frames();

    const int dw = dev->get_stream_width(rs::stream::depth), dh = dev->get_stream_height(rs::stream::depth);
    const uint16_t* z = (const uint16_t*)dev->get_frame_data(rs::stream::depth);
    std::vector<uint16_t> v; v.reserve(dw * dh);
    for (int i = 0; i < dw * dh; ++i) if (z[i]) v.push_back(z[i]);
    std::printf("profundidade %dx%d, escala %.4f m: %.1f%% válidos", dw, dh, scale, 100.0 * v.size() / (dw * dh));
    if (!v.empty()) {
        std::sort(v.begin(), v.end());
        std::printf(", mín %.2f m, mediana %.2f m, máx %.2f m", v.front() * scale, v[v.size() / 2] * scale, v.back() * scale);
    }
    std::printf("\n");
    // visualização: 0,3 m → branco, 4 m → preto; inválido = 0
    std::vector<uint8_t> vis(dw * dh);
    for (int i = 0; i < dw * dh; ++i) {
        float m = z[i] * scale;
        vis[i] = z[i] ? (uint8_t)std::max(20.0f, std::min(255.0f, 255.0f * (4.0f - m) / 3.7f)) : 0;
    }
    save_pgm("prof.pgm", vis.data(), dw, dh);
    save_ppm("cor.ppm", (const uint8_t*)dev->get_frame_data(rs::stream::color),
             dev->get_stream_width(rs::stream::color), dev->get_stream_height(rs::stream::color));
    save_pgm("ir.pgm", (const uint8_t*)dev->get_frame_data(rs::stream::infrared),
             dev->get_stream_width(rs::stream::infrared), dev->get_stream_height(rs::stream::infrared));
    std::printf("emissor=%.0f fps_profundidade=%d\n", dev->get_option(rs::option::r200_emitter_enabled),
                dev->get_stream_framerate(rs::stream::depth));
    dev->stop();
    return 0;
} catch (const rs::error& e) {
    std::printf("erro librealsense: %s (%s)\n", e.what(), e.get_failed_function().c_str());
    return 1;
}
