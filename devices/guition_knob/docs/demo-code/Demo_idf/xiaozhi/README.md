# xiaozhi-esp32 — board reference

Guition livre dans son archive un clone du projet open-source [`xiaozhi-esp32`](https://github.com/78/xiaozhi-esp32) (AI voice assistant pour ESP32), avec un dossier `boards/guition-jc3636k718/` device-specific.

On garde uniquement ce sous-dossier ici (`board-jc3636k718/`) — 4 fichiers, 28 KB — comme **référence de pinout / configuration audio** pour le JC3636K718. Le reste du projet upstream (~20 MB) est volontairement non versionné : récupérable via `git clone https://github.com/78/xiaozhi-esp32.git`.

## Contenu

| Fichier | Rôle |
|---|---|
| `board-jc3636k718/config.h` | Macros board (pins, periphs) |
| `board-jc3636k718/config.json` | Métadonnées board (utilisé par le build system xiaozhi) |
| `board-jc3636k718/README.md` | Notes d'usage du board sous xiaozhi |
| `board-jc3636k718/taiji_pi_s3.cc` | Implémentation board (init audio, codec, etc.) |
