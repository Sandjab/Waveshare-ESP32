# Pull — Phase P1 (plan de données, firmware native-testable) — Plan

> **For agentic workers:** Steps use checkbox (`- [ ]`). TDD, commits fréquents. `pio test -e native` à chaque étape.

**Goal:** Poser la **couche modèle** du pull, sans réseau ni HTTP : un **contexte blackboard** (variables nommées), un **extracteur JSON Pointer**, l'application d'un `{var:val}` au contexte (`dash_set_context`/`ctx_apply_json`), le champ **`bind`** sur les composants, et **`context_apply()`** qui propage les variables liées aux composants. Tout est **native-testable** (`pio test -e native`), **sans device**.

**Architecture:** Nouveau module isolé `src/context.*` (le blackboard + l'extracteur, sans connaissance des `Component`). `dashboard.*` l'intègre : `Dashboard` gagne un `Context ctx`, `Component` un `char bind[]`, et deux opérations `dash_set_context()` (écrit le contexte) + `context_apply()` (consomme : variables → champs des composants liés, réutilise `format_value`). Coexiste avec le push par id (inchangé).

**Tech Stack:** C++17, PlatformIO `env:native` (Unity + ArduinoJson v7). Baseline : **39 test cases**. `pio test -e native` depuis `devices/guition_knob/projects/Rich_Telemetry/`.

**Hors P1 (→ P2, device) :** l'endpoint HTTP `POST /context` (plomberie `WebServer`, non native-testable), le câblage de `context_apply()` dans `loop()`, la tâche FreeRTOS productrice, le client HTTP(S), les `sources`, le store de secrets. P1 livre les **fonctions** ; P2 les **branche**.

**Répertoire de travail :** `devices/guition_knob/projects/Rich_Telemetry/`.

---

## File Structure

- **Create** `src/context.h` / `src/context.cpp` — blackboard (`Context`, `CtxVar`, `ctx_find/ctx_set_num/ctx_set_str/ctx_apply_json`) + `ctx_extract_pointer` (JSON Pointer). Aucune dépendance aux `Component`.
- **Modify** `src/config.h` — `MAX_CTX_VARS`.
- **Modify** `platformio.ini` — `[env:native]` compile aussi `context.cpp`.
- **Modify** `src/dashboard.h` — include `context.h` ; `Component.bind` ; `Dashboard.ctx` ; déclarations `dash_set_context`/`context_apply`.
- **Modify** `src/dashboard.cpp` — parse `bind` ; `dash_set_context` ; `context_apply`.
- **Modify** `test/test_core/test_main.cpp` — tests des nouvelles fonctions + `RUN_TEST`.

---

## Task 1: Module contexte (blackboard) + build natif

**Files:** Create `src/context.h`, `src/context.cpp` ; Modify `src/config.h`, `platformio.ini`, `test/test_core/test_main.cpp`

- [ ] **Step 1: Constante**

Dans `src/config.h`, après `#define MAX_THRESHOLDS 4`, ajouter :
```c
#define MAX_CTX_VARS            32
```

- [ ] **Step 2: `src/context.h`**
```c
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <ArduinoJson.h>
#include "config.h"

enum CtxType { CTX_NONE, CTX_NUM, CTX_STR };

// Variable nommée du contexte partagé (blackboard). num XOR str selon type.
struct CtxVar {
    char     name[ID_LEN];
    CtxType  type;
    double   num;
    char     str[TEXT_LEN];
    uint32_t updated_at;     // timestamp fourni par l'appelant (millis() device, libre en test)
};

struct Context {
    CtxVar vars[MAX_CTX_VARS];
    int    count;
};

int  ctx_find(const Context* c, const char* name);                                  // index ou -1
bool ctx_set_num(Context* c, const char* name, double v, uint32_t now);             // false si plein
bool ctx_set_str(Context* c, const char* name, const char* v, uint32_t now);
int  ctx_apply_json(Context* c, JsonObjectConst obj, uint32_t now);                 // {nom:val} → nb écrites
JsonVariantConst ctx_extract_pointer(JsonVariantConst root, const char* ptr);       // RFC 6901 ; nul si non résolu
```

- [ ] **Step 3: `src/context.cpp` (store seulement — l'extracteur arrive en Task 2)**
```c
#include "context.h"
#include <string.h>

int ctx_find(const Context* c, const char* name) {
    for (int i = 0; i < c->count; i++)
        if (strncmp(c->vars[i].name, name, ID_LEN) == 0) return i;
    return -1;
}

static CtxVar* ctx_slot(Context* c, const char* name) {
    int i = ctx_find(c, name);
    if (i >= 0) return &c->vars[i];
    if (c->count >= MAX_CTX_VARS) return nullptr;
    CtxVar* v = &c->vars[c->count++];
    strlcpy(v->name, name, sizeof(v->name));
    return v;
}

bool ctx_set_num(Context* c, const char* name, double v, uint32_t now) {
    CtxVar* s = ctx_slot(c, name);
    if (!s) return false;
    s->type = CTX_NUM; s->num = v; s->updated_at = now;
    return true;
}

bool ctx_set_str(Context* c, const char* name, const char* v, uint32_t now) {
    CtxVar* s = ctx_slot(c, name);
    if (!s) return false;
    s->type = CTX_STR; strlcpy(s->str, v ? v : "", sizeof(s->str)); s->updated_at = now;
    return true;
}

int ctx_apply_json(Context* c, JsonObjectConst obj, uint32_t now) {
    int n = 0;
    for (JsonPairConst kv : obj) {
        JsonVariantConst v = kv.value();
        if (v.is<const char*>())                 { if (ctx_set_str(c, kv.key().c_str(), v.as<const char*>(), now)) n++; }
        else if (v.is<float>() || v.is<int>())   { if (ctx_set_num(c, kv.key().c_str(), v.as<double>(), now)) n++; }
        // objet/array/bool/null ignorés en v1
    }
    return n;
}
```
(`ctx_extract_pointer` n'est PAS encore défini ; Task 2 l'ajoute. Ne pas l'appeler avant.)

- [ ] **Step 4: Compiler `context.cpp` en natif**

Dans `platformio.ini`, `[env:native]`, ajouter `context.cpp` au filtre :
```ini
build_src_filter = -<*> +<dashboard.cpp> +<format.cpp> +<color.cpp> +<nav_logic.cpp> +<context.cpp>
```

- [ ] **Step 5: Tests du store (écrire d'abord — rouge)**

Dans `test/test_core/test_main.cpp`, ajouter l'include en tête (après les autres) `#include "context.h"`, puis ces tests :
```cpp
void test_ctx_set_find_num(void) {
    Context c{};
    TEST_ASSERT_TRUE(ctx_set_num(&c, "cpu", 42, 100));
    int i = ctx_find(&c, "cpu");
    TEST_ASSERT_TRUE(i >= 0);
    TEST_ASSERT_EQUAL_INT(CTX_NUM, c.vars[i].type);
    TEST_ASSERT_EQUAL_INT(42, (int)c.vars[i].num);
    TEST_ASSERT_EQUAL_UINT32(100, c.vars[i].updated_at);
}
void test_ctx_overwrite_keeps_one_slot(void) {
    Context c{};
    ctx_set_num(&c, "x", 1, 0);
    ctx_set_str(&c, "x", "hi", 5);
    TEST_ASSERT_EQUAL_INT(1, c.count);                 // même nom = même slot
    int i = ctx_find(&c, "x");
    TEST_ASSERT_EQUAL_INT(CTX_STR, c.vars[i].type);
    TEST_ASSERT_EQUAL_STRING("hi", c.vars[i].str);
}
void test_ctx_full_rejects(void) {
    Context c{};
    char nm[8];
    for (int k = 0; k < MAX_CTX_VARS; k++) { snprintf(nm, sizeof(nm), "v%d", k); TEST_ASSERT_TRUE(ctx_set_num(&c, nm, k, 0)); }
    TEST_ASSERT_FALSE(ctx_set_num(&c, "over", 1, 0));  // plein → refus
}
```
Ajouter dans `main()` : `RUN_TEST(test_ctx_set_find_num); RUN_TEST(test_ctx_overwrite_keeps_one_slot); RUN_TEST(test_ctx_full_rejects);`

- [ ] **Step 6: Run + commit**

Run: `pio test -e native` → PASS, 42 test cases (39 + 3), 0 failed.
```bash
git add src/context.h src/context.cpp src/config.h platformio.ini test/test_core/test_main.cpp
git commit -m "Rich_Telemetry: firmware — module contexte (blackboard) + build natif"
```

---

## Task 2: Extracteur JSON Pointer

**Files:** Modify `src/context.cpp`, `test/test_core/test_main.cpp`

- [ ] **Step 1: Test d'abord (rouge — fonction absente)**
```cpp
void test_ptr_nested_object(void) {
    JsonDocument d; deserializeJson(d, "{\"main\":{\"temp\":21}}");
    JsonVariantConst v = ctx_extract_pointer(d.as<JsonVariantConst>(), "/main/temp");
    TEST_ASSERT_FALSE(v.isNull());
    TEST_ASSERT_EQUAL_INT(21, v.as<int>());
}
void test_ptr_array_index(void) {
    JsonDocument d; deserializeJson(d, "{\"list\":[10,20,30]}");
    JsonVariantConst v = ctx_extract_pointer(d.as<JsonVariantConst>(), "/list/1");
    TEST_ASSERT_EQUAL_INT(20, v.as<int>());
}
void test_ptr_missing_is_null(void) {
    JsonDocument d; deserializeJson(d, "{\"a\":1}");
    TEST_ASSERT_TRUE(ctx_extract_pointer(d.as<JsonVariantConst>(), "/a/b").isNull());
    TEST_ASSERT_TRUE(ctx_extract_pointer(d.as<JsonVariantConst>(), "/nope").isNull());
}
void test_ptr_escape(void) {
    JsonDocument d; deserializeJson(d, "{\"a/b\":7}");
    TEST_ASSERT_EQUAL_INT(7, ctx_extract_pointer(d.as<JsonVariantConst>(), "/a~1b").as<int>());
}
```
Ajouter les 4 `RUN_TEST` correspondants dans `main()`.

Run: `pio test -e native` → échoue (compile : `ctx_extract_pointer` non défini). C'est le rouge attendu.

- [ ] **Step 2: Implémenter `ctx_extract_pointer` dans `src/context.cpp`**

Ajouter `#include <stdlib.h>` en tête de `context.cpp`, puis :
```c
// JSON Pointer (RFC 6901) : "/a/b/0", avec déséchappement ~1→/ et ~0→~.
JsonVariantConst ctx_extract_pointer(JsonVariantConst root, const char* ptr) {
    if (!ptr || ptr[0] != '/') return JsonVariantConst();
    JsonVariantConst cur = root;
    char token[64];
    for (const char* p = ptr; *p == '/'; ) {
        p++;
        size_t k = 0;
        while (*p && *p != '/' && k < sizeof(token) - 1) {
            char ch = *p++;
            if (ch == '~' && *p == '1') { ch = '/'; p++; }
            else if (ch == '~' && *p == '0') { ch = '~'; p++; }
            token[k++] = ch;
        }
        token[k] = '\0';
        if (cur.is<JsonObjectConst>())      cur = cur.as<JsonObjectConst>()[token];
        else if (cur.is<JsonArrayConst>())  cur = cur.as<JsonArrayConst>()[(size_t)atoi(token)];
        else                                return JsonVariantConst();
        if (cur.isNull()) return JsonVariantConst();
    }
    return cur;
}
```

- [ ] **Step 3: Run + commit**

Run: `pio test -e native` → PASS, 46 test cases, 0 failed.
```bash
git add src/context.cpp test/test_core/test_main.cpp
git commit -m "Rich_Telemetry: firmware — extracteur JSON Pointer (ctx_extract_pointer)"
```

---

## Task 3: `ctx_apply_json` — appliquer {var:valeur} au contexte

**Files:** Modify `test/test_core/test_main.cpp`

(`ctx_apply_json` est déjà implémenté en Task 1 ; cette tâche ajoute sa couverture.)

- [ ] **Step 1: Tests**
```cpp
void test_ctx_apply_json_num_and_str(void) {
    Context c{};
    JsonDocument d; deserializeJson(d, "{\"cpu\":42,\"host\":\"srv1\"}");
    int n = ctx_apply_json(&c, d.as<JsonObjectConst>(), 7);
    TEST_ASSERT_EQUAL_INT(2, n);
    TEST_ASSERT_EQUAL_INT(42, (int)c.vars[ctx_find(&c,"cpu")].num);
    TEST_ASSERT_EQUAL_INT(CTX_STR, c.vars[ctx_find(&c,"host")].type);
    TEST_ASSERT_EQUAL_STRING("srv1", c.vars[ctx_find(&c,"host")].str);
}
```
Ajouter `RUN_TEST(test_ctx_apply_json_num_and_str);`.

- [ ] **Step 2: Run + commit**

Run: `pio test -e native` → PASS, 47 test cases, 0 failed.
```bash
git add test/test_core/test_main.cpp
git commit -m "Rich_Telemetry: firmware — couverture ctx_apply_json"
```

---

## Task 4: Intégration dashboard — `ctx`, champ `bind`, `dash_set_context`

**Files:** Modify `src/dashboard.h`, `src/dashboard.cpp`, `test/test_core/test_main.cpp`

- [ ] **Step 1: `dashboard.h`**

Ajouter `#include "context.h"` après `#include "config.h"`. Dans `struct Component`, après `uint8_t led_brightness_cfg;` :
```c
    char     bind[ID_LEN];           // nom de variable du contexte (pull) ; vide = push par id
```
Dans `struct Dashboard`, après `bool values_dirty;` :
```c
    Context  ctx;                    // blackboard alimenté par /context (push) et le pull (P2)
```
Après les déclarations existantes :
```c
void dash_set_context(Dashboard* d, const char* json, uint32_t now);
void context_apply(Dashboard* d);
```

- [ ] **Step 2: `dashboard.cpp` — parse `bind` + `dash_set_context`**

Dans la boucle de parse des composants (après `c.led_brightness_cfg = o["brightness"] | 64;`) :
```c
        strlcpy(c.bind, o["bind"] | "", sizeof(c.bind));
```
Et, à la fin du fichier (ou près de `dash_apply_update`), ajouter (l'implémentation de `context_apply` arrive en Task 5) :
```c
void dash_set_context(Dashboard* d, const char* json, uint32_t now) {
    JsonDocument doc;
    if (deserializeJson(doc, json)) return;          // JSON invalide : on garde le contexte
    ctx_apply_json(&d->ctx, doc.as<JsonObjectConst>(), now);
}
```

- [ ] **Step 3: Tests (bind parsé + dash_set_context écrit le contexte)**
```cpp
void test_layout_bind_parsed(void) {
    Dashboard d{}; char err[80];
    const char* L = "{\"components\":{\"t\":{\"type\":\"readout\",\"bind\":\"temp\"}},\"pages\":[]}";
    TEST_ASSERT_TRUE(dash_set_layout(&d, L, err, sizeof(err)));
    TEST_ASSERT_EQUAL_STRING("temp", d.components[dash_find(&d,"t")].bind);
}
void test_dash_set_context_writes_ctx(void) {
    Dashboard d{};
    dash_set_context(&d, "{\"temp\":21}", 3);
    TEST_ASSERT_TRUE(ctx_find(&d.ctx, "temp") >= 0);
    TEST_ASSERT_EQUAL_INT(21, (int)d.ctx.vars[ctx_find(&d.ctx,"temp")].num);
}
```
Ajouter les 2 `RUN_TEST`.

- [ ] **Step 4: Run + commit**

Run: `pio test -e native` → PASS, 49 test cases, 0 failed.
```bash
git add src/dashboard.h src/dashboard.cpp test/test_core/test_main.cpp
git commit -m "Rich_Telemetry: firmware — bind + ctx dans le modèle, dash_set_context"
```

---

## Task 5: `context_apply` — propager les variables aux composants liés

**Files:** Modify `src/dashboard.cpp`, `test/test_core/test_main.cpp`

- [ ] **Step 1: Test d'abord (rouge — `context_apply` non défini)**
```cpp
// helper : layout à un composant lié, de type donné
static const char* bound_layout(const char* type, const char* extra) {
    static char b[256];
    snprintf(b, sizeof(b),
        "{\"components\":{\"x\":{\"type\":\"%s\",\"bind\":\"v\"%s}},"
        "\"pages\":[{\"name\":\"p\",\"place\":[{\"ref\":\"x\"}]}]}", type, extra);
    return b;
}
void test_ctxapply_readout_num_formats(void) {
    Dashboard d{}; char err[80];
    dash_set_layout(&d, bound_layout("readout", ",\"unit\":\"C\""), err, sizeof(err));
    dash_set_context(&d, "{\"v\":21}", 1);
    context_apply(&d);
    int i = dash_find(&d,"x");
    TEST_ASSERT_EQUAL_STRING("21 C", d.components[i].vstr);
    TEST_ASSERT_TRUE(d.components[i].dirty);
}
void test_ctxapply_readout_string(void) {
    Dashboard d{}; char err[80];
    dash_set_layout(&d, bound_layout("readout", ""), err, sizeof(err));
    dash_set_context(&d, "{\"v\":\"OK\"}", 1);
    context_apply(&d);
    TEST_ASSERT_EQUAL_STRING("OK", d.components[dash_find(&d,"x")].vstr);
}
void test_ctxapply_bar_value(void) {
    Dashboard d{}; char err[80];
    dash_set_layout(&d, bound_layout("bar", ""), err, sizeof(err));
    dash_set_context(&d, "{\"v\":63}", 1);
    context_apply(&d);
    TEST_ASSERT_EQUAL_INT(63, d.components[dash_find(&d,"x")].value);
}
void test_ctxapply_unchanged_not_dirty(void) {
    Dashboard d{}; char err[80];
    dash_set_layout(&d, bound_layout("bar", ""), err, sizeof(err));
    dash_set_context(&d, "{\"v\":63}", 1);
    context_apply(&d);
    d.components[dash_find(&d,"x")].dirty = false;
    context_apply(&d);                                  // même valeur : pas de re-dirty
    TEST_ASSERT_FALSE(d.components[dash_find(&d,"x")].dirty);
}
void test_ctxapply_missing_var_keeps_value(void) {
    Dashboard d{}; char err[80];
    dash_set_layout(&d, bound_layout("bar", ""), err, sizeof(err));
    d.components[dash_find(&d,"x")].value = 7;
    context_apply(&d);                                  // variable "v" absente
    TEST_ASSERT_EQUAL_INT(7, d.components[dash_find(&d,"x")].value);
}
```
Ajouter les 5 `RUN_TEST`. Run → rouge (compile : `context_apply` non défini).

- [ ] **Step 2: Implémenter `context_apply` dans `src/dashboard.cpp`**
```c
void context_apply(Dashboard* d) {
    for (int i = 0; i < d->comp_count; i++) {
        Component& c = d->components[i];
        if (c.bind[0] == '\0') continue;                // pas de bind → push par id
        int vi = ctx_find(&d->ctx, c.bind);
        if (vi < 0) continue;                           // variable absente → garde la dernière valeur
        const CtxVar& v = d->ctx.vars[vi];
        bool changed = false;
        switch (c.type) {
            case COMP_BAR:
            case COMP_RING:                             // scalaire → valeur primaire (pct pour le ring)
                if (v.type == CTX_NUM) {
                    int32_t nv = (int32_t)v.num;
                    if (c.value != nv) { c.value = nv; changed = true; }
                }
                break;
            case COMP_READOUT:
            case COMP_LABEL: {                          // num → format_value (unité pour readout) ; str → tel quel
                char nb[TEXT_LEN];
                if (v.type == CTX_STR) strlcpy(nb, v.str, sizeof(nb));
                else format_value(v.num, c.type == COMP_READOUT ? c.unit : "", nb, sizeof(nb));
                if (strncmp(c.vstr, nb, sizeof(c.vstr)) != 0) { strlcpy(c.vstr, nb, sizeof(c.vstr)); changed = true; }
                break;
            }
            default: break;                            // led_ring/sound : pas de bind
        }
        if (changed) { c.dirty = true; d->values_dirty = true; }
    }
}
```

- [ ] **Step 3: Run + commit**

Run: `pio test -e native` → PASS, 54 test cases, 0 failed.
```bash
git add src/dashboard.cpp test/test_core/test_main.cpp
git commit -m "Rich_Telemetry: firmware — context_apply (variables liées → composants)"
```

---

## Self-Review

- **Couverture spec P1 :** blackboard (T1) ✓ ; extracteur JSON Pointer (T2) ✓ ; `ctx_apply_json`/`dash_set_context` (T1/T3/T4) ✓ ; champ `bind` (T4) ✓ ; `context_apply` (T5) ✓. L'endpoint `/context` et le câblage `loop()` sont explicitement **hors P1** (→ P2).
- **Placeholders :** aucun ; code complet, commandes et comptes de tests exacts.
- **Cohérence des types :** `Context`/`CtxVar`/`CtxType` définis en T1 et consommés en T4/T5 ; `ctx_extract_pointer` déclaré T1, défini T2 ; `context_apply`/`dash_set_context` déclarés T4, `context_apply` défini T5.
- **Risque :** `ctx_extract_pointer` déclaré dès T1 (header) mais défini en T2 — il n'est appelé par aucun test avant T2, donc le link natif T1 ne le réclame pas. OK.

## Vérification finale

`pio test -e native` → **54/54**. P1 livré, **sans device**. P2 (endpoint `/context`, câblage `loop()`, tâche productrice, HTTP(S), sources, secrets) = plan séparé, device branché.
