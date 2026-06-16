# Registre de types — Phase 2a (firmware, partie native-testable) — Plan

> **For agentic workers:** Steps use checkbox (`- [ ]`) syntax. TDD, frequent commits.

**Goal:** Côté firmware, (1) ajouter un **test C de conformité** qui lit le schema partagé et vérifie que `parse_type` (via `dash_set_layout`) résout **chaque** type déclaré et **rejette** un inconnu ; (2) refactorer `parse_type` (chaîne de `strcmp`) en **table de noms**. Tout est **native-testable, sans device**.

**Architecture:** Le schema (`schema/layout.schema.json`, `$defs.component.oneOf`) est la liste canonique des types. Le test natif (Unity, `test/test_core/test_main.cpp`) parse le schema (ArduinoJson, déjà dans `lib_deps`) et boucle sur les types ; il échoue rouge si un type du schema n'est pas résolu par le firmware. La table de noms remplace la chaîne de `if (!strcmp(...))`, l'`enum CompType` restant l'identité du type.

**Tech Stack:** C++17, PlatformIO `env:native` (platform native, Unity, ArduinoJson v7). Tests : `pio test -e native` depuis le répertoire du projet. Baseline actuelle : **38 test cases, 38 PASSED**.

**Périmètre — 2a uniquement.** La **vtable** (`apply_one` + les 2 `switch` de `view.cpp`) est la **Phase 2b**, reportée : son rendu LVGL n'est pas native-testable et exige une validation visuelle sur le Guition. (Raffinement noté pour 2b : `build`/`sync` devront exposer le `Placement` + les 3 slots LVGL `s_widget`/`s_sub1`/`s_sub2`, car ring/bar créent plusieurs objets — la signature `(parent,&c)`/`(obj,&c)` de la spec était un croquis.)

**Répertoire de travail :** `devices/guition_knob/projects/Rich_Telemetry/`.

---

## File Structure

- **Modify** `platformio.ini` — `[env:native]` : ajouter le flag `RT_SCHEMA_PATH` (chemin absolu du schema, via `${PROJECT_DIR}`).
- **Modify** `test/test_core/test_main.cpp` — nouveaux includes + fonction `test_schema_types_all_resolve` + `RUN_TEST`.
- **Modify** `src/dashboard.cpp` — `parse_type` : chaîne de `strcmp` → table.

---

## Task 1: Test C de conformité (schema → parse_type), piloté par le schema

**Files:**
- Modify: `platformio.ini`
- Modify: `test/test_core/test_main.cpp`

- [ ] **Step 1: Inject the schema path into the native build**

Dans `platformio.ini`, `[env:native]`, remplacer la ligne `build_flags` par (ajout du dernier flag, quoting compris) :

```ini
build_flags = -DRT_NATIVE_TEST -Isrc -std=gnu++17 '-DRT_SCHEMA_PATH="${PROJECT_DIR}/schema/layout.schema.json"'
```

`${PROJECT_DIR}` est interpolé par PlatformIO en chemin absolu (portable Win/macOS) ; les guillemets simples préservent les guillemets doubles → `RT_SCHEMA_PATH` est un littéral C string.

- [ ] **Step 2: Write the conformance test**

Dans `test/test_core/test_main.cpp`, ajouter ces includes en tête (après les includes existants) :

```cpp
#include <ArduinoJson.h>
#include <stdio.h>
#include <stdlib.h>
```

Puis ajouter cette fonction de test (par ex. juste après `test_layout_unknown_type_rejected`) :

```cpp
// Conformité firmware ↔ schema : pour CHAQUE type déclaré dans le schema partagé
// (component.oneOf → comp_* → type.const), parse_type (via dash_set_layout) doit le
// résoudre ; un type absent du schema doit être rejeté. Échoue rouge si le firmware
// oublie un type que le schema déclare. Le schema est lu depuis RT_SCHEMA_PATH.
void test_schema_types_all_resolve(void) {
    FILE* f = fopen(RT_SCHEMA_PATH, "rb");
    TEST_ASSERT_NOT_NULL_MESSAGE(f, "impossible d'ouvrir RT_SCHEMA_PATH: " RT_SCHEMA_PATH);
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char* schema = (char*)malloc((size_t)n + 1);
    size_t rd = fread(schema, 1, (size_t)n, f); schema[rd] = '\0';
    fclose(f);

    JsonDocument doc;
    DeserializationError e = deserializeJson(doc, schema);
    TEST_ASSERT_TRUE_MESSAGE(!e, "schema JSON invalide");

    JsonArrayConst oneOf = doc["$defs"]["component"]["oneOf"].as<JsonArrayConst>();
    TEST_ASSERT_FALSE_MESSAGE(oneOf.isNull(), "component.oneOf absent du schema");

    int count = 0;
    for (JsonObjectConst ref : oneOf) {
        const char* r = ref["$ref"];                       // ex "#/$defs/comp_ring"
        TEST_ASSERT_NOT_NULL_MESSAGE(r, "entree oneOf sans $ref");
        const char* slash = strrchr(r, '/');
        TEST_ASSERT_NOT_NULL(slash);
        const char* defName = slash + 1;                   // "comp_ring"
        const char* typeName = doc["$defs"][defName]["properties"]["type"]["const"];
        TEST_ASSERT_NOT_NULL_MESSAGE(typeName, defName);

        char layout[192];
        snprintf(layout, sizeof(layout),
            "{\"components\":{\"x\":{\"type\":\"%s\"}},\"pages\":[]}", typeName);
        Dashboard d{}; char err[80];
        TEST_ASSERT_TRUE_MESSAGE(dash_set_layout(&d, layout, err, sizeof(err)), typeName);
        count++;
    }
    free(schema);
    TEST_ASSERT_GREATER_THAN_MESSAGE(0, count, "aucun type extrait du schema");

    // Un type absent du schema doit être rejeté.
    Dashboard d{}; char err[80];
    TEST_ASSERT_FALSE(dash_set_layout(&d,
        "{\"components\":{\"x\":{\"type\":\"definitely_not_a_type\"}},\"pages\":[]}",
        err, sizeof(err)));
}
```

