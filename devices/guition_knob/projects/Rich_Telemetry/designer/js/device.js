// Pont REST avec le device. CORS résolu côté firmware (header + OPTIONS).
function clean(base) { return base.replace(/\/+$/, ''); }

export async function loadLayout(base) {
  const r = await fetch(clean(base) + '/layout');
  if (!r.ok) throw new Error('HTTP ' + r.status);
  return r.json();
}

// Renvoie une blob URL (image/bmp) ; l'appelant doit la revoquer (URL.revokeObjectURL) apres usage.
export async function captureScreenshot(base) {
  const r = await fetch(clean(base) + '/screenshot');
  if (!r.ok) throw new Error('HTTP ' + r.status);
  return URL.createObjectURL(await r.blob());
}

// GET /status : santé du device (ip, page, pages, uptime, composants, état des sources pull).
export async function getStatus(base) {
  const r = await fetch(clean(base) + '/status');
  if (!r.ok) throw new Error('HTTP ' + r.status);
  return r.json();
}

// POST /page : navigue la page affichée SUR LE DEVICE. body = {dir:'next'|'prev'} | {index:N} | {name:'…'}.
export async function setDevicePage(base, body) {
  const r = await fetch(clean(base) + '/page', {
    method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body)
  });
  if (!r.ok) throw new Error('HTTP ' + r.status);
  return r.json();   // {page, name}
}

// POST /update : pousse des valeurs (live preview). payload = {id: valeur, …} (cf. format par type).
export async function pushValues(base, payload) {
  const r = await fetch(clean(base) + '/update', {
    method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(payload)
  });
  const body = await r.json().catch(() => ({}));
  if (!r.ok) throw new Error('HTTP ' + r.status);
  return body;   // {ok, updated, unknown}
}

export async function pushLayout(base, layoutText) {
  const r = await fetch(clean(base) + '/layout', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: layoutText
  });
  const body = await r.json().catch(() => ({}));
  if (!r.ok || body.ok === false) throw new Error(body.error || 'HTTP ' + r.status);
  return body;
}
