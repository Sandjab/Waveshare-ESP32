#!/usr/bin/env python3
"""Pousse des metriques du Mac vers le device Guition (POST /telemetry).

Setup :
    pip install psutil requests

Usage :
    python3 push.py http://guition.local
    python3 push.py http://192.168.1.42 --interval 2

Les unites restent en ASCII (%, GB) car les polices Montserrat de LVGL ne
couvrent pas les accents ni les symboles type degre.
"""
import argparse
import sys
import time

try:
    import psutil
    import requests
except ImportError:
    sys.exit("Dependances manquantes : pip install psutil requests")


def sample():
    cpu = psutil.cpu_percent(interval=None)
    mem = psutil.virtual_memory()
    return {
        "title": "Mac",
        "fields": [
            {"label": "CPU", "value": round(cpu), "unit": "%"},
            {"label": "RAM", "value": round(mem.percent), "unit": "%"},
            {"label": "Free", "value": round(mem.available / 1e9, 1), "unit": "GB"},
        ],
    }


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("url", help="URL de base, ex: http://guition.local")
    ap.add_argument("--interval", type=float, default=2.0,
                    help="intervalle en secondes (defaut 2)")
    args = ap.parse_args()

    endpoint = args.url.rstrip("/") + "/telemetry"
    psutil.cpu_percent(interval=None)  # amorce la mesure CPU
    print(f"push vers {endpoint} toutes les {args.interval}s (Ctrl-C pour arreter)")

    while True:
        try:
            r = requests.post(endpoint, json=sample(), timeout=3)
            print(r.status_code, r.text.strip())
        except requests.RequestException as e:
            print("erreur:", e)
        time.sleep(args.interval)


if __name__ == "__main__":
    main()
