# Pull de données — Phase P2 (producteur réseau) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Donner au firmware Rich_Telemetry la capacité de *tirer* (pull) ses données depuis une ou plusieurs URL HTTP(S) à un rythme défini, en alimentant le blackboard de variables déjà consommé par les composants `bind:"var"` (livré en P1).

**Architecture:** Une **tâche FreeRTOS** épinglée sur le cœur 0 (PRO_CPU) boucle sur les `sources` du layout, fetch chacune à son `interval_s` **hors du `loop()`** (qui est mono-thread coopératif sur le cœur 1), extrait ses variables par JSON Pointer et les écrit dans le contexte. Un **unique mutex** (`g_ctx_mutex`) sérialise tout accès concurrent au contexte et aux sources entre la tâche productrice et le `loop()`/les handlers HTTP ; le fetch lui-même se fait hors mutex (snapshot config → fetch → écriture). Les clés d'API vivent dans un **store de secrets write-only** sur LittleFS, jamais servi par `GET`, référencé dans les `headers` par `$nom`.

**Tech Stack:** ESP32-S3 / arduino-esp32 v3 (IDF 5.1) · FreeRTOS · `HTTPClient` + `WiFiClientSecure` (`.setInsecure()`) · ArduinoJson v7 · LittleFS · PlatformIO (`pio` env `esp32s3` device, `native` pour les tests de parse).

---

## Contexte hérité de P1 (déjà sur `master`, ne pas réécrire)

Le **consommateur** et le **push** du blackboard existent et sont testés (`pio test -e native`, 54/54) :

- `src/context.{h,cpp}` — `Context`, `CtxVar`, `ctx_find`, `ctx_set_num`, `ctx_set_str`, `ctx_apply_json`, `ctx_extract_pointer` (JSON Pointer RFC 6901).
- `src/dashboard.{h,cpp}` — `Dashboard.ctx` ; `Component.bind[ID_LEN]` (parsé) ; `dash_set_context(d, json, now)` ; `context_apply(d)` (pour chaque composant `bind`, lit la variable et marque `dirty`).
- Ces trois fonctions sont **pures** (aucun appel FreeRTOS) — elles compilent en natif. **P2 ne doit pas y mettre de mutex** : ce sont les appelants device qui verrouillent.

P2 ajoute le **producteur** : parse des `sources`, tâche FreeRTOS, client HTTP(S), store de secrets, endpoints `/context` et `/secrets`, et l'appel de `context_apply` dans `loop()`.

## File Structure

| Fichier | Rôle | Statut |
|---|---|---|
| `src/config.h` | Constantes de dimensionnement (`MAX_SOURCES`, `URL_LEN`, `SECRETS_PATH`, …) | Modifié (Task 1, 3) |
| `src/dashboard.h` | `struct SourceHeader/SourceVar/Source` + `Source sources[]` dans `Dashboard` | Modifié (Task 1) |
| `src/dashboard.cpp` | Parse des `sources` dans `dash_set_layout` | Modifié (Task 1) |
| `test/test_core/test_main.cpp` | Tests natifs du parse des sources | Modifié (Task 1) |
| `src/main.cpp` | Crée `g_ctx_mutex`, `secret_store_begin()`, `context_apply` throttlé dans `loop()`, lance la tâche pull | Modifié (Task 2, 3, 4) |
| `src/api.cpp` | Handlers `POST /context`, `POST /secrets` ; `/status` enrichi ; mutex autour de `set_layout` | Modifié (Task 2, 3, 4, 5) |
| `src/secret_store.{h,cpp}` | Store write-only de secrets sur LittleFS | **Créé** (Task 3) |
| `src/net_pull.{h,cpp}` | Tâche FreeRTOS productrice (fetch → extraction → contexte) | **Créé** (Task 4) |

`secret_store.cpp` et `net_pull.cpp` sont **device-only** : ils ne sont PAS dans le `build_src_filter` de l'env `native` (`platformio.ini`), donc ils ne cassent pas `pio test -e native`. Ne pas les y ajouter.

---

## Task 1 : Parse des `sources` (native, TDD)

**Files:**
- Modify: `src/config.h`
- Modify: `src/dashboard.h:43-67` (struct `Dashboard`, après `struct Placement`/`struct Page`)
- Modify: `src/dashboard.cpp:85-110` (dans `dash_set_layout`, après la boucle `pages`)
- Test: `test/test_core/test_main.cpp`

- [ ] **Step 1 : Ajouter les constantes de dimensionnement**

Dans `src/config.h`, après `#define MAX_CTX_VARS 32` :

