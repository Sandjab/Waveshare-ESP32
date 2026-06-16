# Rich_Telemetry — mécanisme d'ajout de types de composants

Design validé le 2026-06-16.

## But

Ajouter un nouveau type de composant (au-delà des 6 actuels : `label`, `readout`,
`bar`, `ring`, `led_ring`, `sound`) demande aujourd'hui ~10 éditions cohérentes
réparties sur 8 fichiers et 3 langages (schema JSON, C firmware, JS designer),
**sans registre central** : la liste des types est recopiée en dur partout, et
en oublier une ne casse rien bruyamment (type inconnu rejeté côté firmware, ou
retombe sur `buildLabel` côté designer — `canvas.js:29`).

On veut réduire ça à **« écrire le rendu du type, et rien d'autre »**, le reste
étant ou bien dérivé d'un registre, ou bien vérifié par un test qui échoue fort
si une pièce manque.

Cadre fixé en amont (décisions de discussion) :

- **Compile-time**, pas runtime. Reflasher le firmware pour un nouveau type est
  acceptable. On n'introduit pas de moteur de widgets embarqué piloté par le JSON.
- **Zéro outillage de build.** Le designer est « vanilla zéro-build » (décision
  verrouillée) ; pas de codegen. La cohérence est garantie par des tests, pas par
  une génération.
- **Code de rendu incompressible.** Le rendu LVGL réel (firmware) et l'aperçu
  best-effort (designer) sont, par nature, du code spécifique par type. Le
  mécanisme ne les supprime pas ; il supprime le **boilerplate de plomberie**
  autour (enum, dispatch, défauts, listes de champs, entrées de palette).

## État de départ — les ~10 sites de duplication

| Couche | Fichier | Par type, aujourd'hui |
|---|---|---|
| Contrat | `schema/layout.schema.json` | un `$defs/comp_X` + une entrée dans `component.oneOf` (l.68) |
| Firmware — modèle | `src/dashboard.h` | une valeur d'`enum CompType` (l.6) + ses champs dans `struct Component` (l.13) |
| Firmware — résolution du type | `src/dashboard.cpp` | une ligne `strcmp` dans `parse_type()` (l.13) |
| Firmware — update valeur (`/update`) | `src/dashboard.cpp` | une branche du `switch` de `apply_one()` (l.109) |
| Firmware — rendu | `src/view.cpp` | **deux** `switch(c.type)` : création des objets LVGL (l.153) + mise à jour des valeurs (l.234) |
| Firmware — physique | `src/led_ring_comp.cpp` / `src/sound_comp.cpp` | un tick dédié (composants matériels uniquement) |
| Designer — palette | `designer/js/palette.js` | une entrée `TYPES` (l.8) + un cas dans `makePlacement()` (l.15) |
| Designer — défauts | `designer/js/mutations.js` | une entrée `DEFAULTS` (l.6) |
| Designer — aperçu | `designer/js/render.js` | une fonction `buildX()` + une entrée `MOCKS` |
| Designer — dispatch | `designer/js/canvas.js` | une branche de `buildNode()` (l.25) + un cas de `position()` si centré (l.32) |
| Designer — inspecteur | `designer/js/inspector.js` | une entrée dans `COMP_FIELDS`, `PLACE_FIELDS`, `MOCK_FIELDS` |

> Note — le parse des **props statiques** du layout (`dashboard.cpp:50-79`) et des
> placements (l.86-98) est **plat, sans `switch` par type** : il lit tous les
> champs inconditionnellement dans la struct plate. Ce n'est donc **pas** un site
> de duplication — ajouter un champ = une ligne. Ce couplage (struct plate ⟺
> parse plat) est cohérent et conservé.

## Principe directeur

**Le schema (`oneOf` / `comp_*`) est la liste canonique de « quels types
existent ».** Il est déjà le contrat partagé firmware↔designer. Chaque côté
garde un **registre de comportement** local (le code incompressible + ses
métadonnées), et **deux tests de conformité** vérifient que chaque côté couvre
*exactement* l'ensemble de types déclaré par le schema. Oublier une pièce ⇒ test
rouge.

## Décisions

