# HANDOFF — Rich_Telemetry, après Pull P2 (2026-06-16)

Document **autoporteur** : la session qui reprend a zéro contexte conversationnel.
Projet : `devices/guition_knob/projects/Rich_Telemetry/` (Guition JC3636K718, ESP32-S3,
écran rond 360×360). Tout l'historique ci-dessous est **mergé et poussé sur `origin/master`**.

Le projet est un **dashboard piloté par config JSON, pensé pour grossir en framework
générique** (≠ les démos `Basic_*`). Privilégier généralité/extensibilité aux raccourcis.

## Où on en est (FAIT, sur `origin/master`)

- **Anneau — UI** : `center_pct`/`center_color`/`font`/`start_angle` exposés (designer + firmware).
- **Mécanisme d'ajout de types — P1 (designer)** : `designer/js/registry.js` = source unique par
  type + test de conformité registre↔schema. **2a (firmware)** : `parse_type` en table de noms
  `COMP_NAMES[]` + test C de conformité (lit le schema).
- **Pull de données — P1 (modèle, natif)** : `src/context.{h,cpp}` (blackboard `Context`/`CtxVar`,
  `ctx_find/ctx_set_num/ctx_set_str/ctx_apply_json`, `ctx_extract_pointer` JSON Pointer RFC6901) ;
  `Dashboard.ctx`, `Component.bind`, `dash_set_context()`, `context_apply()`.