```c
#define MAX_SOURCES             6
#define MAX_HEADERS_PER_SOURCE  4
#define MAX_VARS_PER_SOURCE     6
#define URL_LEN                 192
#define HEADER_NAME_LEN         32
#define HEADER_VAL_LEN          64
#define PTR_LEN                 48
#define CTX_MIN_INTERVAL_S      5
```

- [ ] **Step 2 : Déclarer les structs source dans `dashboard.h`**

Dans `src/dashboard.h`, juste avant `struct Dashboard {` (l. 55) :

```c
struct SourceHeader { char name[HEADER_NAME_LEN]; char value[HEADER_VAL_LEN]; };  // value: littéral ou "$secret"
struct SourceVar    { char name[ID_LEN];          char ptr[PTR_LEN]; };           // variable -> JSON Pointer

struct Source {
    char         name[ID_LEN];
    char         url[URL_LEN];
    uint32_t     interval_s;
    SourceHeader headers[MAX_HEADERS_PER_SOURCE];
    int          header_count;
    SourceVar    vars[MAX_VARS_PER_SOURCE];
    int          var_count;
    // --- runtime (rempli par la tâche productrice en P2) ---
    uint32_t     last_fetch_ms;   // 0 = jamais -> fetch immédiat
    int          last_status;     // dernier code HTTP, ou <0 sur erreur transport/parse
    uint32_t     err_count;
    uint32_t     updated_at;      // millis() du dernier fetch réussi
};
```

Puis dans `struct Dashboard`, juste après `Context ctx;` (l. 66) :

```c
    Source    sources[MAX_SOURCES];
    int       source_count;
```

- [ ] **Step 3 : Écrire les tests de parse (qui échouent)**

Dans `test/test_core/test_main.cpp`, après `test_dash_set_context_writes_ctx` (l. 302) :

```c
static const char* LAYOUT_SOURCES =
  "{\"title\":\"T\",\"background\":\"#000000\","
  "\"sources\":[{"
    "\"name\":\"weather\",\"url\":\"https://api.example/w?city=Paris\",\"interval_s\":600,"
    "\"headers\":{\"X-API-Key\":\"$weather_key\"},"
    "\"vars\":{\"temp\":\"/main/temp\",\"hum\":\"/main/humidity\"}}],"
  "\"components\":{\"t\":{\"type\":\"readout\",\"unit\":\"C\",\"bind\":\"temp\"}},"
  "\"pages\":[{\"name\":\"p\",\"place\":[{\"ref\":\"t\"}]}]}";

void test_sources_parse_counts(void) {
    Dashboard d{}; char err[80];
    TEST_ASSERT_TRUE(dash_set_layout(&d, LAYOUT_SOURCES, err, sizeof(err)));
    TEST_ASSERT_EQUAL_INT(1, d.source_count);
    TEST_ASSERT_EQUAL_STRING("weather", d.sources[0].name);
    TEST_ASSERT_EQUAL_STRING("https://api.example/w?city=Paris", d.sources[0].url);
    TEST_ASSERT_EQUAL_UINT32(600, d.sources[0].interval_s);
}
void test_sources_headers_and_vars(void) {
    Dashboard d{}; char err[80];
    dash_set_layout(&d, LAYOUT_SOURCES, err, sizeof(err));
    TEST_ASSERT_EQUAL_INT(1, d.sources[0].header_count);
    TEST_ASSERT_EQUAL_STRING("X-API-Key",    d.sources[0].headers[0].name);
    TEST_ASSERT_EQUAL_STRING("$weather_key", d.sources[0].headers[0].value);
    TEST_ASSERT_EQUAL_INT(2, d.sources[0].var_count);
    TEST_ASSERT_EQUAL_STRING("temp",       d.sources[0].vars[0].name);   // ArduinoJson préserve l'ordre des clés
    TEST_ASSERT_EQUAL_STRING("/main/temp", d.sources[0].vars[0].ptr);
}
void test_sources_interval_floor(void) {
    Dashboard d{}; char err[80];
    const char* L = "{\"sources\":[{\"name\":\"s\",\"url\":\"http://x/\",\"interval_s\":1}],"
                    "\"components\":{},\"pages\":[]}";
    TEST_ASSERT_TRUE(dash_set_layout(&d, L, err, sizeof(err)));
    TEST_ASSERT_EQUAL_UINT32(CTX_MIN_INTERVAL_S, d.sources[0].interval_s);   // 1 -> borné à 5
}
void test_sources_url_required(void) {
    Dashboard d{}; char err[80];
    const char* L = "{\"sources\":[{\"name\":\"s\"}],\"components\":{},\"pages\":[]}";
    TEST_ASSERT_FALSE(dash_set_layout(&d, L, err, sizeof(err)));   // url manquante -> rejet
}
void test_no_sources_is_zero(void) {
    Dashboard d{}; char err[80];
    dash_set_layout(&d, LAYOUT_OK, err, sizeof(err));   // layout sans 'sources'
    TEST_ASSERT_EQUAL_INT(0, d.source_count);           // rétro-compat
}
```

