// Conversion image -> RGB565 pour les fonds de page. Le NAVIGATEUR fait tout le decodage
// (createImageBitmap + canvas) ; le device ne stocke/affiche que du RGB565 deja pret.
// LV_COLOR_16_SWAP=1 sur le device => octets ranges octet-fort-en-premier (SWAP=true).

export const BG_W = 360, BG_H = 360, BG_BYTES = BG_W * BG_H * 2;
export const SWAP = true;   // LV_COLOR_16_SWAP=1 ; bascule a false si Task 14 montre des couleurs fausses

// Rectangle source pour un fit "cover" (remplit dst, crop centre). Retourne {sx,sy,sw,sh} entiers.
export function coverRect(srcW, srcH, dstW, dstH) {
  const targetAspect = dstW / dstH;
  if (srcW / srcH > targetAspect) {           // source trop large -> crop horizontal
    const sw = Math.round(srcH * targetAspect);
    return { sx: Math.round((srcW - sw) / 2), sy: 0, sw, sh: srcH };
  }
  const sh = Math.round(srcW / targetAspect); // source trop haute -> crop vertical
  return { sx: 0, sy: Math.round((srcH - sh) / 2), sw: srcW, sh };
}

// RGBA8888 (Uint8ClampedArray, 4 octets/pixel) -> RGB565 (Uint8Array, 2 octets/pixel).
export function rgba8888ToRgb565(rgba, swap = SWAP) {
  const px = rgba.length >> 2;
  const out = new Uint8Array(px * 2);
  for (let i = 0, o = 0; i < px; i++) {
    const r = rgba[i * 4], g = rgba[i * 4 + 1], b = rgba[i * 4 + 2];
    const v = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
    if (swap) { out[o++] = (v >> 8) & 0xFF; out[o++] = v & 0xFF; }
    else      { out[o++] = v & 0xFF;        out[o++] = (v >> 8) & 0xFF; }
  }
  return out;
}

// RGB565 (Uint8Array) -> RGBA8888 (Uint8ClampedArray), pour reconstruire un apercu.
export function rgb565ToRgba8888(bytes, swap = SWAP) {
  const px = bytes.length >> 1;
  const out = new Uint8ClampedArray(px * 4);
  for (let i = 0, o = 0; i < px; i++) {
    const b0 = bytes[i * 2], b1 = bytes[i * 2 + 1];
    const v = swap ? (b0 << 8) | b1 : (b1 << 8) | b0;
    const r5 = (v >> 11) & 0x1F, g6 = (v >> 5) & 0x3F, b5 = v & 0x1F;
    out[o++] = r5 << 3;
    out[o++] = g6 << 2;
    out[o++] = b5 << 3;
    out[o++] = 255;
  }
  return out;
}

// FNV-1a 64 bits -> 16 hex minuscules. BigInt pour l'exactitude. Cle d'asset = hash du contenu RGB565.
export function fnv1a64Hex(u8) {
  const PRIME = 0x100000001b3n, MASK = 0xFFFFFFFFFFFFFFFFn;
  let h = 0xcbf29ce484222325n;
  for (let i = 0; i < u8.length; i++) {
    h ^= BigInt(u8[i]);
    h = (h * PRIME) & MASK;
  }
  return h.toString(16).padStart(16, '0');
}