- **Pull de données — P2 (réseau, device) ✅ NOUVEAU, validé on-device** (plan
  `docs/superpowers/plans/2026-06-16-pull-P2-data-context-network.md`) :
  - **parse top-level `sources`** (`struct Source`/`SourceHeader`/`SourceVar` dans `dashboard.h`,
    parse plat dans `dashboard.cpp` ; `url`/`interval_s`/`headers`/`vars`=nom→JSON Pointer ;
    borne mini `CTX_MIN_INTERVAL_S=5`) — native-testé.
  - **`POST /context {var:val}`** (push vers variable) + **`g_ctx_mutex`** + `context_apply()`
    **throttlé non bloquant** (`xSemaphoreTake(...,0)`, 100 ms) dans `loop()`.
  - **store de secrets write-only** `src/secret_store.{h,cpp}` (LittleFS `/secrets.json`) +
    **`POST /secrets`** (jamais GET, absent layout/export).
  - **tâche FreeRTOS** `src/net_pull.{h,cpp}` épinglée **cœur 0**, pile 16 KB : pattern
    **snapshot config sous mutex → fetch HORS mutex → écriture ctx sous mutex** (le lock n'est
    jamais tenu pendant `http.GET()` → l'UI ne gèle pas) ; `HTTPClient`+`WiFiClientSecure`
    (`.setInsecure()`) ; résolution `$secret` dans les headers ; échec = dernière valeur gardée.
  - `dash_set_layout` désormais **sous mutex** (race cross-cœur) ; **`/status` enrichi** d'un
    tableau `sources` (`name`/`last_status`/`err_count`/`updated_at`).
  - **Décisions d'archi prises en exécution** : un **seul** mutex protège ctx+sources ; les
    fonctions modèle (`context_apply`/`dash_set_context`/`dash_set_layout`) **restent pures**
    (aucun FreeRTOS) → toujours native-testables ; ce sont les **appelants device** qui verrouillent.
  - **Vérifié** : `pio test -e native` **59/59** ; `pio run -e esp32s3` SUCCESS (RAM 42.7 %,
    Flash 25.0 %) ; on-device : `/context`→écran, `/secrets` (POST ok/GET 404/absent),
    pull LAN HTTP (live-update + robustesse coupure), pull HTTPS+secret (`httpbin.org/headers`
    echo, pile TLS OK, pas de reset).

Specs (dans `docs/superpowers/specs/`) :
- `2026-06-16-component-type-registry-design.md` — mécanisme de types (Phase 2 = vtable).
- `2026-06-16-chart-meter-components-design.md` — 2 nouveaux widgets.
- `2026-06-16-pull-data-context-design.md` — pull (§ P3 = designer, reste à faire).

## File de travail restante, ordre recommandé

Dépendances : **chart/meter dépendent de la 2b** (la vtable). **P3 = designer, non device-gated.**

### Track 2 — La VTABLE puis les nouveaux widgets (firmware, device)
**Phase 2b** — spec `specs/2026-06-16-component-type-registry-design.md` § Phase 2. Refactor de
dispatch : vtable `{ name, apply_value, build, sync }` indexée par l'enum `CompType`, consolidant
`apply_one` (`dashboard.cpp`, voie `/update`) + les **2 `switch` de `view.cpp`** (build + sync).
**Signature réelle** (la spec est un croquis) : `build`/`sync` exposent le `Placement` + les **3
slots LVGL** `s_widget`/`s_sub1`/`s_sub2` (ring/bar = multi-objets). Types physiques
(`led_ring`/`sound`) : `build/sync = NULL`. Le **parse statique reste plat**, la **struct
`Component` reste plate** (union différée). Rendu **non native-testable** → flash + contrôle
visuel (non-régression des 6 types existants : label/readout/bar/ring/led_ring/sound).

**chart + meter (après 2b)** — spec `specs/2026-06-16-chart-meter-components-design.md`. Les
ajouter comme **premiers nouveaux types via la vtable**. `chart` = `lv_chart`, historique en
**ring buffer dans le modèle** (`/update` pousse un scalaire → append ; `view_sync` mirroir →
idempotent). `meter` = `lv_meter`, scalaire→aiguille, **`thresholds` réutilisés en zones d'arc**.
**LVGL v8.4 obligatoire** : activer `LV_USE_CHART`/`LV_USE_METER` dans `src/lv_conf.h` ; NE PAS
suivre les exemples master de lvgl.io (v9, `lv_meter`→`lv_scale`). Docs : Context7
`/websites/lvgl_io_8_4`. Le parse/append du ring buffer chart est **native-testable** (pousser 35
valeurs → garder les 30 dernières) avant même le rendu. Ces types `bind` aussi (P2) : penser à
les couvrir dans `context_apply` (chart = append, meter = valeur primaire).

### Track 3 — Designer (web app, SANS la carte)
**Pull P3** : éditeur de `sources` (3 colonnes url/interval/headers/vars) + champ **`bind`** dans
l'inspecteur + entrées schema. **chart/meter côté designer** : entrées `registry.js` +
`buildChart`/`buildMeter` (aperçu SVG) + `comp_chart`/`comp_meter` au schema. **⚠ Le contrat
`schema/layout.schema.json` est en retard sur le firmware** : il ne déclare PAS encore
`sources`/`secrets` top-level NI le champ `bind` des composants (le firmware les parse déjà, mais
le test de conformité ne couvre que les *types*). P3 doit les ajouter au schema. Vérif : `node
--test` + conformité + navigateur (servir depuis la **racine projet** sur un **port neuf** — piège
du cache de modules ES, cf. `designer/HANDOFF.md`).

## Workflow de validation sur device (éprouvé pour P2)

Pattern : **un agent (general-purpose) code+compile** (`pio run -e esp32s3` + `pio test -e native`
pour le native-testable) ; **le contrôleur flashe + série + `curl`** ; **l'utilisateur valide le
visuel**. Exécution en **subagent-driven** (un implémenteur frais par task, full texte fourni — ne
PAS faire lire le plan au subagent). **Gotcha de ce harness** : les *reviewers* subagents isolés
**ne relaient pas leur verdict** (juste des notifs idle) → faire les revues spec+qualité
**soi-même** par lecture du diff committé (fiable sur petits diffs). Les *implémenteurs*, eux,
rapportent bien via `SendMessage(to:"main")`. Le **clangd local** signale des faux positifs
`ArduinoJson.h/Arduino.h not found` (pas d'include path PlatformIO) — **ignorer** ; `pio` fait foi.

Astuces série/flash : **pas de `timeout` sur macOS** (utiliser un `until`-loop ; `sleep`
foreground est bloqué → temporiser en `perl -e 'select(undef,undef,undef,1)'`) ; lire l'IP DHCP au
boot via pyserial (`~/.platformio/penv/bin/python`, pulse DTR/RTS pour rebooter et capter `IP=`) ;
détecter un crash via l'`uptime_s` de `GET /status` ; persistance via `GET /layout`.

## Build / flash (Guition)

```bash
# depuis la racine du repo
./build.sh auto Rich_Telemetry --upload          # auto-détecte le device par son MAC, build + flash
# garde-fou MAC : tools/device_mac.py check tourne avant chaque flash
```
Le device tourne déjà notre firmware (CDC permanent) → rien de spécial. (Premier flash d'un Guition
neuf : mode download manuel BOOT — voir `devices/guition_knob/CLAUDE.md` § « First flash ».)

## État actuel du device (fin de session P2)

- **Layout d'usine reposé** (`view_default_layout`, titre « Claude », 2 anneaux + led_ring + sound,
  **sans `sources`** → aucun pull en cours). `GET /status` : `components:4`, `sources:[]`.
- **IP DHCP dernière connue : `192.168.1.35`** (peut changer — relire via série au besoin).
- **Secret de test résiduel** dans le store : `weather_key="PULLP2-OK-42"` (write-only, jamais
  servi, plus référencé — inoffensif ; pas d'endpoint DELETE par conception ; l'écraser via
  `POST /secrets {"weather_key":""}` si gênant).

## Pièges notables
- **Boucle mono-thread** : `WebServer` synchrone + `loop()` coopératif. Tout fetch HTTP est **hors
  `loop()`** (tâche dédiée P2) ; le rendu LVGL (2b/chart) reste dans le `loop()`.
- **LVGL v8.4** épinglé (`platformio.ini`). Widgets « extra » (chart/meter) à activer dans `lv_conf.h`.
- **Pile TLS** de la tâche pull = 16 KB (→ 20480 si un jour un GET HTTPS reset le device).
- **WiFi** : `src/secrets.h` (gitignored, compile-time) pour SSID/pass — **distinct** du store de
  secrets runtime. Le `.local`/mDNS ne résout pas sur ce LAN → IP DHCP directe.
- **Choix délibérés à ne pas « corriger »** : son en timeout-0 (non bloquant), swipes verticaux
  réservés à une future page de config.

## TL;DR pour démarrer
1. Choisir un track : **2b + chart/meter** (firmware, device, specs prêtes → aller direct par
   plan) **ou** **P3 designer** (web app, sans carte).
2. `writing-plans` pour le track choisi (2b/chart/meter ont leurs specs ; P3 dériver de la spec
   pull § P3), puis **subagent-driven** : implémenter → (revue diff soi-même) → flasher → valider
   visuellement.
3. Le schema `layout.schema.json` est le contrat partagé firmware↔designer : le faire évoluer par
   un commit dédié (ajouts `sources`/`secrets`/`bind` + types chart/meter) attendu par P3.
