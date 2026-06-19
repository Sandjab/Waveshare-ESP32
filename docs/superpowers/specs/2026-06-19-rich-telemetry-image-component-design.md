# Rich_Telemetry — composant `image` (statique, redimensionnable, alpha)

**Date** : 2026-06-19
**Projet** : `devices/guition_knob/projects/Rich_Telemetry`
**Statut** : design validé, à planifier

## Objectif

Ajouter un nouveau type de composant au dashboard config-driven : une **image
statique**, placée sur une page et **redimensionnable en étirement libre W×H**,
avec **canal alpha** (transparence des PNG préservée).

Le navigateur fait tout le décodage/rastérisation (comme l'infra existante
`bg-image.js` du fond de page) ; le device n'affiche que du **RGB565A8** déjà
prêt via un objet `lv_img`.

## Contexte et contraintes

- **Réutilise l'infra image existante** (fond de page) : décodage navigateur,
  conversion RGB565, clé d'asset = hash FNV-1a du contenu, upload multipart vers
  LittleFS, sweep des orphelins. Le composant image en est une généralisation
  *placée* et *à taille variable*.
- **LVGL 8.3/8.4** : `lv_img_set_zoom()` n'applique qu'un facteur **uniforme**
  (le scale indépendant x/y n'existe qu'en LVGL v9). ⇒ l'étirement libre W×H
  **ne peut pas** se faire sur le device ; l'image doit être **pré-rastérisée à
  W×H exacts dans le navigateur**.
- **`LV_COLOR_16_SWAP=1`** (cf. `src/lv_conf.h`) : les 2 octets couleur sont
  rangés octet-fort-en-premier (`SWAP=true`, identique au pipeline bgimage).

## Décisions de design

### Étirement libre W×H (re-rendu navigateur)

Au resize via les poignées du canvas, le navigateur **re-convertit la source
aux pixels exacts W×H** et **ré-uploade** (nouvel asset, nouvelle clé). Le
firmware reste trivial : il blit un `lv_img_dsc_t` déjà à la bonne taille, aucun
scaling à l'exécution. Conséquence : il faut conserver la **source originale**
côté navigateur pour pouvoir re-rendre (cf. cache designer).

**Rétention de la source / resize après reload** : tant que la session designer
est vivante, le cache conserve le `ImageBitmap`/blob original → re-rendu net à
chaque resize. Après un **reload de page**, le cache est ré-hydraté depuis le
device (comme `bg-image.js`), donc depuis le **RGB565A8 déjà étiré** : un resize
post-reload re-rend à partir de ce bitmap (dégradation acceptable, non
inversible). Pour retrouver la pleine qualité, l'utilisateur re-sélectionne le
fichier. Choix v1 délibéré : pas de persistance de l'original (évite le gonflement
localStorage / le gaspillage de flash device).

### Alpha supporté — RGB565A8 (`LV_IMG_CF_TRUE_COLOR_ALPHA`)

