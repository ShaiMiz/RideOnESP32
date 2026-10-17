#!/bin/bash
# render.sh OUTDIR : SVG -> PNG via headless Chromium
CH=${CHROME:-chromium}
for f in "$1"/*.svg; do
  read w h <<<$(python3 -c "import re;s=open('$f').read();m=re.search(r'width=\"(\d+)\" height=\"(\d+)\"',s);print(m[1],m[2])")
  $CH --headless --no-sandbox --disable-gpu --hide-scrollbars --screenshot="${f%.svg}.png" --window-size=$w,$((h+200)) "file://$(realpath $f)" 2>/dev/null
  python3 -c "from PIL import Image;im=Image.open('${f%.svg}.png');im.crop((0,0,$w,$h)).save('${f%.svg}.png')"
done
