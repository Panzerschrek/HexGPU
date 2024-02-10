Kod budet razdelön na dve casti - dvižok, kotoryj zapuskajet nemnogocislennyje "GPU" komandy i šejdernaja castj - s logikoj igry i otrisovkij.
Na storone dvižka takže rabotajet polucenije vvoda i peredaca jego kodu igry.

Takže dvižku budet kakim-libo obrazom neobhodimo sohranätj dannyje mira na disk i zagružatj ih s nego.
Dlä etogo nado v tom cisle umetj s "GPU" polucatj dannyje mira i zalivatj ih tuda.
Navernäka na storone dvižka pridötsä otrabatyvatj peremescenije po miru - s peremescenijem tekuscjego okna cankov v zavisimosti ot položenija igroka.
Takže dvižok dolžen zanimatjsä logikoj starta/zaveršenija igry, redaktirovanijem nastrojek i t. d.

Na storone "GPU" rabota delitsä na kod logiki igry i kod otrisovki.
Rabotatj oni mogut s raznoj castotoj.
Otrisovka - s castotoj obnovlenija ekrana, logika igry - s kakoj-to fiksirovannoj castotoj.

Kod logiki vklücajet v sebä obnovlenije mira igrokom, obnovlenije po sobstvennym pricinam.
Süda že podhodit podscöt osvescenija (poblocnogo), ibo eta informaçija takže neobhodima logike igry.
Krome togo jestj obnovlenije vody, padajuscego peska, rastuscej travy i t. d.

Kod otrisovki vklücajet:
* postrojenije mešej geometrii urovnä
* (vozmožno) postrojenije matriç kamery
* otsecenije cankov po piramide vidimosti
* otrisovku cankov
* podgotovku spiska dinamiceskih objektov
* otrisovku dinamiceskih objektov
* (vozmožno) postproçessing
