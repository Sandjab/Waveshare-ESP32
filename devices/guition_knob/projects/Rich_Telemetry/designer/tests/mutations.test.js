import { test } from 'node:test';
import assert from 'node:assert/strict';
import {
  uniqueId, DEFAULTS, addComponent, addPlacement, removePlacement,
  setComponentProp, setPlacementProp, setThresholds,
  addPage, removePage, renamePage, reorderPages
} from '../js/mutations.js';

const fresh = () => ({ components: {}, pages: [{ name: 'P1', place: [] }] });

test('uniqueId incrémente par type', () => {
  const s = fresh();
  assert.equal(uniqueId(s, 'label'), 'label1');
  s.components.label1 = { type: 'label' };
  assert.equal(uniqueId(s, 'label'), 'label2');
});

test('DEFAULTS produit une définition valide par type', () => {
  for (const type of ['label','readout','bar','ring','led_ring','sound']) {
    assert.equal(DEFAULTS[type]().type, type);
  }
});

test('addComponent ajoute à la map components', () => {
  const s = fresh();
  addComponent(s, 'x', { type: 'label', text: 'Hi' });
  assert.deepEqual(s.components.x, { type: 'label', text: 'Hi' });
});

test('addPlacement pousse sur la page', () => {
  const s = fresh();
  addPlacement(s, 0, { ref: 'x', anchor: 'CENTER' });
  assert.equal(s.pages[0].place.length, 1);
  assert.equal(s.pages[0].place[0].ref, 'x');
});

test('removePlacement retire par index', () => {
  const s = fresh();
  s.pages[0].place = [{ ref: 'a' }, { ref: 'b' }];
  removePlacement(s, 0, 0);
  assert.deepEqual(s.pages[0].place.map(p => p.ref), ['b']);
});

test('setComponentProp pose une valeur, vide la supprime', () => {
  const s = fresh();
  s.components.x = { type: 'label' };
  setComponentProp(s, 'x', 'text', 'Hi');
  assert.equal(s.components.x.text, 'Hi');
  setComponentProp(s, 'x', 'text', '');
  assert.equal('text' in s.components.x, false);
});

test('setPlacementProp pose une valeur, vide la supprime', () => {
  const s = fresh();
  s.pages[0].place = [{ ref: 'x', dx: 5 }];
  setPlacementProp(s, 0, 0, 'dy', 12);
  assert.equal(s.pages[0].place[0].dy, 12);
  setPlacementProp(s, 0, 0, 'dx', '');
  assert.equal('dx' in s.pages[0].place[0], false);
});

test('setThresholds pose un tableau non vide, vide le supprime', () => {
  const s = fresh();
  s.components.x = { type: 'ring' };
  setThresholds(s, 'x', [[20, '#FF0000']]);
  assert.deepEqual(s.components.x.thresholds, [[20, '#FF0000']]);
  setThresholds(s, 'x', []);
  assert.equal('thresholds' in s.components.x, false);
});

test('setComponentProp ignore un id inconnu', () => {
  const s = fresh();
  setComponentProp(s, 'missing', 'text', 'Hi'); // ne doit pas throw
  assert.deepEqual(s.components, {});
});

test('setPlacementProp ignore un index hors borne', () => {
  const s = fresh();
  setPlacementProp(s, 0, 99, 'dy', 10); // place index hors borne : ne doit pas throw
  setPlacementProp(s, 99, 0, 'dy', 10); // page index hors borne : ne doit pas throw (parité add/removePlacement)
  assert.equal(s.pages[0].place.length, 0);
});

test('addPage ajoute une page vide nommée en fin de liste', () => {
  const s = fresh();
  addPage(s, 'P2');
  assert.equal(s.pages.length, 2);
  assert.deepEqual(s.pages[1], { name: 'P2', place: [] });
});

test('removePage retire la page par index', () => {
  const s = fresh();
  addPage(s, 'P2');
  removePage(s, 0);
  assert.deepEqual(s.pages.map(p => p.name), ['P2']);
});

test('renamePage change le nom de la page', () => {
  const s = fresh();
  renamePage(s, 0, 'Accueil');
  assert.equal(s.pages[0].name, 'Accueil');
});

test('reorderPages déplace from → to', () => {
  const s = fresh();
  addPage(s, 'P2'); addPage(s, 'P3');          // [P1, P2, P3]
  reorderPages(s, 0, 2);                        // [P2, P3, P1]
  assert.deepEqual(s.pages.map(p => p.name), ['P2', 'P3', 'P1']);
});

test('reorderPages ignore les index hors bornes (no-op)', () => {
  const s = fresh();
  addPage(s, 'P2');                             // [P1, P2]
  reorderPages(s, 0, 5);
  assert.deepEqual(s.pages.map(p => p.name), ['P1', 'P2']);
});
