Horošo by v proekte imetj aljfa-smešivanije dlä suscnostej vrode vody i stekla.
Takže neploho bylo by imetj tuman - on nužen dlä sokrytija podgruzki cankov v otdalenii.

No realizovatj vsö eto vmeste vesjma složno.
V ideale nado otsortirovatj vse poligony ot daljnih k bližnim i sootvetstvujuscim obrazom ih narisovatj s naloženijem tumana.
V konçe nado naložitj tuman na rezuljtirujuscij kadr (s ucötom glubiny).

Problema zaklücajetsä v tom, cto poligonov možet bytj ocenj mnogo - sotni tysäcj.
Sortirovatj ih bylo by vesjma nakladno.
Eto daže s ucötom vozmožnostej po uskoreniju sortirovki po cankam.
Krome togo podobnaja sortirovka ne vomzožna pri ispoljzovanii raznyh šejderov na poverhnostäh s poluprozracnostju.

Poetomu nado kak-to najti sposob, kak možno risovatj i tuman i poligony so smešivanijem bez podobnoj sortirovki.


Poka cto jestj sledujuscaja ideja:

Smešivanije možno sdelatj takim obrazom, ctoby ono prisutstvovalo toljko vblizi kamery.
V otdalenii poluprozracnyje poligony plavno stanovätsä neprozracnymi.
Daleje etogo rasstojanija polnoj neprozracnosti možno risovatj tuman.
Dannuj podhod rešajet problemu sovmestimosti tumana i prozracnosti, razdeläja ih po rasstojaniju ot kamery.

Glavnym obrazom dannoje plavnoje iscezanije prozracnosti budet ispoljzovatjsä dlä vody.
Eto daže cem-to pohože na to, kak po zakonu Frenelä v otdalenii voda ne propuskajet svet (iz-za ugla vzgläda).
Dumaju, i na cöm-to vrode stekla eto budet smotretjsä snosno.

K poluprozracnym poligonam vblizi kamery možno primenitj tehniku vrode "Weighted Blended Order-Independent Transparency".
Eta tehnika pozvoläjet polucitj boleje-meneje snosnyj, no ne tocnyj rezuljtat bez neobhodimosti sortirovki poligonov.
V nej možet bytj ispoljzovano naznacenije vesov smešivanija na osnove glubiny, cto horošo socitajetsä s ogranicennym rasstojanijem poluprozracnosti.

Rasscöty rasstojanija dlä smešivanija i tumana predlagajetsä delatj pri rasterizaçii poligonov.
Pri etom nado ispoljzovatj cestnoje (sfericeskoje) rasstojanije, a ne glubinu, t. k. v takom podhode netu artefaktov pri krucenii kamery.
Dlä vycislenija plotnosti tumana predlagajetsä ispljzovatj linejnuju ili kvadraticnuju funkçiju, t. k. podobnaja funkçija dolžna bytj bystreje boleje tocnoj eksponençialjnoj.

Çvet tumana predlagajetsä opredelätj po vremeni sutok i pogode i domnožatj jego na jarkostj solnecnogo sveta v bloke, gde raspoložena kamera.
Eto dast svetlyj tuman dnöm na poverhnosti i cörnyj tuman nocju i v pescerah.
No pri etom vsö že mogut bytj situaçii, v kotoryh tuman budet ne ocenj podhodäscej jarkosti.
