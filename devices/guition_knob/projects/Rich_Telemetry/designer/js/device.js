// Pont REST avec le device. CORS résolu côté firmware (header + OPTIONS).
function clean(base) { return base.replace(/\/+$/, ''); }

export async function loadLayout(base) {
  const r = await fetch(clean(base) + '/layout');
  if (!r.ok) throw new Error('HTTP ' + r.status);
  return r.json();
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
