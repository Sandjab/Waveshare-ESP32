# HANDOFF Rich_Telemetry — 2026-06-19 (post composant `image`)

> Autoporteur. **Remplace** `2026-06-19-HANDOFF-rich-telemetry.md` (dont le travail « style du label de la Bar » est livré/poussé). Détail durable : mémoire `[[project-rich-telemetry]]`.

## État du dépôt

- **`origin/master` = `master` = HEAD `83bfe54`, synchronisé.** Rien en attente de push.
- Projet : `devices/guition_knob/projects/Rich_Telemetry`.
- Tests au vert : `cd <projet>/designer && node --test` → **163/163** ; `./build.sh guition_knob Rich_Telemetry` → **SUCCESS** (RAM 46.5 % / Flash 25.2 %).
- Untracked sans rapport (ne pas committer) : `docs/superpowers/specs/2026-06-18-dialboard-launch-strategy.md`.

## Livré cette session : composant `image` (statique, redimensionnable, alpha)

Nouveau type de composant **placé**, **étirement libre W×H** (déformation assumée), **transparence** (RGB565A8 / `LV_IMG_CF_TRUE_COLOR_ALPHA`). Mergé + poussé (12 commits : spec + plan + 10 impl/fix). Exécuté **subagent-driven** (10 tâches, revues spec+qualité + revue finale READY TO MERGE).

- Spec : `docs/superpowers/specs/2026-06-19-rich-telemetry-image-component-design.md`
- Plan : `docs/superpowers/plans/2026-06-19-rich-telemetry-image-component.md`
- **Décision à NE PAS « corriger »** : la **taille vit sur le COMPOSANT** (`src`/`w`/`h`), pas le placement (qui ne porte que `anchor`/`dx`/`dy`) — LVGL 8 ne stretch pas un `lv_img` non-uniformément, donc le navigateur **re-rastérise la source à chaque resize** (nouvelle clé). Conséquence assumée : même image sur N pages = une seule taille.
- Asset `/img/<key>.565a`, endpoint `POST/GET /image?key=`, sweep des orphelins au `POST /layout` (par composant). Réutilise la machinerie bgimage (hash FNV-1a, `SWAP=true`). Module designer frère `designer/js/image-asset.js`.
- **Validé on-device** (`192.168.1.35`) : 3 bandes rouge|vert|transparent sur fond bleu → couleurs OK + tiers transparent laisse voir le bleu (alpha OK, pas de rectangle opaque) + sweep `404` après restauration.

## État physique du device (important)

- Le Guition (`192.168.1.35`, MAC `ac:a7:04:ef:74:28`) tourne le **nouveau firmware** (flashé `--upload` seul) **avec le layout user d'origine restauré** (5 pages / 26 composants — sauvegardé dans `inbox/layout-backup-20260619.json`, gitignored).
- **LittleFS PAS ré-uploadé** cette session (flash firmware seul, pour préserver les données runtime). Donc le **designer embarqué** à `http://192.168.1.35/designer/` et le **schéma** servi sont **l'ancienne version (sans `image`)**. Pour les mettre à jour : `./build.sh auto Rich_Telemetry --uploadfs` (⚠ réinitialise l'état runtime LittleFS → re-« Pousser » le layout user ensuite). En attendant, on peut piloter le device avec un **designer local** (repo) pointé sur l'IP (CORS actif).
- Les **bg images du layout user** (`6f7e4016e31048d2` p0, `dc189d978cc23dc8` p3) restent **absentes du device** (404 → fallback couleur) — à re-« Pousser » depuis le designer si besoin.

## Pistes ouvertes (non tranchées)

- **Page de config accessible par swipe vers le haut** (les swipes verticaux sont réservés à ça depuis le début).
- Mineurs `image` écartés sciemment (entrée mal formée only) : schema `src`/`background_image` typés `ascii` génériques (`bg_key_valid` est la garde) ; sweep capé à 16 ; possible resserrer les deux clés en `^[0-9a-f]{1,16}$` un jour.
- Hors scope v1 image (YAGNI) : pas de `/update` image à chaud, pas de rotation/`bind`/verrouillage de ratio.

## Workflow device (rappel)

Flash : `./build.sh auto <projet> --upload` (identifie par MAC). Validation visuelle par moi (contrôleur) : `curl /screenshot` → `sips` PNG → envoi. Toujours **backup `GET /layout`** avant un test destructif, **restore `POST /layout`** après. Cf. `[[feedback-device-validation-workflow]]`.
