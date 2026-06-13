# Rich_Telemetry — Design (Guition JC3636K718)

Date : 2026-06-13
Statut : validé en brainstorming, prêt pour le plan d'implémentation.
Base : `devices/guition_knob/projects/Basic_WiFi_Telemetry`.

## 1. But

Un dashboard de télémétrie **piloté par configuration** pour l'écran rond 360×360 du Guition K718.
Le visuel (composants, position, pages) est décrit par un **fichier de config JSON** ; les valeurs sont
poussées par une **API REST**. Au-delà de l'écran, deux composants « physiques » sont adressables par la
même API : **l'anneau LED** (13× WS2812) et **le son** (haut-parleur via PCM5100A).

Cas d'usage moteur : afficher les **fenêtres d'usage Claude** (5 h et 7 j) sous forme de couronnes
concentriques montrant le % de consommation et le temps avant remise à zéro.

## 2. Décisions de cadrage (verrouillées)

1. **Composants prédéfinis** : catalogue **fermé** de types en firmware, chacun adossé à un widget LVGL
   natif. La config *choisit/positionne/paramètre* ; elle ne décrit pas de widgets arbitraires (pas
   d'interpréteur embarqué — YAGNI).
2. **Config** : poussée par REST (`POST /layout`), **persistée en flash (LittleFS)**, rechargée au boot,
   reconstruit l'UI à chaud.
3. **Son** : bips/tonalités **paramétriques** (fréquence + durée) + quelques bips nommés. Pas de samples
   PCM en v1.
4. **Anneau LED** : modes off / couleur fixe / progression 0–100 / rotation (spinner) / clignotement-pulsation.
5. **Positionnement** : **ancrage LVGL** (`align` + `dx/dy`) pour les widgets ; les couronnes sont
   auto-centrées et placées par `radius`/`thickness`.
6. **Pages** : plusieurs pages, navigation **liste ordonnée circulaire** (wrap-around). Sources :
   encodeur (rotation physique), REST, swipe tactile. Une **même instance** de composant peut figurer sur
   plusieurs pages ; un composant d'une page cachée est **quand même mis à jour** (s'affichera correct quand
   sa page apparaît).
7. **Swipe en deux temps** : v1 livre encodeur + REST (zéro risque HW), puis ajoute le swipe CST816 comme
   dernière étape.

## 3. Architecture

Mono-thread, hérité de la base : `WebServer` synchrone ; les handlers HTTP **stockent l'état + lèvent un
flag** ; le rendu LVGL est **déféré au `loop()`** (pas de souci de thread-safety).

Séparation **état / vue** :

- **Composant (état)** : source de vérité, déclaré une fois par `id` — type + données + style, **sans
  position**.
- **Page (vue)** : liste de *placements* référençant des `id` de composants + leur géométrie **sur cette
  page**. Le même `id` peut être placé sur plusieurs pages, éventuellement à des positions différentes.
- **Catalogue** : `enum` de types + table de dispatch `type → { build(parent, placement), apply_value(comp, json) }`.

### Modèle d'instanciation des vues

Chaque page est un conteneur LVGL (objet écran ou conteneur masquable). Un placement crée le(s) objet(s)
LVGL du composant **comme enfants de la page**. Un composant placé sur N pages possède donc N vues. À la
mise à jour d'un composant, on itère ses vues et on applique la valeur à chacune. LVGL ne dessine que la
page active ⇒ les vues des pages cachées sont tenues à jour sans coût de rendu et sont déjà correctes à
l'affichage.

### Mémoire

Caps statiques (pas de heap fragmenté, cohérent avec la base) — valeurs initiales à ajuster :
`MAX_COMPONENTS=32`, `MAX_PAGES=8`, `MAX_PLACEMENTS_PER_PAGE=12`, `MAX_VIEWS_PER_COMPONENT=8`.
Parse ArduinoJson → copie dans des structs fixes.

## 4. Catalogue de composants (v1)

| Type | Widget LVGL | Config (composant) | Géométrie (placement) | Valeur `/update` |
|---|---|---|---|---|
| `label` | `lv_label` | text, font, color | anchor, dx, dy | string (optionnel) |
| `readout` | `lv_label` | label, unit, font, colors | anchor, dx, dy | nombre **ou** string → `"42 %"` |
| `bar` | `lv_bar` | label, min, max, color, orientation | anchor, dx, dy, width, height | nombre |
| `ring` | `lv_arc` | color, thresholds[], pill, center_pct, countdown, min, max | radius, thickness, gap_deg, start_angle | objet `{pct, reset_in_s}` |
| `led_ring` *(physique)* | WS2812 (`rgb_ring.h`) | brightness défaut | — | objet `{mode, color, value, brightness, period_ms}` |
| `sound` *(physique)* | I2S → PCM5100A | — | — | `{tone, ms}` \| `{name:"ok\|alert\|error"}` \| `{seq:[{tone,ms},…]}` |

### `ring` (couronne)

- Anneau `lv_arc` **ouvert en bas** (gap centré sur le bas). Affiche deux infos : `pct` (remplissage) et un
  texte de temps restant dans l'ouverture (`↻ 1h50`, symbole `LV_SYMBOL_REFRESH`).
