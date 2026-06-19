# HANDOFF — composant `image_anim` (image animée) — Rich_Telemetry

Date : 2026-06-19. Autoporteur. **Remplace** le handoff précédent (`2026-06-19-HANDOFF-rich-telemetry-image.md`, composant image statique, consommé).

## TL;DR — où on en est

Le composant **`image_anim`** (GIF / série d'images : afficher une frame à volonté **ou** jouer une animation loopable, période réglable) est **CODE-COMPLET et entièrement revu**, mais :

- **Branche `feat/image-anim`** (en place dans le checkout principal), **HEAD = `c5e569f`**, **`master` = `83be850`**.
- **PAS mergée, PAS poussée.** Working tree propre (seul `docs/superpowers/specs/2026-06-18-dialboard-launch-strategy.md` traîne en untracked, sans rapport).
- **Validation on-device = LA SEULE CHOSE QUI RESTE** (firmware vérifié en unit+build, jamais flashé pour cette feature).
- 18 commits, +746/-4, 20 fichiers.

**Reprise = (1) flasher + valider on-device, (2) si OK → merger `feat/image-anim` sur `master` (option « garder la branche » avait été choisie en attendant la validation).**

Spec : `docs/superpowers/specs/2026-06-19-image-anim-component-design.md`
Plan : `docs/superpowers/plans/2026-06-19-image-anim-component.md`
Mémoire projet : entrée `image_anim` dans `project-rich-telemetry` (auto-memory).

## Ce qui a été livré

Exécuté **subagent-driven** (16 tâches dev, revue spec+qualité par tâche, revue finale **READY TO MERGE**). Le composant suit le « registre de types » habituel (toutes les couches synchronisées) :

**Firmware** (`devices/guition_knob/projects/Rich_Telemetry/src/`)
- `config.h` : `AIMG_MAX_W/H=360`, `AIMG_PX_BYTES=3`, `AIMG_MAX_FRAMES=32`, `AIMG_MAX_BYTES=1572864`, `AIMG_DIR="/aimg"`.
- `dashboard.h` : enum `COMP_IMAGE_ANIM` ; champs config `aimg_frames`/`aimg_period`/`aimg_rest`/`aimg_loop`/`aimg_autoplay` ; état lecture `aimg_playing`/`aimg_period_ms`/`aimg_loops_left` (**-1 = ∞**)/`aimg_last_ms` ; **frame courante = champ `value`** (réutilisé) ; clé/dims = `image_src`/`image_w`/`image_h` (**réutilisés**, factorisation autorisée par la spec). Déclaration `dash_tick_aimg`.
- `dashboard.cpp` : `COMP_NAMES["image_anim"]` ; parse `aimg_*` + init repos/autoplay ; `apply_image_anim` (`/update` : `{"frame":K}` / `{"play":true,"loop":L,"period":ms}` / `{"stop":true}`) + entrée `APPLY[]` ; cas `context_apply` (bind = frame d'état au repos, clamp `0..frames-1`, **ignoré pendant un play**) ; **`dash_tick_aimg`** (avance la frame, loop fini → settle sur `rest_frame`, loop 0 = ∞).
- `view.cpp` : `s_aimg_buf`/`s_aimg_dsc` (PSRAM : 1 buffer pack + N `lv_img_dsc_t` par composant) ; `aimg_load_component` (charge `/aimg/<src>.565p`, valide taille == N×w×h×3) ; `build_image_anim`/`sync_image_anim` + entrée `VIEW[]` ; free PSRAM + load au `view_rebuild`.
- `main.cpp` : appel `dash_tick_aimg(&g_dash, now_ms)` dans `loop()` avant `view_sync`.
- `api.cpp` : `POST/GET /aimg` (multipart streamé `.565p`) + sweep des orphelins au `POST /layout` (jumeaux de `/image`).

**Designer** (`.../Rich_Telemetry/designer/` + `schema/`)
- `schema/layout.schema.json` : `comp_image_anim` + ref `oneOf` (la copie servie `data/schema/` est **gitignorée**, régénérée par `tools/stage_fs.sh` au flash).
- `js/image-anim-asset.js` : décode GIF (**`ImageDecoder` WebCodecs**, importe la période moyenne) ou série d'images (triées par nom) → N frames RGB565A8 → **pack** ; cache d'aperçu par frame ; fonctions pures (`packFrames`, `referencedAimgKeys`) testées node.
- `js/registry.js` (entrée `image_anim`) + `js/render.js` (`buildImageAnim`, aperçu = frame de repos).
- `js/device.js` (`uploadAimg`/`fetchAimg`) + `js/app.js` (upload des packs avant `pushLayout`, rehydrate au Load).
- `js/inspector.js` : **éditeur de frames** `imageAnimField` (import GIF/série, bande de vignettes cliquables = choix de la frame de repos surlignée, bouton **« Aperçu »** qui anime le widget sur le canvas). Accès au nœud canvas via `#stage [data-ref="<id>"]` — **1 ligne `node.dataset.ref = pl.ref;`** ajoutée à `js/canvas.js`. `style.css` : classes `.insp-aimg-*`.
- `js/validate.js` : gardes `frames ≤ 32` et pack `≤ 1 572 864` octets.
- `docs/index.html` : section manuel `c-image_anim`.

## Décisions structurantes — NE PAS « corriger » sans réfléchir

- **Pack mono-fichier** (N frames RGB565A8 brutes concaténées), pas N assets séparés (approche A de la spec) → sweep/upload calqués sur `/image` (1 clé/composant). Dims+compte sur le **composant** (fichier = octets bruts), comme l'image statique.
- **Le navigateur décode/convertit, le device n'affiche que du RGB565 prêt** (contrat ferme du projet). `SWAP=true` (car `LV_COLOR_16_SWAP=1`), `LV_IMG_CF_TRUE_COLOR_ALPHA`.
- **bind = frame d'état au repos** (transitoire pendant un play) ; **play repart toujours de la frame 0** ; **stop → retombe sur `rest_frame`** (aligné spec, pas freeze).
- **Moteur maison `dash_tick_aimg`** (jumeau de `dash_tick_countdown`), PAS `lv_animimg` (son modèle se bat avec frame-à-volonté + bind + stop ; éviterait un 2ᵉ moteur d'anim). Le tick n'avance que la frame logique + `dirty` ; `view_sync` peint.

## Preuves (arbre final, cette session)

- `pio test -e native -f test_core` : **91/91 PASSED**.
- `node --test` (dans `designer/`) : **169/169 PASSED**.
- `pio run -e esp32s3` (build complet) : **SUCCESS**, RAM 47,3 % / Flash 25,3 %.

## ⚠️ Validation on-device — À FAIRE (la seule étape restante)

Le device (Guition K718) est branché au Mac de l'utilisateur, joignable WiFi par IP directe **`192.168.1.35`** (les `.local` ne résolvent pas sur ce LAN). L'utilisateur flashe ; l'agent fournit les commandes ; visuel capturable via `GET /screenshot` → `sips` PNG → envoi.

```bash
cd /Users/jean-paulgavini/Documents/Dev/Waveshare-ESP32
git checkout feat/image-anim                          # si pas déjà dessus
tools/stage_fs.sh guition_knob Rich_Telemetry          # régénère data/designer + data/schema (dont image_anim)
./build.sh guition_knob Rich_Telemetry --upload        # flash firmware
./build.sh guition_knob Rich_Telemetry --uploadfs      # flash LittleFS (designer + schéma embarqués)
```

Puis dans le designer embarqué **`http://192.168.1.35/designer/`** : déposer un `image_anim`, importer un GIF (≤ 200 px, ≤ 20 frames), régler `period`, choisir la frame de repos, « Pousser ».

```bash
IP=192.168.1.35; ID=sp     # ID = l'id réel du composant
curl -s "http://$IP/layout" | python3 -m json.tool | grep -A8 image_anim          # champs présents
curl -s "http://$IP/screenshot" -o /tmp/a.bmp && sips -s format png /tmp/a.bmp --out /tmp/a.png   # frame de repos
curl -s -X POST "http://$IP/update" -d "{\"$ID\":{\"frame\":3}}"                                    # frame fixe
curl -s -X POST "http://$IP/update" -d "{\"$ID\":{\"play\":true,\"loop\":0,\"period\":80}}"         # boucle ∞
curl -s -X POST "http://$IP/update" -d "{\"$ID\":{\"stop\":true}}"                                  # stop → repos
curl -s -X POST "http://$IP/context" -d "{\"st\":1}"                                                # si bind=st : frame 1
# Sweep : changer l'image (nouvelle clé), re-Pousser, puis :
curl -s "http://$IP/aimg?key=<ancienne_cle>" -o /dev/null -w '%{http_code}\n'                       # attendu 404
```

À confirmer visuellement : frame de repos affichée, `frame:K` fige la bonne frame, `play` anime à la période, `stop`→repos, `bind` sélectionne la frame, `autoplay` (page rechargée), sweep 404.

**Astuces device** (cf. mémoire `feedback-device-validation-workflow`) : pas de `timeout` sur macOS ; backup le layout user avant (`GET /layout` → fichier) et restaure après (`POST /layout`) ; un flash `--upload` seul préserve le LittleFS (designer/schéma restent l'ancienne version → faire `--uploadfs` pour servir le designer avec `image_anim`).

## Connu / différé

- **M2 (Minor, non bloquant)** : l'**autoplay démarre au parse** (`dashboard.cpp`, `dash_set_layout`) sans vérifier que l'asset est chargé. Si on coche `autoplay` ET que le pack `.565p` est absent du LittleFS, `dash_tick_aimg` tourne à vide (avance `value` + `dirty` à chaque `loop()`, churn de `view_sync` — pas de corruption visuelle car `sync_image_anim` sort tôt). **Inoffensif dans le workflow normal** (le designer uploade le pack avant `POST /layout`). Correction propre = déplacer le démarrage autoplay du parse vers `view_rebuild` (`view.cpp`), après un `aimg_load_component` réussi — touche le firmware, donc re-build + re-test. À faire seulement si jugé utile.
- **Hors scope v1 (YAGNI, voir spec)** : timing par frame (période uniforme en v1, le GIF n'importe qu'un défaut), bind-déclencheur-d'anim (le bind sélectionne une frame, ne déclenche pas un play), dédup de frames entre animations.

## Une fois la validation OK

```bash
cd /Users/jean-paulgavini/Documents/Dev/Waveshare-ESP32
git checkout master && git merge feat/image-anim        # fast-forward attendu (master n'a pas bougé depuis 83be850)
git push origin master                                   # master était en avance sur origin AVANT cette feature — vérifier l'état origin
git branch -d feat/image-anim
```
⚠️ Avant de pousser : vérifier le décalage `master` ↔ `origin/master` (la mémoire projet note que `master` avait pris de l'avance sur `origin` lors de features précédentes bg-image/physical — `git log origin/master..master --oneline` pour voir ce qui n'est pas encore poussé). Mettre à jour la mémoire projet (`image_anim` : passer de « branche non mergée » à « mergé/poussé ») et marquer la Task 17 faite.
