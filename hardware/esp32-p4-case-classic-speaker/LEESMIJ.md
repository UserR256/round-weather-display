# Klassieke ronde klok met speaker achterin - versie 1 (proefprint)

Staande ronde behuizing op twee pootjes (133 mm doorsnede, 140 mm hoog, 58 mm diep).
De speaker (100 x 45 mm) zit met 4 boutjes op de binnenkant van het achterdeksel;
het deksel is het rooster. Het glas ligt 2 mm verzonken achter een rand van 8,5 mm.

## Onderdelen
| Bestand | Printen |
|---|---|
| shell.stl | voorkant op het bed, geen support (de pootjes printen rechtop mee) |
| cover.stl | buitenkant op het bed |

De voorrand heeft een afschuining van 45 graden zodat hij netjes met de voorkant op het bed print.

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
- 4x M3x8 verzonken + 4 moertjes: speaker op het deksel (gaten 92 x 37 mm, boutkop aan de buitenkant)
- 4x M3x10 verzonken: deksel in de behuizing (in de 6 mm brede richel)
- Foamtape op de 4 pilaartjes (drukken het glas tegen de voorrand)

## Montage
1. Speaker met de conussen tegen het rooster op het deksel boutjes; het randje centreert hem.
2. Scherm van achteren in de behuizing, glas tegen de voorrand.
3. Speakerstekker (PH2.0) en USB-C aansluiten; USB-kabel door de sleuf onder in het deksel.
4. Foamtape op de pilaartjes, deksel erop en vastschroeven.

## Resetgaatje (fabrieksinstellingen)
Achterin zit een gaatje met een geleidebuisje voor een paperclip. Houd daarmee de BOOT-knop van het board 10 seconden ingedrukt terwijl de klok aan staat: alle instellingen worden gewist en de klok start de installatie opnieuw.
Meet de plek van de BOOT-knop (vanaf de voorkant gezien, in mm vanaf het midden) en zet die in boot_button_xy. Zit de knop op de rand van de print in plaats van op de achterkant, laat het me weten: dan komt het gaatje in de zijwand.

## Eerst nameten (bovenaan in de .scad, sectie MEASURE)
glass_d (115), glass_t (6), board_depth (24), board_r (54), spk_depth (20),
spk_driver_d / spk_driver_x (36 / 22). Daarna in OpenSCAD part = shell/cover, F6, Export STL.
Tip: print eerst een dun ringetje van de voorkant om de pasvorm van het glas te testen.
