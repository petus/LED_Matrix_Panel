# LED Matrix Panel

*[English](README.en.md) · Česky*

Hodiny, letadla v okolí a srážkový radar ČHMÚ na
**RGB LED matici Waveshare 96×48, P2.5 (HUB75)** řízené deskou
**LaskaKit ESP32-S3-DEVKit**. Vše se nastavuje přes webovou stránku
a ukládá do flash paměti ESP32.

## Co zobrazuje

**Obrazovky:** hodiny (ručičkové s vyhlazenými ručičkami nebo digitální), předpověď (teď · dnes ·
zítra), letadlo (natočená ikona, volací znak, typ, rychlost, výška, vzdálenost, „+N“ pro další
letadla v okruhu), radar (animovaná mapa s malými hodinami).

**Režimy obrazovek** (volí se na webu, pamatují si je i po restartu):

| Režim | Chování |
|-------|---------|
| Automatický | hodiny, každých 50 s na 10 s předpověď (nastavitelné); déšť v okruhu → místo nich radar; letadlo *ihned* (má přednost), *občas* (např. 15 s každých 60 s, dokud je v okolí) nebo *nikdy* |
| Střídání | vybrané obrazovky postupně, každá N sekund; letadlo se přeskočí, když žádné neletí, radar se přeskočí, když v okolí neprší (pokud není zapnuto „radar i bez deště“) |
| Pevný | jen hodiny, předpověď, letadlo nebo radar |

Letadlo: vždy to nejbližší. Při nastaveném směru výhledu se letadlo v této výseči
(směr ± šířka, vlastní dosah) zobrazí i za hranicí okruhu a má přednost.
Noc (volitelně): ztlumení, jen hodiny, nebo vypnutý displej (vypnutí platí ve všech režimech).

Na mapě radaru jsou města modré křížky z 5 LED (Praha, Plzeň, Liberec, Ústí n. L., Hradec Králové,
Jihlava, České Budějovice, Brno, Olomouc, Ostrava). Vaše poloha je zelená LED. Když padne
na město (nebo o jeden pixel vedle), má křížek zelený střed a modrá ramena.

Zdroj radaru (web, záložka Radar):

| Zdroj | Co to je |
|-------|----------|
| ČHMÚ | kompozit MAX Z (`png`), všechny odrazy, které radary vidí, nový snímek každých 5 min |
| ČHMÚ masked | stejný kompozit ze složky `png_masked`: jen srážky, které dopadnou na zem (bez virgy a kovadlin vzdálených bouřek) |
| RainViewer | mezinárodní kompozit, dlaždice Web Mercator (zoom 6, 2×2 dlaždice po 256 px), nový snímek každých 10 min, hrubší |

Všechny zdroje končí ve stejné 4bitové mřížce s úrovněmi ČHMÚ (4 až 60 dBZ), takže mapa, legenda
i práh deště fungují stejně. Snímek, který panel už má, se znovu nestahuje.
U kompozitu masked se počítají jen úplně neprůhledné body (poloprůhledné jsou odrazy, které
nedopadnou na zem).

Jazyk: čeština nebo angličtina pro texty na panelu i webovou stránku (Systém → Jazyk / Language).

## Hotový firmware

