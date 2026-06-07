# Basic_WiFi_Telemetry — Guition K718

Design validé le 2026-06-07.

## But

Le device Guition JC3636K718 se connecte au WiFi (STA), lève un petit serveur
HTTP REST, et affiche en LVGL les champs de télémétrie génériques poussés depuis
le Mac. Cas d'usage : proto perso, un seul utilisateur sur le réseau local.

## Décisions

| Sujet | Choix | Pourquoi |
|---|---|---|
| Framework | Arduino + PlatformIO, LVGL 8.4 | Conforme à `Basic_LVGL_Meter` et aux autres projets du device |
| Serveur HTTP | `WebServer` (core Arduino), synchrone | Mono-thread : `handleClient()` + `lv_timer_handler()` dans `loop()` → pas de souci de thread-safety LVGL. Zéro dépendance externe |
| Format | JSON via ArduinoJson | Champs génériques `{label, value, unit}` |
| Découverte | IP affichée à l'écran + mDNS `guition.local` (ESPmDNS) | Peu de code, couvre les deux usages |
| Secrets WiFi | `src/secrets.h` non commité (gitignore) + `secrets.h.example` | Build-time, pas de credentials dans le dépôt |
| Affichage | Liste verticale centrée (titre + lignes label/valeur/unité + footer IP) | Adapté à l'écran rond 360×360 |
| Max champs | 6 | Tient lisiblement sur l'écran rond |
| Push Mac | exemple `curl` (README) + `tools/push.py` (psutil) | L'utilisateur décide quoi envoyer |

Alternatives écartées : `ESPAsyncWebServer` (dépendance + callbacks hors thread
loop = risque LVGL), `esp_http_server` IDF (trop bas niveau ici).

## API REST

| Méthode | Route | Rôle |
|---|---|---|
| `POST` | `/telemetry` | Corps JSON, remplace l'affichage courant |
| `GET` | `/status` | IP, uptime, RSSI, nb de champs (vérif) |
| `GET` | `/` | Page d'aide HTML minimale |

Corps `POST /telemetry` :

```json
{ "title": "Mac mini",
  "fields": [
    { "label": "CPU",  "value": 42,  "unit": "%"  },
    { "label": "RAM",  "value": 9.2, "unit": "GB" },
    { "label": "Temp", "value": 61,  "unit": "°C" } ] }
```

Réponses : `200` ok ; `400` JSON invalide / `fields` absent (corps texte
explicatif) ; au-delà de 6 champs → tronqué, warning série, `200`.

## Affichage (LVGL, 360×360 rond)

```
        Mac mini            ← title (défaut : "Telemetry")
   ─────────────────
     CPU      42 %
     RAM     9.2 GB
     Temp     61 °C
   ─────────────────
   192.168.x.x · guition    ← footer, toujours visible
```

États : avant 1re donnée → écran d'attente avec IP + `guition.local` ;
WiFi déconnecté → bandeau « Reconnexion… ».

## Flux

`script Mac → POST /telemetry → handler parse JSON → stocke struct + flag dirty
→ loop() voit dirty → reconstruit les labels LVGL`. Tout dans le thread `loop`,
donc pas de mutex.

## Robustesse

- WiFi : retry au boot (timeout + affichage), reconnexion auto sur chute.
- JSON malformé → `400`, l'écran ne change pas.
- Handler court, rendu déféré au `loop()` : le serveur ne bloque jamais LVGL.

## Structure

```
devices/guition_knob/projects/Basic_WiFi_Telemetry/
├── platformio.ini        # base LVGL_Meter + ArduinoJson + build_flags
├── README.md             # API + exemples curl
├── tools/push.py         # push optionnel depuis le Mac (psutil)
└── src/{ main.cpp, lv_conf.h, secrets.h.example }
```

`.gitignore` : ajouter `**/secrets.h`.

## Vérification

1. Build : `./build.sh guition Basic_WiFi_Telemetry` compile sans erreur.
2. Flash : `./build.sh auto Basic_WiFi_Telemetry --upload` → écran montre l'IP.
3. Réseau : `curl -X POST http://<ip>/telemetry -d '{...}'` → champs affichés ;
   `GET /status` répond.