- `countdown:true` ⇒ le client pousse `pct` + `reset_in_s` (secondes) ; le **firmware décrémente** chaque
  seconde et reformate l'affichage (fluide même avec des push espacés). `countdown:false` ⇒ on peut pousser
  un `caption` texte directement.
- Options par instance : `pill` (pastille de % en haut), `center_pct` (% au centre), `thresholds`
  (liste `[seuil, couleur]`, ex. vert <70, orange <90, rouge au-delà).
- Concentriques : centre partagé, on distingue par `radius`/`thickness`.

### `led_ring` (anneau physique)

Machine à états tickée dans `loop()` :
- `off`
- `solid` : `{color, brightness}`
- `progress` : `{value:0–100, color}` → N des 13 LEDs allumées
- `spinner` : `{color, period_ms}` → segment tournant
- `blink` / `breathe` : `{color, period_ms}`

Helper existant : `rgb_ring.h` (Adafruit_NeoPixel, GPIO 0). Attention luminosité (13 LEDs blanc plein ≈ 780 mA
> budget USB) — défaut prudent (≈64/255).

### `sound` (haut-parleur)

**Fire-once** : présent dans `/update` ⇒ jeu **enfilé** dans une file. Générateur de tonalité non-bloquant
alimenté depuis `loop()` (I2S → PCM5100A → ampli NS4150B activé par `PIN_PA_MUTE` high). Sources : tonalité
paramétrique (`{tone, ms}`), bip nommé (`{name}`), courte séquence (`{seq}`). Pas de samples PCM en v1.

## 5. Schémas JSON

### Layout (`POST /layout`)

```json
{
  "title": "Claude Usage",
  "background": "#0B0B0F",
  "nav": { "wrap": true },
  "components": {
    "w5h": {"type":"ring","color":"#38BDF8","pill":true,"countdown":true,
            "thresholds":[[70,"#22C55E"],[90,"#F59E0B"],[100,"#EF4444"]]},
    "w7d": {"type":"ring","color":"#A78BFA","pill":true,"countdown":true},
    "cpu": {"type":"readout","label":"CPU","unit":"%"},
    "disk":{"type":"bar","label":"Disk","min":0,"max":100,"color":"#3B82F6"},
    "title":{"type":"label","text":"Claude","font":28,"color":"#F5F5F5"},
    "led": {"type":"led_ring"},
    "buzz":{"type":"sound"}
  },
  "pages": [
    {"name":"usage", "place":[
        {"ref":"w5h","radius":140,"thickness":16,"gap_deg":70},
        {"ref":"w7d","radius":105,"thickness":16,"gap_deg":70},
        {"ref":"title","anchor":"CENTER","dy":-10}
    ]},
    {"name":"system","place":[
        {"ref":"cpu","anchor":"TOP_MID","dy":60},
        {"ref":"disk","anchor":"CENTER","width":220}
    ]}
  ]
}
```

`components` = map `id → définition` (sans position). `pages[].place` = placements (`ref` + géométrie).
`led`/`buzz` déclarés mais jamais placés.

### Update (`POST /update`) — partiel

```json
{ "w5h": {"pct":63,"reset_in_s":6600},
  "w7d": {"pct":38,"reset_in_s":453600},
  "cpu": 42,
  "disk": 71,
  "led": {"mode":"progress","value":63,"color":"#38BDF8"},
  "buzz": {"name":"alert"} }
```

Seuls les `id` présents sont mis à jour ; les autres restent inchangés (invariant central).

## 6. API REST

| Méthode | Route | Rôle | Réponses |
|---|---|---|---|
| `POST` | `/update` | Valeurs partielles | `200 {ok:true, updated:[…], unknown:[…]}` ; `400` si JSON invalide |
| `POST` | `/layout` | Remplace le layout (validé **avant** swap), persiste LittleFS, reconstruit | `200 {ok:true}` ; `400` si invalide (UI courante conservée) |
| `GET` | `/layout` | Layout courant (JSON) | `200` |
| `POST` | `/page` | `{dir:"next"\|"prev"}` \| `{index:N}` \| `{name:"…"}` | `200 {page, name}` ; `400/404` |
| `GET` | `/status` | ip, hostname, rssi, uptime_s, page (courante/total), components, have_layout | `200` |
| `GET` | `/` | Page d'aide HTML (curl) | `200` |

mDNS conservé (`guition.local`). NB : `*.local` non résolu sur le LAN de dev → joindre par IP directe.

## 7. Navigation

Pages = liste ordonnée circulaire. Toutes les sources appellent `model.goto_page(idx)` :

- **Encodeur** : `bidi_switch_knob` (déjà utilisé par `Basic_Audio`), delta lu dans `loop()`. CW = suivant,
  CCW = précédent.
- **REST** : `POST /page`.
- **Swipe** (étape finale) : panneau **CST816** via le composant officiel `espressif/esp_lcd_touch_cst816s`,
  enregistré comme `indev` pointeur LVGL ; gestures via `LV_EVENT_GESTURE` (gauche/droite/haut/bas →
  suivant/précédent). C'est le seul bring-up HW nouveau du repo (faible risque, composant maintenu).

