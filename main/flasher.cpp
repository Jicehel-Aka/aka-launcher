#include "flasher.h"

#include <cstdio>
#include <cerrno>
#include <cstdarg>
#include <cstdlib>
#include <cstring>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_image_format.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "launcher_config.h"
#include "mbedtls/sha256.h"

namespace launcher {

namespace {

const char* TAG = "flasher";
constexpr uint8_t kImageMagic = 0xE9;   // ESP_IMAGE_HEADER_MAGIC

// Partition de l'application : la premiere de sous-type OTA_0 (comme le fait
// la table de partitions de la console, sans offset code en dur).
const esp_partition_t* applicationPartition()
{
    return esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, nullptr);
}

char g_detail[220];

// Ajoute une ligne au detail (et la journalise).
void detailClear() { g_detail[0] = '\0'; }
void detailAdd(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void detailAdd(const char* fmt, ...)
{
    char line[48];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
    ESP_LOGE(TAG, "%s", line);
    const size_t len = strlen(g_detail);
    if (len + strlen(line) + 2 < sizeof g_detail) {
        snprintf(g_detail + len, sizeof g_detail - len, "%s%s", len ? "\n" : "", line);
    }
}

// Version courte du nom d'erreur ("ESP_ERR_OTA_VALIDATE_FAILED" -> "OTA_VALIDATE_FAILED").
const char* shortName(esp_err_t e)
{
    const char* n = esp_err_to_name(e);
    return strncmp(n, "ESP_ERR_", 8) == 0 ? n + 8 : n;
}

void hex8(const uint8_t* h, char* out)   // 4 premiers octets d'un hash -> "a1b2c3d4"
{
    for (int i = 0; i < 4; ++i) sprintf(out + 2 * i, "%02x", h[i]);
}

// Apres un echec de esp_ota_end : relit la partition en direct (sans passer par
// le cache) et compare son SHA-256 a celui du fichier lu sur la SD.
void diagnoseReadback(const esp_partition_t* part, size_t total, const uint8_t* fileHash)
{
    uint8_t* buf = (uint8_t*)malloc(kFlashChunk);
    if (!buf) { detailAdd("relecture: pas de memoire"); return; }
    mbedtls_sha256_context c;
    mbedtls_sha256_init(&c);
    mbedtls_sha256_starts(&c, 0);
    uint8_t first[8] = {0};
    bool readOk = true;
    for (size_t off = 0; off < total; off += kFlashChunk) {
        const size_t n = (total - off < kFlashChunk) ? (total - off) : kFlashChunk;
        if (esp_partition_read(part, off, buf, n) != ESP_OK) { readOk = false; break; }
        if (off == 0) memcpy(first, buf, 8);
        mbedtls_sha256_update(&c, buf, n);
    }
    uint8_t h[32];
    mbedtls_sha256_finish(&c, h);
    mbedtls_sha256_free(&c);
    free(buf);
    char a[9], b[9];
    hex8(fileHash, a);
    hex8(h, b);
    if (!readOk) detailAdd("relecture flash impossible");
    else if (memcmp(h, fileHash, 32) == 0) detailAdd("flash=fichier (sha %s) OK", a);
    else detailAdd("flash!=fichier %s/%s", b, a);
    ESP_LOGE(TAG, "partition %s: octets 0..7 = %02x %02x %02x %02x %02x %02x %02x %02x",
             part->label, first[0], first[1], first[2], first[3], first[4], first[5], first[6], first[7]);
}

}  // namespace

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
    detailClear();
    const esp_partition_t* target = applicationPartition();
    const esp_partition_t* run = esp_ota_get_running_partition();
    ESP_LOGI(TAG, "flash: %s", path);
    ESP_LOGI(TAG, "  tourne sur : %s @0x%x", run ? run->label : "?", run ? (unsigned)run->address : 0);
    if (target)
        ESP_LOGI(TAG, "  cible      : %s @0x%x taille 0x%x", target->label, (unsigned)target->address, (unsigned)target->size);
    if (!target) { detailAdd("pas de partition app0 (OTA_0)"); return FlashResult::NoTargetPartition; }
    if (target == run) { detailAdd("on tourne sur %s : ecriture refusee", target->label); return FlashResult::TargetIsRunning; }

