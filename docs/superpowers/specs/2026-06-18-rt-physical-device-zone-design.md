# Spec — Rich_Telemetry : zone « Device » pour composants physiques (designer)

**Date : 2026-06-18.** Statut : **validé, prêt pour le plan.** Projet :
`devices/guition_knob/projects/Rich_Telemetry/`.

## Problème

Les composants **physiques** (`led_ring`, `sound`) sont des **sorties globales du device** (un seul
anneau WS2812 ; un haut-parleur). Pourtant le designer ne sait les ajouter qu'en les **glissant sur
une page** (création d'un *placement* dans `pages[].place[]`), et les affiche en badges sur le canvas.
C'est une incohérence d'**autoring** : un effet device-global est présenté comme « attaché à une page ».

Au **runtime, ils sont déjà globaux** : `led_ring_tick` / `sound_tick` (`main.cpp:105-106`) balaient le
tableau **global** `Dashboard.components[]` (`led_ring_comp.cpp:13`, `sound_comp.cpp:44`),
indépendamment de la page affichée et du placement. La table de vue les ignore
(`view.cpp` : `COMP_LED_RING`/`COMP_SOUND` = `{nullptr,nullptr}`). Le placement d'un physique est donc
**inerte** côté firmware.

## Objectif

Éditer les composants physiques **exclusivement** dans une nouvelle zone « Device » du designer (hors
pages), reflétant leur nature globale. **Changement designer-seul.**

## Contraintes / faits vérifiés

- **Firmware & schéma INCHANGÉS.** Un composant peut vivre dans la map `components` **sans** placement :
  `dash_set_layout` parse `components` dans une boucle indépendante des pages ; le schéma n'exige que
  `["components","pages"]` au top-level (un composant n'a pas à être placé). Le firmware pilote déjà les
  physiques globalement.
- **Généralisation par le flag `physical`** (déjà dans `registry.js` : led_ring, sound) — pas de hardcode.
  Tout futur type physique en hérite.
- **Rétro-compatible** : les anciens layouts (physiques placés sur une page) s'ouvrent sans erreur ;
  une migration douce retire leurs placements (le composant reste), sans aucun changement de rendu
  device (la position était déjà ignorée).

## Décisions tranchées (validées)

1. **Édition exclusive dans la zone « Device »** : les types `physical` sont **retirés** de la palette
   de page, de la bibliothèque draggable et du canvas. Ajout / édition / suppression uniquement dans le
   panneau « Device ».
2. **Cardinalité** : **led_ring = singleton** (≤ 1 — le device a un seul anneau ; le firmware pilote le
   premier trouvé). **sound = 0..N** (plusieurs émetteurs nommés, déclenchés par id via `/update`).
   Implémenté par un flag `singleton: true` sur l'entrée registry `led_ring`.
3. **Migration automatique** des placements physiques à l'**entrée** d'un layout (autosave au démarrage,
   « Charger » device, import fichier). **Exclu** : le panneau « JSON avancé » (édition brute = échappatoire
   avancée, laissée telle quelle).

## Modèle de données

Inchangé. Les composants physiques restent dans la map globale `components`, **sans** entrée dans
`pages[].place[]`. Aucune nouvelle clé de schéma.

## Architecture designer

**Nouveau module `js/physical.js`** (pur, testé node ; importe `COMPONENTS` de `registry.js` et
`uniqueId`/`addComponent` de `mutations.js` — chaîne déjà node-safe, cf. `registry.test.js`) :
- `isPhysicalType(type)` → `!!COMPONENTS[type]?.physical`
- `physicalTypes()` → liste des types `physical` du registre
- `physicalComponentIds(state)` → ids de `state.components` dont le type est physique
- `addPhysicalComponent(state, type)` → `addComponent(state, uniqueId(state,type), COMPONENTS[type].defaults())`
  **sans** placement
- `removeComponent(state, id)` → supprime `state.components[id]` **et** retire tout placement le
  référençant sur toutes les pages
- `stripPhysicalPlacements(state)` → pour chaque page, retire les `place[]` dont le composant référencé
  est physique (composants conservés) ; idempotent
- `canAddType(state, type)` → `false` si `COMPONENTS[type].singleton` et qu'un composant de ce type
  existe déjà ; `true` sinon

**Nouveau module `js/device-panel.js`** (UI, calqué sur `js/sources.js`) : rend, dans `#device`, une
carte par composant physique (titre = id, champs de config issus du registre `compFields`, bouton
Supprimer) + un bouton d'ajout par type physique (désactivé si `!canAddType`). Édition des champs via
`setComponentProp` (déjà existant). Réutilise les classes CSS `sources-panel` / `src-*` (zéro nouveau CSS).

**Modifications :**
- `js/registry.js` : ajouter `singleton: true` à l'entrée `led_ring`.
- `js/palette.js` : exclure les types `physical` de la liste des créateurs **et** de la bibliothèque
  draggable ; le handler de drop ignore un type/ref physique (défense).
- `js/canvas.js` : la branche `if (def.physical)` (l. 105) ne rend plus de badge → `return` simple
  (les physiques migrés n'apparaissent plus dans `placements()` ; défense pour un layout brut non migré).
- `js/app.js` : instancier `createDevicePanel($('device'), model)` ; appliquer la migration
  (`stripPhysicalPlacements`) au démarrage (sur l'objet `saved` avant `createModel`), à « Charger » (sur
  l'objet device avant `loadJSON`), et après import (commit dans le `onLoad` de `bindFileIO`).
- `index.html` : nouvelle section `<details><summary>Device (sorties physiques)</summary>
  <div id="device" class="sources-panel"></div></details>` dans le `<footer>`, à côté de « Sources ».

## Tests

- **node `--test` — `tests/physical.test.js`** : `addPhysicalComponent` (composant ajouté, **aucun**
  placement, id unique) ; `removeComponent` (purge `components` + retire les placements sur plusieurs
  pages) ; `stripPhysicalPlacements` (retire physiques, **garde** visuels + garde les composants ;
  multi-pages ; idempotent) ; `canAddType` (led_ring singleton : true→false ; sound : toujours true) ;
  `physicalComponentIds`. Pas de régression sur la suite existante.
- **Pas de test firmware** (firmware inchangé).
- **Validation navigateur + on-device** : panneau Device (add/edit/remove led_ring + sound), palette ne
  propose plus de physiques, ouverture d'un ancien layout (placements physiques migrés silencieusement),
  push → device se comporte à l'identique (LED/son toujours pilotés). Capture via `/screenshot`.

## Hors périmètre (YAGNI)

- Renommer l'id d'un composant (limitation existante du designer pour TOUS les composants ; les ids sont
  auto via `uniqueId`). Donc les sounds s'appellent `sound1`, `sound2`… (pas `buzz`) — parité, hors scope.
- Déclencher LED/son depuis le panneau (reste « Valeurs test » / `POST /update`).
- Tout changement firmware ou schéma.
- Migration du panneau « JSON avancé » (échappatoire brute, laissée telle quelle).
- Le sens inverse (comportement LED/son **par page**) — explicitement non voulu.

## Points d'ancrage (fichiers:lignes au 2026-06-18)

- `js/sources.js` — gabarit du panneau (createSources, cards, champs, garde-focus).
- `js/registry.js:86-105` — entrées `led_ring`/`sound` (`physical:true`, `compFields`, `defaults`).
- `js/palette.js:22` (boucle types) / `:44-66` (bibliothèque) / `:77-101` (drop).
- `js/canvas.js:100-108` — boucle de rendu + branche `def.physical`.
- `js/app.js:51-53` (autosave/createModel), `:88` (createSources), `:134-146` (Charger), `:82-85` (bindFileIO).
- `js/mutations.js` — `uniqueId`, `addComponent`, `setComponentProp`, `removePlacement`.
- `index.html:64-79` — `<footer>` (sections `<details>` Sources / JSON avancé).
- Firmware (contexte, NON modifié) : `main.cpp:105-106`, `led_ring_comp.cpp:13`, `sound_comp.cpp:44`,
  `view.cpp` (vtable physiques null).

Voir aussi : [[project-rich-telemetry]], [[project-rt-designer]], [[feedback-device-validation-workflow]].
