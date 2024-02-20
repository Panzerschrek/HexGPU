Pod meši cankov nado kak-to vydelätj pamätj - dlä veršin.
Samyj prostoj sposob delatj eto - pod každyj cank vydelitj odinakovoje kolicestvo pamäti s zapasom - ctoby umestitj geometriju daže samyh složnyh cankov.

No takoj podhod obladajet suscestvennym nedostatkom.
Kolicestvo vydelennoj pod cank pamäti ili budet v nekotoryh slucajah nedostatocnym, ili že summarnyj objom pamäti budet ocenj boljšim.
Poetomu nado najti sposob vydelenija pamäti polucše.

Poka cto videtsä sledujuscij sposob rešatj etu problemu:
Snacala dlä canka vycisläjetsä, skoljko veršin na jego geometriju nužno.
Potom proishodit vydelenije pamäti pod nužnoje kolicestvo (s neboljšim zapasom) ili pereispoljzovanije uže susccestvujuscej pamäti.
Daleje proishosit postrojenije geoemtrii s zapisju v vydelennuju pamätj.

Složnostj vyšeopisannogo podhoda sostoit v tom, cto dinamiceskoje raspredelenije pamäti nado kak-to realizovatj na "GPU".
Po suti eto "malloc", "realloc" i "free", rabotajuscije na "GPU".
I nado po suti realizovatj eti funkçii.

K scastju, jestj faktory, kotoryje mogut uprostitj etu realizaçiju:
* Minimaljnyj razmer allokaçii ogranicen _c_chunk_width_ * _c_chunk_width_ * 2 * 4 veršinami - daby zamostitj šestiugoljnikami kak minimum odnu ploskostj canka
* Maksimaljnyj razmer allokaçii ogranicen maksimaljnym vozmožnym kolicestvom veršin v canke - a imenno 65536, skoljko možno adresovatj 16-bitnymi indeksami
* Kolicestvo allokaçij ograniceno maksimaljnym kolicestvom cankov (plüs neboljšoj zapas)
* Polnoje kolicestvo pamäti pod veršiny cankov ograniceno - vydeläjetsä zaraneje bufer dostatocnogo razmera i sverh nego allokaçii nevozmožny

Jestj räd trebovanij k vozmožnoj realizaçii:
* Ona dolžna bytj dostatocno prostoj, daby izbežatj ošibok napisanija koda. S otladkoj na "GPU" dela obstojat vesjma skverno.
* Ona dolžna bytj odnopotocnoj. Mnogopotocnaja realizaçija možet bytj suscestvenno složneje.
* Vremennaja složnostj vseh operaçij dolžna bytj konstantnoj, daby možno bylo v odnom potoke (posledovateljno) dostatocno bystro vydelitj pamätj dlä vseh cankov.
* Kontroljnyje struktury dolžny hranitjsä otdeljno ot bloka pamäti, s kotorym idöt rabota, t. k. v _GLSL_ _reinterpret_cast_ i jemu podobnoje voljnoje obrascenije s pamätju ne vozmožno.

Plüsom budet nezavisimostj koda allokatora ot tipa vydeläjemyh elementov - daby jego možno bylo pereispoljzovatj dlä razlicnyh no shodnyh nužd.

Stoit ucityvatj vozmožnostj neudaci allokaçii.
V takom slucaje nado prosto ispoljzovatj staruju geometriju canka.
Vozmožno potom kogda-nibudj pamätj najdötsä i geometrija canka možet bytj perestrojena.

Stoit podumatj nad rešenijem potençialjnoj problem fragmentaçii pamäti.

Odno iz vozmožnyh uproscenij - vydelenije pamäti toljko blokami razmerom v stepenj dvojki.
Eto možet privesti k nedoispoljzovaniju pamäti (do 25% v srednem, pocti 50% v hudšem slucaje), no možet i uprostitj kod.

Summarnyj razmer bloka pamäti pod vse canki stoit vycislitj kak maksimaljnoje kolicestvo cankov, pomnožennoje na verojatnoje kolicestvo veršin v osobo-täžölyh cankah.
Predpoložim, cto eto možet bytj okolo 12288 veršin na cank.
