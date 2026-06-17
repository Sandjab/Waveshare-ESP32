import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { createValidator } from '../js/validate.js';

const schema = JSON.parse(
  readFileSync(new URL('../../schema/layout.schema.json', import.meta.url))
);
const validate = createValidator(schema);

// Layout minimal valide réutilisé par les cas (un composant + une page).
function base() {
  return {
    components: { t: { type: 'readout', unit: 'C' } },
    pages: [{ name: 'P1', place: [{ ref: 't', anchor: 'CENTER' }] }]
  };
}

test('schema : sources top-level valides (url/interval/headers/vars)', () => {
  const l = base();
  l.sources = [{
    name: 'weather',
    url: 'https://api.example/w?city=Paris',
    interval_s: 600,
    headers: { 'X-API-Key': '$weather_key' },
    vars: { temp: '/main/temp' }
  }];
  const r = validate(l);
  assert.equal(r.valid, true, JSON.stringify(r.errors));
});

test('schema : une source sans url est rejetée', () => {
  const l = base();
  l.sources = [{ name: 'bad', interval_s: 600 }];
  assert.equal(validate(l).valid, false);
});

test('schema : interval_s sous le plancher 5 est rejeté', () => {
  const l = base();
  l.sources = [{ url: 'http://x', interval_s: 2 }];
  assert.equal(validate(l).valid, false);
});

test('schema : champ bind accepté sur un composant data', () => {
  const l = base();
  l.components.t.bind = 'temp';
  const r = validate(l);
  assert.equal(r.valid, true, JSON.stringify(r.errors));
});

test('schema : secrets top-level reste interdit (write-only, hors layout)', () => {
  const l = base();
  l.secrets = { weather_key: 'xxx' };
  assert.equal(validate(l).valid, false);
});
