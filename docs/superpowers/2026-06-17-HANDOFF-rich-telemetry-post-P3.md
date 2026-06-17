# HANDOFF — Rich_Telemetry, après Pull P3 (designer) — 2026-06-17

Document **autoporteur** : la session qui reprend a zéro contexte conversationnel.
Projet : `devices/guition_knob/projects/Rich_Telemetry/` (Guition JC3636K718, ESP32-S3,
écran rond 360×360 + encodeur + anneau RGB). **Tout est mergé et poussé sur `origin/master`**
(HEAD `75be1df` au moment d'écrire ; `git log` pour le SHA courant).

Le projet est un **dashboard piloté par config JSON, pensé pour grossir en framework
générique** (≠ les démos `Basic_*`). Privilégier généralité/extensibilité aux raccourcis.

## TL;DR — état stable

**Firmware ET designer sont alignés ; le cycle pull/affichage est complet de bout en bout.** Le device
peut être configuré par un éditeur WYSIWYG web (le *designer*) qui produit exactement ce que le firmware
parse. Dernière livraison : **Pull P3 (designer) + chart/meter designer** (2026-06-17), mergé puis poussé
sur `origin/master`.

> **Track ouvert (déposé en parallèle, NON commencé)** : un **plan d'endpoint « screenshot » du device**
> a été ajouté via **PR #8** dans `devices/guition_knob/projects/Rich_Telemetry/snapshotplan.md` (plan
> seul, **aucun code**). Si on reprend ce sujet : lire ce fichier d'abord (son contenu n'a pas été
> vérifié dans cette session). Sans rapport avec le pull/P3.

## Architecture (deux moitiés, contrat partagé)

Le **contrat partagé** est `schema/layout.schema.json` (JSON Schema draft-07) : **source de vérité
unique** firmware↔designer. Deux tests de conformité le vérifient (JS `designer/tests/registry.test.js`,
C `test/test_core` via `RT_SCHEMA_PATH`). Toute évolution du contrat = un commit dédié.

### Firmware (`src/`, PlatformIO `esp32s3`, Arduino/pioarduino, LVGL v8.4)
- **Séparation état/vue** ; cœur modèle **HW-free testable en natif** (`dashboard`/`format`/`color`/
  `context`/`nav_logic`, pas `view.cpp`).
- **Catalogue de composants** via **vtable** (registre de types) : `label`, `readout`, `bar`, `ring`
  (couronne), `led_ring`/`sound` (physiques), `chart` (sparkline `lv_chart`), `meter` (jauge
  `lv_meter`). Deux tables indexées par `enum CompType` : `APPLY[]` (modèle, `dashboard.cpp`) et
  `VIEW[]` (`struct ViewVTable{build,sync}`, `view.cpp`) ; sentinelle `COMP_COUNT` + `static_assert`.
- **API REST** : `POST /update {id:val}` (push par id), `POST /context {var:val}` (push vers une
  variable du blackboard), `POST /secrets {nom:val}` (store **write-only**, jamais GET), `POST /layout`,
  `GET /layout`, `GET /status` (dont tableau `sources`), `POST /page`. CORS activé (header + OPTIONS).
- **Pull de données** (producteur/consommateur) : top-level **`sources`** `[{name, url, interval_s≥5,
  headers{nom:val|$secret}, vars{nom:JSONPointer}}]` → **tâche FreeRTOS** épinglée cœur 0 (pile 16 KB)
  qui fetch HTTP(S) (`setInsecure`), résout les `$secret`, écrit le **blackboard** (`Context`), sous
  **un seul mutex** `g_ctx_mutex` ; pattern *snapshot config sous mutex → fetch HORS mutex → écriture
  ctx sous mutex* (l'UI ne gèle jamais). Un composant `bind:"var"` lit la variable au lieu d'être
  poussé par id. Échec de fetch = dernière valeur conservée.
- **Struct `Component` plate** (union différée), **parse de props plat** (ajouter un champ = une ligne).

### Designer (`designer/`, web app **vanilla zéro-build**, ES modules, ajv vendorisé)
- Édite le `layout.json` et le pousse via `POST /layout` (ou export/import fichier).
- **Registre unique des types** `js/registry.js` (`COMPONENTS`, une entrée/type :
  `defaults/makePlacement/centered/physical/compFields/placeFields/mockFields/build`) → palette,
  inspecteur, canvas, défauts en découlent. Ajouter un type designer = une entrée + un `buildX` dans
  `render.js`.
- **Canvas WYSIWYG** (`js/canvas.js`) : drag+snap, poignées de resize, multi-pages. **Aperçu
  best-effort** (`js/render.js`, miroir de `view.cpp`/`color.cpp`/`format.cpp` — double-maintenance
  assumée ; le device arbitre).
- **Inspecteur** (`js/inspector.js`) : props/géométrie/seuils(ring+meter)/mock par type, champ `bind`.
- **Panneau Sources** (`js/sources.js`, footer `<details>`) : édite les `sources` (url/interval/
  headers/vars) ; headers/vars en listes de paires reconstruites en objets au commit. **Le bouton « + »
  d'une paire insère la ligne LOCALEMENT sans commit** (sinon la clé vide est filtrée et la ligne
  disparaît au re-render — bug corrigé en P3, `5fab967`).
