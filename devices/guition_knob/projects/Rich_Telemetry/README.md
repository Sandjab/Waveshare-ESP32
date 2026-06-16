# Rich_Telemetry — Guition K718

Dashboard configurable par JSON avec pages multiples, widgets variés (anneaux concentriques, labels, readouts, barres), anneau LED physique et buzzer — navigable à la molette, par swipe latéral ou via REST. La mise en page est poussée par HTTP et persistée en flash.

## Configuration WiFi

Les identifiants ne sont pas commités. Copie le modèle et renseigne ton réseau :

```bash
cd src
cp secrets.h.example secrets.h
# édite secrets.h : WIFI_SSID / WIFI_PASS
```

## Build & flash

Depuis la racine du monorepo :

```bash
./build.sh guition_knob Rich_Telemetry             # build seul
./build.sh auto Rich_Telemetry --upload             # build + flash (auto-détecte la carte)
```

## Modèle de configuration

Le layout est un objet JSON avec deux clés principales :

- **`components`** : map `id → définition` (type + style + config). Pas de position.
- **`pages`** : liste ordonnée de pages. Chaque page a un `name` et un `place` = liste de *placements* `{ "ref": <id>, ...géométrie }`.

Un même `id` peut être placé sur plusieurs pages : il n'existe qu'en un seul exemplaire mais son état (valeur) est commun. Un composant mis à jour via `/update` alors qu'il n'est pas sur la page active sera affiché correctement quand sa page devient visible.

```json
{
  "components": {
    "cpu": { "type": "readout", "label": "CPU", "unit": "%", "color": "#38BDF8" }
  },
  "pages": [
    { "name": "sys", "place": [{ "ref": "cpu", "anchor": "CENTER" }] }
  ]
}
```

## Types de composants

| Type | Config | Valeur `/update` | Notes |
|------|--------|-----------------|-------|
| `label` | `text`, `font` (14/20/28/36/48), `color` | string (optionnel) | Texte statique ou mis à jour |
| `readout` | `label`, `unit`, `font`, `color` | nombre ou string | Affiché « CPU 42 % » (label + valeur) |
| `bar` | `label`, `min`, `max`, `color` | nombre | Géométrie : `width`, `height`. Label affiché au-dessus de la barre. |
| `ring` | `color`, `font`, `pill`, `center_pct`, `center_color`, `countdown`, `min`, `max`, `thresholds` | `{"pct":0-100,"reset_in_s":N}` | Voir ci-dessous |
| `led_ring` | *(physique, pas de géométrie)* | objet mode | Anneau 13 WS2812 |
| `sound` | *(physique, pas de géométrie)* | objet tone/name | Tir unique |

### Anneau (`ring`)

Anneau circulaire avec ouverture en bas. Paramètres de config :

- `color` : couleur par défaut (hex).
- `pill` : `true` → affiche un pill de pourcentage sur la partie haute de la bande.
- `center_pct` : `true` → affiche `valeur + unité` au centre de l'anneau, en grand (taille = champ `font` : `14`/`20`/`28`/`36`/`48`). Exclusif avec `pill` (si les deux sont à `true`, `center_pct` gagne). Par défaut le chiffre **suit la couleur du seuil** (comme l'arc). Limite : unités non-ASCII (`°`, `µ`) non rendues — utiliser `%`, `C`, `V`, `rpm`…
- `center_color` : couleur fixe du chiffre central, qui **surcharge** la couleur déduite du seuil. Absent → le chiffre suit le seuil (ou `color` s'il n'y a pas de `thresholds`).
- `countdown` : `true` → décrémente `reset_in_s` d'une unité par seconde et l'affiche dans l'ouverture (ex. `1h50`, `5j6h`, `45s`). On peut aussi pousser une `"caption"` littérale.
- `min` / `max` : plage (défaut 0/100).
- `thresholds` : liste `[[limite, "#hex"], ...]` — la couleur change quand la valeur passe sous la limite.

Géométrie (sur le placement dans `pages`) : `radius`, `thickness`, `gap_deg` (angle d'ouverture), `start_angle` (oriente l'ouverture : offset en degrés horaire depuis le bas — `0`=bas, `90`=gauche, `180`=haut, `270`=droite ; défaut `0`).

Deux anneaux concentriques = même centre, `radius` différents :

