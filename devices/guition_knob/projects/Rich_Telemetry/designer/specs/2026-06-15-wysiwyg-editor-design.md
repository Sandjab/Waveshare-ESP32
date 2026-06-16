# Rich_Telemetry Designer — éditeur WYSIWYG (design)

**Date** : 2026-06-15
**État** : design validé, prêt pour plan d'implémentation
**Branche** : `feat/rt-designer`

## Contexte

Le squelette du designer (`designer/`) sait déjà charger / valider légèrement / pousser un `layout.json` via un textarea, avec un aperçu de *structure* (marqueurs aux ancrages). Ce design décrit le **vrai éditeur WYSIWYG** qui le remplace, conformément aux `TODO (session C)` de `app.js` et du README.

Le format est figé par le contrat partagé **`../schema/layout.schema.json`** (source de vérité unique, déjà sur `master`). Le designer **produit**, le firmware (`src/dashboard.cpp`) **consomme**. Ce design ne touche pas au format ; le seul changement firmware induit est le CORS (voir § Firmware).

## Objectif

Un éditeur utilisable **par quelqu'un qui ne connaît pas le format JSON** : palette de composants, placement à la souris, rendu fidèle « best-effort », garde-fous. Le JSON brut reste accessible (mode avancé) mais n'est plus le mode d'édition principal.

## Décisions verrouillées

| # | Décision | Justification |
|---|---|---|
| 1 | **Usage** : pour n'importe qui (UX guidée) | Choix produit assumé : ambition la plus exigeante. |
| 2 | **Positionnement** : hybride (drag + snap aux ancrages, ancrage visible/éditable) | Intuitif pour un novice tout en produisant le `ancrage + dx/dy` honnête qu'attend le firmware. |
| 3 | **Aperçu** : best-effort HTML/canvas, **indicatif** | Pixel-exact (LVGL WASM) = sous-projet + couplage build, disproportionné. Le device reste l'arbitre final. |
| 4 | **Disposition** : éditeur 3 colonnes | Familier (Figma/PowerPoint), découvrable. |
| 5 | **Stack** : vanilla, **zéro build**, ajv vendorisé | Conforme au principe du squelette/README ; drag/undo/canvas ne nécessitent aucun framework. |
| 6 | **CORS** : header côté firmware | Seule option qui préserve l'autonomie *et* le zéro-dépendance du designer. C'est une politique d'accès générique, pas un couplage IHM. |

## Architecture fichiers (vanilla, modules ES, zéro build)

L'actuel `app.js` (qui mélange réseau / validation / aperçu) éclate en modules à responsabilité unique, chargés via `<script type="module">` — ça tourne en servant le dossier, sans bundler.

```
designer/
├── index.html              # structure 3 colonnes
├── style.css
├── vendor/ajv.min.js       # validateur vendorisé (1 fichier, pas de build)
└── js/
    ├── app.js              # bootstrap + orchestration (câble modèle ↔ vues)
    ├── schema.js           # charge le schema + métadonnées par type (props éditables, défauts)
    ├── model.js            # état layout + mutations + pile undo/redo + sync JSON
    ├── validate.js         # ajv contre layout.schema.json
    ├── render.js           # rendu best-effort des widgets (label/readout/bar/ring)
    ├── canvas.js           # drag, snap aux ancrages, sélection, poignées de redim
    ├── palette.js          # palette des 6 types + bibliothèque de composants définis
    ├── inspector.js        # panneau propriétés (composant + placement)
    ├── pages.js            # onglets de pages (CRUD + réordonner)
    └── device.js           # load/push REST
```

## Modèle de données

`model.js` est la source de vérité interne. Il porte le layout en mémoire et respecte **telle quelle** la séparation du schéma :

- **`components`** : map `id → définition`, **sans position**. La bibliothèque réutilisable.
- **`pages[].place[]`** : placements `{ref, anchor, dx, dy, …géométrie}`. Un même `id` placé sur plusieurs pages = **un seul exemplaire, état partagé** (pas une copie).

Chaque mutation (créer composant, placer sur page, éditer une prop, réordonner les pages…) :
1. pousse un snapshot sur la **pile undo** ;
2. déclenche une **re-validation** (ajv) ;
3. émet un event → les vues (canvas, inspecteur, palette, JSON avancé) se re-rendent.

