# HANDOFF — Rich_Telemetry, session devant le Guition (2026-06-16)

Document **autoporteur** : la session qui reprend a zéro contexte conversationnel.
Projet : `devices/guition_knob/projects/Rich_Telemetry/` (Guition JC3636K718, ESP32-S3,
écran rond 360×360). Tout l'historique ci-dessous est **sur `origin/master`**.

## Où on en est

Une grosse session de design+implémentation a livré, **tout mergé et poussé sur `master`** :

- **Anneau — UI** : champs `center_pct`/`center_color`/`font`/`start_angle` exposés dans
  l'inspecteur du designer + grisage live.
- **Mécanisme d'ajout de types — Phase 1 (designer)** : `designer/js/registry.js` = source
  unique par type + test JS de conformité registre↔schema. Mergé.
- **Mécanisme — Phase 2a (firmware)** : `parse_type` en table de noms + test C de conformité
  (`pio test -e native`). Mergé.
- **Pull de données — Phase P1 (firmware, couche modèle, native)** : module `src/context.{h,cpp}`
  (blackboard de variables nommées + extracteur JSON Pointer) ; `Dashboard.ctx`,
  `Component.bind`, `dash_set_context()`, `context_apply()`. `pio test -e native` **54/54**.
  Mergé.

Specs & plans (dans `docs/superpowers/`) :
- `specs/2026-06-16-component-type-registry-design.md` (+ plans `plans/...-phase1-designer.md`,
  `plans/...-phase2a-firmware.md`) — mécanisme de types.
- `specs/2026-06-16-chart-meter-components-design.md` — 2 nouveaux widgets.
- `specs/2026-06-16-pull-data-context-design.md` (+ plan `plans/...-pull-P1-data-context-native.md`).

## ⚠ À FAIRE EN PREMIER : vérifier le build firmware

P1 et 2a sont **vérifiés en natif uniquement** (`pio test -e native`). Le **build firmware
complet `esp32s3` n'a jamais été lancé** depuis ces changements (pas de cache de plateforme
au moment du handoff → on a reporté). P1 a ajouté `#include "context.h"` à `dashboard.h`
(tire `ArduinoJson.h` partout) et de nouveaux champs de struct — **additif, aucun nouvel
appel dans les fichiers firmware**, donc risque faible, mais à confirmer :

```bash
cd devices/guition_knob/projects/Rich_Telemetry
pio run -e esp32s3            # 1er run = download plateforme pioarduino (long) + compile LVGL
```
Si ça casse, c'est probablement `context.{h,cpp}` sous le toolchain esp32 — corriger avant
d'empiler la suite.

## File de travail (device-gated), ordre recommandé

Dépendances : **chart/meter dépendent de la 2b** (la vtable). Pull P2 est **orthogonal**
(plan de données, pas rendu). P3 = designer, **non device-gated**.

### Track 1 — Finir le PULL (suite directe de P1) — RECOMMANDÉ pour commencer
**Pull P2 (réseau, device)** — spec `specs/2026-06-16-pull-data-context-design.md` § P2.
Additif, faible risque, et ça « finit ce qu'on a commencé ». À écrire (`writing-plans`) puis
implémenter+flasher+valider :
1. Endpoint **`POST /context {var:val}`** dans `src/api.cpp` (calque `h_update`, appelle
   `dash_set_context(d, body, millis())`). Penser à `S->arg("plain")`.
2. Appel **`context_apply(&g_dash)`** dans `loop()` de `src/main.cpp` (cadence : à chaque tour
   ou throttlé comme le LED tick).
3. **Tâche FreeRTOS productrice** : `xTaskCreatePinnedToCore` sur le cœur libre ; boucle sur
   les `sources`, fetch chacune à `interval_s`, extrait les `vars` (`ctx_extract_pointer`),
   écrit le contexte sous **mutex** (`SemaphoreHandle_t` ; `context_apply` et l'API doivent
   prendre le même mutex). Le `loop()` reste non bloquant.
4. **Client HTTP(S)** : `HTTPClient` + `WiFiClientSecure` (`.setInsecure()` en v1, pas de
   pinning). HTTP simple pour le LAN.
5. **Parse `sources`** (top-level du layout : `url`/`interval_s`/`headers`/`vars`) dans
   `dashboard.cpp` (parse plat) + struct.
6. **Store de secrets** write-only : `POST /secrets {nom:val}` → LittleFS (`persist.*`),
   **jamais** servi par GET ni exporté ; résolution des `$nom` dans les `headers` au fetch.
