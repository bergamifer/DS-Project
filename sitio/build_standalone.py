#!/usr/bin/env python3
"""Genera sitio/domosonica.html (standalone, ASCII puro) desde sitio/index.html.

- index.html      = fragmento fuente (sin doctype/head/body)
- domosonica.html = página completa
- img/            = fotos y logos. Desde 2026-11-18 ya no van embebidas en base64
                    adentro del HTML: van como archivos, con variantes AVIF/WebP/JPEG
                    en varios anchos, y la página sólo funciona junto a esta carpeta.
                    Lo que se publica es dist/ entera (ver wrangler.jsonc).

El standalone queda 100%% ASCII (entidades HTML + escapes JS/CSS) para ser
inmune al charset que anuncie el servidor. Correr después de editar index.html:

    python3 sitio/build_standalone.py
"""
import datetime
import json
import os
import re
import shutil

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "index.html")
DST = os.path.join(HERE, "domosonica.html")

DESC = ("Domosonica es una instalación sonora: un campo de ~500 parlantes que "
        "enfoca el sonido en puntos del aire. Un instrumento nuevo, buscando "
        "quién le componga.")

URL = "https://domosonica.com.ar/"
OG_IMG = URL + "og.jpg"
APACHETA = "https://apachetasistemas.com.ar/"

# Datos estructurados: le dicen a Google qué es la página y quién la hizo.
# El @id de Fernando es EL MISMO que usa apachetasistemas.com.ar, así los
# buscadores entienden que la obra y la empresa son de la misma persona.
LD = {
    "@context": "https://schema.org",
    "@graph": [
        {
            "@type": "CreativeWork",
            "@id": URL + "#obra",
            "name": "Domosonica",
            "url": URL,
            "description": DESC,
            "genre": "Instalación sonora",
            "inLanguage": "es-AR",
            "image": OG_IMG,
            "creator": {"@id": APACHETA + "#fernando"},
            "funder": [
                {"@type": "GovernmentOrganization",
                 "name": "Mecenazgo Cultural, Ciudad de Buenos Aires",
                 "url": "https://buenosaires.gob.ar/mecenazgo"},
                {"@type": "Organization", "name": "Balanz Capital",
                 "url": "https://balanz.com"},
            ],
            "sourceOrganization": {"@id": APACHETA + "#organizacion"},
        },
        {
            "@type": "Person",
            "@id": APACHETA + "#fernando",
            "name": "Fernando Bergami",
            "url": APACHETA,
            "worksFor": {"@type": "Organization",
                         "@id": APACHETA + "#organizacion",
                         "name": "Apacheta Sistemas", "url": APACHETA},
            "sameAs": ["https://www.linkedin.com/in/bergamifer/",
                       "https://github.com/bergamifer"],
        },
    ],
}

frag = open(SRC, encoding="utf-8").read()

# --- extraer <title> del fragmento ---
m = re.match(r"\s*<title>(.*?)</title>\s*", frag, re.S)
title = m.group(1).strip()
body = frag[m.end():]

head = f"""<!doctype html>
<html lang="es">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<title>{title}</title>
<meta name="description" content="{DESC}">
<meta name="author" content="Fernando Bergami">
<meta name="theme-color" content="#0a0d12">
<link rel="icon" href="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 100'%3E%3Ctext y='.9em' font-size='90'%3E%F0%9F%94%8A%3C/text%3E%3C/svg%3E">
<link rel="canonical" href="{URL}">
<meta name="robots" content="index, follow, max-image-preview:large">
<meta property="og:type" content="website">
<meta property="og:locale" content="es_AR">
<meta property="og:site_name" content="Domosonica">
<meta property="og:url" content="{URL}">
<meta property="og:title" content="{title}">
<meta property="og:description" content="{DESC}">
<meta property="og:image" content="{OG_IMG}">
<meta property="og:image:width" content="1200">
<meta property="og:image:height" content="630">
<meta property="og:image:alt" content="Simulación de interferencia de Domosonica: una fila de parlantes concentra el sonido en un punto">
<meta name="twitter:card" content="summary_large_image">
<script type="application/ld+json">
{json.dumps(LD, ensure_ascii=False, indent=1)}
</script>
<style>
  *,*::before,*::after{{box-sizing:border-box}}
  html,body{{margin:0;padding:0;background:#0a0d12}}
  img{{max-width:100%}}
</style>
"""

out = head + body + "\n</body>\n</html>\n"