Et enregistrer les tests dans `main()` (après `RUN_TEST(test_dash_set_context_writes_ctx);`) :

```c
    RUN_TEST(test_sources_parse_counts);
    RUN_TEST(test_sources_headers_and_vars);
    RUN_TEST(test_sources_interval_floor);
    RUN_TEST(test_sources_url_required);
    RUN_TEST(test_no_sources_is_zero);
```

- [ ] **Step 4 : Lancer les tests pour les voir échouer**

Run: `cd devices/guition_knob/projects/Rich_Telemetry && pio test -e native`
Expected: échec à la **compilation** (`d.source_count` / `d.sources` n'existent pas encore si l'ordre des steps a glissé) ou échec d'assertion ; en tout cas rouge sur les 5 nouveaux tests.

- [ ] **Step 5 : Implémenter le parse des sources**

Dans `src/dashboard.cpp`, dans `dash_set_layout`, **après** la boucle `pages` (juste avant `t.active_page = 0;`, l. 107) :

```c
    JsonArrayConst srcs = doc["sources"].as<JsonArrayConst>();
    for (JsonObjectConst so : srcs) {
        if (t.source_count >= MAX_SOURCES) { snprintf(err, errn, "trop de sources"); return false; }
        Source& s = t.sources[t.source_count];
        strlcpy(s.name, so["name"] | "", sizeof(s.name));
        strlcpy(s.url,  so["url"]  | "", sizeof(s.url));
        if (s.url[0] == '\0') { snprintf(err, errn, "source '%s' sans url", s.name); return false; }
        uint32_t iv  = so["interval_s"] | 60;
        s.interval_s = iv < CTX_MIN_INTERVAL_S ? CTX_MIN_INTERVAL_S : iv;
        for (JsonPairConst h : so["headers"].as<JsonObjectConst>()) {
            if (s.header_count >= MAX_HEADERS_PER_SOURCE) break;
            strlcpy(s.headers[s.header_count].name,  h.key().c_str(), sizeof(s.headers[0].name));
            strlcpy(s.headers[s.header_count].value, h.value() | "", sizeof(s.headers[0].value));
            s.header_count++;
        }
        for (JsonPairConst v : so["vars"].as<JsonObjectConst>()) {
            if (s.var_count >= MAX_VARS_PER_SOURCE) break;
            strlcpy(s.vars[s.var_count].name, v.key().c_str(), sizeof(s.vars[0].name));
            strlcpy(s.vars[s.var_count].ptr,  v.value() | "", sizeof(s.vars[0].ptr));
            s.var_count++;
        }
        t.source_count++;
    }
```

- [ ] **Step 6 : Lancer les tests pour les voir passer**

Run: `pio test -e native`
Expected: PASS sur les 59 tests (54 existants + 5 nouveaux).

- [ ] **Step 7 : Vérifier que le firmware build toujours**

Run: `pio run -e esp32s3`
Expected: `[SUCCESS]` (la struct a grossi mais reste statique ; pas de nouvel appel).

- [ ] **Step 8 : Commit**

```bash
git add src/config.h src/dashboard.h src/dashboard.cpp test/test_core/test_main.cpp
git commit -m "Rich_Telemetry: firmware — parse des sources (pull P2, native)"
```

---

## Task 2 : Mutex partagé + `POST /context` + `context_apply` dans le `loop()` (device)

But : fermer la boucle **push → contexte → composant** de bout en bout, **sans réseau**. À la fin de cette task, `curl -X POST /context -d '{"temp":21}'` met à jour un composant `bind:"temp"` à l'écran. Le mutex est introduit ici (« armé ») mais il n'y a encore qu'un seul thread (le producteur arrive en Task 4).

**Files:**
- Modify: `src/main.cpp` (global mutex, création dans `setup`, `context_apply` dans `loop`)
- Modify: `src/api.cpp` (handler `/context`, route)

- [ ] **Step 1 : Déclarer et créer le mutex dans `main.cpp`**

En tête de `src/main.cpp`, après `#include "persist.h"` (l. 14) :

```c
#include "freertos/semphr.h"
```

Après `String g_layout_json;` (l. 19) :

```c
SemaphoreHandle_t g_ctx_mutex = nullptr;   // sérialise l'accès à g_dash.ctx / g_dash.sources
```

Dans `setup()`, tout au début, juste après `Serial.begin(115200); delay(200);` (l. 44) :

