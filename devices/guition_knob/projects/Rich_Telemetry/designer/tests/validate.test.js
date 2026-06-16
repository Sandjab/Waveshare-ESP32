import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { createValidator } from '../js/validate.js';
import { DEFAULT_LAYOUT } from '../js/default-layout.js';

const schema = JSON.parse(readFileSync(new URL('../../schema/layout.schema.json', import.meta.url)));
const validate = createValidator(schema);

test('layout par défaut est valide', () => {
  const r = validate(DEFAULT_LAYOUT);
  assert.equal(r.valid, true);
  assert.deepEqual(r.errors, []);
});

test('type de composant inconnu → invalide', () => {
  const bad = structuredClone(DEFAULT_LAYOUT);
  bad.components.titre.type = 'wat';
  assert.equal(validate(bad).valid, false);
});

test('couleur hex invalide → invalide', () => {
  const bad = structuredClone(DEFAULT_LAYOUT);
  bad.background = 'red';
  assert.equal(validate(bad).valid, false);
});

test("ref de placement non résolue → invalide (sémantique, hors JSON Schema)", () => {
  const bad = structuredClone(DEFAULT_LAYOUT);
  bad.pages[0].place[0].ref = 'ghost';
  const r = validate(bad);
  assert.equal(r.valid, false);
  assert.ok(r.errors.some(e => e.includes('ghost')));        // message humanisé : « page 1 : référence inconnue « ghost » »
});

test('erreurs de forme ET sémantique coexistent (pas de court-circuit)', () => {
  const bad = structuredClone(DEFAULT_LAYOUT);
  bad.background = 'red';               // erreur de forme
  bad.pages[0].place[0].ref = 'ghost';  // erreur sémantique
  const r = validate(bad);
  assert.equal(r.valid, false);
  assert.ok(r.errors.some(e => e.includes('background')));   // « background : doit être une couleur #RRGGBB »
  assert.ok(r.errors.some(e => e.includes('ghost')));
});

test("ref absente → erreur de forme seule, pas de 'ref inconnue undefined'", () => {
  const bad = structuredClone(DEFAULT_LAYOUT);
  delete bad.pages[0].place[0].ref;
  const r = validate(bad);
  assert.equal(r.valid, false); // la forme échoue (ref requis par le schema)
  assert.ok(!r.errors.some(e => e.includes('undefined')));
});