Le **JSON avancé** est une vue bidirectionnelle de ce même modèle (éditer le texte → parse → remplace le modèle si valide ; éditer dans l'UI → re-sérialise le texte).

## Modèle de positionnement hybride (le cœur)

Le firmware place chaque widget via l'équivalent de `lv_obj_align(ANCHOR, dx, dy)` : le **point d'ancrage du widget** coïncide avec le **point d'ancrage du parent** (carré 360×360, bords `0 / 180 / 360`), plus l'offset `(dx, dy)`.

- 9 ancrages : `CENTER, TOP_MID, BOTTOM_MID, LEFT_MID, RIGHT_MID, TOP_LEFT, TOP_RIGHT, BOTTOM_LEFT, BOTTOM_RIGHT`.
- `dx = widgetAnchorPoint.x − parentAnchorPoint.x` (idem `y`), arrondis (le schéma exige des entiers).

Pendant le drag (`canvas.js`) :
1. on calcule, pour chaque ancrage, l'offset qu'il produirait ;
2. on retient l'ancrage qui **minimise** `dx² + dy²` ;
3. si `hypot(dx, dy) < SNAP` (≈ 16 px) → **snap** à `dx = dy = 0` (le widget « colle » à l'ancrage, surbrillance) ;
4. l'ancrage retenu et `dx/dy` sont affichés/éditables dans l'inspecteur (override manuel possible).

Conséquence pédagogique assumée : l'écran est **rond**, mais le parent LVGL est **carré** — un ancrage de coin (`TOP_LEFT`…) place le widget hors de la zone ronde visible. Le canvas matérialise le cercle pour le rappeler.

## Rendu best-effort (`render.js`)

Recode en HTML/canvas l'aspect des widgets écran, sans prétendre au pixel-exact :

- **label** : texte (webfont **Montserrat** 14/20/28), couleur.
- **readout** : `label` + valeur d'aperçu + `unit`.
- **bar** : rectangle rempli selon `min/max` + valeur d'aperçu, `label` au-dessus ; géométrie `width/height` du placement.
- **ring** : arc avec **ouverture `gap_deg` en bas**, `thickness`, `radius` ; `pill` de pourcentage ; couleurs de `thresholds` (la couleur s'applique sous la limite) ; `countdown` affiché dans l'ouverture.
- **led_ring** et **sound** : **pas de rendu écran** (physiques) → représentés par un **badge** hors canvas dans la liste des composants de la page.

Les **valeurs affichées** sont des **mocks locaux** éditables dans l'inspecteur (à l'exécution, `/update` les remplace). 

⚠️ **L'aperçu est indicatif** — à documenter dans le README du designer. Il existe une **double-maintenance** : `render.js` est une *2e* implémentation du rendu (la 1re étant `dashboard.cpp`). Tout nouveau widget ou changement de rendu doit être répliqué des deux côtés. On l'assume parce que le device est l'arbitre final.

### Champs réservés — ne pas « corriger »

Le schéma marque `ring.center_pct` et `placement.start_angle` comme **réservés (non rendus en v1)**, choix délibérés. L'inspecteur peut les afficher en lecture/édition mais **étiquetés « réservé »** ; l'éditeur n'invente aucun comportement de rendu pour eux.

### Contrainte ASCII

`text` / `label` / `unit` doivent rester **ASCII** (les Montserrat embarquées ne couvrent pas les accents). Le schéma le contraint (`$defs/ascii`) ; l'inspecteur le signale à la saisie.

## UX par zone (3 colonnes)

- **Palette** (gauche) : les 6 types ; glisser sur le canvas crée un composant. Dessous, la **bibliothèque** des composants définis (glisser un existant sur une autre page = le partager).
- **Canvas** (centre) : écran rond 360×360, rendu best-effort, drag + snap hybride, sélection, poignées de redimensionnement (bar/ring).
- **Inspecteur** (droite) : props du type sélectionné (selon le schéma) + géométrie du placement (ancrage, dx/dy, width/height ou radius/thickness/gap_deg) + valeur d'aperçu mock. Éditeur de `thresholds` du ring (liste `[limite, #couleur]`).
- **Onglets de pages** (haut) : l'ordre = l'ordre de navigation par swipe sur le device. Ajouter / supprimer / renommer / réordonner.
- **Barre device** : Charger / Pousser `/layout` (après CORS firmware) + statut + validation.
- **Pied** : JSON avancé repliable, bouton Valider, état de validation.

## Scope

### v1 — l'éditeur utilisable bout-en-bout

- Drag palette→canvas, snap hybride, sélection / redimensionnement.
- Inspecteur complet (toutes les props des 6 types + géométrie du placement).
- Rendu best-effort des 4 widgets écran + badges `led_ring` / `sound`.
- Éditeur de `thresholds` du ring.
- Pages : ajouter / supprimer / renommer / réordonner.
- Bibliothèque de composants partagés (réutilisation inter-pages).
- Validation ajv live + panneau d'erreurs ; signalement ASCII.
- **Undo / redo** (pile de snapshots).
- JSON avancé bidirectionnel.
- Load / push device (`GET`/`POST /layout`) une fois le CORS firmware en place.
- **Export / import fichier** local (`layout.json`) — filet indépendant du device.

### Reporté (v2+)

- `POST /update` — pousser des valeurs **live** au device depuis l'éditeur.
- `POST /page` — piloter la page affichée sur le device.
- Aperçu **animé** des modes `led_ring` (off/solid/progress/spinner/blink/breathe).
- Presets / templates de dashboards.
- Gestion de plusieurs layouts sauvegardés.
- Aperçu pixel-exact (LVGL WASM) — explicitement écarté.

## Firmware (hors de ce spec, prérequis du lot device)

Unique changement : **CORS** sur le `WebServer` ESP32 — `Access-Control-Allow-Origin: *` sur les réponses REST + handler `OPTIONS` pour le preflight des `POST` JSON. À faire sur la branche embarqué, **commit dédié**. N'est pas une évolution du schéma.

## Découpage d'implémentation

1. Refactor squelette → modules + chargement du schema.
2. `model.js` : état + mutations + undo + sync JSON avancé.
3. `validate.js` : ajv vendorisé contre le schema.
4. `render.js` : rendu best-effort (les 4 widgets + badges).
5. `canvas.js` : drag / snap hybride / sélection / redim.
6. `palette.js` : palette + bibliothèque de composants.
7. `inspector.js` : propriétés + thresholds + valeur mock.
8. `pages.js` : onglets CRUD + réordonner.
9. `device.js` : load / push (après CORS firmware).
10. Export / import fichier + doc « aperçu indicatif » dans le README.

## Risques / points d'attention

- **Double-maintenance du rendu** (`render.js` ↔ `dashboard.cpp`) : risque de dérive, assumé ; le device arbitre.
- **Divergence best-effort** : positions/métriques à quelques px près ; ne pas courir après le pixel.
- **Champs réservés** : ne pas leur inventer de comportement.
- **Écran rond / parent carré** : les ancrages de coin sortent de la zone visible.
- **CORS** : dépendance croisée avec la branche firmware pour le lot 9 (les autres lots n'en dépendent pas — l'export fichier permet de travailler sans device).
