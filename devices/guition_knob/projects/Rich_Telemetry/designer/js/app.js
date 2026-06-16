import { createModel } from './model.js';
import { createValidator } from './validate.js';
import { bindJsonView } from './json-view.js';
import { loadLayout, pushLayout } from './device.js';
import { createCanvas } from './canvas.js';
import { createPalette } from './palette.js';
import { createInspector } from './inspector.js';
import { createPages } from './pages.js';

const $ = id => document.getElementById(id);

async function main() {
  // Le schema partage vit dans ../schema (hors du dossier designer) : servir depuis le parent.
  let schema;
  try {
    const r = await fetch('../schema/layout.schema.json');
    if (!r.ok) throw new Error(`HTTP ${r.status} — servir depuis Rich_Telemetry/, pas designer/`);
    schema = await r.json();
  } catch (e) {
    const s = document.getElementById('status');
    s.textContent = 'Erreur init schema : ' + e.message;
    s.className = 'status err';
    return;
  }
  const validate = createValidator(schema);
  const model = createModel();

  let inspector;
  // Canvas WYSIWYG (page 0). onSelect → inspecteur.
  const canvas = createCanvas({ stage: $('stage'), badges: $('badges') }, model, {
    onSelect: s => inspector.select(s)
  });
  inspector = createInspector($('inspector'), model, {
    rerenderCanvas: canvas.render,
    clearSelection: () => canvas.selectPlacement(null),
    getActivePage: canvas.getActivePage
  });

  // Palette : glisser un type (création) ou un composant de la bibliothèque (partage) sur le canvas,
  // sur la page active, puis sélection du nouveau placement.
  createPalette($('palette'), model, {
    stage: $('stage'),
    getActivePage: canvas.getActivePage,
    onCreated: i => canvas.selectPlacement(i)
  });

  // Onglets de pages : sélectionner la page active + CRUD + réordonner.
  const pages = createPages($('pages'), model, {
    getActivePage: canvas.getActivePage,
    setPage: i => canvas.setPage(i)
  });

  bindJsonView(model, {
    textarea: $('json'), applyBtn: $('apply'), validEl: $('valid'), errorsEl: $('errors')
  }, validate);

  const syncUndo = () => { $('undo').disabled = !model.canUndo(); $('redo').disabled = !model.canRedo(); };
  model.subscribe(syncUndo); syncUndo();
  $('undo').onclick = () => { $('json').blur(); model.undo(); };
  $('redo').onclick = () => { $('json').blur(); model.redo(); };

  const setStatus = (msg, kind) => { $('status').textContent = msg; $('status').className = 'status' + (kind ? ' ' + kind : ''); };
  $('load').onclick = async () => {
    if (!$('base').value) return setStatus('URL device ?', 'err');
    setStatus('Chargement…');
    try { model.loadJSON(JSON.stringify(await loadLayout($('base').value))); setStatus('Chargé', 'ok'); }
    catch (e) { setStatus('Échec : ' + e.message + ' (CORS ? cf. README)', 'err'); }
  };
  $('push').onclick = async () => {
    if (!$('base').value) return setStatus('URL device ?', 'err');
    if ($('json').value.trim() !== model.toJSON().trim()) return setStatus('Modifs JSON non appliquées — clique « Appliquer » d’abord', 'err');
    if (!validate(model.state).valid) return setStatus('Layout invalide', 'err');
    setStatus('Envoi…');
    try { await pushLayout($('base').value, model.toJSON()); setStatus('Poussé et persisté', 'ok'); }
    catch (e) { setStatus('Échec : ' + e.message + ' (CORS ? cf. README)', 'err'); }
  };
}

main();
