Pocti obäzateljno v igre dolžny bytj derevja.

### Trebovanija k rasstanovke derevjev

Pri generaçii mira derevja nado kak-to rasstavlätj.
I eta rasstonovka dolžna bytj determenirovannoj, daby smežnyje canki, generirujemyje otdeljno, ne soderžali "upolovinennyh" derevjev.
Rasstonovka dolžna bytj takže otnositeljno ravnomernoj - ctoby derevja ne stojali sloškom blizko ili sliškom daleko drug ot druga, no pri etom i ne regulärnoj (kvadratno-gnezdovym sposobom).

Želateljno imetj vozmožnostj rasstavlätj derevja s raznoj plotnostju - dlä razlicnyh biomov.
Pri etom odna plotnostj dolžna besšovno perehoditj v druguju, daby derevja ne slipalisj na graniçah biomov.

Takže bylo by neploho imetj derevja raznyh razmerov, rasstanovka kotoryh trebujet razlicnogo rasstojanija meždu nimi.
Vo-pervyh mogut bytj lesa toljko iz malenjkih ili toljko iz boljših derevjev.
Vo-vtoryh mogut bytj lesa s fiksirovannym sootnošenijem malenjkih i boljših derevjev.
V-tretjih mogut bytj lesa s plavnym narastanijem doli boljših derevjev.


### Trebovanija k samim derevjam

Horošo by imetj derevja s nemnogo-otlicajuscimisä parametrami vnutri odnogo vida.
Eto neobhodimo, ctoby derevja ne vyglädili odnoobrazno, no boleje naturaljno.
Razlicatjsä mogut vysota, naklon stvola, detali krony.
Želateljno imetj vozmožnostj delatj kronu assimetricnoj.

Takže nužny derevja s raznym diametrom stvola - kak minimum v odin blok i b tri bloka.


### Vozmožnyj podhod rasstonovki

Odin iz vozmožnyh podhodov, kak možno rasstavitj derevja - "Poisson Disk".
Sam po sebe etot podhod ne ocenj horoš, ibo postanovka ocerednogo dereva trebujet informaçii o sosednih derevjah, cto ne podhodit, t. k. canki generirujutsä nezavisimo.
Poetomu dannyj podhod nado slegka izmenitj.

Možno zaraneje sgenerirovatj takoje raspredelenije dlä nekotoroj prämougoljnoj oblasti i zatem prosto povtorätj jego.
Ctoby ne bylo zametnyh povtorenij, razmer oblasti dolžen bytj okolo 512x512 blokov.
Pri generaçii nado ucestj, cto na kraju tocki v raspredelenii nahodätsä blizko k tockam s drugogo kraja.

Dlä ucöta rasstojanij nado ispoljzovatj ne jevklidovo rasstojanije, a rasstojanije po šestiugoljnoj setke.

Dlä vozmožnosti izmenätj plotnostj možno kak-to pronumerovatj tocki v raspredelenii, tak, cto pri vybore tocek s nomerom N ili menjše dostigalasj nužnaja plotnostj.

Takže algoritm možno modifiçirovatj dlä rasstonovki tocek s razlicnym radiusom - dlä derevjev razlicnogo razmera.


### Generaçija derevjev

Dlä prostoty predpolagajetsä na proçessore sgenerirovatj po nekomu algoritmu nabor modelek derevjev (kak tröhmernyj massiv blokov).
Skažem, 256 štuk.
Na videokarte potom možno prosto vybiratj slucajno odnu iz modelek.

Dannyj podhod dolžen datj horošuju skorostj generaçii cankov, t. k. ne nužno budet pri etom generirovatj sami modeli derevjev, a prosto ispoljzovatj uže gotovyj.


### Generaçija neposredstvenno blokov

Dlä effektivnoj generaçii blokov derevjev v cankah sledujet razdelitj proçess rasstonovki derevjev na dva etapa.

Pervyj etap - opredelitj dlä canka nabor derevjev, nahodäscihsä v nöm (v tom cisle casticno) i dlä každogo dereva jego vysotu - v zavisimosti ot vysoty landšafta v meste stvola.
Pri etom budet proishoditj vyborka iz zaraneje podgotovlennoj povtoräjuscejsä tekstury raspredelenija derevjev.
Spisok derevjev budet ogranicen, t. k. boljše opredelönnogo kolicestva derevjev v odnom canke umestitj neljzä.

Vtoroj etap - v šejdere generaçii blokov mira perebratj vse derevja iz spiska, postrojennogo na predyduscem etape, i rasstavitj bloki, delaja vyborku iz tröhmernoj blokovoj modeli dereva.
Pri etom nado ucestj prioritet blokov derevjev, daby v slucaje s blizkorastuscimi derevjami stvol odnogo dereva mog prorasti cerez kronu drugogo.

Vyšeopisannyj podhod možet prigoditjsä ne toljko dlä derevjev, no i dlä drugih elementov mira s fiksirovannj strukturoj, vrode postrojek.


### Urposcenije otladki

Dlä urposcenija otladki i nastrojki raspredelenija derevjev horošo bylo by imetj vozmožnostj sohranitj promežutocnyje struktury v kakoje-libo izobraženije.