# --- ASCII-ficar: entidades en HTML, \\uXXXX en JS, \\XXXXXX en CSS ---
esc_html = lambda t: "".join(c if ord(c) < 128 else "&#%d;" % ord(c) for c in t)
esc_js = lambda t: "".join(c if ord(c) < 128 else "\\u%04x" % ord(c) for c in t)
esc_css = lambda t: "".join(c if ord(c) < 128 else "\\%06x" % ord(c) for c in t)

pat = re.compile(r"(<style\b[^>]*>)(.*?)(</style>)|(<script\b[^>]*>)(.*?)(</script>)",
                 re.S | re.I)
parts, last = [], 0
for mm in pat.finditer(out):
    parts.append(esc_html(out[last:mm.start()]))
    if mm.group(1) is not None:
        parts.append(mm.group(1) + esc_css(mm.group(2)) + mm.group(3))
    else:
        parts.append(mm.group(4) + esc_js(mm.group(5)) + mm.group(6))
    last = mm.end()
parts.append(esc_html(out[last:]))
res = "".join(parts)

assert res.isascii(), "quedaron bytes no-ASCII"
open(DST, "w").write(res)
print(f"{DST}: {len(res)} bytes, ASCII puro OK")

# --- carpeta dist/ que publica Cloudflare (ver wrangler.jsonc) ---
# Además de la página, los buscadores piden robots.txt y sitemap.xml, y las
# redes sociales piden la imagen de vista previa (og.jpg).
DIST = os.path.join(HERE, "..", "dist")
os.makedirs(DIST, exist_ok=True)
open(os.path.join(DIST, "index.html"), "w").write(res)
shutil.copyfile(os.path.join(HERE, "og.jpg"), os.path.join(DIST, "og.jpg"))
# Fotos y logos: cada foto está en tres anchos (480, 800 y el original) y tres
# formatos (AVIF, WebP, JPEG). El navegador elige el más liviano que entiende y
# que alcanza para su pantalla; un celular baja la de 480 px en AVIF, no la grande.
shutil.copytree(os.path.join(HERE, "img"), os.path.join(DIST, "img"), dirs_exist_ok=True)
# Página de error propia: Cloudflare la sirve con código 404 para cualquier
# dirección que no exista (ver not_found_handling en wrangler.jsonc). Se
# ASCII-fica igual que la portada, y como no tiene <script> alcanza con las
# entidades HTML afuera del <style> (el CSS de la 404 es ASCII puro).
p404 = esc_html(open(os.path.join(HERE, "404.html"), encoding="utf-8").read())
assert p404.isascii()
open(os.path.join(DIST, "404.html"), "w").write(p404)
open(os.path.join(DIST, "robots.txt"), "w").write(
    f"User-agent: *\nAllow: /\nSitemap: {URL}sitemap.xml\n")
hoy = datetime.date.today().isoformat()
open(os.path.join(DIST, "sitemap.xml"), "w").write(
    '<?xml version="1.0" encoding="UTF-8"?>\n'
    '<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">\n'
    f"  <url><loc>{URL}</loc><lastmod>{hoy}</lastmod></url>\n"
    "</urlset>\n")
open(os.path.join(DIST, "_headers"), "w").write(
    "/*\n"
    "  X-Content-Type-Options: nosniff\n"
    "  Referrer-Policy: strict-origin-when-cross-origin\n"
    # HSTS: el navegador recuerda por un año "este sitio va siempre por https"
    # y deja de intentar http. Sólo vale si llega por https, así que funciona
    # junto con "Always Use HTTPS" de Cloudflare. Sin includeSubDomains a
    # propósito: no comprometer subdominios que todavía no existen.
    "  Strict-Transport-Security: max-age=31536000\n"
    # Nadie puede meter el sitio dentro de un iframe ajeno (clickjacking).
    "  X-Frame-Options: DENY\n"
    # El sitio no usa cámara, micrófono ni ubicación: se declaran apagados.
    "  Permissions-Policy: camera=(), microphone=(), geolocation=()\n"
    "/og.jpg\n"
    "  Cache-Control: public, max-age=604800\n"
    # Las imágenes casi no cambian: el navegador las guarda 30 días y no las
    # vuelve a pedir. Si se cambia una, conviene cambiarle también el nombre.
    "/img/*\n"
    "  Cache-Control: public, max-age=2592000\n")
print(f"dist/: index.html, 404.html, og.jpg, img/, robots.txt, sitemap.xml, _headers")
