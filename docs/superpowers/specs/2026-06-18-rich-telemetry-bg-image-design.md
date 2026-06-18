# Spec — Rich_Telemetry étape 2 : image de fond par page

**Date : 2026-06-18.** Statut : **validé, prêt pour le plan d'implémentation.**
Suite de l'étape 1 (couleur de fond par page, commit `51fdc84`). Device : Guition JC3636K718,
écran 360×360, projet `devices/guition_knob/projects/Rich_Telemetry/`.

## Objectif

Permettre à chaque page du dashboard d'avoir une **image de fond**, en override optionnel —
même esprit et même mécanique que la couleur de fond par page déjà livrée à l'étape 1.

## Principe directeur

- **Override optionnel par page**, exactement comme `pages[].background` (couleur) : pas de clé
  d'image sur une page → aucune image → on garde la couleur. Les layouts existants restent inchangés.
- **L'image prime sur la couleur.** La couleur reste posée *en dessous* et sert de fallback :
  pendant le chargement, ou si le fichier image est manquant/corrompu.
- **Découplé.** Le layout (JSON) ne porte qu'une **référence** (clé string) ; les ~253 KB d'octets
  RGB565 vivent dans des fichiers LittleFS séparés. Le JSON ne peut pas porter l'image inline.
- **Aucun décodage sur le MCU.** Le navigateur (designer) décode/redimensionne n'importe quelle
  image et envoie un bitmap **RGB565 déjà prêt** ; l'ESP32 ne fait que stocker + afficher.
  `LV_COLOR_DEPTH = 16` → la copie est directe, zéro conversion à l'exécution.

## Décisions tranchées (validées avec l'utilisateur)

1. **Fit = `cover`** : l'image remplit l'écran, recadrée au centre (crop). Pas de letterbox, pas de
   déformation.
2. **Image > couleur** : la couleur de page est le fallback.
3. **Clé d'asset = hash de contenu** des octets RGB565, calculé par le designer. Deux pages avec la
   *même* image partagent donc automatiquement le fichier (dédup). Réutilisable entre pages, robuste
   au réordonnancement drag&drop. L'utilisateur ne tape pas la clé.
   - **Gotcha** : le designer embarqué est servi en **HTTP simple** (`http://<ip>/designer/`) =
     contexte non sécurisé → `crypto.subtle` (SHA-256 natif) **indisponible**. Le hash doit être un
     hash **JS non-crypto** (p. ex. FNV-1a, éventuellement 64 bits) sur les 259 200 octets — risque
     de collision négligeable pour une poignée d'images, et fonctionne en tout contexte.
4. **Ménage des orphelins** : sweep automatique au `POST /layout` — suppression des `/bg/*.565`
   dont la clé n'est plus référencée par aucune page (justifié par les 3,46 MB de LittleFS).

## Contrainte de budget — flash, pas RAM

- LittleFS = partition `spiffs` de **`0x360000` = 3,46 MB**, partagée avec le designer embarqué et
  le schéma (`default_16MB.csv`).
- PSRAM = 8 MB → **non contraignante** (un fond plein écran = 253 KB).
- Une image RGB565 = 360×360×2 = **259 200 octets** non compressés. On tient donc **une poignée de
  fonds** (~10 max, moins le footprint du designer), ce qui est acceptable pour un dashboard.
- Si l'espace devient serré un jour → pivot possible vers décodage JPEG on-device (l'« option 2 »
  écartée au brainstorming). Hors périmètre ici.

## Modèle de données / schéma

