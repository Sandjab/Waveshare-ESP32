# Rich_Telemetry — nouveaux composants `chart` et `meter`

Design validé le 2026-06-16.

## But

Enrichir le catalogue (aujourd'hui `label`/`readout`/`bar`/`ring`/`led_ring`/`sound`)
avec deux widgets d'**affichage** LVGL **v8.4** qui comblent de vrais manques de
télémétrie :

- **`chart`** — graphe d'**historique** (sparkline défilante).
- **`meter`** — jauge analogique à aiguille + zones colorées.

Filtrage assumé : seuls des widgets **d'affichage** (lecture seule) entrent dans
ce projet piloté par REST sans modèle d'input local. Les widgets interactifs
LVGL (button/slider/switch/dropdown/keyboard…) restent **hors scope**.

## Contrainte de version

Le firmware épingle **`lvgl@^8.4.0`** (`platformio.ini`). `chart` et `meter`
sont des widgets **« extra »** → activer `LV_USE_CHART` et `LV_USE_METER` dans
`src/lv_conf.h`. (En v9 `lv_meter` est remplacé par `lv_scale` — ne PAS suivre
les exemples master de lvgl.io ; référence = Context7 `/websites/lvgl_io_8_4`.)

## Séquencement

**Implémentation différée à la Phase 2b** (la vtable firmware). Les ajouter
maintenant imposerait d'éditer à la main les `switch` de `view.cpp` puis de les
re-câbler dans la vtable. Les deux types seront donc les **premiers types ajoutés
*via* la vtable** — ils valident le mécanisme. Ce document est la **note de
design** ; le plan d'implémentation est écrit en 2b, device branché.

## `chart` — historique

| Aspect | Choix | Pourquoi |
|---|---|---|
| Données | `/update` pousse **un scalaire** (comme `bar`) | Cohérent avec le modèle push-stateless ; faible bande passante |
| Détenteur de l'historique | Le **device** (append) | Le dashboard reste stateless ; le device garde un buffer de N points |
| Config composant | `color`, `min`, `max`, `points` (longueur buffer, défaut 30) | Minimal ; mono-série, type LINE (multi-série/BAR différés, YAGNI) |
| Géométrie | Placeable : `anchor` + `dx`/`dy` + `width`/`height` (comme `bar`) | Un graphe n'est pas centré comme le ring |

**Point d'architecture clé — où vit l'historique.** Pas dans la vue. Le
**modèle** (`Component`) porte un **ring buffer** de N `int16_t` ; `apply_value`
y **append** la valeur ; `view_sync` **recopie le buffer dans les `y_points` de
la série + `lv_chart_refresh`**. Raison : `lv_chart_set_next_value` **n'est pas
idempotent** (chaque appel ajoute un point), or `view_sync` peut tourner
plusieurs fois par valeur. En tenant l'historique dans le modèle et en le
*mirroir*ant, on reste cohérent avec la séparation état/vue **et** l'historique
**survit aux rebuilds** (un changement de layout reconstruit les objets LVGL).

Coût : `int16_t hist[CHART_MAX_POINTS]` (+ head/count) dans la struct plate
(~60 o/composant). Négligeable sur l'ESP32-S3 ; cohérent avec « struct plate
conservée » ([[project-rich-telemetry]]). `points` est borné par
`CHART_MAX_POINTS` (constante de compilation).

API v8.4 : `lv_chart_create`, `lv_chart_set_type(…, LV_CHART_TYPE_LINE)`,
`lv_chart_set_point_count(…, points)`, `lv_chart_set_range(…, min, max)`,
`lv_chart_add_series(…, color, LV_CHART_AXIS_PRIMARY_Y)`, écriture des
`ser->y_points[i]` + `lv_chart_refresh`.

## `meter` — jauge analogique

| Aspect | Choix | Pourquoi |
|---|---|---|
| Données | `/update` pousse **un scalaire** → aiguille | Comme `bar`/`ring` ; `view_sync` idempotent (pas d'historique) |
| Config composant | `color` (aiguille), `min`, `max` (échelle), **`thresholds` réutilisés comme zones d'arc colorées** | Réutilise un concept déjà au schema (DRY) ; chaque seuil → un `lv_meter_add_arc` |
| Lecture centrale | **Aucune en v1** (aiguille seule) | YAGNI ; un readout central pourra venir plus tard |
| Géométrie | **Placeable** (`anchor` + taille), pas forcé-centré | Permet un petit meter décalé ou un grand au centre (`anchor:CENTER`) |

API v8.4 : `lv_meter_create`, `lv_meter_add_scale`, `lv_meter_set_scale_ticks`,
`lv_meter_set_scale_range` (min/max/angle), `lv_meter_add_arc` (une par zone de
`thresholds`), `lv_meter_add_needle_line`, `lv_meter_set_indicator_value(…,
needle, value)` dans `view_sync`.

## Contrat partagé & touchpoints

Chaque type ajoute, de bout en bout (le mécanisme des Phases 1/2a/2b le rend peu
coûteux) :

- **Schema** (`schema/layout.schema.json`) : un `$defs/comp_chart` / `comp_meter`
  + une entrée dans `component.oneOf`. (Source de vérité ; les deux tests de
  conformité — JS et C — la vérifient.)
- **Designer** : une entrée dans `js/registry.js` (`defaults`, `makePlacement`,
  `compFields`, `placeFields`, `mockFields`, `build`) + un `buildChart`/
  `buildMeter` d'aperçu **best-effort** dans `render.js` (SVG : sparkline pour le
  chart, arc+aiguille+zones pour le meter).
- **Firmware (en 2b)** : valeur d'`enum CompType`, ligne `COMP_NAMES`, champs de
  struct (ring buffer pour `chart` ; `meter` réutilise `vmin`/`vmax`/`value`/
  `thresholds`), lecture des props dans le parse plat, et la ligne de vtable
  `{ name, apply_value, build, sync }`.

## Vérification

- **Designer** : `node --test` (registre + conformité JS) vert ; non-régression
  navigateur (création/édition/placement des deux nouveaux types ; aperçu
  plausible).
- **Firmware** : `pio test -e native` — le **parse + l'append du ring buffer du
  chart sont native-testables** (ex. pousser 35 valeurs, vérifier que le buffer
  garde les 30 dernières dans le bon ordre). Le **rendu LVGL** (`build`/`sync`
  des deux) est validé **sur le Guition** (flash + contrôle visuel), en 2b.

## Décisions écartées

- **chart en mode replace** (le dashboard pousse un tableau) — donne plus de
  contrôle sur la fenêtre affichée mais rend le dashboard stateful et consomme
  plus de bande passante ; incohérent avec le push-scalaire des autres widgets.
- **meter forcé-centré comme le ring** — inutilement restrictif ; rien dans le
  firmware ne l'impose pour un `lv_meter`.
- **chart multi-séries / type BAR, readout central du meter** — différés (YAGNI),
  ajoutables plus tard sans rupture (champs optionnels).