| Sujet | Choix | Pourquoi |
|---|---|---|
| Extensibilité | Compile-time | Reflash = non-coût sur un device perso ; le runtime imposerait un mini-moteur de primitives, hors budget |
| Outillage | Zéro codegen ; registres par côté + tests de conformité | Respecte le « vanilla zéro-build » ; le rendu étant incompressible, un codegen n'éliminerait qu'un boilerplate déjà absorbé par les registres |
| Source de vérité du jeu de types | Le **schema** (`oneOf`/`comp_*`) | Déjà le contrat partagé ; les deux côtés se testent contre lui |
| `struct Component` plate | **Conservée** (union différée) | Localisée à `dashboard.h` (édition d'un seul endroit), donc hors du problème « 10 sites » ; RAM négligeable ; struct plate ⟺ parse statique plat (paire cohérente) ; le design registry/vtable marche à l'identique plate ou union |
| Phasage | **Designer d'abord**, firmware ensuite | La Phase 1 est autonome, à plus forte valeur ergonomique, et sans risque firmware |

Alternative écartée — **descripteur unique + codegen** (un manifeste d'où l'on
génère schema + JS + squelette C) : élégant, mais casse le zéro-build et ajoute
un outil à maintenir, pour un gain marginal puisque le rendu reste écrit à la
main des deux côtés.

Alternative écartée — **union discriminée de `struct Component` maintenant** :
vrai gain de propriété/RAM, mais touche *chaque accès de champ* dans
`dashboard.cpp` et `view.cpp` (gros diff risqué) ; orthogonal au but ; faisable
plus tard en refactor séparé si la RAM ou la lisibilité pique.

## Phase 1 — Designer : un registre unique

Un module `designer/js/registry.js` exporte un objet, **une entrée par type**,
qui absorbe tout ce qui est aujourd'hui éparpillé :

```js
export const COMPONENTS = {
  ring: {
    label: 'Anneau',                    // palette (remplace TYPES de palette.js)
    defaults: () => ({ type:'ring', color:'#38BDF8', pill:true, min:0, max:100 }),
    makePlacement: (id) => ({ ref:id, radius:80, thickness:16, gap_deg:70 }),
    centered: true,                     // remplace le special-case de canvas.js position()
    physical: false,                    // true ⇒ badge hors canvas (led_ring/sound)
    compFields: [...],                  // remplace COMP_FIELDS[type]  (inspector.js)
    placeFields: [...],                 // remplace PLACE_FIELDS[type]
    mockFields: [...], mock: {...},     // remplace MOCK_FIELDS[type] + MOCKS[type]
    build: buildRing,                   // l'aperçu — LE seul vrai code (reste dans render.js)
  },
  // label, readout, bar, led_ring, sound …
};
```

Les consommateurs passent du « table locale par type » au « lecture du registre » :

- `palette.js` : `TYPES` → `Object.entries(COMPONENTS).map(([t,d]) => [t, d.label])` ;
  `makePlacement()` → `COMPONENTS[type].makePlacement(...)`.
- `mutations.js` : `DEFAULTS[type]()` → `COMPONENTS[type].defaults()`.
- `inspector.js` : `COMP_FIELDS`/`PLACE_FIELDS`/`MOCK_FIELDS` → champs du registre.
- `canvas.js` : `buildNode()` → `COMPONENTS[comp.type].build(...)` (avec un repli
  défini, pas un `buildLabel` silencieux) ; `position()` → branche `centered`.
- `render.js` : garde les `buildX()` (purs, testés), désormais référencés par le
  registre.

Les `buildX()` restent dans `render.js` (mutation minimale, tests `render.test.js`
inchangés) ; le registre les **référence**, il ne les déplace pas.

**Ajouter un type designer** = une entrée dans `registry.js`, dont le seul code
réel est `build()`.

## Phase 2 — Firmware : table de noms + vtable

Le parse des props statiques **reste plat** (cohérent avec la struct plate :
ajouter un champ = une ligne, l.57-70). On consolide seulement les **3 dispatch
par type**, l'`enum CompType` restant l'index :

1. **`parse_type` → table** `{ "ring", COMP_RING }` parcourue en boucle (tue la
   chaîne de `strcmp`).
2. **Les `switch(c.type)` de `view.cpp` (×2, build + sync) + le `switch` de
   `apply_one` (`/update`) → une vtable** indexée par l'enum :
   ```c
   struct CompVTable {
     const char* name;                                        // ↔ remplace parse_type
     void (*apply_value)(Component&, JsonVariantConst);        // ↔ branche d'apply_one (/update)
     lv_obj_t* (*build)(lv_obj_t* parent, const Component&);   // ↔ switch « création » view.cpp
     void (*sync)(lv_obj_t*, const Component&);                // ↔ switch « mise à jour » view.cpp
   };
   ```
   Les types **physiques** (`led_ring`, `sound`) laissent `build/sync = NULL`
   (gérés par leur tick dédié) ; le moteur saute les entrées nulles.

Ce qui **reste à écrire** pour un type : sa valeur d'enum, ses champs (une ligne
dans le parse plat + la struct), et `apply_value/build/sync` (le rendu réel). Le
dispatch découle de la table.

## Tests de conformité (le garde-fou)

- **JS** (`designer/tests/registry.test.js`, `node --test`) : les clés de
  `COMPONENTS` == l'ensemble des `type.const` des `comp_*` du schema (via
  `oneOf`). Échoue si un type est dans le registre mais pas le schema, ou
  l'inverse. Vérifie aussi que chaque entrée a les clés requises (`label`,
  `defaults`, `build`, …).
- **C natif** (`test/test_core`, `pio test -e native`) : on parse
  `schema/layout.schema.json` (ArduinoJson dispo), on extrait les `type.const`,
  et on assert que `parse_type` résout **chacun** et **rejette** un nom inconnu.
  La boucle est bouclée : schema = vérité, les deux côtés testés contre lui.

## Ce que devient « ajouter un type »

1. **Schema** : un `comp_X` + une ligne dans `oneOf`.
2. **Designer** : une entrée dans `registry.js` (+ un `buildX()` dans `render.js`).
3. **Firmware** : une valeur d'enum, ses champs (parse plat + struct), une ligne
   de vtable (+ `applyX/buildX/syncX`).

Oublier un côté ⇒ son test de conformité casse. Le seul code « créatif » restant
est le rendu (un côté C, un côté JS) — exactement l'objectif.

## Vérification

- **Phase 1** : `node --test` (registre + conformité JS) vert ; non-régression
  fonctionnelle de l'éditeur vérifiée navigateur (le câblage DOM n'est pas couvert
  par les tests node). Critère fort : créer/éditer/supprimer chaque type existant
  via la palette/l'inspecteur se comporte comme avant le refactor.
- **Phase 2** : `pio test -e native` (cœur modèle + conformité C) vert ; rendu
  LVGL validé par flash + contrôle visuel sur device (workflow habituel).

## Phasage

Phase 1 (designer) est autonome et livrable seule. On peut s'arrêter après si le
firmware paraît assez peu douloureux tel quel. La struct reste plate dans les
deux phases ; l'union discriminée est un suivi séparé, non planifié ici.