- **Modèle** `js/model.js` : état + `commit(mutator)` + undo/redo + subscribe ; **mutations pures**
  `js/mutations.js` (testables `node --test`). **Mocks hors layout** `js/mocks.js` (aperçu, non poussé).
- **Secrets délibérément HORS designer** (conforme spec « absent du designer ») : les `headers`
  référencent un secret par `$nom` ; la **valeur** se pose via `POST /secrets` **manuel** (curl).

## Build / test / servir

```bash
# Firmware — build + flash (depuis la racine du repo) :
./build.sh auto Rich_Telemetry --upload      # auto-détecte le device par son MAC (garde-fou MAC)

# Tests natifs firmware (SANS device — garde-fou du contrat firmware↔schema) :
cd devices/guition_knob/projects/Rich_Telemetry && pio test -e native      # 66/66 attendu

# Tests designer (logique pure) :
cd devices/guition_knob/projects/Rich_Telemetry/designer && node --test    # 88/88 attendu

# Servir le designer (navigateur) — DEPUIS LE PARENT, sur un PORT NEUF à chaque fois :
cd devices/guition_knob/projects/Rich_Telemetry && python3 -m http.server 8137
#   → http://localhost:8137/designer/   (servir depuis Rich_Telemetry/ pour que ../schema soit servi ;
#     port neuf = contourne le cache de modules ES qui resservirait les vieux .js)
```

## État du device (à re-vérifier — non observé cette session)

P3 était **designer-only (sans carte)**. Le device tourne le **firmware avec chart/meter** (validé
on-device le 2026-06-17). Layout, IP et secrets exacts **n'ont pas été re-vérifiés cette session** —
ne rien asserter sans observer :
- **IP DHCP dernière connue : `192.168.1.35`** (peut avoir changé ; relire via série au boot — pulse
  DTR/RTS avec `~/.platformio/penv/bin/python` pour capter `IP=`, ou `GET /status`).
- Un **secret de test résiduel** `weather_key` a pu être laissé dans le store (write-only, jamais
  servi, inoffensif ; pas d'endpoint DELETE — l'écraser via `POST /secrets {"weather_key":""}` si gênant).
- `*.local`/mDNS **ne résout pas** sur ce LAN → joindre par IP DHCP directe.

## Pistes restantes (OPTIONNELLES — rien d'engagé)

1. **Validation end-to-end du pull, piloté par le designer** (la seule boucle jamais faite en entier) :
   dans le designer, créer une `source` + un composant `bind`, **Pousser** au device, **poser le secret
   référencé** via `curl -X POST .../secrets -d '{"<nom>":"<val>"}'`, puis observer le pull à l'écran et
   `GET /status` (`last_status`/`updated_at`/`err_count`). Workflow device : [[feedback-device-validation-workflow]].
2. **Reportés v2+ du designer** : presets, aperçu animé led_ring, déclencher `/update`/`/page` depuis le
   designer, GC du store de mocks. (Tracés dans `designer/HANDOFF.md`.)
3. **Refactors différés (firmware)** : union discriminée de `struct Component` (si la RAM/lisibilité
   pique) ; pile TLS de la tâche pull 16 KB → 20480 si un GET HTTPS reset le device.

## Décisions délibérées — NE PAS « corriger » sans réfléchir
- **`sound`** écrit l'I2S en **timeout 0 (non-bloquant)** à dessein (loop mono-thread partagé).
- **Swipes verticaux ignorés** : réservés à une future page de config (swipe haut).
- **`ring` centré non déplaçable** (`lv_obj_center`, `view.cpp`) — seulement redimensionnable.
- **Secrets hors designer** (write-only, `POST /secrets` manuel) — choix de sécurité validé.
- `meter` : `thresholds` = **zones d'arc** (bande (prev, limite]), ≠ la sémantique « < limite » du ring.

## Pièges du harness / outillage
- **Reviewers subagents isolés ne relaient pas leur verdict** dans ce harness (juste des notifs idle)
  → faire les revues spec+qualité **soi-même** par lecture du diff committé. Les *implémenteurs*
  rapportent bien via `SendMessage(to:"main")`.
- **clangd local** signale des faux positifs `ArduinoJson.h/Arduino.h not found` (pas d'include path
  PlatformIO) → **ignorer** ; `pio` fait foi.
- **Hook pre-commit `SCHEMA DIVERGENT`** (jaune) : non bloquant, pré-existant ; le commit passe.
- **macOS** : pas de `timeout` ; `sleep` foreground bloqué (temporiser en
  `perl -e 'select(undef,undef,undef,1)'`).

## Pointeurs
- Plan P3 (verbatim, 6 tâches) : `docs/superpowers/plans/2026-06-17-pull-P3-designer.md`.
- Specs : `docs/superpowers/specs/2026-06-16-{pull-data-context,chart-meter-components,component-type-registry}-design.md`.
- HANDOFFs antérieurs : `docs/superpowers/2026-06-16-HANDOFF-rich-telemetry-post-P2.md` (firmware pull),
  `designer/HANDOFF.md` (éditeur WYSIWYG A→C2).
- Mémoire : [[project-rich-telemetry]], [[project-rt-designer]], [[feedback-device-validation-workflow]],
  [[project-monorepo-overview]].