Indicateur de page : points en bas de l'écran (sur chaque page).

## 8. Cycle `loop()`

1. `server.handleClient()`
2. si `layout_dirty` : détruit les pages/vues, reconstruit depuis le modèle, recale la page active.
3. si `values_dirty` : pour chaque composant marqué, applique la valeur à toutes ses vues.
4. tick **countdown** (1 Hz) : décrémente `reset_in_s` des rings `countdown`, reformate la légende.
5. tick **led_ring** (~30 Hz) : anime selon le mode.
6. tick **sound** : alimente l'I2S depuis la file de lecture.
7. poll **encodeur** → `goto_page` si delta.
8. `lv_timer_handler()` ; `delay(5)`.

Note de risque : `rgb_ring.show()` (NeoPixel) coupe brièvement les interruptions (~0,4 ms pour 13 LEDs) ;
acceptable à ~30 Hz mais à surveiller vis-à-vis du timing I2S/WiFi si une animation tourne pendant un son.

## 9. Persistance & gestion d'erreurs

- **LittleFS** : au boot, monte le FS ; charge `/layout.json` s'il existe, sinon un **layout par défaut
  compilé** (le device n'est jamais vide). `POST /layout` écrit `/layout.json` puis reconstruit.
  *À vérifier* : la table de partitions `default_16MB.csv` inclut bien une partition FS ; sinon, CSV custom.
  Un `data/layout.json` peut aussi servir de fallback uploadé via `uploadfs`.
- JSON invalide → `400`, état inchangé.
- Layout invalide (type inconnu, enum erroné, caps dépassés) → **validé entièrement avant** de détruire l'UI
  courante ; refus `400` sinon.
- `id` inconnus dans `/update` → `200`, listés dans `unknown` (tolérant, orienté télémétrie).
- Échec d'écriture LittleFS → `500`, layout RAM conservé.
- Reconnexion WiFi automatique (hérité de la base).

## 10. Structure de fichiers

```
devices/guition_knob/projects/Rich_Telemetry/
  platformio.ini
  README.md
  data/layout.json              # layout par défaut (fallback uploadfs)
  src/
    main.cpp                    # setup/loop, WiFi, câblage des modules
    config.h                    # caps + constantes
    lv_conf.h
    secrets.h(.example)
    dashboard.{h,cpp}           # modèle : composants, pages, page active, apply_update, set_layout, ticks
    components.{h,cpp}          # catalogue écran : label, readout, bar, ring (build/apply)
    led_ring_comp.{h,cpp}       # machine à états anneau LED
    sound_comp.{h,cpp}          # moteur tonalités + file de lecture (I2S)
    nav.{h,cpp}                 # encodeur + REST → goto_page  (+ touch_cst816 à l'étape finale)
    api.{h,cpp}                 # routes HTTP
    persist.{h,cpp}             # LittleFS load/save layout
  tools/push.py                 # client exemple (fenêtres Claude + métriques système)
```

`lib_extra_dirs` pointe sur `../../lib` (guition_knob_hw) et `../../../../shared/lib` (comme la base).
`lib_deps` : lvgl 8.4, ArduinoJson 7, Adafruit NeoPixel, `esp_lcd_touch_cst816s` (étape swipe).

## 11. Tests (Rule 9 — vérifier l'intention)

On factorise les fonctions **pures** (sans LVGL/HW) dans des modules compilables en natif et on les teste via
un `[env:native]` PlatformIO (Unity/doctest) :

- **Invariant central** : un `/update` partiel laisse les composants non cités **inchangés** (ce test échoue
  si la sémantique de mise à jour partielle régresse).
- `id` inconnu → signalé dans `unknown`, **non appliqué**.
- `format_remaining(seconds)` aux bornes : 59 s, 60 s, 1 h, 24 h, multi-jours.
- `nav_next` / `nav_prev` : wrap-around correct aux extrémités (liste de 1, de N).
- Sélection de couleur par seuil : aux frontières 70 / 90 / 100.
- Parse layout : map `components` + `pages[].place`, refus si type inconnu / cap dépassé.

Smoke-test on-device scripté via `tools/push.py` (séquence de `curl` couvrant chaque type + navigation).

## 12. Hors-scope v1 (YAGNI)

Voyant/LED écran (`lv_led`), jauge à aiguille (`lv_meter`), compteur gros chiffre, sparkline (`lv_chart`) ;
navigation par graphe directionnel ; transitions animées entre pages ; samples PCM nommés ; authentification.
Tous réintroductibles : nouveaux types = ajout dans le catalogue ; nav graphe = extension du modèle de pages.

## 13. Limite héritée

Polices Montserrat LVGL = ASCII seul ⇒ labels/unités accentués ne s'affichent pas (utiliser `%`, `GB`, `C`,
`Mbps`…). Le symbole `↻` (`LV_SYMBOL_REFRESH`) est disponible (font symboles intégrée).