- `schema/layout.schema.json` : ajout de `pages[].background_image`, **optionnel**, type `string`
  (la clé d'asset).
- Octets stockés en LittleFS sous `/bg/<clé>.565`, format **RGB565, 360×360, 259 200 octets pile**.
- La clé est le **hash de contenu** (JS non-crypto, cf. décision 3) des octets RGB565 → dédup
  automatique entre pages portant la même image.

## Endpoints firmware (`src/api.cpp`)

- **`POST /bgimage?key=<clé>`** : body = binaire RGB565. Valide `Content-Length == 259200`, sinon
  répond **`400`** (fail loud). Écrit `/bg/<clé>.565` en LittleFS. Si la clé correspond à une page
  actuellement visible → rafraîchit l'affichage.
- **`GET /bgimage?key=<clé>`** : renvoie les octets stockés (permet au designer de ré-afficher
  l'aperçu après un reload). **`404`** si absent.
- **Retirer une image** = enlever la clé de la page dans le layout (puis sweep au `POST /layout`).
  Pas de route DELETE dédiée.

## Rendu firmware

- `src/dashboard.h` : `struct Page` gagne un champ pour la clé d'image (parsé comme `background`
  l'est déjà).
- `src/dashboard.cpp` : parse `pages[].background_image` (chaîne vide / absente → pas d'image).
- `src/view.cpp`, juste après l.380 (le `bg_color`/`bg_opa` de l'étape 1) : pour chaque page avec
  une clé non vide :
  - `heap_caps_malloc(259200, MALLOC_CAP_SPIRAM)`,
  - lire `/bg/<clé>.565` dans le buffer,
  - remplir un `lv_img_dsc_t` (`header.cf = LV_IMG_CF_TRUE_COLOR`, `w = h = 360`,
    `data_size = 259200`, `data = buf`),
  - `lv_obj_set_style_bg_img_src(cont, dsc, 0)` — **pas de scaling** (l'image est déjà à la taille
    du conteneur).
- **Lifecycle** : `view_rebuild` appelle `lv_obj_clean(scr)` à chaque reload → on tient un **registre
  statique** des `lv_img_dsc_t`/buffers PSRAM alloués, **libéré en tête de `view_rebuild`** avant de
  reconstruire (sinon fuite PSRAM à chaque `POST /layout`).
- La couleur de fond (`bg_color`, l.379) reste posée en dessous comme fallback.

## Conversion navigateur (designer)

- File picker → `createImageBitmap(file)` (décodeurs natifs du navigateur : PNG/JPEG/WebP/GIF…).
- Dessin sur un `<canvas>` 360×360 en mode **`cover`** (mise à l'échelle pour remplir + crop centré).
- `getImageData` → boucle de conversion **RGBA8888 → RGB565** (vers un `Uint16Array`).
- `POST /bgimage?key=<clé>` avec le body = `ArrayBuffer` des octets RGB565.
- **Endianness** : l'ordre d'octets RGB565 attendu par LVGL (`LV_COLOR_DEPTH 16` sur ESP32) est à
  **confirmer on-device** ; trivial à inverser dans la boucle si les couleurs sortent fausses.

## UI designer

- Panneau page (`js/inspector.js`, à côté du champ « Fond page » couleur) : « Image de fond » =
  bouton *Choisir une image* + miniature de l'aperçu + bouton *↺ retirer*.
- `js/mutations.js` : `setPageBackgroundImage(state, i, key)` + clear — **purs et testés**,
  symétriques de `setPageBackground` / `effectivePageBg`.
- `js/canvas.js` : dessine l'image (déjà convertie, ou re-décodée pour l'aperçu) comme fond du
  stage ; **prime sur la couleur**.
- **Round-trip** : au chargement, pour chaque page avec une clé, `GET /bgimage?key=` pour l'aperçu.
  Embarqué (`http://<ip>/designer/`) = même origin, direct. Designer local (`http.server`) =
  aperçu indisponible sauf à pointer sur le device (note, pas bloquant).

## Tests

- **node `--test`** (designer) :
  - conversion RGBA→RGB565 sur des valeurs connues (+ vérif endianness),
  - math du crop `cover` (rapport d'aspect → rectangle source),
  - `setPageBackgroundImage` / effective override (hérite vs override), symétrique des tests couleur.
- **`pio test -e native`** (firmware) :
  - parse de la clé depuis le layout (présente / absente / vide),
  - validation de la taille (259 200 → ok ; autre → rejet),
  - lifecycle : libération des buffers au rebuild (pas de fuite).
  - (Le rendu LVGL réel n'est pas testable en `native` → validé on-device.)
- **On-device** : flash, `POST /bgimage`, `GET /screenshot` → PNG → vérif visuelle.

## Points d'ancrage (fichiers:lignes au 2026-06-18)

- `src/view.cpp:373-381` — création du conteneur de page + `bg_color`/`bg_opa` (étape 1) ; l'image
  s'ajoute juste après.
- `src/view.cpp:355-358` — tête de `view_rebuild` (`lv_obj_clean`) → point de libération des buffers.
- `src/api.cpp:194-208` — table des routes (`server.on(...)`, `serveStatic`) → ajout `/bgimage`.
- `src/api.cpp:96` — `persist_save` au `POST /layout` → point du sweep des orphelins.
- `src/dashboard.cpp` (boucle pages, parse de `background`) — ajout du parse de `background_image`.
- `schema/layout.schema.json` — `pages[].background` (modèle de l'override) → ajout `background_image`.
- Designer : `js/mutations.js`, `js/inspector.js` (`renderPagePanel`), `js/canvas.js` — symétrie
  avec les ajouts couleur de l'étape 1.

## Hors périmètre (YAGNI)

- Décodage d'image sur le MCU (JPEG/PNG on-device) — pivot futur seulement si la flash sature.
- Fits `contain` / `stretch` configurables — `cover` en dur pour l'instant.
- Image de fond **globale** (au niveau layout, pas page) — non demandé ; la mécanique par-page suffit.
- Transitions / animations de fond.

## Suivi connexe

- Le **designer embarqué doit être re-stagé** après cette feature
  (`bash tools/stage_fs.sh && pio run -e esp32s3 -t uploadfs` depuis le projet) pour servir l'UI à
  jour ; ⚠ `uploadfs` écrase le layout persisté **et les `/bg/*.565`** → re-`POST` après. Déjà noté
  comme suivi ouvert dans le HANDOFF de l'étape 1.

Voir aussi : `docs/superpowers/2026-06-18-HANDOFF-rich-telemetry-page-bg.md`, [[project-rich-telemetry]],
[[project-rt-designer]], [[feedback-device-validation-workflow]].