- [ ] **Step 3: Register the test**

Dans le `main()` de `test_main.cpp`, ajouter (par ex. juste après `RUN_TEST(test_layout_unknown_type_rejected);`) :

```cpp
    RUN_TEST(test_schema_types_all_resolve);
```

- [ ] **Step 4: Run the native suite**

Run: `pio test -e native`
Expected: PASS — **39 test cases** (38 baseline + 1 nouveau), 0 failed. Ce test passe contre le `parse_type` actuel (chaîne de `strcmp`) : il **caractérise** le comportement courant et **gardera** le refactor de Task 2.

Si `fopen` échoue (message « impossible d'ouvrir RT_SCHEMA_PATH: … » avec le chemin), c'est que l'interpolation `${PROJECT_DIR}` / le quoting n'a pas pris : NE PAS contourner en silence — STOP, reporter le chemin tenté et l'environnement (le quoting des `build_flags` PlatformIO est le suspect).

- [ ] **Step 5: Commit**

```bash
git add platformio.ini test/test_core/test_main.cpp
git commit -m "Rich_Telemetry: firmware — test C de conformité types↔schema (parse_type)"
```

---

## Task 2: `parse_type` → table de noms

**Files:**
- Modify: `src/dashboard.cpp`

- [ ] **Step 1: Replace the strcmp chain with a table**

Dans `src/dashboard.cpp`, remplacer la fonction `parse_type` actuelle (lignes 13–22) :

```cpp
static CompType parse_type(const char* s) {
    if (!s) return COMP_NONE;
    if (!strcmp(s,"label"))    return COMP_LABEL;
    if (!strcmp(s,"readout"))  return COMP_READOUT;
    if (!strcmp(s,"bar"))      return COMP_BAR;
    if (!strcmp(s,"ring"))     return COMP_RING;
    if (!strcmp(s,"led_ring")) return COMP_LED_RING;
    if (!strcmp(s,"sound"))    return COMP_SOUND;
    return COMP_NONE;
}
```

par :

```cpp
// Table nom→type : seul point d'énumération des types côté parse. Le test de conformité
// (test_schema_types_all_resolve) garantit qu'elle couvre exactement les types du schema.
static const struct { const char* name; CompType type; } COMP_NAMES[] = {
    { "label",    COMP_LABEL    }, { "readout", COMP_READOUT  }, { "bar",   COMP_BAR   },
    { "ring",     COMP_RING     }, { "led_ring", COMP_LED_RING }, { "sound", COMP_SOUND },
};

static CompType parse_type(const char* s) {
    if (!s) return COMP_NONE;
    for (const auto& e : COMP_NAMES)
        if (!strcmp(s, e.name)) return e.type;
    return COMP_NONE;
}
```

Comportement strictement préservé (y compris la garde `!s` et le défaut `COMP_NONE`).

- [ ] **Step 2: Run the native suite**

Run: `pio test -e native`
Expected: PASS — **39 test cases**, 0 failed. Le refactor est gardé par le test de conformité (Task 1) **et** par les tests existants (`test_layout_types_and_geom` résout `ring`, `test_layout_unknown_type_rejected` rejette un inconnu).

- [ ] **Step 3: Commit**

```bash
git add src/dashboard.cpp
git commit -m "Rich_Telemetry: firmware — parse_type en table de noms"
```

---

## Self-Review (à exécuter après écriture)

- **Couverture spec (Phase 2a) :** « `parse_type` → table » = Task 2 ✓ ; « test C natif : parse le schema, assert `parse_type` résout chacun et rejette un inconnu » = Task 1 ✓. La vtable reste explicitement hors 2a (Phase 2b).
- **Pas de placeholder :** code complet pour les deux tâches ; chemins et commandes exacts.
- **Cohérence des types :** `RT_SCHEMA_PATH` défini en Task 1 (build flag) et consommé en Task 1 (test) ; `COMP_NAMES`/`parse_type` cohérents avec l'`enum CompType` de `dashboard.h`.
- **Risque connu :** quoting `${PROJECT_DIR}` dans `build_flags` — rendu loud par le message `fopen` (Step 4).

## Vérification finale

`pio test -e native` → 39/39. Aucune validation device nécessaire pour 2a (firmware compilé en natif ; LVGL non impliqué). La 2b (vtable view.cpp) sera planifiée séparément et validée sur le Guition.