```c
    g_ctx_mutex = xSemaphoreCreateMutex();
```

- [ ] **Step 2 : Appeler `context_apply` (throttlé, non bloquant) dans `loop()`**

Dans `src/main.cpp`, dans `loop()`, juste avant `if (g_dash.values_dirty) view_sync(&g_dash);` (l. 88) :

```c
    static uint32_t last_ctx = 0;
    if (millis() - last_ctx >= 100) {
        last_ctx = millis();
        if (g_ctx_mutex && xSemaphoreTake(g_ctx_mutex, 0) == pdTRUE) {   // 0 = non bloquant : on saute le tour si occupé
            context_apply(&g_dash);
            xSemaphoreGive(g_ctx_mutex);
        }
    }
```

- [ ] **Step 3 : Ajouter le handler `POST /context` dans `api.cpp`**

En tête de `src/api.cpp`, après `#include "persist.h"` (l. 9) :

```c
#include "freertos/semphr.h"
```

Après `extern String g_layout_json;` (l. 11) :

```c
extern SemaphoreHandle_t g_ctx_mutex;
```

Après `h_update` (l. 25), ajouter :

```c
static void h_set_context() {
    if (!S->hasArg("plain")) { S->send(400, "text/plain", "Empty body\n"); return; }
    if (g_ctx_mutex) xSemaphoreTake(g_ctx_mutex, portMAX_DELAY);
    dash_set_context(D, S->arg("plain").c_str(), millis());
    if (g_ctx_mutex) xSemaphoreGive(g_ctx_mutex);
    S->send(200, "application/json", "{\"ok\":true}\n");
}
```

Dans `api_register`, après `server.on("/update", HTTP_POST, h_update);` (l. 99) :

```c
    server.on("/context", HTTP_POST, h_set_context);
```

- [ ] **Step 4 : Build**

Run: `pio run -e esp32s3`
Expected: `[SUCCESS]`.

- [ ] **Step 5 : Flash + validation curl (device branché)**

```bash
cd /Users/jean-paulgavini/Documents/Dev/Waveshare-ESP32
./build.sh auto Rich_Telemetry --upload
```

Poser un layout minimal avec un composant lié, puis pousser une variable. Remplacer `IP` par l'IP DHCP du device (`GET /status` la donne ; le `.local` ne résout pas sur ce LAN) :

```bash
IP=192.168.x.x
curl -s -X POST http://$IP/layout -H 'Content-Type: application/json' -d '{
  "title":"P2","background":"#0B0B0F",
  "components":{"t":{"type":"readout","label":"Temp","unit":"C","bind":"temp"}},
  "pages":[{"name":"p","place":[{"ref":"t","anchor":"CENTER"}]}]}'
curl -s -X POST http://$IP/context -H 'Content-Type: application/json' -d '{"temp":21}'
```
Expected : `{"ok":true}`, et **à l'écran** le readout affiche `21 C`. Repousser `{"temp":25}` → l'écran passe à `25 C` (le `loop()` applique le contexte en <100 ms). **Validation visuelle utilisateur.**

- [ ] **Step 6 : Commit**

```bash
git add src/main.cpp src/api.cpp
git commit -m "Rich_Telemetry: firmware — mutex ctx + POST /context + context_apply dans loop (pull P2)"
```

---

## Task 3 : Store de secrets write-only + `POST /secrets` (device)

**Files:**
- Modify: `src/config.h`
- Create: `src/secret_store.h`
- Create: `src/secret_store.cpp`
- Modify: `src/main.cpp` (`secret_store_begin()` dans `setup`)
- Modify: `src/api.cpp` (handler `/secrets`, route)

- [ ] **Step 1 : Constantes du store dans `config.h`**

Dans `src/config.h`, après `#define LAYOUT_PATH "/layout.json"` (l. 15) :

```c
#define SECRETS_PATH            "/secrets.json"
#define SECRET_VAL_LEN          80
```

- [ ] **Step 2 : Créer `src/secret_store.h`**

```c
#pragma once
#include <Arduino.h>
// Store write-only de secrets sur LittleFS (/secrets.json), distinct du layout.
// Jamais servi par GET : seul le fetch (net_pull) le lit pour résoudre les $refs.
bool secret_store_begin();                                     // s'assure que le fichier existe
bool secret_store_merge(const char* json);                     // {nom:val,...} -> merge + écrit ; false si JSON invalide
bool secret_store_get(const char* name, char* out, size_t n);  // copie la valeur ; false si absente
```

- [ ] **Step 3 : Créer `src/secret_store.cpp`**

