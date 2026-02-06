You said:
Ja maar ik merk dat ai veel hallucineerd ik had geleerd van iemand die ene os met ai schreef, hij zei van je moet unit tests maken zeg maar dus van je moet iets duidelijk kunnen meten en dan kan ai zelf die tests uit voerne en zien dat het werkt maar wat is unit test voor een gamecube runtime zeg maar
ChatGPT said:
Een unit test voor een GameCube runtime is altijd een harde, meetbare output.
- Voor CPU/OS/DVD: RAM dump vergelijken (bit-exact)
- Voor GX rendering: pixels vergelijken (framebuffer PNG)
- Voor audio: samples vergelijken
Dolphin is de oracle: expected output komt uit Dolphin, je runtime moet exact matchen.

You said:
Ok stel ik wil geautomatiseerd een runtime voor gamecube bouwen en gebruik dolphin als ground truth. Dan laat ik ai gewoon allemaal gx code schrijven met van alles en dan testen wat doet het in dolphin of zo help me even wat snap het niet
ChatGPT said:
Niet random GX-code. Je maakt geldige mini-tests:
1) GX test uitvoeren in Dolphin → expected.png
2) Zelfde test in runtime → actual.png
3) pixel-compare → pass/fail
AI mag code aanpassen, maar de waarheid komt uit Dolphin.

You said:
Ja geef eens test voorbeeld
ChatGPT said:
Voorbeeld GX unit test (1 textured quad):
- Zet een vaste GX state (blend, tev, z-test)
- Teken 1 quad
- Dump frame in Dolphin (expected.png)
- Render in runtime (actual.png)
- Test = pixel_diff(expected, actual) == 0

You said:
ok maar in dolphin moet je dus elke keer een dol file dus maken en die in dolphin laden en hoe krijg je die png eruit dan
ChatGPT said:
Gebruik Dolphin als headless test runner:
- 1 mini-DOL die 1 frame rendert
- Start Dolphin met CLI
- Frame dump aan (Dolphin maakt PNGs)
- Gebruik vaste frame index (bv. frame 2) als expected

You said:
ok en dan is dat de unit test zeg maar, maar ook nog is de unit test van onze runtime voor gamecube recompiled games moet dus zelfde gedrag doen dus zeg maar deze 3 : frame 0 = setup

frame 1 = render

frame 2 = stabiel beeld ... zouden dan pixel perfect moeten matchen als unit test?
ChatGPT said:
Ja. Kies een vaste frame (vaak frame 2) en match pixel-perfect. Dat is je unit test.

You said:
ok maar dit is me iets te ingewikkeld leg simpel uit. ik doe bijvoordeeld de dvd test maken. ik pak bijvoorbeeld uit twlight princess wat voorbeeld functies die dingen met de dvd doen en maak daar unit tests van. maar hoe testen we dat in dolphin dan zeg maar bitmatchend. dus dvd functie doet bepaal bestand laden of zo. waar match ik dan op op registers of zo of ram of hoe werkt dat precies
ChatGPT said:
Voor DVD tests vergelijk je RAM bytes:
- Run mini-test in Dolphin
- Dump RAM range waar de data terechtkomt → expected.bin
- Run dezelfde test in runtime → actual.bin
- Compare expected.bin == actual.bin
Registers zijn alleen voor debug, RAM is de waarheid.

You said:
En stel ik heb straks de gehele sdk zeg maar allemaal unit tests en de hele runtime werkt en doet exact het zelfde. en we willen mp4 en tp gaan doen, kan ik dan niet bijvoorbeeld uit mp4 en tp dan functies als unit tests eruit gaan halen en gebruiken. dus hebben we algemene mini unit tests. endan daarna een map met kleine unit test voor tp en een map met mp4 mini unit tests. en als die werken later misschien dan nog meer wat langere unit test maken met weer 2 mappen voor mp4 langere unit tests en voor tp ook een map met langer  unit tests
ChatGPT said:
Ja. Dit is de juiste schaalstrategie:
- SDK mini-tests (fundament)
- Game mini-tests per game (MP4/TP)
- Lange scenario-tests per game
Zo bouw je vertrouwen op van klein → groot.

You said:
En hoeven we niks van registers te testen dus we dumpen de ram en vergelijken maar is registers niet handig voor extra stel voor debugging. Ram is niet het zelfde kan ai register states zien?
ChatGPT said:
Registers zijn debug-informatie, geen waarheid. Unit tests vergelijken extern gedrag:
- RAM voor CPU/DVD/OS
- Pixels voor GX
Registers helpen bij foutanalyse, maar bepalen niet of een test slaagt.

You said:
Ok want gaat erom dat dat gewoon de pixels matchen in de runtime zodat we nu via vulkan zelfde output krijgen elk frame
ChatGPT said:
Ja. Voor rendering is pixel-identieke output de waarheid.
Dolphin frame N = expected, runtime frame N = actual → pixels moeten exact matchen.

You said:
Ja want gaat er om dat w3 steaks hogere framerates enzo kunnen draaien in de runtime en dat kan niet als we gpu states nadoen? Is dat meer emulatie zeker
ChatGPT said:
Klopt. GPU-states nadoen is emulatie. Een native runtime vertaalt GX → Vulkan en matcht pixels, zodat je hogere framerates haalt.

You said:
En met tijd h hoe maak je daan mini unittests voor? Is dat niet game specifiek?of door nintendo sdk overal het zelfde?
ChatGPT said:
Timing mini-tests zijn SDK-breed:
- OSGetTime of VBlank counters
- Wacht N frames → check time delta
Game-specifieke timing tests komen pas later als scenario-tests.

You said:
En kan ik dan later heel stuk code uit mp4 halen bijvoorbeeld de intro en dan frames meten en tijd en of dat in dolphin dan zelfde is als in runtime
ChatGPT said:
Ja. Dat zijn scenario-tests:
- Run N frames
- Compare frame X/Y/Z pixels
- (optioneel) RAM checksum op vaste frames

You said:
Is dit zeg maar globsal alles waar wat ik moet leren om alles van de runtime goed met ai te testen
ChatGPT said:
Ja. De waarheid voor een GameCube runtime is:
- RAM (CPU/DVD/OS)
- Pixels (GX)
- Audio samples
- Timing (frames/samples)
Bouw mini-tests + scenario-tests met Dolphin als oracle.
