"""NoiseTrack: gera pagina_html.h a partir de pagina.html.

Rode depois de editar pagina.html (na pasta firmware/noisetrack):
    C:\\.platformio\\penv\\Scripts\\python.exe gerar_pagina_h.py

O firmware compila só o pagina_html.h; o pagina.html é a fonte, e também pode
ser aberto direto no navegador do PC (modo demonstração).
"""
from pathlib import Path

PASTA = Path(__file__).resolve().parent
ORIGEM = PASTA / "pagina.html"
DESTINO = PASTA / "pagina_html.h"
DELIMITADOR = "PAGINA_NT"   # o texto ")PAGINA_NT\"" não pode aparecer no HTML

html = ORIGEM.read_text(encoding="utf-8")
if f'){DELIMITADOR}"' in html:
    raise SystemExit(f"pagina.html contem ){DELIMITADOR}\" e quebraria o arquivo gerado")

conteudo = (
    "// NoiseTrack: página do ESP32 (Parte 10).\n"
    "// GERADO por gerar_pagina_h.py a partir de pagina.html. Não editar à mão:\n"
    "// edite pagina.html e rode o script de novo.\n"
    "#pragma once\n"
    "#include <Arduino.h>\n"
    "\n"
    f'static const char PAGINA_HTML[] PROGMEM = R"{DELIMITADOR}({html}){DELIMITADOR}";\n'
)
DESTINO.write_text(conteudo, encoding="utf-8", newline="\n")
print(f"{DESTINO.name}: {len(html.encode('utf-8'))} bytes de HTML")