```c
#include "secret_store.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "config.h"

bool secret_store_begin() {
    if (LittleFS.exists(SECRETS_PATH)) return true;
    File f = LittleFS.open(SECRETS_PATH, "w");
    if (!f) return false;
    f.print("{}");
    f.close();
    return true;
}

// Charge le store dans doc ; objet vide si absent ou corrompu (jamais d'échec dur).
static void load_doc(JsonDocument& doc) {
    File f = LittleFS.open(SECRETS_PATH, "r");
    if (!f) { doc.to<JsonObject>(); return; }
    DeserializationError e = deserializeJson(doc, f);
    f.close();
    if (e) doc.to<JsonObject>();
}

bool secret_store_merge(const char* json) {
    JsonDocument incoming;
    if (deserializeJson(incoming, json)) return false;
    JsonObjectConst in = incoming.as<JsonObjectConst>();
    if (in.isNull()) return false;

    JsonDocument store;
    load_doc(store);
    JsonObject obj = store.as<JsonObject>();
    if (obj.isNull()) obj = store.to<JsonObject>();
    for (JsonPairConst kv : in)
        if (kv.value().is<const char*>())            // secrets = chaînes uniquement
            obj[kv.key()] = kv.value().as<const char*>();

    File f = LittleFS.open(SECRETS_PATH, "w");        // incoming/store restent vivants jusqu'ici
    if (!f) return false;
    serializeJson(store, f);
    f.close();
    return true;
}

bool secret_store_get(const char* name, char* out, size_t n) {
    JsonDocument store;
    load_doc(store);
    JsonVariantConst v = store[name];
    if (!v.is<const char*>()) { if (n) out[0] = '\0'; return false; }
    strlcpy(out, v.as<const char*>(), n);
    return true;
}
```

- [ ] **Step 4 : Monter le store au boot dans `main.cpp`**

Dans `src/main.cpp`, en tête après `#include "persist.h"` (l. 14) :

```c
#include "secret_store.h"
```

Dans `setup()`, juste après `persist_begin();` (l. 50) :

```c
    secret_store_begin();   // LittleFS déjà monté par persist_begin()
```

- [ ] **Step 5 : Handler `POST /secrets` dans `api.cpp`**

En tête de `src/api.cpp`, après `#include "persist.h"` (l. 9) :

```c
#include "secret_store.h"
```

Après `h_set_context` (Task 2), ajouter :

```c
static void h_set_secrets() {
    if (!S->hasArg("plain")) { S->send(400, "text/plain", "Empty body\n"); return; }
    if (!secret_store_merge(S->arg("plain").c_str())) { S->send(400, "text/plain", "Invalid JSON\n"); return; }
    S->send(200, "application/json", "{\"ok\":true}\n");   // ne renvoie JAMAIS le contenu
}
```

Dans `api_register`, après la route `/context` (Task 2) :

```c
    server.on("/secrets", HTTP_POST, h_set_secrets);   // pas de route GET : write-only par conception
```

- [ ] **Step 6 : Build + flash + validation**

```bash
pio run -e esp32s3
cd /Users/jean-paulgavini/Documents/Dev/Waveshare-ESP32 && ./build.sh auto Rich_Telemetry --upload
```

```bash
IP=192.168.x.x
curl -s -X POST http://$IP/secrets -H 'Content-Type: application/json' -d '{"weather_key":"abc123"}'   # -> {"ok":true}
curl -s -X POST http://$IP/secrets -d 'not json'                                                        # -> 400 Invalid JSON
curl -s http://$IP/secrets                                                                              # -> 404 (pas de GET)
curl -s http://$IP/layout | grep -c weather_key                                                         # -> 0 (jamais dans le layout)
```
Expected : le secret est accepté, jamais relu par `GET /secrets` ni présent dans `/layout`. (Sa **résolution effective** est prouvée en Task 4.) **Pas de validation visuelle ici.**

- [ ] **Step 7 : Commit**

```bash
git add src/config.h src/secret_store.h src/secret_store.cpp src/main.cpp src/api.cpp
git commit -m "Rich_Telemetry: firmware — store de secrets write-only + POST /secrets (pull P2)"
```

---

## Task 4 : Tâche FreeRTOS productrice — fetch HTTP(S) → extraction → contexte (device)

But : la tâche tire les `sources` à leur `interval_s`, résout les `$refs`, extrait par JSON Pointer, écrit le contexte sous mutex. Le `loop()` reste non bloqué.

**Files:**
- Create: `src/net_pull.h`
- Create: `src/net_pull.cpp`
- Modify: `src/main.cpp` (lancer la tâche dans `start_services`)
- Modify: `src/api.cpp` (mutex autour de `dash_set_layout` — la race cross-cœur devient réelle)

