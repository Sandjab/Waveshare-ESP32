# Basic_WiFi_Telemetry — Guition K718

Le device se connecte au WiFi (STA), lève un petit serveur HTTP REST, et affiche
sur l'écran rond 360×360 les champs de télémétrie poussés depuis le Mac.

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
./build.sh guition Basic_WiFi_Telemetry            # build seul
./build.sh auto Basic_WiFi_Telemetry --upload      # build + flash (auto-détecte la carte)
```

Au boot, l'écran affiche `Connexion WiFi...` puis, une fois connecté,
`En attente...` avec en pied de page l'adresse : `guition.local | 192.168.x.x`.

## API REST

| Méthode | Route        | Rôle                                      |
|---------|--------------|-------------------------------------------|
| `POST`  | `/telemetry` | Corps JSON, remplace l'affichage courant  |
| `GET`   | `/status`    | IP, hostname, uptime, RSSI, nb de champs  |
| `GET`   | `/`          | Page d'aide HTML                          |

### `POST /telemetry`

```json
{
  "title": "Mac mini",
  "fields": [
    { "label": "CPU",  "value": 42,  "unit": "%"  },
    { "label": "RAM",  "value": 9.2, "unit": "GB" },
    { "label": "Disk", "value": 71,  "unit": "%"  }
  ]
}
```

- `title` est optionnel (défaut `Telemetry`).
- `value` accepte un nombre (entier ou décimal) ou une chaîne.
- Maximum **6 champs** ; au-delà, les suivants sont ignorés (warning sur le port série).
- Réponses : `200 {"ok":true}` ; `400` si le JSON est invalide ou si `fields` est absent (l'écran ne change pas).

Exemple `curl` (remplace l'IP par celle affichée à l'écran, ou `guition.local`) :

```bash
curl -X POST http://guition.local/telemetry \
  -H 'Content-Type: application/json' \
  -d '{"title":"Mac","fields":[{"label":"CPU","value":42,"unit":"%"}]}'

curl http://guition.local/status
```

## Push automatique depuis le Mac

`tools/push.py` envoie en boucle quelques métriques système (CPU, RAM) :

```bash
pip install psutil requests
python3 tools/push.py http://guition.local            # ou http://<ip>
python3 tools/push.py http://192.168.1.42 --interval 2
```

Adapte la fonction `sample()` pour envoyer les champs qui t'intéressent.

## Limite d'affichage (ASCII uniquement)

Les polices Montserrat embarquées par LVGL ne couvrent que l'ASCII. Les `label`
et `unit` contenant des accents ou des symboles (`é`, `°`, `·`…) afficheront un
caractère manquant. Utilise des unités ASCII (`%`, `GB`, `C`, `Mbps`). Pour
supporter d'autres caractères, il faudrait régénérer une police LVGL avec la
plage Unicode voulue.

## Notes techniques

- Serveur `WebServer` (core Arduino) synchrone : `handleClient()` et
  `lv_timer_handler()` tournent dans `loop()` → pas de souci de thread-safety LVGL.
- Les handlers HTTP stockent l'état et lèvent un flag ; le rendu LVGL est déféré
  au `loop()`.
- Reconnexion WiFi automatique ; l'écran affiche `Reconnexion...` en cas de chute.
- mDNS : le device est joignable via `guition.local`.