```json
{ "ref": "w5h", "radius": 176, "thickness": 16, "gap_deg": 70 },
{ "ref": "w7d", "radius": 141, "thickness": 16, "gap_deg": 70 }
```

Valeur poussée via `/update` :

```json
{ "pct": 42, "reset_in_s": 3600 }
```

### Anneau LED (`led_ring`)

Commande l'anneau physique de 13 WS2812 :

```json
{
  "mode": "progress",
  "value": 42,
  "color": "#38BDF8",
  "brightness": 128,
  "period_ms": 500
}
```

Modes : `off`, `solid`, `progress`, `spinner`, `blink`, `breathe`.

### Son (`sound`)

Tir unique. Deux formes :

```json
{ "tone": 880, "ms": 200 }
```
```json
{ "name": "ok" }
```

Noms prédéfinis : `ok`, `alert`, `error`.

## API REST

Port 80 par défaut. mDNS `guition.local` (sur certains LAN le mDNS est filtré — utilise l'IP DHCP directe).

| Méthode | Route | Rôle |
|---------|-------|------|
| `POST` | `/update` | Mise à jour partielle des valeurs. Corps `{"id": valeur, ...}`. Réponse `{"ok":true,"updated":N,"unknown":[...]}`. JSON invalide → 400. |
| `POST` | `/layout` | Remplace le layout complet (validé avant swap, persisté en flash). Réponse `{"ok":true}` ou 400 + message d'erreur. |
| `GET` | `/layout` | Layout actif au format JSON. |
| `POST` | `/page` | Navigation : `{"dir":"next"\|"prev"}`, `{"index":N}` ou `{"name":"..."}`. Réponse `{"page":N,"name":"..."}`. Index hors plage ou nom inconnu → 404. |
| `GET` | `/status` | ip, hostname, rssi, uptime_s, page courante, liste des pages, composants. |
| `GET` | `/` | Page d'aide HTML. |

**CORS** : activé (`Allow-Origin: *`, preflight `OPTIONS` → `204`) pour qu'un éditeur web (le designer) puisse pousser un layout depuis un navigateur. Adapté à un usage LAN mono-utilisateur ; restreindre l'origine si l'exposition change.

### Exemples `curl`

```bash
# Pousser des valeurs
curl -X POST http://<ip>/update \
  -H 'Content-Type: application/json' \
  -d '{"w5h":{"pct":35,"reset_in_s":12600},"led":{"mode":"solid","color":"#22C55E"}}'

# Changer de page
curl -X POST http://<ip>/page \
  -H 'Content-Type: application/json' \
  -d '{"name":"usage"}'

# Lire le statut
curl http://<ip>/status
```

## Navigation

Trois sources de navigation entre les pages :

- **Encodeur rotatif** : CW = page suivante, CCW = page précédente.
- **Swipe latéral** (touch) : swipe vers la droite = page suivante, vers la gauche = page précédente. Les swipes verticaux sont ignorés (réservés).
- **REST** : `POST /page`.

Navigation circulaire (la dernière page boucle vers la première). Un indicateur de points en bas de l'écran indique la page active (affiché uniquement si >1 page).

## Persistance

Le layout poussé via `POST /layout` est sauvegardé en **LittleFS** et rechargé au boot. En l'absence de layout sauvegardé (ou si le JSON est invalide), le firmware utilise le layout par défaut compilé (les deux anneaux d'usage Claude).

Le fichier `data/layout.json` contient ce layout par défaut ; il est flashé en LittleFS via `--uploadfs` si on veut en faire le point de départ :

```bash
./build.sh guition_knob Rich_Telemetry --uploadfs
```

## Client exemple — `tools/push.py`

Pousse en boucle des valeurs synthétiques d'usage Claude (5 h / 7 j) :

```bash
python3 tools/push.py http://<ip>
python3 tools/push.py http://192.168.1.35 --interval 2
```

Aucune dépendance externe — stdlib Python uniquement.

## Limite d'affichage (ASCII uniquement)

Les polices Montserrat embarquées par LVGL ne couvrent que l'ASCII. Les `text`, `label` et `unit` contenant des accents ou des symboles afficheront un caractère manquant. Utilise des unités ASCII (`%`, `GB`, `C`, `Mbps`). Le countdown utilise `j`/`h`/`m`/`s` (ASCII).

## Tests

```bash
pio test -e native
```

Tests unitaires de la logique (layout parser, navigation, composants) — pas de device nécessaire.