    FILE* f = fopen(path, "rb");
    if (!f) { detailAdd("ouverture SD impossible (errno %d)", errno); return FlashResult::OpenFailed; }

    fseek(f, 0, SEEK_END);
    const long total = ftell(f);
    fseek(f, 0, SEEK_SET);
    ESP_LOGI(TAG, "  fichier    : %ld octets", total);
    if (total < 32) { fclose(f); detailAdd("fichier trop petit (%ld)", total); return FlashResult::NotAnImage; }
    if ((size_t)total > target->size) { fclose(f); detailAdd("fichier %ld > app0 %u", total, (unsigned)target->size); return FlashResult::TooBig; }

    uint8_t* buf = (uint8_t*)malloc(kFlashChunk);
    if (!buf) { fclose(f); detailAdd("memoire insuffisante"); return FlashResult::BeginFailed; }

    FlashResult result = FlashResult::Ok;
    esp_ota_handle_t handle = 0;
    bool begun = false;
    size_t written = 0, shortReads = 0;
    mbedtls_sha256_context sha;
    mbedtls_sha256_init(&sha);
    mbedtls_sha256_starts(&sha, 0);
    uint8_t fileHash[32] = {0};
    esp_err_t err = ESP_OK;

    do {
        // Premier bloc : controle de l'en-tete avant d'effacer quoi que ce soit.
        size_t n = fread(buf, 1, kFlashChunk, f);
        if (n == 0) { detailAdd("lecture SD: 0 octet (errno %d)", errno); result = FlashResult::ReadFailed; break; }
        if (buf[0] != kImageMagic) { detailAdd("en-tete: 0x%02x au lieu de 0xE9", buf[0]); result = FlashResult::NotAnImage; break; }

        err = esp_ota_begin(target, (size_t)total, &handle);
        if (err != ESP_OK) {
            detailAdd("ota_begin 0x%x", (unsigned)err);
            detailAdd("%s", shortName(err));
            result = FlashResult::BeginFailed;
            break;
        }
        begun = true;

        while (true) {
            mbedtls_sha256_update(&sha, buf, n);
            err = esp_ota_write(handle, buf, n);
            if (err != ESP_OK) {
                detailAdd("ota_write 0x%x a l'offset %u", (unsigned)err, (unsigned)written);
                detailAdd("%s", shortName(err));
                result = FlashResult::WriteFailed;
                break;
            }
            written += n;
            if (progress) progress(written, (size_t)total, ctx);
            if (written >= (size_t)total) break;
            n = fread(buf, 1, kFlashChunk, f);
            if (n == 0) {
                detailAdd("SD: fin a %u/%ld (errno %d)", (unsigned)written, total, errno);
                result = FlashResult::ReadFailed;
                break;
            }
            if (n != kFlashChunk && written + n < (size_t)total) ++shortReads;   // lecture SD incomplete au milieu du fichier
        }
        if (result != FlashResult::Ok) break;
        mbedtls_sha256_finish(&sha, fileHash);

        // Valide l'image (somme de controle + hash) et l'ecrit comme valide.
        err = esp_ota_end(handle);
        begun = false;
        if (err != ESP_OK) {
            detailAdd("ota_end 0x%x", (unsigned)err);
            detailAdd("%s", shortName(err));
            detailAdd("ecrit %u/%ld, SD courtes: %u", (unsigned)written, total, (unsigned)shortReads);
            diagnoseReadback(target, written, fileHash);
            result = FlashResult::ValidationFailed;
            break;
        }

        err = esp_ota_set_boot_partition(target);
        if (err != ESP_OK) {
            detailAdd("set_boot 0x%x", (unsigned)err);
            detailAdd("%s", shortName(err));
            result = FlashResult::SetBootFailed;
        }
    } while (false);