Arduino IDE není potřeba: soubor ze složky `bin` nahrajte od adresy **0x0**,
např. v Chromu / Edge přes [esp32flasher.chiptron.cz](https://esp32flasher.chiptron.cz).

| Soubor | Použití |
|--------|---------|
| `LED_Matrix_Panel_CZ.merged.bin` | první instalace, panel i web **česky** |
| `LED_Matrix_Panel_EN.merged.bin` | první instalace, panel i web **anglicky** |
| `LED_Matrix_Panel.bin` | další aktualizace přes WiFi (*Systém → Aktualizace firmwaru*), nastavení zůstane |

Jazyk souboru určuje jen výchozí nastavení. Deska, která už má nastavení uložené, si svůj
jazyk ponechá. Pro výchozí jazyk před nahráním smažte flash (nebo použijte *Tovární nastavení*),
případně jazyk jednoduše přepněte na webu (Systém → Jazyk / Language).

## První spuštění

1. Panel vytvoří otevřenou WiFi síť **`LED-Matrix-XXXX`** a ukáže QR kód pro připojení.
2. Připojte se k ní telefonem, stránka s nastavením se otevře sama (nebo zadejte `http://192.168.4.1`).
3. Na záložce **WiFi** vyberte domácí síť a zadejte heslo.
4. Nastavovací síť se zavře, panel se připojí k domácí WiFi a na 10 s ukáže svou
   adresu (při každém startu), např. `192.168.1.39` a `ledmatrix.local`.
   Pak se objeví hodiny. Telefon se vrátí ke své obvyklé síti.

Pokud se k domácí WiFi nepodaří připojit do 20 s (špatné heslo, vypnutý router), nastavovací
síť se vrátí. S uloženou sítí se každých 5 minut zavře a zkusí to znovu,
ale jen když k ní není připojený žádný telefon.

Rádio je vždy buď stanice, nebo přístupový bod, nikdy obojí. Ve smíšeném režimu
AP+STA stanice prohledává jiné kanály a telefony v nastavovací síti
neustále ztrácejí spojení.

## Webové rozhraní (česky / anglicky)

| Záložka | Nastavení |
|---------|-----------|
| Stav | čas, aktuální obrazovka, letadlo, radar, počasí, paměť; režim obrazovek (automatický / střídání / hodiny / letadlo / radar / předpověď) |
| WiFi | vyhledání sítí, připojení, zapomenutí |
| Poloha | vyhledání obce (geokódování Open-Meteo, běží v prohlížeči), zeměpisná šířka / délka |
| Letadla | zapnuto/vypnuto, okruh, minimální výška, jen s volacím znakem, interval dotazů, minimální doba zobrazení; směr výhledu (8 směrů), ± šířka, vlastní dosah, s malou mapkou výseče |
| Radar | zapnuto/vypnuto, zdroj (ČHMÚ / ČHMÚ masked / RainViewer), okruh deště, práh dBZ, počet snímků, rychlost animace, zdržení posledního snímku, legenda, města, podklad mapy |
| Hodiny a počasí | ručičkové / digitální, vteřiny, datum; barvy ručiček, vteřinovky, číslic, značek ciferníku, dne v týdnu + měsíce, dne + roku; předpověď zapnuto/vypnuto a časování |
| Displej | automatický režim (letadlo ihned / občas / nikdy), střídání (obrazovky, interval, radar bez deště); jas ve dne / v noci, noc podle času nebo od západu do východu slunce, ztlumit nebo vypnout, jen hodiny |
| Systém | jazyk, název zařízení, časové pásmo, záloha / obnova (JSON), aktualizace firmwaru, restart, tovární nastavení |

Nastavení se ukládá do NVS (Preferences) a přežije aktualizaci firmwaru. Hodnoty `DEFAULT_*`
v `config.h` se použijí jen při úplně prvním startu nebo po továrním nastavení.

JSON API (používá ho stránka, hodí se pro skripty):
`GET /api/status`, `GET|POST /api/config`, `GET /api/scan`, `POST /api/wifi`,
`POST /api/wifi/forget`, `POST /api/mode`, `GET /api/export`, `POST /api/import`,
`POST /api/reboot`, `POST /api/reset`, `POST /update` (firmware .bin).

## Hardware

| Díl | Poznámka |
|-----|----------|
| LaskaKit ESP32-S3-DEVKit | s PSRAM i bez ní |
| Waveshare RGB-Matrix-P2.5-96x48-F | HUB75, skenování 1/24, V2 = řádkový budič SM5368 |
| Zdroj 5 V, 4 A nebo víc | panel se **ne**napájí z ESP32 |

## Zapojení (14 GPIO)

Na panelu 96x48-F **V2** řídí řádky posuvný registr SM5368 jen přes A, B, C.
Piny D a E panel nepoužívá (jejich zapojení ničemu nevadí).

| Pin HUB75 | Signál | GPIO ESP32-S3 | Lišta DEVKitu |
|-----------|--------|---------------|---------------|
| 1  | R1  | 4  | levá  |
| 2  | G1  | 5  | levá  |
| 3  | B1  | 6  | levá  |
| 5  | R2  | 7  | levá  |
| 6  | G2  | 15 | levá  |
| 7  | B2  | 16 | levá  |
| 9  | A   | 17 | levá  |
| 10 | B   | 18 | levá  |
| 11 | C   | 8  | levá  |
| 12 | D   | 41 | pravá |
| 8  | E   | 21 | pravá |
| 14 | LAT | 40 | pravá |
| 15 | OE  | 39 | pravá |
| 13 | CLK | 38 | pravá |
| 4, 16 | GND | GND | obě |

**GND ESP32 vždy propojte s GND zdroje panelu.**
Piny jde změnit v `config.h`.

**Vodiče mezi ESP32 a panelem držte co nejkratší.** Panel se budí rychlými signály po 14 vodičích.
U dlouhých vodičů (volné drátky, desítky centimetrů) fungují jako anténa: mohou rušit WiFi
(pomalá webová stránka, timeouty, telefon padá z nastavovací sítě) a LED mohou problikávat.
Firmware proto budí panel na 8 MHz a se sníženou silou budičů pinů
(`PANEL_I2S_MHZ` a `PANEL_DRIVE_STRENGTH` v `config.h`). Pokud potíže přetrvávají, vodiče zkraťte,
veďte je dál od antény ESP32 a použijte plochý kabel se zemí.

Záměrně vynechané: 0, 3, 45, 46 (strapping), 45 (RGB LED), 19/20 (USB),
43/44 (UART), 2/42 (µŠup I²C), 47 (napájení µŠup), 9 (ADC baterie), 35 až 37 (octal PSRAM).

## Software

1. Arduino IDE 2.x + balíček desek **esp32 by Espressif Systems** (3.x)
2. Library Manager:
   - **ESP32 HUB75 LED MATRIX PANEL DMA Display** (od mrcodetastic, **v3.1.0 nebo novější**, podpora SM5368),
     Adafruit GFX se nainstaluje jako závislost
   - **ArduinoJson** (od Benoit Blanchon, v7): data o letadlech, webové API
3. Deska **ESP32S3 Dev Module**, *Tools*:
   - **PSRAM** podle modulu: *OPI* (…R8), *QSPI* (…R2), *Disabled*, když žádnou nemá
   - **Flash Size: 16MB (128Mb)**
   - **Partition Scheme: 16M Flash (3MB APP/9.9MB FATFS)**: dva sloty pro aplikaci, aby fungovala aktualizace přes web
   PNGdec (radarové snímky) z Library Manageru **není** potřeba: verze 1.1.7 je přibalená ve složce
   `src/PNGdec` s větší vyrovnávací pamětí řádku (5508 B). Snímky ČHMÚ mají šířku 680 px, masked
   jsou RGBA a do výchozí velikosti se nevejde ani jedno. Nainstalovaná knihovna PNGdec se nepoužije.
4. Žádný `build_opt.h`, knihovny běží s výchozím nastavením. Pokud ve složce sketche
   zůstal `build_opt.h` ze starší verze projektu, smažte ho **i** cache překladu
   (složka `arduino/sketches` v systémové složce temp, ve Windows `%LOCALAPPDATA%\Temp`).
5. Nahrávejte přes USB-C konektor označený **UART**
   (kvůli Serial Monitoru na tomto portu nechte *USB CDC On Boot: Disabled*)

Další aktualizace mohou jít přes webovou stránku: v Arduino IDE *Sketch → Export Compiled Binary*,
pak *Systém → Aktualizace firmwaru*.

## Struktura projektu

```
LED_Matrix_Panel.ino  jen setup() / loop()
config.h          piny, typ panelu, výchozí hodnoty, limity, přepínače DEBUG
settings.h/.cpp   uživatelské nastavení ve flash (NVS) + JSON
wifi_setup.h/.cpp domácí WiFi, nastavovací přístupový bod, captive portal, mDNS
web.h/.cpp        webový server, JSON API, aktualizace firmwaru
web_page.h        stránka s nastavením (HTML/CSS/JS v jednom řetězci)

station.h/.cpp    která obrazovka se ukáže, noční režim, jas
screens.h/.cpp    hodiny, letadlo, radar, nastavovací obrazovka a obrazovka s IP
sky_data.h/.cpp   data sdílená síťovou úlohou a displejem (mutex)
fetcher.h/.cpp    síťová úloha na pozadí na jádře 0
net.h/.cpp        pomocné funkce pro HTTPS (streamování, rostoucí buffery)
time_sync.h/.cpp  čas z HTTP hlavičky „Date“
sun.h/.cpp        východ / západ slunce (NOAA)
adsb.h/.cpp       nejbližší letadlo z adsb.fi + počet v okruhu
chmu.h/.cpp       radar ČHMÚ (obyčejný / masked): výpis adresáře, stažení PNG, dekódování
rainviewer.h/.cpp radar RainViewer: index JSON, dlaždice, převod barev na dBZ
png_stream.h      plní PNGdec přímo z HTTPS proudu (posuvné okno)
src/PNGdec/       PNGdec 1.1.7 (bitbank2, Apache 2.0), přibalený s větší pamětí řádku
forecast.h/.cpp   Open-Meteo: počasí teď, dnes, zítra
weather_icons.h/.cpp  ikony počasí 16x16 kreslené primitivy GFX
radar_palette.h   barevná stupnice ČHMÚ, 4bitová mřížka radaru (2304 B / snímek)
radar_map.h/.cpp  mapa Česka 96x48 (Natural Earth 1:10m, Web Mercator) + projekce
map_view.h/.cpp   vrstvy mapy: země, hranice, města, domov, legenda, déšť
qrcode.h/.c       generátor QR kódu (ricmoo/QRCode, MIT)

display.h/.cpp    inicializace panelu, kreslicí plátno, přenos změněných pixelů, jas
colors.h          pomocné funkce pro RGB565
debug.h           makra DEBUG_LOG()
console.h/.cpp    testovací příkazy přes sériovou linku
```

Jak to funguje:
- **Hlavní smyčka** (jádro 1): stavový automat WiFi, webový server, výběr obrazovky a kreslení. Obrazovka
  se překreslí, jen když se změní obsah (tik hodin, snímek radaru, nová data). Kreslí se
  do plátna v RAM a do DMA bufferu panelu se zapíšou jen pixely, které se liší.
- **Síťová úloha** (jádro 0): ptá se adsb.fi každých pár sekund, ČHMÚ každých 5 minut a Open-Meteo každých 15 minut,
  výsledky publikuje do `SkyData`. Stahování nikdy neblokuje displej ani webovou stránku.
  Mezi dvěma snímky radaru se znovu zeptá na letadla, takže jsou aktuální i během stahování radaru.
  Úsporný režim WiFi je vypnutý (velká stahování zpomaloval na pár kB/s).
- **Radar**: názvy souborů ČHMÚ vyplývají z času (snímek každých 5 minut), takže se panel ptá na
  nejnovější možný snímek a jde zpět, dokud nějaký nenajde. Výpis adresáře (přes 300 kB) se čte jen
  jako záloha (hodiny ještě nejdou, nečekané názvy). Všechny snímky ČHMÚ i všechny dlaždice jednoho
  snímku RainVieweru jdou přes jedno udržované HTTPS spojení. Každé PNG se dekóduje po řádcích *už během stahování* (posuvné okno 4 kB,
  žádný buffer pro celý soubor) rovnou do mřížky 96×48. Každý
  pixel panelu si ponechá nejsilnější ozvěnu z oblasti, kterou pokrývá. Na nejnovějším snímku
  se navíc kontroluje déšť v okruhu.
- **Čas** se bere z hlavičky `Date` těchto odpovědí (bez NTP). Když jsou letadla,
  radar i předpověď vypnuté, kvůli času se každých 30 minut pošle malý dotaz.

## Ladění

```cpp
#define DEBUG_ENABLED  1   // 0 = no serial output, no debug code in firmware
#define DEBUG_CONSOLE  1   // single-key commands in the Serial Monitor
```

Konzole (115200 Bd): `m` režim obrazovky AUTO/CLOCK/PLANE/RADAR, `r` stáhnout radar hned,
`+`/`-` jas, `i` informace (čas, WiFi, letadlo, radar, paměť), `h` nápověda.

## Řešení problémů

| Příznak | Řešení |
|---------|--------|
| Obraz posunutý o 1 px, nečitelný text | přepněte `PANEL_CLK_PHASE` (Waveshare = `false`) |
| Úvodní pruhy R, G, B mají jiné barvy | `PANEL_SWAP_RB` v `config.h` prohodí červenou a modrou (Waveshare 96x48-F V2 = `1`) |
| Špatné barvy (bílé ručičky vypadají modře, chybí tmavé barvy) | zapomenutý `build_opt.h` ze starší verze, smažte ho i cache překladu, viz *Software* krok 4 |
| Svítí celé řádky / pruhy | `PANEL_LINE_DECODER` (96x48-F V2 = `SM5368`, V1 = `TYPE138`) |
| V logu při startu „PSRAM chip is not connected“ | nastavte *Tools → PSRAM* podle modulu (nebo Disabled) |
| Sousední LED pobliká / svítí | zkontrolujte GND panelu, krátký plochý kabel a zdroj 5 V; dlouhé vodiče to zhoršují (viz *Zapojení*) |
| Pomalá webová stránka, timeouty, telefon padá z nastavovací sítě | rušení z vodičů panelu: zkraťte je, veďte je dál od antény; `PANEL_DRIVE_STRENGTH` / `PANEL_I2S_MHZ` v `config.h` (klávesa `d` v konzoli zkouší sílu budičů za běhu) |
| `HTTP -1 connection refused` u každého dotazu | obvykle málo RAM pro TLS; řádek v logu ukazuje největší volný blok (TLS potřebuje dva bloky po ~17 kB); méně snímků radaru |
| Webová stránka nejde otevřít | zkontrolujte adresu zobrazenou po startu; `http://ledmatrix.local` potřebuje mDNS (funguje na většině telefonů a počítačů) |
| ESP32 se restartuje při vysokém jasu | silnější zdroj 5 V, nižší jas ve webovém rozhraní |

## Data a licence

- Letadla: otevřená data [adsb.fi](https://adsb.fi) (nekomerční použití)
- Radar: [otevřená data ČHMÚ](https://opendata.chmi.cz) (kompozit MAX Z, `png` / `png_masked`)
- Radar: [RainViewer](https://www.rainviewer.com/api.html) (osobní / nekomerční použití, max. 100 dotazů za minutu)
- Předpověď: [Open-Meteo](https://open-meteo.com) (CC BY 4.0, zdarma pro nekomerční použití)
- Mapa: [Natural Earth](https://www.naturalearthdata.com) (public domain)
- Vyhledání obce: [geokódování Open-Meteo](https://open-meteo.com) (v prohlížeči)
- QR kód: ricmoo/QRCode (MIT)
- Dekodér PNG: [PNGdec](https://github.com/bitbank2/PNGdec) 1.1.7 od Larry Banka (Apache 2.0), přibalený ve `src/PNGdec`
