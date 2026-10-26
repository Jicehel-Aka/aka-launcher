#include "flasher.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_image_format.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_rom_crc.h"
#include "esp_system.h"
#include "spi_flash_mmap.h"
#include "launcher_config.h"

namespace launcher {

namespace {

const char* TAG = "flasher";
constexpr uint8_t kImageMagic = 0xE9;   // ESP_IMAGE_HEADER_MAGIC

struct Parts {
    Layout layout = Layout::None;
    const esp_partition_t* game = nullptr;     // application installee
    const esp_partition_t* loader = nullptr;   // le Launcher
};

const Parts& parts()
{
    static Parts p;
    static bool done = false;
    if (done) return p;
    done = true;
    auto find = [](esp_partition_subtype_t sub) {
        return esp_partition_find_first(ESP_PARTITION_TYPE_APP, sub, nullptr);
    };
    const esp_partition_t* test = find(ESP_PARTITION_SUBTYPE_APP_TEST);
    const esp_partition_t* fact = find(ESP_PARTITION_SUBTYPE_APP_FACTORY);
    const esp_partition_t* ota0 = find(ESP_PARTITION_SUBTYPE_APP_OTA_0);
    const esp_partition_t* ota1 = find(ESP_PARTITION_SUBTYPE_APP_OTA_1);
    if (test && fact)      { p.layout = Layout::ResetReason; p.game = fact; p.loader = test; }
    else if (ota0 && ota1) { p.layout = Layout::Ota;         p.game = ota0; p.loader = ota1; }
    return p;
}

// ---------------------------------------------------------------- details d'erreur
char g_detail[240];

void detailf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void detailf(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(g_detail, sizeof g_detail, fmt, ap);
    va_end(ap);
    ESP_LOGW(TAG, "%s", g_detail);
}

const char* layoutName()
{
    switch (parts().layout) {
    case Layout::ResetReason: return "loader/game";
    case Layout::Ota:         return "app1/app0";
    default:                  return "?";
    }
}

// Verifie qu'une partition contient une image d'application complete (checksum + SHA-256).
esp_err_t imageCheck(const esp_partition_t* p)
{
    if (!p) return ESP_ERR_NOT_FOUND;
    esp_partition_pos_t pos = {p->address, p->size};
    esp_image_metadata_t meta;
    return esp_image_verify(ESP_IMAGE_VERIFY_SILENT, &pos, &meta);
}
bool imageValid(const esp_partition_t* p) { return imageCheck(p) == ESP_OK; }

// Ecriture sequentielle avec effacement a la volee : chaque secteur est efface juste avant
// son premier octet, donc seul ce qui est ecrit est efface et la progression reste fluide.
esp_err_t writeErasing(const esp_partition_t* p, size_t offset, const void* data, size_t len)
{
    const size_t end = offset + len;
    for (size_t s = (offset + SPI_FLASH_SEC_SIZE - 1) / SPI_FLASH_SEC_SIZE * SPI_FLASH_SEC_SIZE; s < end;
         s += SPI_FLASH_SEC_SIZE) {
        const esp_err_t err = esp_partition_erase_range(p, s, SPI_FLASH_SEC_SIZE);
        if (err != ESP_OK) return err;
    }
    return esp_partition_write(p, offset, data, len);
}

// CRC32 de la partition relue (sans passer par le cache), sur `len` octets.
bool partitionCrc(const esp_partition_t* p, size_t len, uint32_t* crc)
{
    uint8_t* buf = (uint8_t*)heap_caps_malloc(kFlashChunk, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!buf) return false;
    uint32_t c = 0;
    bool ok = true;
    for (size_t off = 0; off < len;) {
        const size_t n = (len - off < kFlashChunk) ? (len - off) : kFlashChunk;
        if (esp_partition_read(p, off, buf, n) != ESP_OK) { ok = false; break; }
        c = esp_rom_crc32_le(c, buf, n);
        off += n;
    }
    free(buf);
    *crc = c;
    return ok;
}

}  // namespace

Layout layout() { return parts().layout; }

const char* lastErrorDetail() { return g_detail; }

const char* errorKey(FlashResult r)
{
    switch (r) {
    case FlashResult::Ok:                return "OK";
    case FlashResult::NoTargetPartition: return "LAUNCHER_ERR_NO_PART";
    case FlashResult::TargetIsRunning:   return "LAUNCHER_ERR_RUNNING";
    case FlashResult::OpenFailed:        return "LAUNCHER_ERR_OPEN";
    case FlashResult::NotAnImage:        return "LAUNCHER_ERR_IMAGE";
    case FlashResult::TooBig:            return "LAUNCHER_ERR_BIG";
    case FlashResult::BeginFailed:       return "LAUNCHER_ERR_BEGIN";
    case FlashResult::ReadFailed:        return "LAUNCHER_ERR_READ";
    case FlashResult::WriteFailed:       return "LAUNCHER_ERR_WRITE";
    case FlashResult::ValidationFailed:  return "LAUNCHER_ERR_VALID";
    case FlashResult::SetBootFailed:     return "LAUNCHER_ERR_BOOT";
    case FlashResult::NotApplicable:     return "OK";
    }
    return "?";
}

FlashResult flashApp(const char* path, ProgressFn progress, void* ctx)
{
    g_detail[0] = 0;
    const Parts& P = parts();
    const esp_partition_t* target = P.game;
    const esp_partition_t* running = esp_ota_get_running_partition();
    if (!target) { detailf("layout=%s: partition du jeu absente", layoutName()); return FlashResult::NoTargetPartition; }
    if (target == running) {
        detailf("tourne depuis %s = cible", running->label);
        return FlashResult::TargetIsRunning;
    }

    FILE* f = fopen(path, "rb");
    if (!f) { detailf("fopen %s", path); return FlashResult::OpenFailed; }

    fseek(f, 0, SEEK_END);
    const long total = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (total < 32) { fclose(f); detailf("fichier trop court (%ld octets)", total); return FlashResult::NotAnImage; }
    if ((size_t)total > target->size) {
        fclose(f);
        detailf("%ld octets > %u (%s)", total, (unsigned)target->size, target->label);
        return FlashResult::TooBig;
    }

    uint8_t* buf = (uint8_t*)malloc(kFlashChunk);
    if (!buf) { fclose(f); detailf("malloc %u", (unsigned)kFlashChunk); return FlashResult::BeginFailed; }

    FlashResult result = FlashResult::Ok;
    size_t written = 0;
    unsigned shortReads = 0;
    uint32_t fileCrc = 0;

    do {
        // Premier bloc : on verifie l'en-tete avant d'effacer quoi que ce soit.
        size_t n = fread(buf, 1, kFlashChunk, f);
        if (n == 0) { detailf("read @0: n=0"); result = FlashResult::ReadFailed; break; }
        if (buf[0] != kImageMagic) {
            detailf("en-tete 0x%02x (attendu 0xE9)", buf[0]);
            result = FlashResult::NotAnImage;
            break;
        }

        while (true) {
            const esp_err_t e = writeErasing(target, written, buf, n);
            if (e != ESP_OK) {
                detailf("ecriture @0x%x: %s (0x%x)", (unsigned)written, esp_err_to_name(e), (unsigned)e);
                result = FlashResult::WriteFailed;
                break;
            }
            fileCrc = esp_rom_crc32_le(fileCrc, buf, n);
            written += n;
            if (progress) progress(written, (size_t)total, ctx);
            if (written >= (size_t)total) break;

            const size_t want = ((size_t)total - written < kFlashChunk) ? ((size_t)total - written) : kFlashChunk;
            n = fread(buf, 1, want, f);
            if (n < want) {                       // lecture SD incomplete : une seconde chance
                shortReads++;
                const size_t more = fread(buf + n, 1, want - n, f);
                n += more;
            }
            if (n == 0) {
                detailf("read @0x%x: n=0 (SD courtes: %u)", (unsigned)written, shortReads);
                result = FlashResult::ReadFailed;
                break;
            }
        }
        if (result != FlashResult::Ok) break;

        // Une image partielle ou corrompue est rejetee ici, et aussi par le bootloader.
        const esp_err_t v = imageCheck(target);
        uint32_t flashCrc = 0;
        const bool readOk = partitionCrc(target, written, &flashCrc);
        const char* cmp = !readOk ? "relecture impossible" : (flashCrc == fileCrc ? "flash=fichier" : "flash!=fichier");
        if (v != ESP_OK) {
            detailf("verif image: %s (0x%x)\necrit %u/%ld, SD courtes: %u\n%s crc %08x/%08x",
                    esp_err_to_name(v), (unsigned)v, (unsigned)written, total, shortReads,
                    cmp, (unsigned)fileCrc, (unsigned)flashCrc);
            result = FlashResult::ValidationFailed;
        } else {
            detailf("OK %s: %u octets, SD courtes: %u\n%s crc %08x",
                    target->label, (unsigned)written, shortReads, cmp, (unsigned)fileCrc);
        }
    } while (false);

    if (result != FlashResult::Ok && g_detail[0]) {
        char tmp[sizeof g_detail + 80];
        const int len = snprintf(tmp, sizeof tmp, "%s\n[%s] run=%s cible=%s", g_detail, layoutName(),
                                 running ? running->label : "?", target->label);
        const size_t keep = (len > 0 && (size_t)len < sizeof g_detail) ? (size_t)len : sizeof g_detail - 1;
        memcpy(g_detail, tmp, keep);
        g_detail[keep] = 0;
    }

    free(buf);
    fclose(f);
    ESP_LOGI(TAG, "flash %s -> %s", path, errorKey(result));
    return result;
}

bool runningAsGame()
{
    const esp_partition_t* run = esp_ota_get_running_partition();
    return run && parts().game && run->address == parts().game->address;
}

FlashResult installSelfToLoader(ProgressFn progress, void* ctx)
{
    g_detail[0] = 0;
    const esp_partition_t* src = esp_ota_get_running_partition();
    if (!runningAsGame()) return FlashResult::NotApplicable;
    const esp_partition_t* dst = parts().loader;
    if (!dst) { detailf("layout=%s: loader absent", layoutName()); return FlashResult::NoTargetPartition; }

    // Longueur reelle de l'image (en-tete + segments + checksum + hash).
    esp_partition_pos_t pos = {src->address, src->size};
    esp_image_metadata_t meta;
    const esp_err_t m = esp_image_get_metadata(&pos, &meta);
    if (m != ESP_OK) { detailf("metadata %s: %s", src->label, esp_err_to_name(m)); return FlashResult::NotAnImage; }
    const size_t total = meta.image_len;
    if (total > dst->size) {
        detailf("%u octets > %u (%s)", (unsigned)total, (unsigned)dst->size, dst->label);
        return FlashResult::TooBig;
    }

    uint8_t* buf = (uint8_t*)heap_caps_malloc(kFlashChunk, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!buf) return FlashResult::BeginFailed;

    FlashResult result = FlashResult::Ok;
    for (size_t off = 0; off < total;) {
        const size_t n = (total - off < kFlashChunk) ? (total - off) : kFlashChunk;
        esp_err_t e = esp_partition_read(src, off, buf, n);
        if (e != ESP_OK) { detailf("lecture %s @0x%x: %s", src->label, (unsigned)off, esp_err_to_name(e)); result = FlashResult::ReadFailed; break; }
        e = writeErasing(dst, off, buf, n);
        if (e != ESP_OK) { detailf("ecriture %s @0x%x: %s", dst->label, (unsigned)off, esp_err_to_name(e)); result = FlashResult::WriteFailed; break; }
        off += n;
        if (progress) progress(off, total, ctx);
    }
    free(buf);
    if (result == FlashResult::Ok) {
        const esp_err_t v = imageCheck(dst);
        if (v != ESP_OK) { detailf("verif %s: %s (0x%x)", dst->label, esp_err_to_name(v), (unsigned)v); result = FlashResult::ValidationFailed; }
    }

    // La copie laissee dans la partition du jeu est conservee (ecrire dans la partition en cours
    // d'execution fait un abort : CONFIG_SPI_FLASH_DANGEROUS_WRITE_ABORTS) ; compareLoader() la
    // rend inoffensive.
    ESP_LOGI(TAG, "auto-installation %s -> %s : %s", src->label, dst->label, errorKey(result));
    return result;
}

bool selfInstallNeedsPowerOff() { return parts().layout == Layout::ResetReason; }

[[noreturn]] void restartToLoaderNow()
{
    const esp_partition_t* l = parts().loader;
    if (l && parts().layout == Layout::Ota) {
        const esp_err_t e = esp_ota_set_boot_partition(l);
        ESP_LOGI(TAG, "boot -> %s : %s", l->label, esp_err_to_name(e));
    }
    esp_restart();
    while (true) {}
}

LoaderMatch compareLoader()
{
    const esp_partition_t* game = parts().game;
    const esp_partition_t* loader = parts().loader;
    if (!game || !loader) return LoaderMatch::Error;

    // Longueur de l'image de la partition du jeu ; pas d'image valide = rien a comparer.
    esp_partition_pos_t pos = {game->address, game->size};
    esp_image_metadata_t meta;
    if (esp_image_get_metadata(&pos, &meta) != ESP_OK) return LoaderMatch::Different;
    const size_t total = meta.image_len;
    if (total > loader->size) return LoaderMatch::Different;

    uint8_t* a = (uint8_t*)heap_caps_malloc(2 * kFlashChunk, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!a) return LoaderMatch::Error;
    uint8_t* b = a + kFlashChunk;

    LoaderMatch result = LoaderMatch::Same;
    for (size_t off = 0; off < total;) {
        const size_t n = (total - off < kFlashChunk) ? (total - off) : kFlashChunk;
        if (esp_partition_read(game, off, a, n) != ESP_OK ||
            esp_partition_read(loader, off, b, n) != ESP_OK) { result = LoaderMatch::Error; break; }
        if (memcmp(a, b, n) != 0) { result = LoaderMatch::Different; break; }
        off += n;
    }
    free(a);
    return result;
}

bool hasApplication()
{
    const esp_partition_t* app = parts().game;
    return app && app != esp_ota_get_running_partition() && imageValid(app);
}

[[noreturn]] void restartToGame()
{
    const esp_partition_t* g = parts().game;
    if (g && parts().layout == Layout::Ota) esp_ota_set_boot_partition(g);
    esp_restart();
    while (true) {}
}

}  // namespace launcher