La transparence est conservée. Layout mémoire (à confirmer en phase plan, cf.
infra) : **3 octets/pixel interleaved** = 2 octets couleur RGB565 (swappés,
octet-fort d'abord) + 1 octet alpha. `len == w*h*3`.

### La taille vit sur le COMPOSANT (entorse assumée à la convention)

Pour bar/chart/meter, `width`/`height` vivent dans le **placement**. Pour
l'image en étirement libre, `src` + `w` + `h` vivent sur le **composant** ; le
placement ne porte que la **position** (`anchor`/`dx`/`dy`).

**Justification** : pour un raster pré-étiré, les octets, la largeur et la
hauteur forment **un fait indivisible** (`len == w*h*3`). LVGL 8 ne sait pas
étirer un `lv_img` de façon non-uniforme ; si une page demandait une taille que
l'asset n'a pas, ce serait irréalisable. Garder le triplet `(src,w,h)` ensemble
rend l'invariant **local** et le firmware robuste.

**Conséquence assumée** : une même image placée sur plusieurs pages s'affiche à
**une seule taille** partout (ex. un logo identique dans le coin de chaque page).
Comportement raisonnable.

## Architecture / flux de données

1. **Choix du fichier** (inspecteur, nouveau champ `image`) → `createImageBitmap`
   décode tout format → dessin à **W×H étiré** (pas de cover-crop, contrairement
   au fond plein écran) → `getImageData` RGBA → conversion **RGB565A8** → hash
   FNV-1a = `key` → mise en cache (source + bytes + dataURL d'aperçu).
2. **Resize** (poignées) → écrit `component.w`/`component.h` → re-rastérise à la
   nouvelle taille (débounce sur relâchement) → nouvelle `key` → ré-upload →
   sweep de l'ancien asset orphelin.
3. **Upload** : `POST /image?key=<hex>` → stream multipart vers LittleFS
   `/img/<key>.565a`. Validation à l'upload : `written ≤ IMG_MAX_BYTES` **et**
   `written % 3 == 0`. Validation forte au parse du layout : `len == w*h*3`.
   `GET /image?key=<hex>` réhydrate les aperçus designer après reload.
4. **Rendu device** (`view.cpp`) : charge `/img/<src>.565a` en PSRAM → monte
   `lv_img_dsc_t {header.cf = LV_IMG_CF_TRUE_COLOR_ALPHA, header.w=w, header.h=h,
   data}` → `lv_img_create` + `lv_img_set_src` → position via `anchor/dx/dy`.
   Buffer PSRAM libéré au rebuild du layout (même cycle de vie que `s_bg_buf`).

## Composants / fichiers touchés

### Designer (`designer/`, stagé vers `data/designer/`)
- `js/registry.js` — type `image` (palette, défauts `{type:'image'}`, `compFields`
  avec le picker, `placeFields` = `anchor/dx/dy`, `build → buildImage`).
- `js/render.js` — `buildImage(comp, pl)` : aperçu depuis le dataURL caché, à W×H.
- `js/image-asset.js` *(nouveau)* — miroir de `bg-image.js` + canal alpha :
  `imageFileToImage(file, w, h)`, conversion `rgba8888ToRgb565a8`, reconstruction
  d'aperçu, cache (clé → source + bytes + url), `referencedKeys(state)` sur les
  composants de type `image`.
- `js/inspector.js` — nouveau type de champ `image` (file picker / remplacement).
- `js/canvas.js`, `js/geometry.js`, `js/mutations.js` — resize des poignées →
  écrit `component.w/h` (cas spécial image) + déclenche re-render + upload.
- `js/file-io.js`, `js/app.js` — upload des assets image à la sauvegarde /
  changement ; réhydratation des aperçus via `GET /image`.
- `schema/layout.schema.json` *(et copie `data/schema/`)* — `comp_image` $def
  (`type`/`src`/`w`/`h`) + ajout au `oneOf` de `component`.
- `tests/` — `registry.test.js` (parité), `schema.test.js`, nouveau
  `image-asset.test.js` (conversion RGB565A8 + hash).

### Firmware (`src/`)
- `dashboard.h` — `COMP_IMAGE` dans l'enum `CompType` ; champs `src[ID_LEN]`,
  `uint16_t w, h` sur la struct du composant.
- `dashboard.cpp` — entrée `{ "image", COMP_IMAGE }` dans `COMP_NAMES` ; parse
  `src`/`w`/`h` ; ligne vtable `APPLY[COMP_IMAGE] = nullptr` (image non poussée
  par `/update` en v1, mais la ligne est requise par le `static_assert`).
- `view.cpp` — case `COMP_IMAGE` dans le builder : chargement PSRAM + `lv_img`.
  Tableau `s_img_buf[MAX_PAGES][MAX_PLACEMENTS_PER_PAGE]` + libération au rebuild.
- `config.h` — `IMG_DIR "/img"`, `IMG_MAX_BYTES` (cap = 360×360×3 = 388800 ;
  *voir note ci-dessous*).
- `api.cpp` — handlers `/image` POST (upload) + GET (fetch) ; extension du sweep
  des orphelins (`api.cpp` ~ ligne 111) pour collecter aussi les `src` des
  composants de type image.

### Doc
- `docs/index.html` — section du manuel pour le composant image.

## À vérifier en phase plan

- **Layout exact RGB565A8 en LVGL 8.3/8.4** : confirmer via context7
  (`/websites/lvgl_io_8_4`) que `LV_IMG_CF_TRUE_COLOR_ALPHA` à `LV_COLOR_DEPTH=16`
  = 3 octets/px interleaved (2 couleur swappés + 1 alpha) et que `lv_img` le
  blend nativement. Ajuster le pipeline navigateur + le `dsc` firmware en
  conséquence si le layout diffère.
- **Cap PSRAM** : `IMG_MAX_BYTES` = 360×360×3 = 388 800 o max/image. Plusieurs
  images/page ⇒ documenter le plafond global (PSRAM 8 MB ⇒ marge large pour une
  poignée d'images, mais borner explicitement).
- **`MAX_PLACEMENTS_PER_PAGE`** couvre les images comme tout autre widget.

## Hors scope v1 (YAGNI)

- Pas de `/update` pour changer l'image à chaud.
- Pas de rotation, pas de `bind`, pas de verrouillage de ratio.
- L'étirement libre (déformation possible) est le choix retenu.

## Critères de succès

- Le designer permet d'ajouter une image, de choisir un fichier (PNG avec
  transparence inclus), de la redimensionner librement ; l'aperçu reflète le
  rendu device.
- Le layout sérialisé valide contre le schéma (`node --test` au vert).
- Le firmware compile (`pio` au vert) et affiche l'image placée, à la bonne
  taille, avec la transparence respectée sur le fond de page.
- Validation on-device : une image PNG à fond transparent posée sur une page
  colorée s'affiche sans rectangle opaque autour (capture via `GET /screenshot`).