- [ ] **Step 1 : Créer `src/net_pull.h`**

```c
#pragma once
#include "dashboard.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
// Tâche productrice : fetch périodique des sources -> contexte (sous mutex). À appeler
// une fois, après la connexion WiFi. Le mutex est partagé avec loop()/les handlers HTTP.
void net_pull_begin(Dashboard* d, SemaphoreHandle_t mutex);
```

- [ ] **Step 2 : Créer `src/net_pull.cpp`**

```c
#include "net_pull.h"
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <string.h>
#include "config.h"
#include "context.h"
#include "secret_store.h"

static Dashboard*        s_d   = nullptr;
static SemaphoreHandle_t s_mtx = nullptr;

static inline void lock()   { if (s_mtx) xSemaphoreTake(s_mtx, portMAX_DELAY); }
static inline void unlock() { if (s_mtx) xSemaphoreGive(s_mtx); }

// Résout "$nom" via le store de secrets ; sinon copie la valeur littérale.
static void resolve_header(const char* in, char* out, size_t n) {
    if (in[0] == '$') { if (!secret_store_get(in + 1, out, n) && n) out[0] = '\0'; return; }
    strlcpy(out, in, n);
}

static void record_error(int idx, int code) {
    lock();
    s_d->sources[idx].last_status = code;
    s_d->sources[idx].err_count++;
    unlock();
}

// Copie locale de la config d'une source : permet de relâcher le mutex pendant le fetch (long).
struct SourceJob {
    char url[URL_LEN];
    char hname[MAX_HEADERS_PER_SOURCE][HEADER_NAME_LEN];
    char hval [MAX_HEADERS_PER_SOURCE][HEADER_VAL_LEN];
    int  header_count;
    char vname[MAX_VARS_PER_SOURCE][ID_LEN];
    char vptr [MAX_VARS_PER_SOURCE][PTR_LEN];
    int  var_count;
};

static void fetch_one(int idx) {
    SourceJob job;
    // 1) snapshot config + résolution des secrets, sous mutex
    lock();
    Source& s = s_d->sources[idx];
    strlcpy(job.url, s.url, sizeof(job.url));
    job.header_count = s.header_count;
    for (int i = 0; i < s.header_count; i++) {
        strlcpy(job.hname[i], s.headers[i].name, HEADER_NAME_LEN);
        resolve_header(s.headers[i].value, job.hval[i], HEADER_VAL_LEN);
    }
    job.var_count = s.var_count;
    for (int i = 0; i < s.var_count; i++) {
        strlcpy(job.vname[i], s.vars[i].name, ID_LEN);
        strlcpy(job.vptr[i],  s.vars[i].ptr,  PTR_LEN);
    }
    unlock();

    // 2) fetch HORS mutex (peut bloquer plusieurs secondes)
    bool https = strncmp(job.url, "https", 5) == 0;
    WiFiClientSecure tls;
    WiFiClient       tcp;
    HTTPClient http;
    bool begun = https ? (tls.setInsecure(), http.begin(tls, job.url)) : http.begin(tcp, job.url);
    if (!begun) { record_error(idx, -1); return; }
    for (int i = 0; i < job.header_count; i++)
        if (job.hval[i][0]) http.addHeader(job.hname[i], job.hval[i]);
    int code = http.GET();
    if (code != 200) { http.end(); record_error(idx, code); return; }
    String payload = http.getString();
    http.end();

    // 3) parse réponse
    JsonDocument doc;
    if (deserializeJson(doc, payload)) { record_error(idx, -2); return; }
    JsonVariantConst root = doc.as<JsonVariantConst>();

    // 4) extraction + écriture du contexte, sous mutex
    lock();
    uint32_t now = millis();
    for (int i = 0; i < job.var_count; i++) {
        JsonVariantConst v = ctx_extract_pointer(root, job.vptr[i]);
        if (v.isNull()) continue;                         // chemin non résolu -> garde la dernière valeur
        if (v.is<const char*>())               ctx_set_str(&s_d->ctx, job.vname[i], v.as<const char*>(), now);
        else if (v.is<float>() || v.is<int>()) ctx_set_num(&s_d->ctx, job.vname[i], v.as<double>(), now);
    }
    s_d->sources[idx].last_status = code;
    s_d->sources[idx].updated_at  = now;
    unlock();
}

static void pull_task(void*) {
    for (;;) {
        if (WiFi.status() == WL_CONNECTED) {
            int n; lock(); n = s_d->source_count; unlock();
            uint32_t now = millis();
            for (int i = 0; i < n; i++) {
                uint32_t last, iv;
                lock(); last = s_d->sources[i].last_fetch_ms; iv = s_d->sources[i].interval_s; unlock();
                if (last != 0 && now - last < iv * 1000UL) continue;     // pas encore l'heure
                lock(); s_d->sources[i].last_fetch_ms = now; unlock();   // marque avant fetch (anti double-tir)
                fetch_one(i);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

void net_pull_begin(Dashboard* d, SemaphoreHandle_t mutex) {
    s_d = d; s_mtx = mutex;
    // Cœur 0 (PRO_CPU) : le loopTask Arduino tourne sur le cœur 1. Pile 16 KB pour le
    // handshake TLS mbedtls — si HTTPS reset le device (stack overflow), monter à 20480.
    xTaskCreatePinnedToCore(pull_task, "pull", 16384, nullptr, 1, nullptr, 0);
}
```

