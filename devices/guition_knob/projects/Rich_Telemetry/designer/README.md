# Rich_Telemetry — Designer

IHM WYSIWYG **autonome** pour concevoir le `layout.json` de Rich_Telemetry et le pousser au device. Web app statique : aucune dépendance, aucun build, cross-platform (navigateur). **Ne touche pas au firmware.**

> État : **éditeur WYSIWYG multi-pages complet** (Plans A → C2). Palette + bibliothèque de composants réutilisables, canvas drag-and-drop avec snap aux ancrages, inspecteur (props/géométrie/seuils/aperçu mock), onglets de pages (créer/renommer/réordonner/supprimer), export/import `layout.json`, validation live ajv avec messages humanisés, undo/redo. Le panneau *JSON avancé* reste disponible. Le load/push `/layout` vers le device nécessite le CORS firmware (voir plus bas). Détails : `specs/` et `plans/`.

## Lancer

À cause de la politique CORS / `file://`, sers le dossier plutôt que d'ouvrir `index.html` en double-clic. De plus, le designer charge le schéma partagé via `../schema/layout.schema.json` — il faut donc **servir depuis le dossier parent** `Rich_Telemetry/` (pas depuis `designer/`) afin que `schema/` soit accessible sur le même origin :

```bash
# Servir depuis le dossier parent pour que ../schema soit accessible
cd devices/guition_knob/projects/Rich_Telemetry
python3 -m http.server 8000
# puis http://localhost:8000/designer/
```

## Contrat partagé

Le format est défini par **`../schema/layout.schema.json`** — la *source de vérité unique* partagée avec le firmware (`src/dashboard.cpp`). Le designer produit, le firmware consomme. Toute évolution du format = un commit dédié sur le schéma, mergé sur `master`, puis rebase des branches embarqué/designer.

## Endpoints utilisés

> Sans device (ou en attendant le CORS firmware), utilise **Exporter / Importer** dans l'en-tête pour sauvegarder/recharger un `layout.json` en fichier local.

| Action | Requête |
|---|---|
| Charger le layout actif | `GET <device>/layout` |
| Pousser un nouveau layout (validé + persisté flash) | `POST <device>/layout` |
| (à venir) live preview de valeurs | `POST <device>/update` |
| (à venir) navigation | `POST <device>/page` |

mDNS `guition.local` peut être filtré sur certains LAN → utilise l'IP DHCP directe.

## Limite connue à résoudre — CORS

Le firmware (`WebServer` ESP32) ne renvoie pas d'en-têtes CORS. Depuis un autre origin (localhost:8000 → IP device), le navigateur peut bloquer la lecture des réponses. Pistes pour la session C :

1. ajouter `Access-Control-Allow-Origin: *` aux réponses REST côté firmware (touche l'embarqué → à coordonner avec la session embarqué) ;
2. ou un petit proxy local côté designer ;
3. ou, à terme, servir le designer depuis le device.

À trancher au brainstorming de la session C — c'est le seul point qui pourrait recréer un couplage embarqué↔IHM.

## ASCII uniquement

`text`/`label`/`unit` doivent rester ASCII (polices Montserrat embarquées). Le designer devra le signaler (le schéma le contraint déjà via `$defs/ascii`).

## Aperçu : indicatif, pas pixel-exact

Le canvas est une **2e implémentation** du rendu (la 1re étant le firmware, `src/view.cpp` + `src/dashboard.cpp`).
Il vise le « best-effort » : positions et métriques à quelques pixels près, polices approchées. **Le device
reste l'arbitre final.** Conséquences à connaître :

- Les **valeurs affichées sont des mocks** (voir `MOCKS` dans `js/render.js`) ; à l'exécution, `/update` les remplace.
- Le **ring est toujours centré** (le firmware fait `lv_obj_center`) : `anchor`/`dx`/`dy` sont ignorés pour un ring ;
  dans l'éditeur il n'est que redimensionnable (radius / thickness / gap_deg).
- Tout changement de rendu firmware (nouveau widget, nouveau style) **doit être répliqué** dans `js/render.js`.
- Le **positionnement vertical de la pastille `%` et de la légende countdown** du ring est approximatif :
  l'offset diffère légèrement de l'alignement firmware (`lv_obj_align_to`, view.cpp:42-44). Cosmétique.