Décisions verrouillées (voir spec) : coexistence push/pull (le `/update` par id reste
intact) ; blackboard de variables nommées ; secrets séparés référencés par `$nom`.
Validation : `curl` (poser un secret, poser un layout avec sources, observer le pull en
série + à l'écran sur un composant `bind`).

### Track 2 — La VTABLE puis les nouveaux widgets
**Phase 2b (firmware, device)** — spec `specs/2026-06-16-component-type-registry-design.md`
§ Phase 2. Refactor de dispatch : vtable `{ name, apply_value, build, sync }` indexée par
l'enum, consolidant `apply_one` (`/update`) + les **2 `switch` de `view.cpp`** (build l.153,
sync l.234). **Signature réelle** (la spec était un croquis) : `build`/`sync` exposent le
`Placement` + les **3 slots LVGL** `s_widget`/`s_sub1`/`s_sub2` (ring/bar = multi-objets).
Types physiques (led_ring/sound) : `build/sync = NULL`. Le **parse statique reste plat**, la
**struct reste plate** (union différée). Rendu **non native-testable** → flash + contrôle
visuel (non-régression des 6 types existants).

**chart + meter (après 2b)** — spec `specs/2026-06-16-chart-meter-components-design.md`. Les
ajouter comme **premiers nouveaux types via la vtable**. `chart` = `lv_chart`, historique en
**ring buffer dans le modèle** (`/update` pousse un scalaire → append ; `view_sync` mirroir →
idempotent). `meter` = `lv_meter`, scalaire→aiguille, **`thresholds` réutilisés en zones
d'arc**. **LVGL v8.4 obligatoire** : activer `LV_USE_CHART`/`LV_USE_METER` dans
`src/lv_conf.h` ; NE PAS suivre les exemples master de lvgl.io (v9, `lv_meter`→`lv_scale`).
Référence docs : Context7 `/websites/lvgl_io_8_4`. Le parse/append du ring buffer chart est
**native-testable** (pousser 35 valeurs → garder les 30 dernières) avant même le rendu.

### Track 3 — Designer (NON device-gated, faisable sans la carte)
**Pull P3** : éditeur de `sources` (3 colonnes) + champ `bind` dans l'inspecteur + entrée
schema. **chart/meter côté designer** : entrées `registry.js` + `buildChart`/`buildMeter`
(aperçu SVG) + `comp_chart`/`comp_meter` au schema. Vérif : `node --test` + conformité +
navigateur (servir depuis la **racine projet** sur un **port neuf** — piège du cache de
modules ES, cf. `designer/HANDOFF.md`).

## Workflow de validation sur device (rappel)

Pattern éprouvé : **un agent code+compile** ; **le contrôleur flashe + série + `curl`** ;
**l'utilisateur valide le visuel**. Astuces : pas de `timeout` sur macOS (utiliser un
`until`-loop ou `perl -e 'alarm'`) ; reset DTR/RTS via pyserial si le port se bloque ;
persistance vérifiable via `GET /layout` ; détecter un crash via l'uptime de `GET /status`.

## Build / flash (Guition)

```bash
# depuis la racine du repo
./build.sh guition_knob Rich_Telemetry --upload      # build + flash
./build.sh auto Rich_Telemetry --upload              # auto-détecte le device par son MAC
# garde-fou MAC : tools/device_mac.py check <device_dir> tourne avant chaque flash
```
**Premier flash d'un Guition neuf** (firmware vendor sans CDC) : mode download manuel — tenir
**BOOT**, brancher l'USB, relâcher BOOT ; il ré-énumère en `303A:1001`. Détail + gotcha
d'enrôlement MAC : `devices/guition_knob/CLAUDE.md` § « First flash ». (Si le device tourne
déjà notre firmware, rien de spécial.)

## Pièges notables
- **Boucle mono-thread** : `WebServer` synchrone + `loop()` coopératif. Tout fetch HTTP doit
  être **hors `loop()`** (tâche dédiée) — c'est toute la raison d'être de l'archi pull.
- **LVGL v8.4** épinglé (`platformio.ini`). Widgets « extra » à activer dans `lv_conf.h`.
- **WiFi** : `src/secrets.h` (gitignored) pour SSID/pass. Le device est joignable par **IP
  DHCP directe** (le `.local`/mDNS ne résout pas sur ce LAN — voir mémoire).
- **Choix délibérés à ne pas « corriger »** : son en timeout-0 (non bloquant), swipes
  verticaux réservés à une future page de config.

## TL;DR pour démarrer
1. `pio run -e esp32s3` (confirmer que le firmware build après P1/2a). 
2. Choisir un track : **Pull P2** (finir le pull, recommandé) **ou** **2b + chart/meter**
   (refactor + widgets) **ou** **P3 designer** (sans carte).
3. `writing-plans` pour le track choisi (sauf 2b/chart/meter dont les specs sont prêtes — y
   aller direct par plan), puis implémenter → flasher → valider visuellement.