- [ ] **Step 3 : Lancer la tâche dans `start_services` (`main.cpp`)**

En tête de `src/main.cpp`, après `#include "secret_store.h"` (Task 3) :

```c
#include "net_pull.h"
```

Dans `start_services()`, juste après `server.begin();` (l. 39) :

```c
    net_pull_begin(&g_dash, g_ctx_mutex);   // garde-fou `started` au-dessus -> lancée une seule fois
```

- [ ] **Step 4 : Protéger `dash_set_layout` par le mutex (la race cross-cœur devient réelle)**

Dans `src/api.cpp`, `h_set_layout` applique désormais un nouveau layout (donc de nouvelles `sources`) pendant que la tâche pull lit `D->sources`. Encadrer l'appel. Remplacer (l. 56) :

```c
    if (!dash_set_layout(D, body.c_str(), err, sizeof(err))) {
```

par :

```c
    if (g_ctx_mutex) xSemaphoreTake(g_ctx_mutex, portMAX_DELAY);
    bool ok = dash_set_layout(D, body.c_str(), err, sizeof(err));
    if (g_ctx_mutex) xSemaphoreGive(g_ctx_mutex);
    if (!ok) {
```

- [ ] **Step 5 : Build**

Run: `pio run -e esp32s3`
Expected: `[SUCCESS]`. (Vérifier qu'aucune nouvelle alerte de taille flash/RAM critique.)

- [ ] **Step 6 : Flash + validation pull réelle (device + WiFi)**

```bash
pio run -e esp32s3
cd /Users/jean-paulgavini/Documents/Dev/Waveshare-ESP32 && ./build.sh auto Rich_Telemetry --upload
```

**Cas A — LAN HTTP, sans secret.** Servir un JSON sur la machine de dev :

```bash
echo '{"main":{"temp":21,"humidity":58}}' > /tmp/w.json
( cd /tmp && python3 -m http.server 8000 ) &   # http://<IP_DEV>:8000/w.json
```

Poser le layout avec une source qui pointe la machine de dev (remplacer `IP_DEV`/`IP_DEVICE`) :

```bash
IP=IP_DEVICE
curl -s -X POST http://$IP/layout -H 'Content-Type: application/json' -d '{
  "title":"Pull","background":"#0B0B0F",
  "sources":[{"name":"lan","url":"http://IP_DEV:8000/w.json","interval_s":5,
              "vars":{"temp":"/main/temp","hum":"/main/humidity"}}],
  "components":{"t":{"type":"readout","label":"Temp","unit":"C","bind":"temp"},
                "h":{"type":"readout","label":"Hum","unit":"%","bind":"hum"}},
  "pages":[{"name":"p","place":[{"ref":"t","anchor":"TOP_MID","dy":40},
                                {"ref":"h","anchor":"BOTTOM_MID","dy":-40}]}]}'
```
Expected (≤ 5 s) : l'écran affiche `21 C` / `58 %`. Modifier `/tmp/w.json` (`temp:30`) → l'écran suit au prochain intervalle. Surveiller la série pour l'absence de crash ; `GET /status` montre `uptime_s` qui croît.

**Cas B — HTTPS + secret.** Poser un secret puis une source HTTPS publique (ex. une API renvoyant du JSON imbriqué) avec un header `$ref` :

```bash
curl -s -X POST http://$IP/secrets -d '{"weather_key":"<clé réelle>"}'
# puis un /layout avec "headers":{"X-API-Key":"$weather_key"} et l'URL https de l'API,
# vars pointant les champs JSON voulus.
```
Expected : la variable se remplit depuis l'API HTTPS, le header secret est bien envoyé (sinon `last_status` ≠ 200), aucun reset (sinon → monter la pile à 20480). **Validation visuelle utilisateur sur les deux cas.**

- [ ] **Step 7 : Commit**

```bash
git add src/net_pull.h src/net_pull.cpp src/main.cpp src/api.cpp
git commit -m "Rich_Telemetry: firmware — tâche productrice pull HTTP(S) -> contexte (pull P2)"
```

---

## Task 5 : Observabilité des sources dans `GET /status` (device)

But : exposer par source `{name, last_status, err_count, updated_at}` pour diagnostiquer les fetchs (demandé par la spec § Défauts → Observabilité).

**Files:**
- Modify: `src/api.cpp` (`h_status`)

- [ ] **Step 1 : Enrichir `h_status`**

Dans `src/api.cpp`, `h_status`, juste avant `String out; serializeJson(doc, out);` (l. 36) :

```c
    if (g_ctx_mutex) xSemaphoreTake(g_ctx_mutex, portMAX_DELAY);
    JsonArray arr = doc["sources"].to<JsonArray>();
    for (int i = 0; i < D->source_count; i++) {
        JsonObject o     = arr.add<JsonObject>();
        o["name"]        = D->sources[i].name;          // char[] -> ArduinoJson copie
        o["last_status"] = D->sources[i].last_status;
        o["err_count"]   = D->sources[i].err_count;
        o["updated_at"]  = D->sources[i].updated_at;
    }
    if (g_ctx_mutex) xSemaphoreGive(g_ctx_mutex);
```

- [ ] **Step 2 : Build + flash + validation**

```bash
pio run -e esp32s3
cd /Users/jean-paulgavini/Documents/Dev/Waveshare-ESP32 && ./build.sh auto Rich_Telemetry --upload
```

```bash
curl -s http://$IP/status | python3 -m json.tool
```
Expected : un tableau `sources` avec, pour la source LAN, `last_status:200` et `updated_at` non nul ; débrancher la source (couper le `http.server`) → au bout d'un intervalle `err_count` s'incrémente et `last_status` change (≠ 200), sans crash.

- [ ] **Step 3 : Commit**

```bash
git add src/api.cpp
git commit -m "Rich_Telemetry: firmware — observabilité des sources dans /status (pull P2)"
```

---

## Self-Review (couverture de la spec)

- **Producteur / tâche FreeRTOS cœur libre** → Task 4 (`xTaskCreatePinnedToCore(..., 0)`). ✅
- **Contexte (blackboard) + mutex** → Task 2 (mutex) ; écriture sous mutex en Task 4. ✅
- **Consommateur `context_apply` dans loop** → Task 2. ✅
- **Coexistence push/pull (`/update` intact, `/context` nouveau)** → `/update` non touché ; `/context` en Task 2. ✅
- **Schema `sources` (name/url/interval_s/headers/vars), borne mini, JSON Pointer** → Task 1 (parse + borne) ; extraction Task 4 (réutilise `ctx_extract_pointer` de P1). ✅
- **`bind` optionnel par composant** → déjà livré en P1 (rien à faire). ✅
- **Secrets write-only, jamais en GET, résolus au fetch** → Task 3 (store) + Task 4 (`resolve_header`). ✅
- **Défauts v1 : TLS `setInsecure`, dernière valeur connue sur échec, observabilité, types num+str** → Task 4 (`setInsecure`, `if v.isNull() continue`, `ctx_set_num/str`) + Task 5 (observabilité). Péremption visuelle : explicitement différée (spec), pas de task. ✅
- **Décision « fetch hors loop() »** → toute l'archi tâche dédiée (Task 4). ✅

**Note de honnêteté sur la vérification** : seule la **Task 1** est couverte par des tests unitaires (`pio test -e native`). Les Tasks 2–5 sont **device-gated** : leur preuve est `build esp32s3` + flash + `curl`/série + **contrôle visuel utilisateur** (pas de test automatisé du réseau/LittleFS/FreeRTOS). Workflow : un agent code+compile, le contrôleur flashe+série+curl, l'utilisateur valide l'écran.

## Pièges à garder en tête (rappel du handoff)

- **Pile TLS** : 16 KB pour la tâche pull ; un reset pendant un GET HTTPS = stack overflow mbedtls → monter à 20480.
- **`.local` ne résout pas** sur ce LAN → toujours l'IP DHCP (via `GET /status`).
- **WiFi** : `src/secrets.h` (gitignored, compile-time) pour SSID/pass — **distinct** du store de secrets runtime de P2.
- **Mutex non bloquant dans `loop()`** (`xSemaphoreTake(..., 0)`) : ne JAMAIS bloquer l'UI ; les handlers et la tâche, eux, peuvent attendre (`portMAX_DELAY`).
- **`net_pull.cpp`/`secret_store.cpp` hors build natif** : ne pas les ajouter au `build_src_filter` de l'env `native`.