    mbedtls_sha256_free(&sha);
    if (begun) esp_ota_abort(handle);
    free(buf);
    fclose(f);
    ESP_LOGI(TAG, "flash %s -> %s (%u octets ecrits, %u lectures SD courtes)", path, errorKey(result),
             (unsigned)written, (unsigned)shortReads);
    return result;
}

bool runningFromApp0()
{
    const esp_partition_t* run = esp_ota_get_running_partition();
    return run && run->subtype == ESP_PARTITION_SUBTYPE_APP_OTA_0;
}

FlashResult installSelfToApp1(ProgressFn progress, void* ctx)
{
    detailClear();
    const esp_partition_t* src = esp_ota_get_running_partition();
    if (!src || src->subtype != ESP_PARTITION_SUBTYPE_APP_OTA_0) return FlashResult::NotApplicable;
    const esp_partition_t* dst =
        esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, nullptr);
    if (!dst) { detailAdd("pas de partition app1 (OTA_1)"); return FlashResult::NoTargetPartition; }

    // Longueur reelle de l'image (en-tete + segments + somme + hash).
    esp_partition_pos_t pos = {src->address, src->size};
    esp_image_metadata_t meta;
    if (esp_image_get_metadata(&pos, &meta) != ESP_OK) { detailAdd("image courante illisible"); return FlashResult::NotAnImage; }
    const size_t total = meta.image_len;
    if (total > dst->size) { detailAdd("image %u > app1 %u", (unsigned)total, (unsigned)dst->size); return FlashResult::TooBig; }

    uint8_t* buf = (uint8_t*)heap_caps_malloc(kFlashChunk, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!buf) return FlashResult::BeginFailed;

    FlashResult result = FlashResult::Ok;
    esp_ota_handle_t handle = 0;
    esp_err_t e = esp_ota_begin(dst, total, &handle);
    if (e != ESP_OK) {
        detailAdd("ota_begin 0x%x", (unsigned)e);
        free(buf);
        return FlashResult::BeginFailed;
    }
    for (size_t off = 0; off < total && result == FlashResult::Ok;) {
        const size_t n = (total - off < kFlashChunk) ? (total - off) : kFlashChunk;
        e = esp_partition_read(src, off, buf, n);
        if (e != ESP_OK) { detailAdd("lecture app0 0x%x @%u", (unsigned)e, (unsigned)off); result = FlashResult::ReadFailed; break; }
        e = esp_ota_write(handle, buf, n);
        if (e != ESP_OK) { detailAdd("ota_write 0x%x @%u", (unsigned)e, (unsigned)off); result = FlashResult::WriteFailed; break; }
        off += n;
        if (progress) progress(off, total, ctx);
    }
    if (result == FlashResult::Ok) {
        e = esp_ota_end(handle);
        if (e != ESP_OK) { detailAdd("ota_end 0x%x", (unsigned)e); detailAdd("%s", shortName(e)); result = FlashResult::ValidationFailed; }
        else if ((e = esp_ota_set_boot_partition(dst)) != ESP_OK) { detailAdd("set_boot 0x%x", (unsigned)e); result = FlashResult::SetBootFailed; }
    } else {
        esp_ota_abort(handle);
    }
    free(buf);
    ESP_LOGI(TAG, "self install app0 -> app1: %s", errorKey(result));
    return result;
}

bool selectApplication()
{
    const esp_partition_t* app = applicationPartition();
    if (!app) return false;
    uint8_t magic = 0;
    if (esp_partition_read(app, 0, &magic, 1) != ESP_OK || magic != kImageMagic) return false;
    return esp_ota_set_boot_partition(app) == ESP_OK;
}

[[noreturn]] void restart()
{
    esp_restart();
    while (true) {}
}

}  // namespace launcher
