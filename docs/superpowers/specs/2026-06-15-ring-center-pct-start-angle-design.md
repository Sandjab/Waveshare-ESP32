# Anneau Rich_Telemetry — activer `center_pct` et `start_angle`

Design validé le 2026-06-15.

## But

Activer deux champs du composant `ring` qui étaient **parsés mais réservés (non
rendus)** depuis la v1 de Rich_Telemetry :

- `center_pct` (bool, sur le composant) → affiche une lecture au **centre** de l'anneau.
- `start_angle` (entier, sur le placement) → **oriente l'ouverture** de l'anneau
  ailleurs qu'en bas.

Le projet est pensé comme un framework générique amené à grossir : on privilégie
la généralité (afficher l'`unit`, pas seulement `%`) et la rétro-compatibilité
totale des layouts existants.

## État de départ

- Rendu de l'anneau : `lv_arc` dans `src/view.cpp` (`build_ring`,
  `ring_place_labels`, branche `COMP_RING` de l'update).
- Ouverture **codée en dur en bas** : `lv_arc_set_bg_angles(arc, 90 + gap/2, 90 - gap/2)`
  (convention LVGL : 0°=droite, 90°=bas, sens horaire).
- Labels existants : `cap` (caption/countdown) dans l'ouverture en bas ; `pill`
  (petit badge `value%`) en haut.
- Parsing déjà en place : `c.center_pct` (`dashboard.cpp:65`), `q.start_angle`
  (`dashboard.cpp:95`). Aucune migration de structure nécessaire.

## Décisions

| Sujet | Choix | Pourquoi |
|---|---|---|
| `start_angle` — convention | Offset en degrés, **sens horaire, depuis le bas**. `0`=bas, `90`=gauche, `180`=haut, `270`=droite | `0` reproduit le comportement v1 → rétro-compatible sans toucher aux layouts existants |
| `start_angle` — labels | Les labels **suivent l'ouverture** : `cap` placé par trigonométrie à l'angle `90+start_angle`, `pill` à l'opposé, centre invariant | Le caption vit conceptuellement *dans* l'ouverture ; le laisser fixe le désaligne dès que `start_angle≠0` |
| `center_pct` — contenu | Label central = **`value` + `unit`** du composant (pas de suffixe si `unit` vide) | Plus générique que `%` ; un anneau peut représenter température, tours/min, etc. |
| `center_pct` — police/couleur | Police = champ `font` du composant (28 conseillé) ; couleur = couleur du composant | Réutilise le réglage existant, pas de nouveau champ |
| `center_pct` vs `pill` | **Exclusifs** ; si les deux à `true`, `center_pct` gagne. Réutilise le slot d'affichage du pill | Les deux montrent la valeur → redondants ; évite d'ajouter de la RAM (slot supplémentaire par placement) |

Alternative écartée : `start_angle` en **angle absolu façon horloge** (0=haut) —
plus intuitif dans l'absolu mais casserait le défaut (les layouts existants
devraient passer `start_angle=180`).

## Sémantique de rendu

### `start_angle`
```
θ_open = 90 + start_angle            (centre de l'ouverture, degrés LVGL)
lv_arc_set_bg_angles(arc, θ_open + gap/2, θ_open - gap/2)
```
Placement des labels sur le rayon intérieur `r = radius − thickness`, convention
LVGL (`dx = r·cos θ`, `dy = r·sin θ`, y vers le bas) :
- `cap` (countdown/caption) : `θ = 90 + start_angle` (dans l'ouverture).
- `pill` (si présent) : `θ = 270 + start_angle` (opposé).
- centre (`center_pct`) : centre géométrique, invariant.

`start_angle = 0` ⇒ `cap` en bas, `pill` en haut : identique à la v1.

### `center_pct`
Label centré réutilisant le helper existant `format_value(value, unit, …)`
(déjà testé) → **valeur, espace, unité** : `72 %`, `61 C`, `1500 rpm` ; `72`
seul si `unit` vide. Cohérent avec les readouts du projet, et évite de réinventer
un format (DRY). Mis à jour dans la branche `COMP_RING` de l'update, au même
titre que l'indicateur d'arc.

## Limite assumée

L'affichage utilise les polices **Montserrat ASCII** (14/20/28). Une `unit`
non-ASCII (`°`, `µ`) ne rendra pas tant que l'item « police étendue » du backlog
n'est pas fait. Unités ASCII recommandées en attendant : `%`, `C`, `F`, `V`,
`W`, `A`, `rpm`, `ms`, `dB`. (Choix délibéré, pas un bug.)

## Contrat partagé & doc

- `schema/layout.schema.json` : retirer les mentions « RÉSERVÉ » de `center_pct`
  et `start_angle`, documenter leur sémantique (source de vérité firmware↔designer).
- `README.md` (section anneau) : documenter les deux champs + la limite ASCII.

## Vérification

Rendu LVGL pur → non testable en natif ; le parsing des deux champs est déjà
couvert par les tests existants. Validation réelle = **flash + contrôle visuel
sur le device** (workflow habituel : build/flash par un contrôleur, contrôle
visuel par l'utilisateur). Cas à observer :

1. `center_pct` avec `unit` ASCII → valeur+unité au centre, gros.
2. `center_pct` + `pill` tous deux à `true` → seul le centre s'affiche.
3. `start_angle` = `0` → aucun changement visuel vs layout actuel (non-régression).
4. `start_angle` = `90`/`180` sur un anneau avec countdown → ouverture *et*
   caption pivotent ensemble, restent alignés.

## Évolutions validées sur device (2026-06-16)

La validation visuelle a fait évoluer `center_pct` au-delà de la spec initiale
(les deux décisions « police = font » et « couleur = couleur du composant » sont
remplacées par) :

- **Couleur** : le chiffre central **suit la couleur du seuil** (comme l'arc) par
  défaut. Un champ optionnel **`center_color`** (hex) la **surcharge**. Implémenté
  via `c.center_color` + flag `c.center_color_set` (`dashboard.cpp`), rendu dans
  `view_sync` (`ccol = center_color_set ? center_color : col`). Sans `thresholds`,
  `col` retombe sur `color` → le centre prend `color`.
- **Taille** : palette de polices étendue à `14/20/28/36/48` (ajout de Montserrat
  36 et 48 dans `lv_conf.h`, `pick_font` étendu), pilotable par `font` — y compris
  pour les `readout`/`label` qui partagent `pick_font`.
- Tests natifs `center_color` ajoutés ; `schema` + `README` mis à jour.
