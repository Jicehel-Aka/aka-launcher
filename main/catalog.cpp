#include "catalog.h"

#include <dirent.h>
#include <sys/stat.h>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace launcher {

namespace {

// --- Extraction minimale d'une valeur chaine : "cle": "valeur" -----------
// Suffisant pour meta.json (objet plat). Gere \" \\ \n \t et saute \uXXXX.
bool jsonString(const char* json, const char* key, char* out, size_t cap)
{
    char pat[48];
    snprintf(pat, sizeof pat, "\"%s\"", key);
    const char* p = strstr(json, pat);
    if (!p) return false;
    p = strchr(p + strlen(pat), ':');
    if (!p) return false;
    ++p;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') ++p;

    size_t n = 0;
    if (*p == '"') {
        ++p;
        while (*p && *p != '"') {
            char c = *p++;
            if (c == '\\' && *p) {
                char e = *p++;
                if (e == 'n' || e == 't') c = ' ';
                else if (e == 'u') {                    // \uXXXX : approx. ASCII
                    unsigned v = 0;
                    for (int i = 0; i < 4 && isxdigit((unsigned char)*p); ++i, ++p)
                        v = v * 16 + (isdigit((unsigned char)*p) ? *p - '0'
                                                                 : (tolower(*p) - 'a' + 10));
                    c = (v < 0x80) ? (char)v : '?';
                } else c = e;
            }
            if (n + 1 < cap) out[n++] = c;
        }
    } else {                                            // nombre / litteral
        while (*p && *p != ',' && *p != '}' && *p != '\n' && n + 1 < cap) out[n++] = *p++;
        while (n && (out[n - 1] == ' ' || out[n - 1] == '\r')) --n;
    }
    out[n] = '\0';
    return true;
}

void loadMeta(const char* dir, AppEntry& a)
{
    char path[320];
    snprintf(path, sizeof path, "%s/%s/%s", kSdRoot, dir, kMetaName);
    FILE* f = fopen(path, "rb");
    if (!f) return;
    static char buf[1024];
    size_t n = fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    buf[n] = '\0';

    char raw[kMaxDescLen];
    if (jsonString(buf, "title", raw, sizeof raw))       asciiFold(raw, a.title, sizeof a.title);
    if (jsonString(buf, "author", raw, sizeof raw))      asciiFold(raw, a.author, sizeof a.author);
    if (jsonString(buf, "version", raw, sizeof raw))     asciiFold(raw, a.version, sizeof a.version);
    if (jsonString(buf, "date", raw, sizeof raw))        asciiFold(raw, a.date, sizeof a.date);
    if (jsonString(buf, "description", raw, sizeof raw)) asciiFold(raw, a.description, sizeof a.description);
}

bool fileSize(const char* path, uint32_t* size)
{
    struct stat st;
    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) return false;
    if (size) *size = (uint32_t)st.st_size;
    return true;
}

int compareTitles(const void* x, const void* y)
{
    return strcasecmp(((const AppEntry*)x)->title, ((const AppEntry*)y)->title);
}

}  // namespace

void asciiFold(const char* s, char* out, size_t cap)
{
    // Table pour U+00C0..U+00FF (lettres latin-1) ; le reste devient '?'.
    static const char* kLatin1 =
        "AAAAAAACEEEEIIII" "DNOOOOOxOUUUUYTs"
        "aaaaaaaceeeeiiii" "dnooooo/ouuuuyty";
    size_t n = 0;
    while (*s && n + 1 < cap) {
        unsigned char c = (unsigned char)*s;
        if (c < 0x80) { out[n++] = (c >= 32 || c == '\t') ? (char)c : ' '; ++s; }
        else if (c == 0xC3 && s[1]) { out[n++] = kLatin1[(unsigned char)s[1] - 0x80]; s += 2; }
        else if (c == 0xE2 && s[1] && s[2]) {            // ponctuation : ' ' - ...
            out[n++] = '\''; s += 3;
        } else {                                          // autre sequence UTF-8
            out[n++] = '?';
            ++s;
            while (((unsigned char)*s & 0xC0) == 0x80) ++s;
        }
    }
    out[n] = '\0';
}

int Catalog::scan()
{
    m_count = 0;
    DIR* d = opendir(kSdRoot);
    if (!d) return -1;

    while (struct dirent* e = readdir(d)) {
        if (m_count >= kMaxApps) break;
        if (e->d_name[0] == '.' || strlen(e->d_name) >= sizeof(AppEntry::folder)) continue;

        char path[320];
        snprintf(path, sizeof path, "%s/%s", kSdRoot, e->d_name);
        struct stat st;
        if (stat(path, &st) != 0 || !S_ISDIR(st.st_mode)) continue;

        AppEntry& a = m_apps[m_count];
        memset(&a, 0, sizeof a);
        snprintf(path, sizeof path, "%s/%s/%s", kSdRoot, e->d_name, kFirmwareName);
        if (!fileSize(path, &a.firmwareSize)) continue;      // pas une application

        strncpy(a.folder, e->d_name, sizeof a.folder - 1);
        asciiFold(e->d_name, a.title, sizeof a.title);       // titre par defaut
        loadMeta(e->d_name, a);
        snprintf(path, sizeof path, "%s/%s/%s", kSdRoot, e->d_name, kScreenName);
        a.hasPreview = fileSize(path, nullptr);
        if (!a.hasPreview) {
            snprintf(path, sizeof path, "%s/%s/%s", kSdRoot, e->d_name, kScreenNameAlt);
            a.hasPreview = fileSize(path, nullptr);
        }
        ++m_count;
    }
    closedir(d);

    qsort(m_apps, m_count, sizeof(AppEntry), compareTitles);
    return (int)m_count;
}

int Catalog::indexOf(const char* folder) const
{
    for (size_t i = 0; i < m_count; ++i)
        if (strcmp(m_apps[i].folder, folder) == 0) return (int)i;
    return -1;
}

void Catalog::path(size_t i, const char* file, char* out, size_t cap) const
{
    snprintf(out, cap, "%s/%s/%s", kSdRoot, m_apps[i].folder, file);
}

}  // namespace launcher
