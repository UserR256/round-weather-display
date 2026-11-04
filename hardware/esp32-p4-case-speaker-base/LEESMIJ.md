# Klokbehuizing met speakervoet - versie 1 (proefprint)

Voor: Waveshare ESP32-P4-WIFI6-Touch-LCD-3.4C + dubbele speaker 100 x 45 mm.
De klok staat 15 graden achterover op een voet met de speaker erin; het rooster zit aan de voorkant.

## Onderdelen (STL)
| Bestand | Printen | Formaat |
|---|---|---|
| shell.stl  | voorkant (scherm-opening) op het bed, geen support | 123 x 131 x 35 mm |
| cover.stl  | buitenkant op het bed, pilaartjes naar boven | 119 x 119 x 26 mm |
| base.stl   | bovenkant op het bed (open onderkant naar boven) | 109 x 48 x 54 mm |
| bottom.stl | plat | 106 x 45 x 3 mm |


## Materiaal en printinstellingen: PETG (aanbevolen)
PETG kan beter tegen warmte (tot ongeveer 75 graden) dan PLA (ongeveer 55 graden), is taaier en houdt zelftappende schroeven beter vast. Mat PETG oogt het mooist.

| Instelling | Waarde |
|---|---|
| Nozzle | 235-245 graden |
| Bed | 75-85 graden (PEI: lijmstift of release-middel, PETG plakt soms te goed) |
| Laaghoogte | 0,2 mm |
| Wanden | 4 (stevige schroefgaten) |
| Infill | 20%, gyroid |
| Koelventilator | 30-50% |
| Support | geen |
| Snelheid | buitenwand max. 60 mm/s voor een strak oppervlak |

Tegen draadjes (stringing) in de roosters: filament droog houden, retractie iets verhogen.
Houtlook gewenst? Hout-PLA kan, maar vervormt al rond 55 graden: niet op een zonnige vensterbank.

## Schroeven
- 4x M3x10 verzonken: achterdeksel op de klok
- 4x M3x8: speaker op de voorwand van de voet (gaten 92 x 37 mm)
- 2x M3x10: voet aan de klok (van binnenuit de voet omhoog)
- 4x M3x8 verzonken: bodemplaat
- 4 rubber voetjes 10 mm (verdiepingen in de bodemplaat)
- Een paar stukjes foamtape: tussen de pilaartjes van het deksel en de rand van het glas
Gaten zijn 2,5 mm voor zelftappende M3-schroeven (zet screw_pilot_d = 4.0 voor inzetmoeren).

## Montage
1. Speaker met de conussen tegen het rooster in de voet schroeven.
2. USB-C-kabel door de sleuf achter in de voet, door het gat in de bovenkant omhoog de klok in; speakerdraadjes ook.
3. Scherm van achteren in de klok, glas tegen de voorrand.
4. USB-C en speakerstekker (PH2.0) aansluiten, foamtape op de pilaartjes, deksel erop.
5. Klok op de voet zetten en met 2 schroeven van binnenuit vastzetten; bodemplaat erop.

## EERST NAMETEN (staat bovenaan in de .scad, sectie MEASURE)
De online specificaties geven alleen de buitenmaat (115 mm) en het beeldvlak (87,6 mm). Controleer:
1. glass_d   - diameter van het glas (nu 115,0)
2. glass_t   - dikte glas + LCD aan de rand (nu 6,0)
3. board_depth - hoe ver de print + connectoren (USB-A!) achter het scherm uitsteken (nu 24)
4. board_r   - grootste straal van de print achter het scherm; de 3 pilaartjes drukken daarbuiten op de glasrand (nu 54)
5. spk_depth - hoe diep de speaker achter zijn plaat is (nu 20)
6. spk_driver_d / spk_driver_x - diameter en hartafstand van de twee conussen (nu 36 / 22)
7. Of de USB-C-poort bereikbaar is met een haakse stekker binnen board_depth

Wijzig de waarden in OpenSCAD (Window > Customizer), kies part = shell/cover/base/bottom, F6 en Export STL.
Tip: print eerst alleen een dunne ring van de voorkant om de pasvorm van het glas te testen.
