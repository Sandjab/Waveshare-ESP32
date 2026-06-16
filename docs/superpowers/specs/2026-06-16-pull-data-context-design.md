# Rich_Telemetry — pull de données via un contexte producteur/consommateur

Design validé le 2026-06-16.

## But

Aujourd'hui le device est **passif** : un client externe **pousse** les données
(`POST /update {composantId: valeur}`). On veut que le device puisse aussi
**tirer** (pull) ses données depuis **une ou plusieurs URL**, à un **rythme**
défini, en alimentant un **contexte partagé** dans lequel les composants liés
trouvent leurs valeurs — un modèle **producteur/consommateur asynchrone**.

Sources visées : **LAN auto-hébergé (HTTP)** *et* **APIs tierces (HTTPS, auth,
JSON imbriqué)**.

## Contrainte décisive (vérifiée sur le code)

Le firmware tourne un **`WebServer` synchrone** et un `loop()` strictement
**mono-thread coopératif** (`server.handleClient()` → `led_ring_tick` →
`sound_tick` → `lv_timer_handler()` → `delay(5)` — `main.cpp`). Un `GET` HTTP
**bloquant** dans ce `loop()` figerait l'UI. Le pull **ne peut donc pas** se
faire en ligne dans le `loop()`. → Le pull se fait dans une **tâche FreeRTOS
dédiée** (l'ESP32-S3 est bi-cœur), ce qui **sélectionne d'office** le modèle
contexte producteur/consommateur.

## Architecture

- **Producteur** — une **tâche FreeRTOS** (épinglée sur le cœur libre via
  `xTaskCreatePinnedToCore`) boucle sur les sources, fetch chacune à son
  `interval_s`, extrait ses variables, écrit le **contexte**. Le `loop()` n'est
  jamais bloqué.
- **Contexte (blackboard)** — KV de variables nommées, protégé par **mutex**
  (`SemaphoreHandle_t`). Entrée = `name → { type (num|str), valeur, updated_at }`.
- **Consommateur** — un pas **`context_apply()`** dans le `loop()` prend le mutex,
  et pour chaque composant `bind:"var"` applique la valeur courante de la variable
  (réutilise le chemin d'`apply` existant : `value`/`vstr`, marque `dirty` si
  changé). `view_sync` pousse ensuite au rendu, inchangé.

## Données — push et pull coexistent (additif, rétro-compatible)

| Voie | Mécanisme | Statut |
|---|---|---|
| Push par id | `POST /update {cpu: 42}` → composant `cpu` | **Inchangé** |
| Pull | tâche → variables du contexte ; composant `bind:"temp"` lit `temp` | Nouveau |
| Push vers variable | **`POST /context {temp: 21}`** → même blackboard que le pull | Nouveau |

`/update` (par id) reste intact. `/context` (par nom de variable) est le pendant
push du blackboard. Un composant est **soit** push-fed par son id (aujourd'hui),
**soit** `bind:"var"` (lit le contexte) — pas les deux.

## Schema (ajouts au layout)

```jsonc
"sources": [{
  "name": "weather",
  "url": "https://api.example/weather?city=Paris",
  "interval_s": 600,                                  // borne mini imposée (ex. 5 s)
  "headers": { "X-API-Key": "$weather_key" },         // $nom = réf. au store de secrets
  "vars": { "temp": "/main/temp", "hum": "/main/humidity" }  // nom → JSON Pointer (RFC 6901)
}],
"components": {
  "t": { "type": "readout", "unit": "C", "bind": "temp" }    // lit la variable "temp"
}
```

- « Une ou plusieurs URL » = **plusieurs sources** (1 source = 1 URL).
- Extraction par **JSON Pointer** (RFC 6901, `/a/b/0`) — walker maison (ArduinoJson
  n'a pas d'éval de pointer).
- `bind` est un champ **optionnel** ajouté à chaque type de composant data
  (`readout`/`bar`/`ring`/`chart`/`meter`/`label`). Absent = comportement push
  actuel.

## Secrets

Store **write-only** sur LittleFS, distinct du layout : `POST /secrets
{weather_key: "…"}`. **Jamais** servi par `GET`, **absent** du layout, du designer
et de l'export. Les `headers` du layout référencent par nom (`$weather_key`),
résolus **à l'instant du fetch** par la tâche producteur. → aucune clé ne fuit par
`GET /layout` / le designer / un `layout.json` partagé.

## Défauts (sécurité / robustesse) — v1

| Sujet | Choix v1 | Évolution différée |
|---|---|---|
| TLS | `WiFiClientSecure.setInsecure()` (pas de pinning) | Pinning de certif / CA bundle |
| Échec de fetch | Garde la **dernière valeur connue** ; n'écrase pas | — |
| Observabilité | Compteur d'erreurs + `updated_at` par source dans `GET /status` | — |
| Péremption | Pas d'indication visuelle (valeur figée) | Badge « stale » après N×interval |
| Types de variable | nombre **et** chaîne courte | — |

## Phasage (mappé sur la testabilité, comme le mécanisme de registre)

- **P1 — plan de données (firmware), native-testable, sans device.** Le contexte
  (blackboard), `context_apply()`, le champ `bind`, l'endpoint `/context`, et
  l'extraction JSON Pointer. **Aucun réseau.** `pio test -e native` : écrire une
  variable → un composant lié se met à jour ; extraction de chemins imbriqués ;
  coexistence push-id / bind ; dernière-valeur-connue.
- **P2 — producteur (réseau), device.** Tâche FreeRTOS + client HTTP(S) + parsing
  des `sources` + store de secrets + résolution des `$refs`. Validé sur le Guition
  (WiFi + vraies URLs, HTTP et HTTPS, série + écran).
- **P3 — designer.** Éditeur de `sources` (3 colonnes : url/interval/headers/vars)
  + champ `bind` dans l'inspecteur + entrée schema. Browser-testable (node `--test`
  + conformité + navigateur).

Chaque phase produit un livrable testable seul. P1 d'abord (autonome, sans device).

## Vérification

- **P1** : `pio test -e native` (cœur du plan de données, HW-free).
- **P2** : flash + `curl` (poser un secret, poser un layout avec sources, observer
  le pull en série/écran) — workflow device habituel.
- **P3** : `node --test` (registre + conformité) + navigateur.

## Décisions écartées

- **Pull inline par composant** (un composant porte son propre `url`/`interval`) —
  serait du sucre au-dessus des sources nommées ; différé (YAGNI). Le binding par
  variable couvre déjà le cas (plusieurs composants partagent une source = un fetch).
- **Unifier push et pull** (tout devient variables, `/update` écrit des variables) —
  plus net mais casse la sémantique `id→valeur` de `/update` et force la migration
  de tous les layouts existants. Rejeté au profit de la **coexistence**.
- **Clés d'API dans le layout** (`sources[].headers` en clair) — plus simple mais
  fuite par `GET /layout` / designer / export. Rejeté au profit du **store write-only**.
- **Contexte = JSON brut par source + chemin au composant** — contexte plus gros,
  ré-extraction par composant, push moins unifiable. Rejeté au profit du **blackboard
  de variables nommées** (extraction centralisée dans la source).
- **Fetch bloquant dans `loop()`** — fige l'UI. Rejeté au profit de la **tâche dédiée**.
